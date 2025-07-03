// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <algorithm>
#include <future>
#include <atomic>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "availability_handler_test_globals.hpp"
#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>

class availability_handler_test_manager : public testing::Test {
protected:
    void SetUp() { VSOMEIP_INFO << "Setting up availability_handler_test_manager"; }

    void TearDown() { VSOMEIP_INFO << "Tearing down availability_handler_test_manager"; }
};

/**
 * @test Re-register the availability handler. Check if the handler, after re-registering,
 * becomes available again after the service stops and then resumes offering the service.
 * 1. Server: offer a service
 * 2. Client: register an availability handler for it --> UNKNOWN
 * 3. Client request the service
 * 4. Client: wait for AVAILABLE
 * 5. Client: re-register the availability handler --> AVAILABLE
 * 6. Server: stop the service --> UNAVAILABLE
 * 7. Server: offer again --> new AVAILABLE should appear
 */
TEST_F(availability_handler_test_manager, availability_handler_double_registration) {
    struct shm_remove {
        shm_remove() { boost::interprocess::shared_memory_object::remove("AvailabilityHandlerSteps"); }
        ~shm_remove() { boost::interprocess::shared_memory_object::remove("AvailabilityHandlerSteps"); }
    } remover;

    // Create a shared memory object.
    boost::interprocess::shared_memory_object shm(
            boost::interprocess::open_or_create, // only create
            "AvailabilityHandlerSteps", // name
            boost::interprocess::read_write // read-write mode
    );

    int seconds_to_timeout = 5;

    ASSERT_NO_THROW({
        // Set size
        shm.truncate(sizeof(availability_handler::availability_handler_test_steps));

        // Map the whole shared memory in this process
        boost::interprocess::mapped_region region(
                shm, // What to map
                boost::interprocess::read_write // Map it as read-write
        );

        void* addr = region.get_address();

        availability_handler::availability_handler_test_steps* availability_handler_shared_ =
                new (addr) availability_handler::availability_handler_test_steps;

        std::string exec_cmd_service = "./availability_handler_test_service_starter.sh";
        std::string exec_cmd_client = "./availability_handler_test_client_starter.sh";

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    service_lock(availability_handler_shared_->service_mutex_);

            std::cout << "S1 Launching Service" << std::endl;
            ASSERT_EQ(system(exec_cmd_service.c_str()), 0);

            std::cout << "S1 - Waiting for service to register..." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->service_cv_, service_lock, seconds_to_timeout,
                    availability_handler_shared_->service_status_,
                    availability_handler::availability_handler_test_steps::STATE_REGISTERED);

            std::cout << "Notify S1 - Service registered." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->service_cv_);

            std::cout << "S2 - Waiting for offers to be sent." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->service_cv_, service_lock, seconds_to_timeout,
                    availability_handler_shared_->offer_status_,
                    availability_handler::availability_handler_test_steps::OFFERS_SENT);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    client_lock(availability_handler_shared_->client_mutex_);

            std::cout << "C1 Launching client and register the availability handler" << std::endl;
            ASSERT_EQ(system(exec_cmd_client.c_str()), 0);

            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->client_status_,
                    availability_handler::availability_handler_test_steps::STATE_REGISTERED);

            std::cout << "C1 - Waiting to receive the NOT AVAILABLE (UNKNOWN state)." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->availability_status_,
                    availability_handler::availability_handler_test_steps::UNAVAILABILITY_RECEIVED);

            std::cout << "Notify C1 - Client registered." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->client_cv_);

            std::cout << "C2 - Waiting for requests to be sent." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->request_status_,
                    availability_handler::availability_handler_test_steps::REQUESTS_SENT);

            std::cout << "C2 - Waiting for availability to be received." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->availability_status_,
                    availability_handler::availability_handler_test_steps::AVAILABILITY_RECEIVED);

            std::cout << "Notify C2 - Client availability received." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->client_cv_);

            std::cout << "C3 - Waiting for the handler to re-register." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->reregister_status_,
                    availability_handler::availability_handler_test_steps::REREGISTER_DONE);

            std::cout << "C4 - Waiting for availability to be received." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->availability_status_,
                    availability_handler::availability_handler_test_steps::AVAILABILITY_RECEIVED);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    service_lock(availability_handler_shared_->service_mutex_);
        
            std::cout << "Notify S2 - Client availability received." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->service_cv_);

            std::cout << "S3 - Waiting for stop offers to be sent" << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->service_cv_, service_lock, seconds_to_timeout,
                    availability_handler_shared_->offer_status_,
                    availability_handler::availability_handler_test_steps::STOP_OFFERS_SENT);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    client_lock(availability_handler_shared_->client_mutex_);

            std::cout << "C5 - Waiting for unavailability to be received." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->availability_status_,
                    availability_handler::availability_handler_test_steps::UNAVAILABILITY_RECEIVED);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    service_lock(availability_handler_shared_->service_mutex_);
        
            std::cout << "Notify S3 - Client unavailability received." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->service_cv_);

            std::cout << "S4 - Waiting for offers to be sent" << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->service_cv_, service_lock, seconds_to_timeout,
                    availability_handler_shared_->offer_status_,
                    availability_handler::availability_handler_test_steps::OFFERS_SENT);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                client_lock(availability_handler_shared_->client_mutex_);

            std::cout << "C6 - Waiting for availability to be received." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->availability_status_,
                    availability_handler::availability_handler_test_steps::AVAILABILITY_RECEIVED);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    service_lock(availability_handler_shared_->service_mutex_);

            std::cout << "Notify S4 - Client availability received again" << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->service_cv_);

            std::cout << "S5 - Waiting for stop offers to be sent" << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->service_cv_, service_lock, seconds_to_timeout,
                    availability_handler_shared_->offer_status_,
                    availability_handler::availability_handler_test_steps::STOP_OFFERS_SENT);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                client_lock(availability_handler_shared_->client_mutex_);

            std::cout << "Notify C6 - Services are being released." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->client_cv_);

            std::cout << "C7 - Waiting for client to be deregistered" << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->client_cv_, client_lock, seconds_to_timeout,
                    availability_handler_shared_->client_status_,
                    availability_handler::availability_handler_test_steps::STATE_DEREGISTERED);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    service_lock(availability_handler_shared_->service_mutex_);
        
            std::cout << "Notify S5 - Client is deregistered." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->service_cv_);

            std::cout << "S6 - Waiting for service to be deregistered." << std::endl;
            availability_handler::availability_handler_utils::wait_and_check_unlocked(
                    availability_handler_shared_->service_cv_, service_lock, seconds_to_timeout,
                    availability_handler_shared_->service_status_,
                    availability_handler::availability_handler_test_steps::STATE_DEREGISTERED);
        }

        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    service_lock(availability_handler_shared_->service_mutex_);
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>
                    client_lock(availability_handler_shared_->client_mutex_);
 
            std::cout << "Notify both - Service is deregistered. All applications deregistered going down." << std::endl;
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->client_cv_);
            availability_handler::availability_handler_utils::notify_component_unlocked(availability_handler_shared_->service_cv_);
        }

        // Explicitly destroy the object that was created with placement new
        availability_handler_shared_->~availability_handler_test_steps();
    });
}

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif

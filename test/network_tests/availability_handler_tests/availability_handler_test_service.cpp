// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <fstream>
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

class availability_handler_test_service : public vsomeip_utilities::base_logger {
public:
    availability_handler_test_service() : vsomeip_utilities::base_logger("AHTS", "AVAILABILITY HANDLER TEST SERVICE") {
        app_ = vsomeip::runtime::get()->create_application("availability_handler_test_service");
        SERVICE_ID = availability_handler::Service_ID;
        INSTANCE_ID = availability_handler::Instance_ID;
    }

    void run() {
        // Create a shared memory object.
        boost::interprocess::shared_memory_object shm(boost::interprocess::open_only, // only create
                                                      "AvailabilityHandlerSteps", // name
                                                      boost::interprocess::read_write // read-write mode
        );

        ASSERT_NO_THROW({
            // Map the whole shared memory in this process
            boost::interprocess::mapped_region region(shm, // What to map
                                                      boost::interprocess::read_write // Map it as read-write
            );

            void* addr = region.get_address();

            availability_handler_shared_ = static_cast<availability_handler::availability_handler_test_steps*>(addr);

            init();

            VSOMEIP_INFO << "Availability Handler Test Service is initializing...";

            std::promise<bool> its_promise;
            application_thread_ = std::thread([&]() {
                its_promise.set_value(true);
                app_->start();
            });
            EXPECT_TRUE(its_promise.get_future().get());

            VSOMEIP_INFO << "Availability Handler Test Service is started.";

            {
                std::unique_lock local_lock(local_service_mutex_);
                local_service_cv_.wait(local_lock, [&]() { return state_ == vsomeip::state_type_e::ST_REGISTERED; });

                VSOMEIP_INFO << "Availability Handler Test Service is registered.";

                if (state_ == vsomeip::state_type_e::ST_REGISTERED) {
                    send_offers();

                    send_stop_offers();

                    send_offers();
                }
                stop();
            }
        });
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::thread application_thread_;
    vsomeip::service_t SERVICE_ID;
    vsomeip::instance_t INSTANCE_ID;
    vsomeip::state_type_e state_ = vsomeip::state_type_e::ST_DEREGISTERED;
    availability_handler::availability_handler_test_steps* availability_handler_shared_;
    std::condition_variable local_service_cv_;
    std::mutex local_service_mutex_;

    void init() {
        ASSERT_TRUE(app_->init()) << "[Service] Couldn't initialize application";

        app_->register_state_handler(std::bind(&availability_handler_test_service::on_state, this, std::placeholders::_1));
    }

    void stop() {
        send_stop_offers();

        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->service_mutex_);

        availability_handler_shared_->service_status_ = availability_handler::availability_handler_test_steps::STATE_DEREGISTERED;

        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->service_cv_, lock);

        app_->clear_all_handler();

        app_->stop();

        application_thread_.join();
    }

    void on_state(vsomeip::state_type_e _state) {
        std::scoped_lock local_lock(local_service_mutex_);
        state_ = _state;

        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (state_ == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");

        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->service_mutex_);

        if (state_ == vsomeip::state_type_e::ST_REGISTERED) {
            availability_handler_shared_->service_status_ = availability_handler::availability_handler_test_steps::STATE_REGISTERED;
        } else {
            availability_handler_shared_->service_status_ = availability_handler::availability_handler_test_steps::STATE_NOT_REGISTERED;
        }

        local_service_cv_.notify_one();

        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->service_cv_, lock);
    }

    void send_offers() {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->service_mutex_);

        VSOMEIP_INFO << "Sending offers for services...";
        availability_handler_shared_->offer_status_ = availability_handler::availability_handler_test_steps::OFFERS_NOT_SENT;

        app_->offer_service(SERVICE_ID, INSTANCE_ID);

        availability_handler_shared_->offer_status_ = availability_handler::availability_handler_test_steps::OFFERS_SENT;

        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->service_cv_, lock);
    }

    void send_stop_offers() {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->service_mutex_);

        VSOMEIP_INFO << "Stopping offers for services...";
        availability_handler_shared_->offer_status_ = availability_handler::availability_handler_test_steps::STOP_OFFERS_NOT_SENT;

        app_->stop_offer_service(SERVICE_ID, INSTANCE_ID);

        availability_handler_shared_->offer_status_ = availability_handler::availability_handler_test_steps::STOP_OFFERS_SENT;

        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->service_cv_, lock);
    }
};

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main() {
    availability_handler_test_service service;
    service.run();
}
#endif

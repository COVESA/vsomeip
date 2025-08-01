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

class availability_handler_test_client : public vsomeip_utilities::base_logger {
public:
    availability_handler_test_client() : vsomeip_utilities::base_logger("AHTC", "AVAILABILITY HANDLER TEST CLIENT") {
        app_ = vsomeip::runtime::get()->create_application("availability_handler_test_client");
        SERVICE_ID = availability_handler::Service_ID;
        INSTANCE_ID = availability_handler::Instance_ID;
    }

    void run() {
        // Create a shared memory object.
        boost::interprocess::shared_memory_object shm(boost::interprocess::open_only, // only open
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

            std::promise<bool> its_promise;
            application_thread_ = std::thread([&]() {
                its_promise.set_value(true);
                app_->start();
            });
            EXPECT_TRUE(its_promise.get_future().get());

            {
                std::unique_lock local_lock(local_client_mutex_);
                local_client_cv_.wait(local_lock, [&]() { return state_ == vsomeip::state_type_e::ST_REGISTERED; });

                if (state_ == vsomeip::state_type_e::ST_REGISTERED) {
                    send_requests();

                    register_availability_handler();
                }
                stop();
            }
        });
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::thread application_thread_;
    vsomeip::state_type_e state_ = vsomeip::state_type_e::ST_DEREGISTERED;
    vsomeip::service_t SERVICE_ID;
    vsomeip::instance_t INSTANCE_ID;
    availability_handler::availability_handler_test_steps* availability_handler_shared_;
    std::condition_variable local_client_cv_;
    std::mutex local_client_mutex_;

    void init() {
        ASSERT_TRUE(app_->init()) << "[Client] Couldn't initialize application";

        app_->register_state_handler(std::bind(&availability_handler_test_client::on_state, this, std::placeholders::_1));

        app_->register_availability_handler(SERVICE_ID, INSTANCE_ID,
                                            std::bind(&availability_handler_test_client::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3));
    }

    void stop() {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->client_mutex_);

        app_->release_service(SERVICE_ID, INSTANCE_ID);

        availability_handler_shared_->client_status_ = availability_handler::availability_handler_test_steps::STATE_DEREGISTERED;

        app_->clear_all_handler();
        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->client_cv_, lock);

        app_->stop();

        application_thread_.join();
    }

    void register_availability_handler() {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->client_mutex_);

        availability_handler_shared_->availability_status_ =
                availability_handler::availability_handler_test_steps::AVAILABILITY_NOT_RECEIVED;

        availability_handler_shared_->reregister_status_ = availability_handler::availability_handler_test_steps::REREGISTER_NOT_DONE;

        app_->register_availability_handler(SERVICE_ID, INSTANCE_ID,
                                            std::bind(&availability_handler_test_client::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3));

        availability_handler_shared_->reregister_status_ = availability_handler::availability_handler_test_steps::REREGISTER_DONE;

        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->client_cv_, lock);
    }

    void on_state(vsomeip::state_type_e _state) {
        std::scoped_lock local_lock(local_client_mutex_);
        state_ = _state;

        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");

        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->client_mutex_);

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            availability_handler_shared_->client_status_ = availability_handler::availability_handler_test_steps::STATE_REGISTERED;
        } else {
            availability_handler_shared_->client_status_ = availability_handler::availability_handler_test_steps::STATE_NOT_REGISTERED;
        }

        local_client_cv_.notify_one();

        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->client_cv_, lock);
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->client_mutex_);

        availability_handler_shared_->availability_status_ =
                availability_handler::availability_handler_test_steps::AVAILABILITY_NOT_RECEIVED;

        VSOMEIP_INFO << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << _instance << "] is "
                     << (_is_available ? "available." : "NOT available.");

        if (_is_available) {
            availability_handler_shared_->availability_status_ =
                    availability_handler::availability_handler_test_steps::AVAILABILITY_RECEIVED;
        } else {
            availability_handler_shared_->availability_status_ =
                    availability_handler::availability_handler_test_steps::UNAVAILABILITY_RECEIVED;
        }
    }

    void send_requests() {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(availability_handler_shared_->client_mutex_);

        availability_handler_shared_->request_status_ = availability_handler::availability_handler_test_steps::REQUESTS_NOT_SENT;

        app_->request_service(SERVICE_ID, INSTANCE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        availability_handler_shared_->request_status_ = availability_handler::availability_handler_test_steps::REQUESTS_SENT;

        availability_handler::availability_handler_utils::notify_and_wait_unlocked(availability_handler_shared_->client_cv_, lock);
    }
};

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main() {
    availability_handler_test_client client;
    client.run();
}
#endif

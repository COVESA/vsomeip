// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "offer_test_globals.hpp"


class offer_test_service_availability_checker {
public:
	offer_test_service_availability_checker(struct offer_test::service_info _service_info) :
			service_info_(_service_info),
            app_(vsomeip::runtime::get()->create_application()),
            wait_until_registered_(true),
            wait_for_stop_(true),
            stop_thread_(std::bind(&offer_test_service_availability_checker::wait_for_stop, this)) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(
                std::bind(&offer_test_service_availability_checker::on_state, this,
                        std::placeholders::_1));

        // register availability for all other services and request their event.
        app_->register_availability_handler(service_info_.service_id,
                service_info_.instance_id,
                std::bind(&offer_test_service_availability_checker::on_availability, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
        app_->request_service(service_info_.service_id,
              service_info_.instance_id);

        app_->start();
    }

    ~offer_test_service_availability_checker() {
        stop_thread_.join();
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "MY Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_registered_ = false;
            condition_.notify_one();
        }
    }

    void on_availability(vsomeip::service_t _service,
                         vsomeip::instance_t _instance, bool _is_available) {
        VSOMEIP_INFO << "MY Service [" << std::setw(4)
        << std::setfill('0') << std::hex << _service << "." << _instance
        << "] is " << (_is_available ? "available":"not available") << ".";
        std::lock_guard<std::mutex> its_lock(mutex_);
        if(_is_available) {
            wait_for_stop_ = false;
            stop_condition_.notify_one();
        }
    }

    void wait_for_stop() {
        VSOMEIP_INFO << " MY offer_test_service_availability_check wait_for_stop() ";
        std::unique_lock<std::mutex> its_lock(stop_mutex_);
        while (wait_for_stop_) {
            stop_condition_.wait(its_lock);
        }
        //VSOMEIP_INFO << "[" << std::setw(4) << std::setfill('0') << std::hex
        //        << client_number_ << "] all services are available. Going down";
        VSOMEIP_INFO << " MY offer_test_service_availability_check is going down ";
        app_->clear_all_handler();
        app_->stop();
    }

private:
    struct offer_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;

    bool wait_until_registered_;
    std::mutex mutex_;
    std::condition_variable condition_;

    bool wait_for_stop_;
    std::mutex stop_mutex_;
    std::condition_variable stop_condition_;
    std::thread stop_thread_;
};

TEST(someip_offer_test_external, wait_for_availability_and_exit)
{
    offer_test_service_availability_checker its_sample(
         offer_test::service);
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif

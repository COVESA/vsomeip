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
#include <atomic>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include "../../implementation/logging/include/logger.hpp"

#include "offer_test_globals.hpp"

class offer_test_big_sd_msg_service {
public:
    offer_test_big_sd_msg_service(struct offer_test::service_info _service_info) :
            service_info_(_service_info),
            // service with number 1 uses "vsomeipd" as application name
            // this way the same json file can be reused for all local tests
            // including the ones with vsomeipd
            app_(vsomeip::runtime::get()->create_application("offer_test_big_sd_msg_service")),
            counter_(0),
            wait_until_registered_(true),
            shutdown_method_called_(false),
            offer_thread_(std::bind(&offer_test_big_sd_msg_service::run, this)) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(
                std::bind(&offer_test_big_sd_msg_service::on_state, this,
                        std::placeholders::_1));

        // offer field
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(offer_test::big_msg_eventgroup_id);
        for (std::uint16_t s = 1; s <= offer_test::big_msg_number_services; s++) {
            app_->offer_event(s, 0x1,
                    offer_test::big_msg_event_id, its_eventgroups, false);
        }

        app_->register_message_handler(vsomeip::ANY_SERVICE,
                vsomeip::ANY_INSTANCE, service_info_.shutdown_method_id,
                std::bind(&offer_test_big_sd_msg_service::on_shutdown_method_called, this,
                        std::placeholders::_1));
        app_->start();
    }

    ~offer_test_big_sd_msg_service() {
        offer_thread_.join();
    }

    void offer() {
        for (std::uint16_t s = 1; s <= offer_test::big_msg_number_services; s++) {
            app_->offer_service(s,0x1,0x1,0x1);
        }
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_registered_ = false;
            condition_.notify_one();
        }
    }

    void on_shutdown_method_called(const std::shared_ptr<vsomeip::message> &_message) {
        app_->send(vsomeip::runtime::get()->create_response(_message));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        VSOMEIP_WARNING << "************************************************************";
        VSOMEIP_WARNING << "Shutdown method called -> going down!";
        VSOMEIP_WARNING << "************************************************************";
        shutdown_method_called_ = true;
        for (std::uint16_t s = 1; s <= offer_test::big_msg_number_services; s++) {
            app_->stop_offer_service(s,0x1,0x1,0x1);
        }
        app_->clear_all_handler();
        app_->stop();
    }

    void run() {
        VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                << service_info_.service_id << "] Running";
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (wait_until_registered_) {
            condition_.wait(its_lock);
        }

        VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                << service_info_.service_id << "] Offering";
        offer();
    }

private:
    struct offer_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;
    std::uint32_t counter_;

    bool wait_until_registered_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_method_called_;
    std::thread offer_thread_;
};

TEST(someip_offer_test_big_sd_msg, notify_increasing_counter)
{
    offer_test_big_sd_msg_service its_sample(offer_test::service);
}


#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif

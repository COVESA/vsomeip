// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#include <csignal>
#endif
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>

#include <gtest/gtest.h>
#include "../../implementation/logging/include/logger.hpp"

#include <vsomeip/vsomeip.hpp>

#include "subscribe_notify_test_globals.hpp"

class subscribe_notify_test_one_event_two_eventgroups_service {
public:
    subscribe_notify_test_one_event_two_eventgroups_service(subscribe_notify_test::service_info _info) :
            app_(vsomeip::runtime::get()->create_application()),
            wait_for_shutdown_(true),
            info_(_info),
            notify_thread_(std::bind(&subscribe_notify_test_one_event_two_eventgroups_service::wait_for_shutdown, this)) {
    }

    ~subscribe_notify_test_one_event_two_eventgroups_service() {
        notify_thread_.join();
    }

    bool init() {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return false;
        }
        app_->register_state_handler(
                std::bind(&subscribe_notify_test_one_event_two_eventgroups_service::on_state, this,
                        std::placeholders::_1));

        app_->register_message_handler(
                info_.service_id,
                info_.instance_id,
                subscribe_notify_test::set_method_id,
                std::bind(&subscribe_notify_test_one_event_two_eventgroups_service::on_set, this,
                          std::placeholders::_1));

        app_->register_message_handler(
                info_.service_id,
                info_.instance_id,
                info_.method_id,
                std::bind(&subscribe_notify_test_one_event_two_eventgroups_service::on_message, this,
                          std::placeholders::_1));

        app_->register_message_handler(
                info_.service_id,
                info_.instance_id,
                subscribe_notify_test::shutdown_method_id,
                std::bind(&subscribe_notify_test_one_event_two_eventgroups_service::on_shutdown, this,
                          std::placeholders::_1));

        std::set<vsomeip::eventgroup_t> its_groups;
        // the service offers three events in two eventgroups
        // one of the events is in both eventgroups
        its_groups.insert(info_.eventgroup_id);
        app_->offer_event(info_.service_id, info_.instance_id,
                info_.event_id, its_groups, true);
        app_->offer_event(info_.service_id, info_.instance_id,
                 static_cast<vsomeip::event_t>(info_.event_id + 2), its_groups, true);
        its_groups.erase(info_.eventgroup_id);
        its_groups.insert(static_cast<vsomeip::eventgroup_t>(info_.eventgroup_id + 1));
        app_->offer_event(info_.service_id, info_.instance_id,
                static_cast<vsomeip::event_t>(info_.event_id + 1), its_groups, true);
        app_->offer_event(info_.service_id, info_.instance_id,
                static_cast<vsomeip::event_t>(info_.event_id + 2), its_groups, true);
        payload_ = vsomeip::runtime::get()->create_payload();

        return true;
    }

    void start() {
        app_->start();
    }

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    /*
     * Handle signal to shutdown
     */
    void stop() {
        {
            std::lock_guard<std::mutex> its_lock(shutdown_mutex_);
            wait_for_shutdown_ = false;
            shutdown_condition_.notify_one();
        }
        app_->clear_all_handler();
        stop_offer();
        notify_thread_.join();
        app_->stop();
    }
#endif

    void offer() {
        app_->offer_service(info_.service_id, info_.instance_id);
    }

    void stop_offer() {
        app_->stop_offer_service(info_.service_id, info_.instance_id);
    }

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.") << std::endl;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            offer();
        }
    }

    void on_shutdown(const std::shared_ptr<vsomeip::message> &_message) {
        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_message);
        its_response->set_payload(payload_);
        app_->send(its_response, true);
        {
            std::lock_guard<std::mutex> its_lock(shutdown_mutex_);
            wait_for_shutdown_ = false;
            shutdown_condition_.notify_one();
        }
    }

    void on_set(const std::shared_ptr<vsomeip::message> &_message) {
        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_message);
        payload_ = _message->get_payload();
        its_response->set_payload(payload_);
        app_->send(its_response, true);
        app_->notify(info_.service_id, info_.instance_id, info_.event_id, payload_);
        app_->notify(info_.service_id, info_.instance_id, static_cast<vsomeip::event_t>(info_.event_id + 1), payload_);
        app_->notify(info_.service_id, info_.instance_id, static_cast<vsomeip::event_t>(info_.event_id + 2), payload_);
    }

    void on_message(const std::shared_ptr<vsomeip::message> &_message) {
        app_->send(vsomeip::runtime::get()->create_response(_message),true);
    }

    void wait_for_shutdown() {
        {
            std::unique_lock<std::mutex> its_lock(shutdown_mutex_);
            while (wait_for_shutdown_) {
                shutdown_condition_.wait(its_lock);
            }
            wait_for_shutdown_= true;
        }

        app_->clear_all_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stop_offer();
        app_->stop();
    }

private:
    std::shared_ptr<vsomeip::application> app_;

    std::mutex shutdown_mutex_;
    bool wait_for_shutdown_;
    std::condition_variable shutdown_condition_;

    std::shared_ptr<vsomeip::payload> payload_;

    subscribe_notify_test::service_info info_;

    std::thread notify_thread_;
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    subscribe_notify_test_one_event_two_eventgroups_service *its_service_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_service_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_service_ptr->stop();
    }
#endif


TEST(someip_subscribe_notify_test_one_event_two_eventgroups, wait_for_attribute_set)
{
    subscribe_notify_test_one_event_two_eventgroups_service its_service(
            subscribe_notify_test::service_info_subscriber_based_notification);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_service_ptr = &its_service;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    if (its_service.init()) {
        its_service.start();
    }
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif



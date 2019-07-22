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
#include "../../implementation/logging/include/logger.hpp"

#include "../debug_diag_job_plugin_tests/debug_diag_job_plugin_test_globals.hpp"

static int service_number(0);

class debug_diag_job_plugin_test_service {
public:
    debug_diag_job_plugin_test_service(struct debug_diag_job_plugin_test::service_info _service_info_local,
                                       struct debug_diag_job_plugin_test::service_info _service_info_remote,
                               std::uint32_t _events_to_offer) :
            service_info_local_(_service_info_local),
            service_info_remote_(_service_info_remote),
            app_(vsomeip::runtime::get()->create_application()),
            wait_until_registered_(true),
            events_to_offer_(_events_to_offer),
            offer_thread_(std::bind(&debug_diag_job_plugin_test_service::run, this)) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(
                std::bind(&debug_diag_job_plugin_test_service::on_state, this,
                        std::placeholders::_1));
        app_->register_message_handler(service_info_local_.service_id,
                service_info_local_.instance_id, service_info_local_.method_id,
                std::bind(&debug_diag_job_plugin_test_service::on_shutdown_method_called,
                          this, std::placeholders::_1));
        app_->register_message_handler(service_info_remote_.service_id,
                service_info_remote_.instance_id, service_info_remote_.method_id,
                std::bind(&debug_diag_job_plugin_test_service::on_shutdown_method_called,
                          this, std::placeholders::_1));

        for (auto si : {service_info_local_, service_info_remote_}) {
            // offer field
            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(si.eventgroup_id);
            for (std::uint16_t i = 0; i < events_to_offer_; i++) {
                app_->offer_event(si.service_id, si.instance_id,
                        static_cast<vsomeip::event_t>(si.event_id + i), its_eventgroups, true);
            }

            // set value to field
            std::shared_ptr<vsomeip::payload> its_payload =
                    vsomeip::runtime::get()->create_payload();
            vsomeip::byte_t its_data[2] = {static_cast<vsomeip::byte_t>((si.service_id & 0xFF00) >> 8),
                    static_cast<vsomeip::byte_t>((si.service_id & 0xFF))};
            its_payload->set_data(its_data, 2);
            for (std::uint16_t i = 0; i < events_to_offer_; i++) {
                app_->notify(si.service_id, si.instance_id,
                        static_cast<vsomeip::event_t>(si.event_id + i), its_payload);
            }
        }

        if (service_number == 1) {
            // the debug mode service only needs to be offered one time
            // debug mode service and event

            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(1);
            app_->offer_event(0xfc10, 0x80, 0x8001, its_eventgroups, true);
            std::shared_ptr<vsomeip::payload> its_payload =
                    vsomeip::runtime::get()->create_payload();
            vsomeip::byte_t data[1] = {0x1};
            its_payload->set_data(data, 1);
            app_->notify(0xfc10, 0x80, 0x8001, its_payload);
        }
        app_->start();
    }

    ~debug_diag_job_plugin_test_service() {
        offer_thread_.join();
    }

    void offer() {
        if (service_number == 1) {
            // the debug mode service only needs to be offered one time
            app_->offer_service(0xfc10, 0x80, 1, vsomeip::ANY_MINOR);
        }
        for (auto si : {service_info_local_, service_info_remote_}) {
            app_->offer_service(si.service_id, si.instance_id);
            VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                    << si.service_id << "] Offering";
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
        static bool shutdown_called_local = false;
        static bool shutdown_called_remote = false;
        if (_message->get_method() == service_info_local_.method_id) {
            shutdown_called_local = true;
        } else if (_message->get_method() == service_info_remote_.method_id) {
            shutdown_called_remote = true;
        }
        if (shutdown_called_local && shutdown_called_remote) {
            VSOMEIP_WARNING << "Shutdown method called -> going down!";
            for (auto si : {service_info_local_, service_info_remote_}) {
                app_->stop_offer_service(si.service_id, si.instance_id);
                VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0')
                        << std::hex << si.service_id << "] Stop Offering";
            }
            app_->clear_all_handler();
            app_->stop();
        }
    }

    void run() {
        VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                << service_info_local_.service_id << "] Running";
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (wait_until_registered_) {
            condition_.wait(its_lock);
        }
        offer();
    }

private:
    debug_diag_job_plugin_test::service_info service_info_local_;
    debug_diag_job_plugin_test::service_info service_info_remote_;
    std::shared_ptr<vsomeip::application> app_;

    bool wait_until_registered_;
    std::uint32_t events_to_offer_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread offer_thread_;
};

static std::uint32_t offer_multiple_events;

TEST(someip_debug_diag_job_plugin_test, offer_one_remote_and_one_local_service)
{
        debug_diag_job_plugin_test_service its_sample(
                debug_diag_job_plugin_test::service_infos_local[service_number],
                debug_diag_job_plugin_test::service_infos_remote[service_number],
                offer_multiple_events);
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if(argc < 2) {
        std::cerr << "Please specify a service number and subscription type, like: " << argv[0] << " 2 SAME_SERVICE_ID" << std::endl;
        std::cerr << "Valid service numbers are in the range of [1,2]" << std::endl;
        return 1;
    }

    service_number = std::stoi(std::string(argv[1]), nullptr);

    offer_multiple_events = 1;
    return RUN_ALL_TESTS();
}
#endif

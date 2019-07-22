// Copyright (C) 2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "security_config_plugin_test_service.hpp"
#include "../security_config_plugin_tests/security_config_plugin_test_globals.hpp"

security_config_plugin_test_service::security_config_plugin_test_service() :
    app_(vsomeip::runtime::get()->create_application()),
    is_registered_(false),
    blocked_(false),
    number_of_received_messages_(0),
    offer_thread_(std::bind(&security_config_plugin_test_service::run, this)) {
}

bool security_config_plugin_test_service::init() {
    std::lock_guard<std::mutex> its_lock(mutex_);

    if (!app_->init()) {
        ADD_FAILURE() << "Couldn't initialize application";
        return false;
    }
    app_->register_message_handler(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
            vsomeip::ANY_METHOD,
            std::bind(&security_config_plugin_test_service::on_message, this,
                    std::placeholders::_1));

    app_->register_message_handler(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
            vsomeip::ANY_METHOD,
            std::bind(&security_config_plugin_test_service::on_message, this,
                    std::placeholders::_1));

    app_->register_message_handler(security_config_plugin_test::security_config_test_serviceinfo_3.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_3.instance_id,
            security_config_plugin_test::security_config_test_serviceinfo_3.method_id,
            std::bind(&security_config_plugin_test_service::on_message_try_offer, this,
                    std::placeholders::_1));

    app_->register_message_handler(security_config_plugin_test::security_config_test_serviceinfo_3.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_3.instance_id,
            vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN,
            std::bind(&security_config_plugin_test_service::on_message_shutdown, this,
                    std::placeholders::_1));

    app_->register_state_handler(
            std::bind(&security_config_plugin_test_service::on_state, this,
                    std::placeholders::_1));

    // offer allowed field 0x8001 eventgroup 0x01
    std::set<vsomeip::eventgroup_t> its_eventgroups;
    its_eventgroups.insert(security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id);

    app_->offer_event(security_config_plugin_test::security_config_test_serviceinfo_1.service_id, security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                static_cast<vsomeip::event_t>(0x8001), its_eventgroups, true);

    // offer never allowed field 0x8004 eventgroup 0x01
    app_->offer_event(security_config_plugin_test::security_config_test_serviceinfo_1.service_id, security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                static_cast<vsomeip::event_t>(0x8004), its_eventgroups, true);


    // offer allowed field 0x8002 eventgroup 0x01
    app_->offer_event(security_config_plugin_test::security_config_test_serviceinfo_2.service_id, security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                static_cast<vsomeip::event_t>(0x8002), its_eventgroups, true);

    // set value to fields
    std::shared_ptr<vsomeip::payload> its_payload =
            vsomeip::runtime::get()->create_payload();
    vsomeip::byte_t its_data[2] = {static_cast<vsomeip::byte_t>((security_config_plugin_test::security_config_test_serviceinfo_1.service_id & 0xFF00) >> 8),
            static_cast<vsomeip::byte_t>((security_config_plugin_test::security_config_test_serviceinfo_1.service_id & 0xFF))};
    its_payload->set_data(its_data, 2);

    app_->notify(security_config_plugin_test::security_config_test_serviceinfo_1.service_id, security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
            static_cast<vsomeip::event_t>(0x8001), its_payload);

    app_->notify(security_config_plugin_test::security_config_test_serviceinfo_1.service_id, security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
            static_cast<vsomeip::event_t>(0x8004), its_payload);

    app_->notify(security_config_plugin_test::security_config_test_serviceinfo_2.service_id, security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
            static_cast<vsomeip::event_t>(0x8002), its_payload);

    return true;
}

void security_config_plugin_test_service::start() {
    VSOMEIP_INFO << "Starting...";
    app_->start();
}

void security_config_plugin_test_service::stop() {
    VSOMEIP_INFO << "Stopping...";
    app_->clear_all_handler();
    app_->stop();
}

void security_config_plugin_test_service::join_offer_thread() {
    if (offer_thread_.joinable()) {
        offer_thread_.join();
    }
}

void security_config_plugin_test_service::offer() {
    // offer the initially in global security config file denied service / instance
    app_->offer_service(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_1.instance_id);

    // offer the initially in global security config file denied service / instance
    app_->offer_service(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_2.instance_id);

    // offer the allowed control service / instance
    app_->offer_service(security_config_plugin_test::security_config_test_serviceinfo_3.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_3.instance_id);
}

void security_config_plugin_test_service::stop_offer() {
    app_->stop_offer_service(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_1.instance_id);
    app_->stop_offer_service(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_2.instance_id);
    app_->stop_offer_service(security_config_plugin_test::security_config_test_serviceinfo_3.service_id,
            security_config_plugin_test::security_config_test_serviceinfo_3.instance_id);
}

void security_config_plugin_test_service::on_state(vsomeip::state_type_e _state) {
    VSOMEIP_INFO << "Application " << app_->get_name() << " is "
            << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." :
                    "deregistered.");

    if(_state == vsomeip::state_type_e::ST_REGISTERED) {
        if(!is_registered_) {
            is_registered_ = true;
            std::lock_guard<std::mutex> its_lock(mutex_);
            blocked_ = true;
            // "start" the run method thread
            condition_.notify_one();
        }
    }
    else {
        is_registered_ = false;
    }
}

void security_config_plugin_test_service::on_message(const std::shared_ptr<vsomeip::message>& _request) {
    VSOMEIP_INFO << "Received a message with Client/Session [" << std::setw(4)
        << std::setfill('0') << std::hex << _request->get_client() << "/"
        << std::setw(4) << std::setfill('0') << std::hex
        << _request->get_session() << "] method: " << _request->get_method() ;

    // send response
    std::shared_ptr<vsomeip::message> its_response =
            vsomeip::runtime::get()->create_response(_request);

    app_->send(its_response, true);

    number_of_received_messages_++;
    if(number_of_received_messages_ == vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_SECURITY_TESTS) {
        VSOMEIP_INFO << "Received all messages!";
    }
}

void security_config_plugin_test_service::on_message_shutdown(
        const std::shared_ptr<vsomeip::message>& _request) {
    (void)_request;
    VSOMEIP_INFO << "Shutdown method was called, going down now.";
    stop();
}

void security_config_plugin_test_service::on_message_try_offer(
        const std::shared_ptr<vsomeip::message>& _request) {
    (void)_request;
    VSOMEIP_INFO << "Try offering method was called.";
    stop_offer();
    offer();

    // set value to fields
    std::shared_ptr<vsomeip::payload> its_payload =
            vsomeip::runtime::get()->create_payload();
    vsomeip::byte_t its_data[2] = {static_cast<vsomeip::byte_t>((security_config_plugin_test::security_config_test_serviceinfo_1.service_id & 0xFF00) >> 8),
            static_cast<vsomeip::byte_t>((security_config_plugin_test::security_config_test_serviceinfo_1.service_id & 0xFF))};
    its_payload->set_data(its_data, 2);
    app_->notify(security_config_plugin_test::security_config_test_serviceinfo_1.service_id, security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
            static_cast<vsomeip::event_t>(0x8001), its_payload);

    app_->notify(security_config_plugin_test::security_config_test_serviceinfo_1.service_id, security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
            static_cast<vsomeip::event_t>(0x8004), its_payload);

    // set value to fields
    std::shared_ptr<vsomeip::payload> its_payload_2 =
            vsomeip::runtime::get()->create_payload();
    vsomeip::byte_t its_data_2[2] = {static_cast<vsomeip::byte_t>((security_config_plugin_test::security_config_test_serviceinfo_2.service_id & 0xFF00) >> 8),
            static_cast<vsomeip::byte_t>((security_config_plugin_test::security_config_test_serviceinfo_2.service_id & 0xFF))};
    its_payload_2->set_data(its_data_2, 2);
    app_->notify(security_config_plugin_test::security_config_test_serviceinfo_2.service_id, security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
            static_cast<vsomeip::event_t>(0x8002), its_payload_2);

}

void security_config_plugin_test_service::run() {
    std::unique_lock<std::mutex> its_lock(mutex_);
    while (!blocked_)
        condition_.wait(its_lock);

   offer();
}

TEST(someip_security_test, basic_security_update_) {
    security_config_plugin_test_service test_service;
    if (test_service.init()) {
        test_service.start();
        test_service.join_offer_thread();
    }
}

#ifndef _WIN32
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif

// Copyright (C) 2015-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "restart_routing_test_service.hpp"

routing_restart_test_service::routing_restart_test_service() :
    app_(vsomeip::runtime::get()->create_application()),
    is_registered_(false),
    blocked_(false),
    number_of_received_messages_(0),
    offer_thread_(std::bind(&routing_restart_test_service::run, this)) {
}

bool routing_restart_test_service::init() {
    std::lock_guard<std::mutex> its_lock(mutex_);

    if (!app_->init()) {
        ADD_FAILURE() << "Couldn't initialize application";
        return false;
    }
    app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID,
            std::bind(&routing_restart_test_service::on_message, this,
                    std::placeholders::_1));

    app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID,
            vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN,
            std::bind(&routing_restart_test_service::on_message_shutdown, this,
                    std::placeholders::_1));

    app_->register_state_handler(
            std::bind(&routing_restart_test_service::on_state, this,
                    std::placeholders::_1));
    return true;
}

void routing_restart_test_service::start() {
    VSOMEIP_INFO << "Starting...";
    app_->start();
}

void routing_restart_test_service::stop() {
    VSOMEIP_INFO << "Stopping...";
    app_->clear_all_handler();
    app_->stop();
}

void routing_restart_test_service::join_offer_thread() {
    if (offer_thread_.joinable()) {
        offer_thread_.join();
    }
}

void routing_restart_test_service::offer() {
    app_->offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
}

void routing_restart_test_service::stop_offer() {
    app_->stop_offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
}

void routing_restart_test_service::on_state(vsomeip::state_type_e _state) {
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

void routing_restart_test_service::on_message(const std::shared_ptr<vsomeip::message>& _request) {
    ASSERT_EQ(vsomeip_test::TEST_SERVICE_SERVICE_ID, _request->get_service());
    ASSERT_EQ(vsomeip_test::TEST_SERVICE_METHOD_ID, _request->get_method());

    VSOMEIP_INFO << "Received a message with Client/Session [" << std::setw(4)
        << std::setfill('0') << std::hex << _request->get_client() << "/"
        << std::setw(4) << std::setfill('0') << std::hex
        << _request->get_session() << "]";

    // send response
    std::shared_ptr<vsomeip::message> its_response =
            vsomeip::runtime::get()->create_response(_request);

    app_->send(its_response);

    number_of_received_messages_++;
    if(number_of_received_messages_ == vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_ROUTING_RESTART_TESTS) {
        VSOMEIP_INFO << "Received all messages!";
    }
}

void routing_restart_test_service::on_message_shutdown(
        const std::shared_ptr<vsomeip::message>& _request) {
    (void)_request;
    VSOMEIP_INFO << "Shutdown method was called, going down now.";
    stop();
}

void routing_restart_test_service::run() {
    std::unique_lock<std::mutex> its_lock(mutex_);
    while (!blocked_)
        condition_.wait(its_lock);

   offer();
}

TEST(someip_restart_routing_test, send_response_for_every_request) {
    routing_restart_test_service test_service;
    if (test_service.init()) {
        test_service.start();
        test_service.join_offer_thread();
    }
}

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif

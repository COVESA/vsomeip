// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "big_payload_test_service.hpp"

#include "big_payload_test_globals.hpp"

big_payload_test_service::big_payload_test_service() :
                app_(vsomeip::runtime::get()->create_application()),
                is_registered_(false),
                blocked_(false),
                number_of_received_messages_(0),
                offer_thread_(std::bind(&big_payload_test_service::run, this))
{
}

void big_payload_test_service::init()
{
    std::lock_guard<std::mutex> its_lock(mutex_);

    app_->init();
    app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID,
            std::bind(&big_payload_test_service::on_message, this,
                    std::placeholders::_1));

    app_->register_state_handler(
            std::bind(&big_payload_test_service::on_state, this,
                    std::placeholders::_1));
}

void big_payload_test_service::start()
{
    VSOMEIP_INFO << "Starting...";
    app_->start();
}

void big_payload_test_service::stop()
{
    VSOMEIP_INFO << "Stopping...";
    app_->unregister_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID);
    app_->unregister_state_handler();
    app_->stop();
}

void big_payload_test_service::join_offer_thread()
{
    offer_thread_.join();
}

void big_payload_test_service::offer()
{
    app_->offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
}

void big_payload_test_service::stop_offer()
{
    app_->stop_offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
}

void big_payload_test_service::on_state(vsomeip::state_type_e _state)
{
    VSOMEIP_INFO << "Application " << app_->get_name() << " is "
            << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." :
                    "deregistered.");

    if(_state == vsomeip::state_type_e::ST_REGISTERED)
    {
        if(!is_registered_)
        {
            is_registered_ = true;
            std::lock_guard<std::mutex> its_lock(mutex_);
            blocked_ = true;
            // "start" the run method thread
            condition_.notify_one();
        }
    }
    else
    {
        is_registered_ = false;
    }
}

void big_payload_test_service::on_message(const std::shared_ptr<vsomeip::message>& _request)
{
    VSOMEIP_INFO << "Received a message with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _request->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _request->get_session() << "] size: " << std::dec
            << _request->get_payload()->get_length();

    ASSERT_EQ(_request->get_payload()->get_length(), big_payload_test::BIG_PAYLOAD_SIZE);
    bool check(true);
    vsomeip::length_t len = _request->get_payload()->get_length();
    vsomeip::byte_t* datap = _request->get_payload()->get_data();
    for(unsigned int i = 0; i < len; ++i) {
        check = check && datap[i] == big_payload_test::DATA_CLIENT_TO_SERVICE;
    }
    if(!check) {
        GTEST_FATAL_FAILURE_("wrong data transmitted");
    }

    number_of_received_messages_++;

    // send response
    std::shared_ptr<vsomeip::message> its_response =
            vsomeip::runtime::get()->create_response(_request);

    std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()
    ->create_payload();
    std::vector<vsomeip::byte_t> its_payload_data;
    for (unsigned int i = 0; i < big_payload_test::BIG_PAYLOAD_SIZE; ++i) {
        its_payload_data.push_back(big_payload_test::DATA_SERVICE_TO_CLIENT);
    }
    its_payload->set_data(its_payload_data);
    its_response->set_payload(its_payload);

    app_->send(its_response, true);

    if(number_of_received_messages_ == vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND) {
        ASSERT_EQ(vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND, number_of_received_messages_);
        std::lock_guard<std::mutex> its_lock(mutex_);
        blocked_ = true;
        condition_.notify_one();
    }
}

void big_payload_test_service::run()
{
    std::unique_lock<std::mutex> its_lock(mutex_);
    while (!blocked_) {
        condition_.wait(its_lock);
    }

    offer();

    // wait for shutdown
    blocked_ = false;
    while (!blocked_) {
        condition_.wait(its_lock);
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
    app_->stop();
}

TEST(someip_big_payload_test, receive_ten_messages_and_send_reply)
{
    big_payload_test_service test_service;
    test_service.init();
    test_service.start();
    test_service.join_offer_thread();
}

#ifndef WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif

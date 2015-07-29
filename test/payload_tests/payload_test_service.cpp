// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "payload_test_service.hpp"

// this variables are changed via cmdline parameters
static bool use_tcp = false;

payload_test_service::payload_test_service(bool _use_tcp) :
                app_(vsomeip::runtime::get()->create_application()),
                is_registered_(false),
                use_tcp_(_use_tcp),
                offer_thread_(std::bind(&payload_test_service::run, this)),
                blocked_(false),
                number_of_received_messages_(0)
{
}

void payload_test_service::init()
{
    std::lock_guard<std::mutex> its_lock(mutex_);

    app_->init();
    app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID,
            std::bind(&payload_test_service::on_message, this,
                    std::placeholders::_1));

    app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID,
            vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN,
            std::bind(&payload_test_service::on_message_shutdown, this,
                    std::placeholders::_1));

    app_->register_event_handler(
            std::bind(&payload_test_service::on_event, this,
                    std::placeholders::_1));
}

void payload_test_service::start()
{
    VSOMEIP_INFO << "Starting...";
    app_->start();
}

void payload_test_service::stop()
{
    VSOMEIP_INFO << "Stopping...";
    app_->unregister_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID);
    app_->unregister_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
            vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN);
    app_->unregister_event_handler();
    app_->stop();
}

void payload_test_service::join_offer_thread()
{
    offer_thread_.join();
}

void payload_test_service::offer()
{
    app_->offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
}

void payload_test_service::stop_offer()
{
    app_->stop_offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
}

void payload_test_service::on_event(vsomeip::event_type_e _event)
{
    VSOMEIP_INFO << "Application " << app_->get_name() << " is "
            << (_event == vsomeip::event_type_e::ET_REGISTERED ? "registered." :
                    "deregistered.");

    if(_event == vsomeip::event_type_e::ET_REGISTERED)
    {
        if(!is_registered_)
        {
            is_registered_ = true;
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

void payload_test_service::on_message(const std::shared_ptr<vsomeip::message>& _request)
{
    number_of_received_messages_++;
    if(number_of_received_messages_ % vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_PAYLOAD_TESTS == 0)
    {
        VSOMEIP_INFO << "Received a message with Client/Session [" << std::setw(4)
                << std::setfill('0') << std::hex << _request->get_client() << "/"
                << std::setw(4) << std::setfill('0') << std::hex
                << _request->get_session() << "]";
    }

    ASSERT_EQ(_request->get_service(), vsomeip_test::TEST_SERVICE_SERVICE_ID);
    ASSERT_EQ(_request->get_method(), vsomeip_test::TEST_SERVICE_METHOD_ID);

    // Check the protocol version this shall be set to 0x01 according to the spec.
    // TR_SOMEIP_00052
    ASSERT_EQ(_request->get_protocol_version(), 0x01);
    // Check the message type this shall be 0xx (REQUEST) according to the spec.
    // TR_SOMEIP_00055
    ASSERT_EQ(_request->get_message_type(), vsomeip::message_type_e::MT_REQUEST);

    // make sure the message was sent from the service
    ASSERT_EQ(_request->get_client(), vsomeip_test::TEST_CLIENT_CLIENT_ID);

    std::shared_ptr<vsomeip::payload> pl = _request->get_payload();
    vsomeip::byte_t* pl_ptr = pl->get_data();
    for (int i = 0; i < pl->get_length(); i++)
    {
        ASSERT_EQ(*(pl_ptr+i), vsomeip_test::PAYLOAD_TEST_DATA);
    }

    // send response
    std::shared_ptr<vsomeip::message> its_response =
            vsomeip::runtime::get()->create_response(_request);

    app_->send(its_response, true);
}

void payload_test_service::on_message_shutdown(
        const std::shared_ptr<vsomeip::message>& _request)
{
    VSOMEIP_INFO << "Shutdown method was called, going down now.";
    stop();
}

void payload_test_service::run()
{
    std::unique_lock<std::mutex> its_lock(mutex_);
    while (!blocked_)
        condition_.wait(its_lock);

   offer();
}

TEST(someip_payload_test, send_response_for_every_request)
{
    payload_test_service test_service(use_tcp);
    test_service.init();
    test_service.start();
    test_service.join_offer_thread();
}

#ifndef WIN32
int main(int argc, char** argv)
{
    std::string help("--help");

    int i = 1;
    while (i < argc)
    {
        if(help == argv[i])
        {
            VSOMEIP_INFO << "Parameters:\n"
                    << "--help: print this help";
        }
        i++;
    }

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif

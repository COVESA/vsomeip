// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstdlib>
#include "common/test_main.hpp"

#include "header_factory_test_service.hpp"

header_factory_test_service::header_factory_test_service() :
    app_(vsomeip::runtime::get()->create_application()), number_of_received_messages_(0) { }

void header_factory_test_service::start() {
    if (!app_->init()) {
        ADD_FAILURE() << "Couldn't initialize application";
        return;
    }
    app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                                   vsomeip_test::TEST_SERVICE_METHOD_ID,
                                   std::bind(&header_factory_test_service::on_message, this, std::placeholders::_1));

    app_->offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
    app_->start();
}

void header_factory_test_service::on_message(const std::shared_ptr<vsomeip::message>& _request) {
    bool stop = false;
    {
        std::scoped_lock its_lock(sync_mtx_);
        VSOMEIP_INFO << "Received a message with Client/Session [" << std::hex << std::setfill('0') << std::setw(4)
                     << _request->get_client() << "/" << std::setw(4) << _request->get_session() << "]";

        number_of_received_messages_++;

        ASSERT_EQ(_request->get_service(), vsomeip_test::TEST_SERVICE_SERVICE_ID);
        ASSERT_EQ(_request->get_method(), vsomeip_test::TEST_SERVICE_METHOD_ID);

        // Check the protocol version this shall be set to 0x01 according to the spec.
        // TR_SOMEIP_00052
        ASSERT_EQ(_request->get_protocol_version(), 0x01);
        // Check the message type this shall be 0x00 (REQUEST) according to the spec.
        // TR_SOMEIP_00055
        ASSERT_EQ(_request->get_message_type(), vsomeip::message_type_e::MT_REQUEST);

        // check the session id.
        ASSERT_EQ(_request->get_session(), static_cast<vsomeip::session_t>(number_of_received_messages_));

        // send response
        std::shared_ptr<vsomeip::message> its_response = vsomeip::runtime::get()->create_response(_request);

        app_->send(its_response);

        if (number_of_received_messages_ >= vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND) {
            stop = true;
        }
    }
    if (stop) {
        app_->stop();
    }
}

TEST(someip_header_factory_test, receive_message_ten_times_test) {
    header_factory_test_service test_service;
    test_service.start();
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "common/test_main.hpp"

#include "header_factory_test_client.hpp"

header_factory_test_client::header_factory_test_client() :
    app_(vsomeip::runtime::get()->create_application()), request_(vsomeip::runtime::get()->create_request(false)),
    number_of_messages_to_send_(vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND), number_of_acknowledged_messages_(0) {
    request_->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
    request_->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
    request_->set_method(vsomeip_test::TEST_SERVICE_METHOD_ID);
}

void header_factory_test_client::start() {
    ASSERT_TRUE(app_->init());

    app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip::ANY_METHOD,
                                   std::bind(&header_factory_test_client::on_message, this, std::placeholders::_1));

    app_->register_availability_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                                        std::bind(&header_factory_test_client::on_availability, this, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3));
    app_->request_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, false);
    app_->start();
}

void header_factory_test_client::on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    VSOMEIP_INFO << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << _instance << "] is "
                 << (_is_available ? "available." : "NOT available.");

    if (vsomeip_test::TEST_SERVICE_SERVICE_ID == _service && vsomeip_test::TEST_SERVICE_INSTANCE_ID == _instance) {
        if (_is_available) {
            for (uint32_t i = 0; i < number_of_messages_to_send_; i++) {
                app_->send(request_);
                VSOMEIP_INFO << "Client/Session [" << std::hex << std::setfill('0') << std::setw(4) << request_->get_client() << "/"
                             << std::setw(4) << request_->get_session() << "] sent a request to Service [" << std::setw(4)
                             << request_->get_service() << "." << std::setw(4) << request_->get_instance() << "]";
            }
        }
    }
}

void header_factory_test_client::on_message(const std::shared_ptr<vsomeip::message>& _response) {
    bool stop = false;
    {
        std::scoped_lock its_lock(sync_mtx_);
        VSOMEIP_INFO << "Received a response from Service [" << std::hex << std::setfill('0') << std::setw(4) << _response->get_service()
                     << "." << std::setw(4) << _response->get_instance() << "] to Client/Session [" << std::setw(4)
                     << _response->get_client() << "/" << std::setw(4) << _response->get_session() << "]";
        number_of_acknowledged_messages_++;
        EXPECT_EQ(_response->get_service(), vsomeip_test::TEST_SERVICE_SERVICE_ID);
        EXPECT_EQ(_response->get_instance(), vsomeip_test::TEST_SERVICE_INSTANCE_ID);
        EXPECT_EQ(_response->get_session(), static_cast<vsomeip::session_t>(number_of_acknowledged_messages_));
        if (number_of_acknowledged_messages_ == number_of_messages_to_send_) {
            stop = true;
        }
    }
    if (stop) {
        app_->stop();
    }
}

TEST(someip_header_factory_test, send_message_ten_times_test) {
    header_factory_test_client test_client_;
    test_client_.start();
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include "common/test_main.hpp"

#include "security_client.hpp"

security_client::security_client(bool _test_external_communication, bool _is_remote_client_allowed, bool _is_offer_test,
                                 bool _offer_allowed) :
    app_(vsomeip::runtime::get()->create_application()), is_available_(false), sender_(std::bind(&security_client::run, this)),
    received_responses_(0), received_allowed_events_(0), test_external_communication_(_test_external_communication),
    is_remote_client_allowed_(_is_remote_client_allowed), is_offer_test_(_is_offer_test), offer_allowed_(_offer_allowed) { }

bool security_client::init() {
    if (!app_->init()) {
        ADD_FAILURE() << "Couldn't initialize application";
        return false;
    }

    app_->register_state_handler(std::bind(&security_client::on_state, this, std::placeholders::_1));

    app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip::ANY_METHOD,
                                   std::bind(&security_client::on_message, this, std::placeholders::_1));

    app_->register_availability_handler(
            vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
            std::bind(&security_client::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    app_->register_availability_handler(
            0x111, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
            std::bind(&security_client::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    app_->register_availability_handler(
            vsomeip_test::TEST_SERVICE_SERVICE_ID, 0x02,
            std::bind(&security_client::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    return true;
}

void security_client::start() {
    VSOMEIP_INFO << "Starting...";

    app_->start();
}

void security_client::stop() {
    VSOMEIP_INFO << "Stopping...";

    if (is_remote_client_allowed_ || is_offer_test_) {
        shutdown_service();

        // Wait for the service to become unavailable, confirming the shutdown
        // message was received and processed before we tear down.
        std::unique_lock its_lock(mutex_);
        if (!condition_.wait_for(its_lock, std::chrono::seconds(5), [this] { return !is_available_; })) {
            GTEST_NONFATAL_FAILURE_("Service didn't become unavailable within time");
        }
    }

    app_->clear_all_handler();
    app_->stop();
}

void security_client::on_state(vsomeip::state_type_e _state) {
    if (_state == vsomeip::state_type_e::ST_REGISTERED) {
        app_->request_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, false);

        // request not allowed service ID
        app_->request_service(0x111, vsomeip_test::TEST_SERVICE_INSTANCE_ID, false);

        // request not allowed instance ID
        app_->request_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, 0x02, false);

        // request events of eventgroup 0x01 which holds events 0x8001 (allowed) and 0x8002 (denied)
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(0x01);
        app_->request_event(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                            static_cast<vsomeip::event_t>(0x8001), its_eventgroups, vsomeip::event_type_e::ET_FIELD,
                            vsomeip::reliability_type_e::RT_UNRELIABLE);
        app_->request_event(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                            static_cast<vsomeip::event_t>(0x8002), its_eventgroups, vsomeip::event_type_e::ET_FIELD,
                            vsomeip::reliability_type_e::RT_UNRELIABLE);

        app_->subscribe(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, 0x01, vsomeip::DEFAULT_MAJOR,
                        static_cast<vsomeip::event_t>(0x8001));

        app_->subscribe(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, 0x01, vsomeip::DEFAULT_MAJOR,
                        static_cast<vsomeip::event_t>(0x8002));
    }
}

void security_client::on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {

    VSOMEIP_INFO << std::hex << "Client 0x" << app_->get_client() << " : Service [" << std::hex << std::setfill('0') << std::setw(4)
                 << _service << "." << _instance << "] is " << (_is_available ? "available." : "NOT available.");

    // check that only the allowed service / instance ID gets available
    if (_is_available) {
        EXPECT_EQ(vsomeip_test::TEST_SERVICE_SERVICE_ID, _service);
        EXPECT_EQ(vsomeip_test::TEST_SERVICE_INSTANCE_ID, _instance);
    }

    if (vsomeip_test::TEST_SERVICE_SERVICE_ID == _service && vsomeip_test::TEST_SERVICE_INSTANCE_ID == _instance) {
        std::unique_lock its_lock(mutex_);
        if (is_available_ && !_is_available) {
            is_available_ = false;
            condition_.notify_one();
        } else if (_is_available && !is_available_) {
            is_available_ = true;
            condition_.notify_one();
        }
    }
}

void security_client::on_message(const std::shared_ptr<vsomeip::message>& _response) {
    VSOMEIP_INFO << "Received a response from Service [" << std::hex << std::setfill('0') << std::setw(4) << _response->get_service() << "."
                 << std::setw(4) << _response->get_instance() << "] to Client/Session [" << std::setw(4) << _response->get_client() << "/"
                 << std::setw(4) << _response->get_session() << "]";

    if (_response->get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
        EXPECT_EQ(vsomeip_test::TEST_SERVICE_SERVICE_ID, _response->get_service());
        EXPECT_EQ(vsomeip_test::TEST_SERVICE_INSTANCE_ID, _response->get_instance());
        EXPECT_EQ(vsomeip_test::TEST_SERVICE_METHOD_ID, _response->get_method());

        if (_response->get_service() == vsomeip_test::TEST_SERVICE_SERVICE_ID
            && _response->get_instance() == vsomeip_test::TEST_SERVICE_INSTANCE_ID
            && _response->get_method() == vsomeip_test::TEST_SERVICE_METHOD_ID) {
            received_responses_++;
            if (received_responses_ == vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_SECURITY_TESTS) {
                VSOMEIP_WARNING << std::hex << app_->get_client() << ": Received all messages ~> going down!";
            }
        }
    } else if (_response->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
        // check that only allowed event 0x8001 is received
        EXPECT_EQ(vsomeip_test::TEST_SERVICE_SERVICE_ID, _response->get_service());
        EXPECT_EQ(vsomeip_test::TEST_SERVICE_INSTANCE_ID, _response->get_instance());
        EXPECT_EQ(0x8001, _response->get_method());
        received_allowed_events_++;
    }
}

void security_client::run() {
    {
        std::unique_lock its_lock(mutex_);
        if (!condition_.wait_for(its_lock, std::chrono::seconds(10), [this] { return is_available_; })) {
            ADD_FAILURE() << "Service did not become available";
        }
    }
    for (uint32_t i = 0; i < vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_SECURITY_TESTS; ++i) {
        auto request = vsomeip::runtime::get()->create_request(false);
        request->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
        request->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
        request->set_method(vsomeip_test::TEST_SERVICE_METHOD_ID);

        // send a request which is allowed by policy -> expect answer
        app_->send(request);

        // send a request with a not allowed method ID -> expect no answer
        request->set_method(0x888);
        app_->send(request);

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    if (is_offer_test_) {
        // The consumer authorizes (allow) or omits (deny) the provider's offer.
        // When missing, every response/notification is dropped by the receive-side
        // offer check.
        if (offer_allowed_) {
            EXPECT_EQ(vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_SECURITY_TESTS, received_responses_);
            EXPECT_EQ(received_allowed_events_, (uint32_t)0x01);
        } else {
            EXPECT_EQ((uint32_t)0, received_responses_);
            EXPECT_EQ((uint32_t)0, received_allowed_events_);
        }
    } else if (!test_external_communication_) {
        EXPECT_EQ(vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_SECURITY_TESTS, received_responses_);
        EXPECT_EQ(received_allowed_events_, (uint32_t)0x01);
    } else if (test_external_communication_ && !is_remote_client_allowed_) {
        EXPECT_EQ((uint32_t)0, received_responses_);
        EXPECT_EQ((uint32_t)0, received_allowed_events_);
    } else if (test_external_communication_ && is_remote_client_allowed_) {
        EXPECT_EQ(vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_SECURITY_TESTS, received_responses_);
        EXPECT_EQ(received_allowed_events_, (uint32_t)0x01);
    }
    stop();
}

void security_client::join_sender_thread() {
    if (sender_.joinable()) {
        sender_.join();
    }
}

void security_client::shutdown_service() {
    auto request = vsomeip::runtime::get()->create_request(false);
    request->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
    request->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
    request->set_method(vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN);
    app_->send(request);
}

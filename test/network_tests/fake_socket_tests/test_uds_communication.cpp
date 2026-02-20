// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/fake_tcp_socket_handle.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/service_state.hpp"

#include "sample_interfaces.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {
static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct test_uds_communication : public base_fake_socket_fixture {
    test_uds_communication() {
        use_configuration("multiple_client_one_process_uds.json");
        create_app(routingmanager_name_);
        create_app(server_name_);
        create_app(client_name_);
    }
    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
    }

    void start_server() {
        cafe_server_ = start_client(server_name_);
        ASSERT_NE(cafe_server_, nullptr);
        ASSERT_TRUE(cafe_server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        cafe_server_->offer(interfaces::cafe);
    }

    void start_client_app() {
        cafe_client_ = start_client(client_name_);
        ASSERT_NE(cafe_client_, nullptr);
        ASSERT_TRUE(cafe_client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    void start_apps() {
        start_router();
        start_server();
        start_client_app();
    }

    void request_service() { cafe_client_->request_service(interfaces::cafe.instance_); }
    [[nodiscard]] bool await_service() {
        return cafe_client_->availability_record_.wait_for_last(service_availability::available(interfaces::cafe.instance_));
    }
    [[nodiscard]] bool subscribe_to_event() {
        request_service();
        cafe_client_->subscribe_event(interfaces::cafe.event_one_);
        return cafe_client_->subscription_record_.wait_for_last(
                event_subscription::successfully_subscribed_to(interfaces::cafe.event_one_));
    }
    [[nodiscard]] bool subscribe_to_field() {
        request_service();
        cafe_client_->subscribe_field(interfaces::cafe.field_two_);
        return cafe_client_->subscription_record_.wait_for_last(
                event_subscription::successfully_subscribed_to(interfaces::cafe.field_two_));
    }
    void send_first_message() { cafe_server_->send_event(interfaces::cafe.event_one_, {}); }
    void send_field_message() { cafe_server_->send_event(interfaces::cafe.field_two_, field_payload_); }

    void stop_offer() { cafe_server_->stop_offer(interfaces::cafe.instance_); }

    std::vector<unsigned char> field_payload_{0xf, 0xa, 0xd, 0xe};
    message_checker cafe_checker_ = message_checker{std::nullopt, interfaces::cafe.instance_, interfaces::cafe.field_two_.event_id_,
                                                    vsomeip::message_type_e::MT_NOTIFICATION, field_payload_};

    app* routingmanagerd_{};
    app* cafe_client_{};
    app* cafe_server_{};
};

TEST_F(test_uds_communication, field_subscription) {
    start_apps();

    send_field_message();
    ASSERT_TRUE(subscribe_to_field());

    ASSERT_TRUE(cafe_client_->message_record_.wait_for(cafe_checker_));
}

}

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/sockets/fake_tcp_socket_handle.hpp"
#include "helpers/service_state.hpp"
#include "helpers/availability_checker.hpp"

#include "sample_interfaces.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {
static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};
static std::string const server_name_two_{"server_two"}; // without a fixed-id in config
static std::string const client_name_{"client"};
static std::string const client_name_two_{"client_two"}; // without a fixed-id in config

struct stress_service_reoffered : public base_fake_socket_fixture {
    stress_service_reoffered() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(server_name_);
        create_app(client_name_);
    }
    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));
    }

    void start_server() {
        server_ = start_client(server_name_);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer(service_instance_);
        server_->offer_event(offered_event_);
        server_->offer_field(offered_field_);
    }

    void start_client_app() {
        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    [[nodiscard]] bool subscribe_to_field() {
        client_->request_service(service_instance_);
        client_->subscribe_field(offered_field_);
        return client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_));
    }
    void send_field_message() { server_->send_event(offered_field_, field_payload_); }

    interface interface_{0x3344};
    service_instance service_instance_{interface_.instance_};
    event_ids offered_field_{interface_.fields_[0]};
    event_ids offered_event_{interface_.events_[0]};

    std::vector<unsigned char> field_payload_{0x42, 0x13};
    message first_expected_field_message_{client_session{0, 2}, // todo, why is the session a two here?
                                          service_instance_, offered_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                                          field_payload_};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
};

TEST_F(stress_service_reoffered, connection_break_vs_routing) {
    /// an absolute nightmare scenario
    /// 1) server offers, client requests and uses
    /// 2) server stops offering
    /// 3) later, client -> server connection breaks
    /// 4) *MEANWHILE*, server offers again
    /// ensure that after this nightmare, that client
    /// - client -> server connection is re-established
    /// - client sees ON_AVAILABLE last


    start_router();

    // obviously this sequence is inherently racy, so do a few loops
    for (size_t i = 0; i < 100; ++i) {
        TEST_LOG << "Iteration " << i;

        create_app(server_name_);
        start_server();
        send_field_message();

        create_app(client_name_);
        start_client_app();

        // client requests and uses..
        ASSERT_TRUE(subscribe_to_field());
        ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));

        client_->message_record_.clear();
        client_->subscription_record_.clear();
        client_->availability_record_.clear();

        // server stops offering..
        server_->stop_offer(service_instance_);

        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

        // client -> server connection breaks..
        // NOTE: only client is informed, but irrelevant, if anything it adds to the "raciness"!
        ASSERT_TRUE(disconnect(client_name_, boost::asio::error::timed_out, server_name_, std::nullopt));

        server_->offer(service_instance_);
        send_field_message();

        // we *MUST* see available/subscribed/connection
        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
        ASSERT_TRUE(await_connection(client_name_, server_name_));
        ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

        // *EXTRA* paranoia - check that client saw ON_AVAILABLE as last notification
        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

        stop_client(client_name_);
        stop_client(server_name_);
    }
}
}

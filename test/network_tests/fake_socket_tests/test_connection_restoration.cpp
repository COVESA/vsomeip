// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/service_state.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <stdlib.h>

namespace vsomeip_v3::testing {
static std::string const routingmanager_name_ {"routingmanagerd"};
static std::string const server_name_ {"server"};
static std::string const client_name_ {"client"};

struct test_client_helper : public base_fake_socket_fixture {
    test_client_helper() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(server_name_);
        create_app(client_name_);
    }
    void start_apps() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));

        server_ = start_client(server_name_);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer(service_instance_);
        server_->offer_event(offered_event_);
        server_->offer_field(offered_field_);

        client_ = start_client(client_name_);
        ASSERT_TRUE(client_->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
    }
    [[nodiscard]] bool
    subscribe_to_event(std::chrono::milliseconds timeout = std::chrono::seconds(6)) {
        client_->request_service(service_instance_);
        client_->subscribe_event(offered_event_);
        return client_->subscription_record_.wait_for(
                event_subscription::successfully_subscribed_to(offered_event_), timeout);
    }
    [[nodiscard]] bool
    subscribe_to_field(std::chrono::milliseconds timeout = std::chrono::seconds(6)) {
        client_->request_service(service_instance_);
        client_->subscribe_field(offered_field_);
        return client_->subscription_record_.wait_for(
                event_subscription::successfully_subscribed_to(offered_field_), timeout);
    }
    void send_first_message() { server_->send_event(offered_event_, {}); }
    void send_field_message() { server_->send_event(offered_field_, field_payload_); }
    void answer_requests_with(std::vector<unsigned char> _payload) {
        server_->answer_request(request_, [payload = _payload] { return payload; });
        expected_reply_.payload_ = _payload;
    }

    service_instance service_instance_ {0x3344, 0x1};
    event_ids offered_event_ {service_instance_, 0x8002, 0x1};
    message first_expected_message_ {client_session {0, 1},
                                     service_instance_,
                                     offered_event_.event_id_,
                                     vsomeip::message_type_e::MT_NOTIFICATION,
                                     {}};
    event_ids offered_field_ {service_instance_, 0x8003, 0x6};
    std::vector<unsigned char> field_payload_ {0x42, 0x13};
    message first_expected_field_message_ {
            client_session {0, 2}, // todo, why is the session a two here?
            service_instance_, offered_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
            field_payload_};

    vsomeip_v3::method_t method_ {0x1111};
    request request_ {service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_request_ {client_session {0x3490 /*client id*/, 1},
                               service_instance_,
                               method_,
                               vsomeip::message_type_e::MT_REQUEST,
                               {}};
    message expected_reply_ {client_session {0x3490 /*client id*/, 1},
                             service_instance_,
                             method_,
                             vsomeip::message_type_e::MT_RESPONSE,
                             {}};

    app* routingmanagerd_ {};
    app* client_ {};
    app* server_ {};
};

TEST_F(test_client_helper, event_subscription) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();

    EXPECT_TRUE(client_->message_record_.wait_for(first_expected_message_));
}
TEST_F(test_client_helper, field_subscription) {
    start_apps();

    send_field_message();
    ASSERT_TRUE(subscribe_to_field());

    EXPECT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));
}

TEST_F(test_client_helper, request_reply) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    answer_requests_with({0x2, 0x3});
    client_->send_request(request_);

    ASSERT_TRUE(server_->message_record_.wait_for(expected_request_));
    EXPECT_TRUE(client_->message_record_.wait_for(expected_reply_));
}
TEST_F(test_client_helper, the_server_sends_subscribe_ack_when_the_routing_info_is_late) {
    start_apps();
    send_field_message();

    // ensure that the routing info is not received by the server before the subscription
    ASSERT_TRUE(delay_message_processing(routingmanager_name_, server_name_, true));
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true));
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);

    // TODO what could we await here?
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(delay_message_processing(routingmanager_name_, server_name_, false));
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, false));

    ASSERT_TRUE(client_->subscription_record_.wait_for(
            event_subscription::successfully_subscribed_to(offered_field_),
            std::chrono::seconds(3)));

    EXPECT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));
}

TEST_F(test_client_helper, reproduction_allow_reconnects_on_first_try_between_router_and_client) {
    GTEST_SKIP() << "Is now failing in the gating job, needs further investigation";
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(disconnect(routingmanager_name_, boost::asio::error::timed_out, client_name_,
                           std::nullopt));
    ASSERT_TRUE(disconnect(client_name_, std::nullopt, routingmanager_name_,
                           boost::asio::error::timed_out));
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the client:
    set_ignore_connections(client_name_, false);
    // resume of the client:
    ASSERT_TRUE(disconnect(routingmanager_name_, std::nullopt, client_name_,
                           boost::asio::error::connection_reset));
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, routingmanager_name_,
                           std::nullopt));

    // in order for the server to be able to send an event, the server needs to connect back
    // to the client. This can only be traced via the socket connection state afaik.
    // But the connection will break down only after some connection attempt to the router failed?
    // Therefore first await that the client <-> router connections are re-established, before
    // awaiting that the client <-> server connections are re-established.
    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(routingmanager_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(server_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(client_name_, server_name_, std::chrono::seconds(6)));

    server_->send_event(offered_event_, {0x2, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x2, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for(next_expected_message));

    // Expect three connection that where established from the client towards,
    // the routing manager:
    // 1. client registers initially
    // 2. client tries to repair the connection
    EXPECT_EQ(2, connection_count(client_name_, routingmanager_name_));
}

TEST_F(test_client_helper, client_server_connection_breakdown_on_client_suspend_with_events) {
    GTEST_SKIP() << "the second disconnect is no longer reliable to execute, as this connection is "
                    "shutdown by the server itself";
    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(
            disconnect(server_name_, boost::asio::error::timed_out, client_name_, std::nullopt));
    ASSERT_TRUE(
            disconnect(client_name_, std::nullopt, server_name_, boost::asio::error::timed_out));
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the client:
    set_ignore_connections(client_name_, false);
    ASSERT_TRUE(disconnect(server_name_, std::nullopt, client_name_,
                           boost::asio::error::connection_reset));
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_,
                           std::nullopt));

    ASSERT_TRUE(await_connection(server_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(client_name_, server_name_, std::chrono::seconds(6)));

    server_->send_event(offered_event_, {0x5, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for(next_expected_message));
}

TEST_F(test_client_helper, client_server_connection_breakdown_on_server_suspend_with_events) {
    GTEST_SKIP() << "Currently no fix provided for this";
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // simulating a suspend of a server:
    set_ignore_connections(server_name_, true);
    ASSERT_TRUE(
            disconnect(client_name_, boost::asio::error::timed_out, server_name_, std::nullopt));
    ASSERT_TRUE(
            disconnect(server_name_, std::nullopt, client_name_, boost::asio::error::timed_out));
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the server:
    set_ignore_connections(server_name_, false);
    ASSERT_TRUE(disconnect(client_name_, std::nullopt, server_name_,
                           boost::asio::error::connection_reset));
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::connection_reset, client_name_,
                           std::nullopt));

    ASSERT_TRUE(await_connection(server_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(client_name_, server_name_, std::chrono::seconds(6)));

    server_->send_event(offered_event_, {0x5, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for(next_expected_message));
}

TEST_F(test_client_helper,
       field_updates_are_resend_when_a_broken_routing_connection_enforces_a_resubscription) {
    GTEST_SKIP() << "No fix  delivered yet";
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(disconnect(routingmanager_name_, boost::asio::error::timed_out, client_name_,
                           std::nullopt));

    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    // resume of the client:
    set_ignore_connections(client_name_, false);
    // resume of the client:
    ASSERT_TRUE(disconnect(routingmanager_name_, std::nullopt, client_name_,
                           boost::asio::error::connection_reset));

    // TODO:
    // Because the re-connection needs to timeout before being successful, let's
    // await the reconnect before continueing with the test.
    std::this_thread::sleep_for(std::chrono::seconds(4));

    // because the client will re-subscribe it should receive another event after the subscription
    auto new_message = first_expected_field_message_;
    new_message.client_session_.session_ += 1;
    EXPECT_TRUE(client_->message_record_.wait_for(new_message));
}

TEST_F(test_client_helper, server_times_out_on_first_connect_attemp_with_client) {
    GTEST_SKIP() << "Currently no fix provided for this";
    start_apps();

    report_on_connect(client_name_, {boost::asio::error::timed_out});

    EXPECT_TRUE(subscribe_to_event());
}

TEST_F(test_client_helper, client_ignores_server_connection_attempt_once) {
    GTEST_SKIP() << "Currently no fix provided for this";
    start_apps();

    ignore_connections(client_name_, 1);

    EXPECT_TRUE(subscribe_to_event());
}

struct test_single_connection_breakdown
    : test_client_helper,
      ::testing::WithParamInterface<std::pair<std::string, std::string>> { };

TEST_P(test_single_connection_breakdown,
       ensure_that_every_dropped_connection_is_restored_with_request_reply) {
    auto [from, to] = GetParam();
    if (from == routingmanager_name_ || (from == server_name_ && to == client_name_)) {
        GTEST_SKIP() << "Currently no fix provided for: " << from << " -> " << to;
    }

    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    answer_requests_with({0x2, 0x3});
    client_->send_request(request_);
    ASSERT_TRUE(server_->message_record_.wait_for(expected_request_));
    ASSERT_TRUE(client_->message_record_.wait_for(expected_reply_));

    // suspend one direction
    ASSERT_TRUE(disconnect(from, boost::asio::error::timed_out, to, std::nullopt));
    // resume the direction
    ASSERT_TRUE(disconnect(from, std::nullopt, to, boost::asio::error::connection_reset));

    for (int i = 0; i < 10; ++i) {
        client_->send_request(request_);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    auto next_reply = expected_reply_;
    // It seems some messages are dropped, but the session bumped anyhow.
    next_reply.client_session_.session_ += 8;
    EXPECT_TRUE(client_->message_record_.wait_for(next_reply)) << next_reply;
}

TEST_P(test_single_connection_breakdown,
       ensure_that_every_dropped_connection_is_restored_with_events) {
    auto [from, to] = GetParam();
    if ((from == routingmanager_name_ && to == client_name_) || from == server_name_
        || (from == client_name_ && to == server_name_)) {
        GTEST_SKIP() << "Currently no fix provided for: " << from << " -> " << to;
    }

    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // suspend one direction
    ASSERT_TRUE(disconnect(from, boost::asio::error::timed_out, to, std::nullopt));
    // resume the direction
    ASSERT_TRUE(disconnect(from, std::nullopt, to, boost::asio::error::connection_reset));

    for (int i = 0; i < 10; ++i) {
        send_first_message();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    auto some_expected_message = first_expected_message_;
    // It seems some messages are dropped, but the session bumped anyhow.
    some_expected_message.client_session_.session_ += 8;
    EXPECT_TRUE(client_->message_record_.wait_for(some_expected_message)) << some_expected_message;
}

INSTANTIATE_TEST_SUITE_P(test_all_permutations, test_single_connection_breakdown,
                         ::testing::Values(std::pair {routingmanager_name_, client_name_},
                                           std::pair {client_name_, routingmanager_name_},
                                           std::pair {routingmanager_name_, server_name_},
                                           std::pair {server_name_, routingmanager_name_},
                                           std::pair {client_name_, server_name_},
                                           std::pair {server_name_, client_name_}));

}

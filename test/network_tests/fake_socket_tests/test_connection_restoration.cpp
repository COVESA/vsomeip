// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/fake_tcp_socket_handle.hpp"
#include "helpers/service_state.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {
static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct test_client_helper : public base_fake_socket_fixture {
    test_client_helper() {
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
        ASSERT_TRUE(server_->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer(service_instance_);
        server_->offer_event(offered_event_);
        server_->offer_field(offered_field_);
    }

    void start_apps() {
        start_router();
        start_server();

        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
    }
    [[nodiscard]] bool subscribe_to_event(std::chrono::milliseconds timeout = std::chrono::seconds(6)) {
        client_->request_service(service_instance_);
        client_->subscribe_event(offered_event_);
        return client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_event_), timeout);
    }
    [[nodiscard]] bool subscribe_to_field(std::chrono::milliseconds timeout = std::chrono::seconds(6)) {
        client_->request_service(service_instance_);
        client_->subscribe_field(offered_field_);
        return client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_), timeout);
    }
    void send_first_message() { server_->send_event(offered_event_, {}); }
    void send_field_message() { server_->send_event(offered_field_, field_payload_); }
    void answer_requests_with(std::vector<unsigned char> _payload) {
        server_->answer_request(request_, [payload = _payload] { return payload; });
        expected_reply_.payload_ = _payload;
    }

    service_instance service_instance_{0x3344, 0x1};
    event_ids offered_event_{service_instance_, 0x8002, 0x1};
    message first_expected_message_{
            client_session{0, 1}, service_instance_, offered_event_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, {}};
    event_ids offered_field_{service_instance_, 0x8003, 0x6};
    std::vector<unsigned char> field_payload_{0x42, 0x13};
    message first_expected_field_message_{client_session{0, 2}, // todo, why is the session a two here?
                                          service_instance_, offered_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                                          field_payload_};

    vsomeip_v3::method_t method_{0x1111};
    request request_{service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_request_{client_session{0x3490 /*client id*/, 1}, service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_reply_{client_session{0x3490 /*client id*/, 1}, service_instance_, method_, vsomeip::message_type_e::MT_RESPONSE, {}};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
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

TEST_F(test_client_helper, request_reply_no_sub) {
    start_apps();

    client_->request_service(service_instance_);
    answer_requests_with({0x2, 0x3});

    // wait for availability, before sending the request
    // otherwise no guarantee that it is sent out
    EXPECT_TRUE(client_->availability_record_.wait_for(service_availability::available(service_instance_)));
    client_->send_request(request_);

    ASSERT_TRUE(server_->message_record_.wait_for(expected_request_));
    EXPECT_TRUE(client_->message_record_.wait_for(expected_reply_));
}

TEST_F(test_client_helper, request_reply_with_sub) {
    start_apps();

    // NOTE: subscription acknowledge happens after service availability, which is why this works!
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

    ASSERT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_),
                                                       std::chrono::seconds(3)));

    EXPECT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));
}

TEST_F(test_client_helper, reproduction_allow_reconnects_on_first_try_between_router_and_client) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(disconnect(routingmanager_name_, boost::asio::error::timed_out, client_name_, std::nullopt));
    ASSERT_TRUE(disconnect(client_name_, std::nullopt, routingmanager_name_, boost::asio::error::timed_out));

    client_->subscription_record_.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the client:
    set_ignore_connections(client_name_, false);
    // resume of the client:
    std::ignore = disconnect(routingmanager_name_, std::nullopt, client_name_, boost::asio::error::connection_reset);
    std::ignore = disconnect(client_name_, boost::asio::error::connection_reset, routingmanager_name_, std::nullopt);

    // in order for the server to be able to send an event, the server needs to connect back
    // to the client. This can only be traced via the socket connection state afaik.
    // But the connection will break down only after some connection attempt to the router failed?
    // Therefore first await that the client <-> router connections are re-established, before
    // awaiting that the client <-> server connections are re-established.
    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(routingmanager_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(server_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(client_name_, server_name_, std::chrono::seconds(6)));

    // client needs to re-subscribe, wait for it
    EXPECT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_event_)));

    server_->send_event(offered_event_, {0x2, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x2, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for(next_expected_message)) << client_->message_record_;

    // Expect two connection that where established from the client towards,
    // the routing manager:
    // 1. client registers initially
    // 2. client tries to repair the connection
    // more connection attempts can happen because of "disconnect cascades", e.g., routing breaking the connection repair when
    // the routing -> client connection breaks (see also 0fe029daa, server_endpoint::disconnect_from)
    EXPECT_LE(2, connection_count(client_name_, routingmanager_name_));
}

TEST_F(test_client_helper, client_server_connection_breakdown_on_client_suspend_with_events) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::timed_out, client_name_, std::nullopt));
    std::ignore = disconnect(client_name_, std::nullopt, server_name_, boost::asio::error::timed_out);

    client_->subscription_record_.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the client:
    set_ignore_connections(client_name_, false);
    std::ignore = disconnect(server_name_, std::nullopt, client_name_, boost::asio::error::connection_reset);
    std::ignore = disconnect(client_name_, boost::asio::error::connection_reset, server_name_, std::nullopt);

    ASSERT_TRUE(await_connection(server_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(client_name_, server_name_, std::chrono::seconds(6)));

    // client needs to re-subscribe, wait for it
    EXPECT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_event_)));

    server_->send_event(offered_event_, {0x5, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for(next_expected_message));
}

TEST_F(test_client_helper, client_server_connection_breakdown_on_server_suspend_with_events) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // simulating a suspend of a server:
    set_ignore_connections(server_name_, true);
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::timed_out, server_name_, std::nullopt));
    std::ignore = disconnect(server_name_, std::nullopt, client_name_, boost::asio::error::timed_out);

    client_->subscription_record_.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the server:
    set_ignore_connections(server_name_, false);
    std::ignore = disconnect(client_name_, std::nullopt, server_name_, boost::asio::error::connection_reset);
    std::ignore = disconnect(server_name_, boost::asio::error::connection_reset, client_name_, std::nullopt);

    ASSERT_TRUE(await_connection(server_name_, client_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(client_name_, server_name_, std::chrono::seconds(6)));

    // client needs to re-subscribe, wait for it
    EXPECT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_event_)));

    server_->send_event(offered_event_, {0x5, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for(next_expected_message));
}

TEST_F(test_client_helper, field_updates_are_resend_when_a_broken_routing_connection_enforces_a_resubscription) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(disconnect(routingmanager_name_, boost::asio::error::timed_out, client_name_, std::nullopt));

    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    // resume of the client:
    set_ignore_connections(client_name_, false);
    // resume of the client:
    std::ignore = disconnect(routingmanager_name_, std::nullopt, client_name_, boost::asio::error::connection_reset);

    // TODO:
    // Because the re-connection needs to timeout before being successful, let's
    // await the reconnect before continueing with the test.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // because the client will re-subscribe it should receive another event after the subscription
    auto new_message = first_expected_field_message_;
    new_message.client_session_.session_ += 1;
    EXPECT_TRUE(client_->message_record_.wait_for(new_message));
}

TEST_F(test_client_helper, server_times_out_on_first_connect_attemp_with_client) {
    start_apps();

    report_on_connect(client_name_, {boost::asio::error::timed_out});

    EXPECT_TRUE(subscribe_to_event());
}

TEST_F(test_client_helper, client_ignores_server_connection_attempt_once) {
    start_apps();

    ignore_connections(client_name_, 1);

    EXPECT_TRUE(subscribe_to_event());
}

TEST_F(test_client_helper, given_server_to_client_breaksdown_when_the_connection_is_re_established_then_the_subscription_confirmed) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));
    client_->subscription_record_.clear();

    // give the client a head start when cleaning the "sibling" connection, by holding back the
    // server execution.
    ASSERT_TRUE(block_on_close_for(client_name_, std::nullopt, server_name_, std::chrono::milliseconds(200)));

    // break the server->client connection. Notice that if the blocking above is in place,
    // the "wrong" order of cleaning up server->client + client->server will lead to a race
    // condition.
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::timed_out, client_name_, std::nullopt));

    EXPECT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_),
                                                       std::chrono::seconds(6)));
}

TEST_F(test_client_helper, break_client_with_eof_before_finishing_registration) {

    // Start apps manually to break the connection before the registration happens
    routingmanagerd_ = start_client(routingmanager_name_);
    ASSERT_NE(routingmanagerd_, nullptr);
    ASSERT_TRUE(await_connectable(routingmanager_name_, std::chrono::seconds(6)));

    client_ = start_client(client_name_);
    ASSERT_NE(client_, nullptr);
    ASSERT_TRUE(await_connectable(client_name_, std::chrono::seconds(6)));
    // Do not wait for client registering and break the connection immediately

    TEST_LOG << " ##### BREAKING Client connection before registration #####";
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::eof, routingmanager_name_, boost::asio::error::eof, socket_role::sender));

    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(routingmanager_name_, client_name_, std::chrono::seconds(6)));
    EXPECT_TRUE(client_->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
}

TEST_F(test_client_helper, break_host_side_only_with_broken_pipe) {
    start_apps();

    ASSERT_TRUE(
            disconnect(routingmanager_name_, boost::asio::error::broken_pipe, client_name_, boost::asio::error::eof, socket_role::sender));

    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_, std::chrono::seconds(6)));
    ASSERT_TRUE(await_connection(routingmanager_name_, client_name_, std::chrono::seconds(6)));
}

TEST_F(test_client_helper, handle_early_routing_info_within_server_after_reconnect) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));
    client_->subscription_record_.clear();

    // regression test for the following nightmare sequence:
    // 1. client -> server breaks; depending on implementation might also cause server -> client to break
    // 2. client does REQUEST_SERVICE_ID to routing
    // 3. routing sends ROUTING_INFO+CONFIG_ID to both client+server
    // 4. .. server receives ROUTING_INFO first
    // 5. server -> client (triggered by 1) *NOW* breaks
    // 6. server erases ROUTING_INFO (depending on implementation
    // 7. client receives ROUTING_INFO, sends SUBSCRIBE to server
    // 8. server does not respond because of not having any ROUTING_INFO

    // preparation
    // freeze routing -> client (to hold back ROUTING_INFO, see 4. and 7.)
    ASSERT_TRUE(delay_message_processing(routingmanager_name_, client_name_, true));
    // freeze close reception in server, for server -> client (to hold back close(), see 5.)
    ASSERT_TRUE(set_ignore_inner_close(server_name_, true, client_name_, false));
    // clear commands so we can peek
    clear_command_record(routingmanager_name_, server_name_);

    // trigger 1.
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::eof));
    // 4.
    ASSERT_TRUE(wait_for_command(routingmanager_name_, server_name_, protocol::id_e::CONFIG_ID));
    // 5.
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::connection_reset, client_name_, std::nullopt));
    // 7.
    ASSERT_TRUE(delay_message_processing(routingmanager_name_, client_name_, false));

    EXPECT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_),
                                                       std::chrono::seconds(6)));
}

TEST_F(test_client_helper, missing_initial_events) {
    /**
     * Regression test for the following scenario:
     * 0. router starts
     * 1. server starts
     * 2. server can not bind to port to connect to router
     * 3. server offers field
     * 4. server sets initial value
     * 5. client connects to router
     * 6. client requests the service
     * 7. client subscribes
     * 8. server can connect to router
     * 9. server receives subscription
     * 10. client receives confirmation
     * 11. sometimes the initial field is missing
     **/
    start_router();
    // 2.
    fail_on_bind(server_name_, true);
    // 1.
    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    // 3.
    server_->offer(service_instance_);
    server_->offer_event(offered_event_);
    server_->offer_field(offered_field_);
    // 4.
    send_field_message();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // 5.
    client_ = start_client(client_name_);
    ASSERT_NE(client_, nullptr);
    ASSERT_TRUE(client_->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
    // 6. + 7.
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    // 8.
    fail_on_bind(server_name_, false);
    ASSERT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_),
                                                       std::chrono::seconds(3)));

    EXPECT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));
}


TEST_F(test_client_helper, handle_late_clean_up_within_server_after_reconnect) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_field_message_));
    client_->subscription_record_.clear();
    // ensure we can await the routing info
    clear_command_record(routingmanager_name_, server_name_);

    // try to ensure that:
    // 1. client -> server breaks
    // 2. client requests the routing info
    // 3. the server receives the routing info, before the subscription request is received
    // 4. the client subscribes
    // 5. the server cleans up the connection as a last step

    // ensure that the routing info requested by the client will be received later (for 3.)
    ASSERT_TRUE(delay_message_processing(routingmanager_name_, client_name_, true));
    // for 4.
    ASSERT_TRUE(set_ignore_inner_close(server_name_, true, client_name_, false));

    // trigger 1./2.
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::eof));

    // 3. await the routing info in the server
    ASSERT_TRUE(wait_for_command(routingmanager_name_, server_name_, protocol::id_e::CONFIG_ID));

    // 4. a) receive the routing info b) await the new connection c) await the subscribe command
    ASSERT_TRUE(delay_message_processing(routingmanager_name_, client_name_, false));
    ASSERT_TRUE(await_connection(client_name_, server_name_, std::chrono::seconds(3)));
    ASSERT_TRUE(wait_for_command(client_name_, server_name_, protocol::id_e::SUBSCRIBE_ID));

    // 5. this requires from the server to remove the info
    std::ignore = disconnect(server_name_, boost::asio::error::connection_reset, client_name_, std::nullopt);

    EXPECT_TRUE(client_->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_),
                                                       std::chrono::seconds(6)));
}

struct test_restart_clients : test_client_helper {

    bool subscribe(app* client) {
        client->request_service(service_instance_);
        client->subscribe_field(offered_field_);
        return client->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_),
                                                     std::chrono::seconds(3));
    }

    std::string const client_one_{"client-one"};
    std::string const client_two_{"client-two"};
};

TEST_F(test_restart_clients, test_restart_client_one_and_two_in_reverse_order) {
    start_router();
    start_server();
    {
        create_app(client_one_);
        create_app(client_two_);

        auto* one = start_client(client_one_);
        ASSERT_TRUE(one->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
        ASSERT_TRUE(subscribe(one));
        auto* two = start_client(client_two_);
        ASSERT_TRUE(two->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
    }

    // Hold back the routing info update from client_one for the server (including the remove_client
    // command)
    ASSERT_TRUE(delay_message_processing(routingmanager_name_, server_name_, true));
    TEST_LOG << "[step] stopping the apps";
    stop_client(client_one_);
    stop_client(client_two_);
    TEST_LOG << "[step] restarting the apps";
    {
        create_app(client_one_);
        create_app(client_two_);

        auto* two = start_client(client_two_);
        ASSERT_TRUE(two->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
        auto* one = start_client(client_one_);
        ASSERT_TRUE(one->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED));
        one->request_service(service_instance_);
        one->subscribe_field(offered_field_);
        ASSERT_TRUE(await_connection(client_one_, server_name_, std::chrono::seconds(3)));
        ASSERT_TRUE(wait_for_command(client_one_, server_name_, protocol::id_e::SUBSCRIBE_ID));

        TEST_LOG << "[step] forward routing info";
        // ensure server receives updated routing info
        ASSERT_TRUE(delay_message_processing(routingmanager_name_, server_name_, false));
        // now the new event should be send
        EXPECT_TRUE(one->subscription_record_.wait_for(event_subscription::successfully_subscribed_to(offered_field_)));
    }
}

TEST_F(test_restart_clients, test_restart_client_in_loop) {
    /// sanity test
    /// keep registering and unregistering the same client, over-and-over-and-over-again
    /// shows issues with 7c8d356f2

    start_router();
    start_server();

    for (size_t i = 0; i < 5; ++i) {
        create_app(client_one_);
        auto* one = start_client(client_one_);
        ASSERT_TRUE(one->app_state_record_.wait_for(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(6)));

        TEST_LOG << "[step] stopping the app";
        stop_client(client_one_);
    }
}

TEST_F(test_restart_clients, test_restart_service_availability) {
    /// another sanity test
    /// keep registering and unregistering the same client (which has a fixed client-id),
    ///  and verify whether it always sees a wanted service available

    start_router();
    start_server();

    for (size_t i = 0; i < 10; ++i) {
        create_app(client_one_);
        auto* one = start_client(client_one_);
        one->request_service(service_instance_);

        ASSERT_TRUE(one->availability_record_.wait_for(service_availability::available(service_instance_), std::chrono::seconds(6)));

        TEST_LOG << "[step] stopping the app";
        stop_client(client_one_);
    }
}

struct test_single_connection_breakdown : test_client_helper, ::testing::WithParamInterface<std::pair<std::string, std::string>> { };

TEST_P(test_single_connection_breakdown, ensure_that_every_dropped_connection_is_restored_with_request_reply) {

    auto [from, to] = GetParam();
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    answer_requests_with({0x2, 0x3});
    client_->send_request(request_);
    ASSERT_TRUE(server_->message_record_.wait_for(expected_request_));
    ASSERT_TRUE(client_->message_record_.wait_for(expected_reply_));

    // break one direction
    ASSERT_TRUE(disconnect(from, boost::asio::error::timed_out, to, boost::asio::error::connection_reset));

    for (int i = 0; i < 10; ++i) {
        client_->send_request(request_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    auto next_reply = expected_reply_;
    // It seems some messages are dropped, but the session bumped anyhow.
    next_reply.client_session_.session_ += 8;
    EXPECT_TRUE(client_->message_record_.wait_for(next_reply)) << next_reply;
}

TEST_P(test_single_connection_breakdown, ensure_that_every_dropped_connection_is_restored_with_events) {
    auto [from, to] = GetParam();

    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for(first_expected_message_));

    // break single connection
    ASSERT_TRUE(disconnect(from, boost::asio::error::timed_out, to, boost::asio::error::connection_reset));

    for (int i = 0; i < 10; ++i) {
        send_first_message();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    auto some_expected_message = first_expected_message_;
    // It seems some messages are dropped, but the session bumped anyhow.
    some_expected_message.client_session_.session_ += 8;
    EXPECT_TRUE(client_->message_record_.wait_for(some_expected_message)) << some_expected_message;
}

INSTANTIATE_TEST_SUITE_P(test_all_permutations, test_single_connection_breakdown,
                         ::testing::Values(std::pair{routingmanager_name_, client_name_}, std::pair{client_name_, routingmanager_name_},
                                           std::pair{routingmanager_name_, server_name_}, std::pair{server_name_, routingmanager_name_},
                                           std::pair{client_name_, server_name_}, std::pair{server_name_, client_name_}));
}

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
#include "helpers/command_gate.hpp"

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

struct test_connection_restoration : public base_fake_socket_fixture {
    test_connection_restoration() {
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

    void start_apps() {
        start_router();
        start_server();
        start_client_app();
    }

    void request_service() { client_->request_service(service_instance_); }
    [[nodiscard]] bool await_service() {
        return client_->availability_record_.wait_for_last(service_availability::available(service_instance_));
    }
    [[nodiscard]] bool subscribe_to_event() {
        request_service();
        client_->subscribe_event(offered_event_);
        return client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_));
    }
    [[nodiscard]] bool subscribe_to_field() {
        request_service();
        client_->subscribe_field(offered_field_);
        return client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_));
    }
    void send_first_message() { server_->send_event(offered_event_, {}); }
    void send_field_message() { server_->send_event(offered_field_, field_payload_); }
    void answer_requests_with(std::vector<unsigned char> _payload) {
        server_->answer_request(request_, [payload = _payload] { return payload; });
        expected_reply_.payload_ = _payload;
    }

    void stop_offer() { server_->stop_offer(service_instance_); }

    [[nodiscard]] bool wait_for_last_available(app* _app, service_instance const& _si) {
        return _app->availability_record_.wait_for_last(service_availability::available(_si));
    }

    service_instance service_instance_{0x3344, 0x1};
    service_instance service_instance_two_{0x3345, 0x1};
    event_ids offered_event_{service_instance_, 0x8002, 0x1};
    message first_expected_message_{
            client_session{0, 1}, service_instance_, offered_event_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, {}};
    event_ids offered_field_{service_instance_, 0x8003, 0x6};
    std::vector<unsigned char> field_payload_{0x42, 0x13};
    message first_expected_field_message_{client_session{0, 2}, // todo, why is the session a two here?
                                          service_instance_, offered_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                                          field_payload_};
    message_checker const field_checker_{std::nullopt, service_instance_, offered_field_.event_id_,
                                         vsomeip::message_type_e::MT_NOTIFICATION, field_payload_};

    vsomeip_v3::method_t method_{0x1111};
    request request_{service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_request_{client_session{0x3490 /*client id*/, 1}, service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_reply_{client_session{0x3490 /*client id*/, 1}, service_instance_, method_, vsomeip::message_type_e::MT_RESPONSE, {}};

    std::vector<service_instance> service_instances{{0x3344, 0x1}, {0x3345, 0x1}, {0x3346, 0x1}, {0x3347, 0x1},
                                                    {0x3348, 0x1}, {0x3349, 0x1}, {0x334A, 0x1}, {0x334B, 0x1},
                                                    {0x334C, 0x1}, {0x334D, 0x1}, {0x334E, 0x1}};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
};

TEST_F(test_connection_restoration, client_renews_connection_deletes_client_info) {
    /**
     * When a server does not notice that a client connection is broken,
     * a new connection needs to trigger a clean-up of the former client state.
     * In the following scenario a single client will be restarted,
     * but subscribing in the first lifecycle to a field, but in the second
     * to some other event.
     * If the server correctly cleaned up the state, no update of the initial
     * subscription is received.
     **/
    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    ASSERT_TRUE(set_ignore_inner_close(client_name_, false, server_name_, true));
    TEST_LOG << " ##### Stopping the client #####";
    stop_client(client_name_);
    TEST_LOG << " ##### Starting the client #####";
    create_app(client_name_);
    start_client_app();
    ASSERT_TRUE(subscribe_to_event());
    TEST_LOG << " ##### Sending the field #####";

    send_field_message();
    // 10ms are enough to be at least flaky if the server is not correctly cleaning
    // the state
    ASSERT_FALSE(wait_for_command(client_name_, server_name_, protocol::id_e::SEND_ID, socket_role::client, std::chrono::milliseconds(10)));
}
TEST_F(test_connection_restoration, re_registered_client_deletes_client_info) {
    /**
     * When the router does not notice that a client connection is broken,
     * a new connection (fixed client id scenario) needs to trigger a clean-up
     * of the former client state.
     * In the following scenario a single client will be restarted,
     * but requesting in the first lifecycle only to a service.
     * If the router properly cleans up the client data, then no notification
     * should be send on an unavail/avail toggle to the client.
     **/
    start_apps();
    request_service();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // ensure that the routing_managers error handler will not be invoked
    ASSERT_TRUE(set_ignore_inner_close(client_name_, false, routingmanager_name_, true));
    // ensure that the router will not allow for reconnect
    set_ignore_connections(routingmanager_name_, true);
    // a disconnect is faster then a graceful shutdown (and with the option above the router will not notice)
    TEST_LOG << " ##### Stopping the client #####";
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::timed_out, routingmanager_name_, std::nullopt));
    // this ensures that the connection was really dropped for the client (and speeds up the stop)
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    stop_client(client_name_);

    TEST_LOG << " ##### Starting the client #####";
    create_app(client_name_);
    // allow the client to re-register
    set_ignore_connections(routingmanager_name_, false);
    start_client_app();

    clear_command_record(client_name_, routingmanager_name_); // because ROUTING_INFO is also the registration completion
    // when the client state is not cleaned-up, then the client would be informed about the dropped service
    stop_offer();
    // 10ms are enough to be at least flaky if the server is not correctly cleaning
    // the state
    ASSERT_FALSE(wait_for_command(client_name_, routingmanager_name_, protocol::id_e::ROUTING_INFO_ID, socket_role::client,
                                  std::chrono::milliseconds(10)));
}

TEST_F(test_connection_restoration, when_a_client_connection_is_broken_just_after_accept_then_it_can_be_cleand_up) {

    /**
     * 1. The client attempts to connect
     * 2. router receives the connection and answers the assign_client_id
     * 3. router tries to send the config_id
     * 4. connection is broken
     * 5. endpoint on router side would enter the failed state (if it could)
     * 6. endpoint_manager only now registers the error handler
     * 7. endpoint did not clean-up itself.
     **/
    start_router();
    auto disconnect_on_assign_command = [this](auto const& _command) {
        if (_command.id_ == vsomeip_v3::protocol::id_e::ASSIGN_CLIENT_ACK_ID) {
            // 1. ensure the connection is broken
            if (!disconnect(client_name_, boost::asio::error::timed_out, routingmanager_name_, std::nullopt)) {
                TEST_LOG << "[ERROR] this shouldn't happen!!!!!";
            }
            // 2. second registration attempt should be successful
            set_custom_command_handler(client_name_, routingmanager_name_, nullptr);
        }
        return false;
    };
    // some iterations are needed as the former defect would only sometimes manifest
    // But due to the test inherent restart of the sender (+debounce of the restart),
    // any iteration is costing 100ms -> keep the iteration count low, to ensure
    // this test is at least flaky.
    for (int i = 0; i < 5; ++i) {
        set_custom_command_handler(client_name_, routingmanager_name_, disconnect_on_assign_command, socket_role::server);

        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        stop_client(client_name_);
        create_app(client_name_);
    }
}

TEST_F(test_connection_restoration, outdated_routing_info_will_not_cause_a_wrong_permanent_connection) {

    // This test ensures that outdated routing_info can not lead to a wrong
    // connection mapping:
    // 1. Service is offered
    // 2. Client requests service
    // 3. Router sends info out, while service application is stopped
    // 4. A new application is started claiming the former ip address + port
    // 5. Client establishes connection to new app, thinking this app offers the
    //    service.
    // 6. Ensure that the connection is not kept

    // 1.
    start_apps();
    // to ensure 3., that the routing info is not received by the client, before the service is stopped
    ASSERT_TRUE(delay_message_processing(client_name_, routingmanager_name_, true,
                                         socket_role::client)); // Note that the client can still request a service

    // 2.
    clear_command_record(client_name_, routingmanager_name_); // because ROUTING_INFO is also the registration completion
    request_service();
    client_->subscribe_field(offered_field_);
    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, protocol::id_e::ROUTING_INFO_ID, socket_role::client));

    // 3.
    stop_client(server_name_); // stop server

    // 4. Note: The test requires that the new_app claims the port just freed from the server
    auto new_app_name = "new_application";
    create_app(new_app_name);
    auto new_app = start_client(new_app_name);
    ASSERT_NE(new_app, nullptr);
    ASSERT_TRUE(new_app->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // 5. trigger client tries to connect
    ASSERT_TRUE(delay_message_processing(client_name_, routingmanager_name_, false, socket_role::client));

    // 6. we expect that the established connection is dropped (note that the predicate includes the check that connection has been
    // established)
    EXPECT_TRUE(wait_for_connection_drop(client_name_, new_app_name));
}

TEST_F(test_connection_restoration, reproduction_allow_reconnects_on_first_try_between_router_and_client) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(disconnect(client_name_, std::nullopt, routingmanager_name_, boost::asio::error::timed_out));

    client_->subscription_record_.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the client:
    set_ignore_connections(client_name_, false);
    // resume of the client:
    std::ignore = disconnect(client_name_, boost::asio::error::connection_reset, routingmanager_name_, std::nullopt);

    // in order for the server to be able to send an event, the server needs to connect back
    // to the client. This can only be traced via the socket connection state afaik.
    // But the connection will break down only after some connection attempt to the router failed?
    // Therefore first await that the client <-> router connections are re-established, before
    // awaiting that the client <-> server connections are re-established.
    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_));
    ASSERT_TRUE(await_connection(client_name_, server_name_));

    // client needs to re-subscribe, wait for it
    EXPECT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    server_->send_event(offered_event_, {0x2, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x2, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for_last(next_expected_message)) << client_->message_record_;

    // Expect two connection that where established from the client towards,
    // the routing manager:
    // 1. client registers initially
    // 2. client tries to repair the connection
    // more connection attempts can happen because of "disconnect cascades", e.g., routing breaking the connection repair when
    // the routing -> client connection breaks (see also 0fe029daa, server_endpoint::disconnect_from)
    EXPECT_LE(2, connection_count(client_name_, routingmanager_name_));
}

TEST_F(test_connection_restoration, client_server_connection_breakdown_on_client_suspend_with_events) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));

    // simulating a suspend of a client (the client should not react to any action of the server)
    ASSERT_TRUE(set_ignore_inner_close(client_name_, true, server_name_, false));
    set_ignore_connections(client_name_, true);
    set_ignore_nothing_to_read_from(client_name_, server_name_, socket_role::client, true);

    ASSERT_TRUE(disconnect(client_name_, std::nullopt, server_name_, boost::asio::error::timed_out));

    client_->subscription_record_.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the client:
    set_ignore_connections(client_name_, false);
    set_ignore_nothing_to_read_from(client_name_, server_name_, socket_role::client, false);
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, std::nullopt));

    ASSERT_TRUE(await_connection(client_name_, server_name_));

    // client needs to re-subscribe, wait for it
    EXPECT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    server_->send_event(offered_event_, {0x5, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for_last(next_expected_message));
}

TEST_F(test_connection_restoration, client_server_connection_breakdown_on_server_suspend_with_events) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));

    // simulating a suspend of a server:
    set_ignore_connections(server_name_, true);
    set_ignore_nothing_to_read_from(client_name_, server_name_, socket_role::server, true);
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::timed_out, server_name_, std::nullopt));

    client_->subscription_record_.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the server:
    set_ignore_connections(server_name_, false);
    set_ignore_nothing_to_read_from(client_name_, server_name_, socket_role::server, false);
    std::ignore = disconnect(client_name_, std::nullopt, server_name_, boost::asio::error::connection_reset);

    ASSERT_TRUE(await_connection(client_name_, server_name_));

    // client needs to re-subscribe, wait for it
    EXPECT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    server_->send_event(offered_event_, {0x5, 0x3});
    auto next_expected_message = first_expected_message_;
    next_expected_message.client_session_.session_ = 2;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(client_->message_record_.wait_for_last(next_expected_message));
}

TEST_F(test_connection_restoration, field_updates_are_resend_when_a_broken_routing_connection_enforces_a_resubscription) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));

    // simulating a suspend of a client:
    set_ignore_nothing_to_read_from(client_name_, routingmanager_name_, socket_role::client, true);
    ASSERT_TRUE(disconnect(client_name_, std::nullopt, routingmanager_name_, boost::asio::error::timed_out));

    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    // resume of the client:
    set_ignore_nothing_to_read_from(client_name_, routingmanager_name_, socket_role::client, false);
    std::ignore = disconnect(client_name_, boost::asio::error::connection_reset, routingmanager_name_, std::nullopt);

    // because the client will re-subscribe it should receive another event after the subscription
    auto new_message = first_expected_field_message_;
    new_message.client_session_.session_ += 1;
    EXPECT_TRUE(client_->message_record_.wait_for_last(new_message));
}

TEST_F(test_connection_restoration, server_times_out_on_first_connect_attemp_with_client) {
    start_apps();

    report_on_connect(client_name_, {boost::asio::error::timed_out});

    EXPECT_TRUE(subscribe_to_event());
}

TEST_F(test_connection_restoration, client_receives_an_operation_abort_on_connection_attempt) {
    /**
     * 0. client -> router broke.
     * 1. client tries to reconnect
     * 2. timeout for connection attempt expires
     * 3. client retries
     * 4. clients connect_cbk is invoked with operation_abort from previous stop socket
     * 5. client should not hang
     **/
    start_apps();

    client_->app_state_record_.clear();

    report_on_connect(routingmanager_name_, {boost::asio::error::timed_out, boost::asio::error::operation_aborted});
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::timed_out, routingmanager_name_, std::nullopt));

    EXPECT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

TEST_F(test_connection_restoration, client_ignores_server_connection_attempt_once) {
    start_apps();

    ignore_connections(client_name_, 1);

    EXPECT_TRUE(subscribe_to_event());
}

TEST_F(test_connection_restoration,
       given_server_to_client_breaksdown_when_the_connection_is_re_established_then_the_subscription_confirmed) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
    client_->subscription_record_.clear();

    // give the client a head start when cleaning the "sibling" connection, by holding back the
    // server execution.
    ASSERT_TRUE(block_on_close_for(client_name_, std::nullopt, server_name_, std::chrono::milliseconds(200)));

    // break the server->client connection. Notice that if the blocking above is in place,
    // the "wrong" order of cleaning up server->client + client->server will lead to a race
    // condition.
    // --> No longer applicable, but we keep the test for now?
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::timed_out));

    EXPECT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
}

TEST_F(test_connection_restoration, break_client_with_eof_before_finishing_registration) {

    // Start apps manually to break the connection before the registration happens
    routingmanagerd_ = start_client(routingmanager_name_);
    ASSERT_NE(routingmanagerd_, nullptr);
    ASSERT_TRUE(await_connectable(routingmanager_name_));

    client_ = start_client(client_name_);
    ASSERT_NE(client_, nullptr);
    ASSERT_TRUE(await_connectable(client_name_));
    // Do not wait for client registering and break the connection immediately

    TEST_LOG << " ##### BREAKING Client connection before registration #####";
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::eof, routingmanager_name_, boost::asio::error::eof, socket_role::client));

    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_));
    EXPECT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

TEST_F(test_connection_restoration, break_host_side_only_with_broken_pipe) {
    start_apps();

    ASSERT_TRUE(
            disconnect(client_name_, boost::asio::error::eof, routingmanager_name_, boost::asio::error::broken_pipe, socket_role::server));

    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_));
}

TEST_F(test_connection_restoration, handle_late_clean_up_within_server_after_reconnect) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
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
    ASSERT_TRUE(delay_message_processing(client_name_, routingmanager_name_, true, socket_role::server));
    // for 4.
    ASSERT_TRUE(set_ignore_inner_close(client_name_, false, server_name_, true));

    // trigger 1./2.
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::eof));

    // 3. await the routing info in the server
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, protocol::id_e::CONFIG_ID, socket_role::client));

    // 4. a) receive the routing info b) await the new connection c) await the subscribe command
    ASSERT_TRUE(delay_message_processing(client_name_, routingmanager_name_, false, socket_role::server));
    ASSERT_TRUE(await_connection(client_name_, server_name_));
    ASSERT_TRUE(wait_for_command(client_name_, server_name_, protocol::id_e::SUBSCRIBE_ID, socket_role::server));

    // 5. this requires from the server to remove the info
    std::ignore = disconnect(server_name_, boost::asio::error::connection_reset, client_name_, std::nullopt);

    EXPECT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
}

TEST_F(test_connection_restoration, availability_callback_is_only_called_once_on_connection_break) {
    /**
     * Regression test for the following scenario:
     * 0. router, server and client are started
     * 1. client subscribes the service
     * 2. first break the connection client -> server
     * 3. then break the connection daemon -> server
     * small delay between 2. and 3. to ensure that the client REQUESTS the service again before the daemon is able to handle 3.
     * 4. verify that the client only received 1 ON_AVAILABLE and 1 ON_UNAVAILABLE
     **/

    std::vector<service_availability> expected_availabilities = {service_availability::available(service_instance_),
                                                                 service_availability::unavailable(service_instance_)};

    std::vector<service_availability> unexpected_availabilities = {service_availability::available(service_instance_),
                                                                   service_availability::unavailable(service_instance_),
                                                                   service_availability::available(service_instance_)};

    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    // Delay processing of messages from daemon->server to simulate a situation where the connection
    // is broken but the daemon does not know it yet, this further ensures that the availability is not sent due to test timings
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true, socket_role::server));

    // ensure to former service request is recorded
    clear_command_record(client_name_, routingmanager_name_);

    // Break client -> server connection first
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::eof, server_name_, std::nullopt));

    // Wait for the client to re-request the service:
    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, protocol::id_e::REQUEST_SERVICE_ID, socket_role::server));

    // Break daemon -> server connection later
    ASSERT_TRUE(disconnect(server_name_, std::nullopt, routingmanager_name_, boost::asio::error::eof));

    // Wait for some time to ensure that the availabilities are not checked too early
    ASSERT_FALSE(client_->availability_record_.wait_for(
            [&unexpected_availabilities](const auto& record) { return record == unexpected_availabilities; },
            std::chrono::milliseconds(300)))
            << client_->availability_record_;

    ASSERT_TRUE(client_->availability_record_.wait_for([&expected_availabilities](const auto& record) {
        return record == expected_availabilities;
    })) << client_->availability_record_;
}

TEST_F(test_connection_restoration, service_re_request_is_answered) {
    /**
     * 0. router, server and client are started
     * 1. client subscribes the service
     * 2. break the client -> server connection to trigger a re-request of the service
     * 3. await the router->server connection to be re-established
     * 4. verify that the re-request was processed and the client received the availability
     **/

    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    // Clear previous received availabilities to wait for a new one after the re-request
    VSOMEIP_INFO << "Clearing availabilities";
    client_->availability_record_.clear();

    // Break client->server connection to simulate a re-request from the client
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::eof, server_name_, std::nullopt));

    ASSERT_TRUE(await_connection(server_name_, routingmanager_name_));

    // As the server will come back online it is expected that the service becomes available again
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
}

TEST_F(test_connection_restoration, client_does_not_receive_availability_for_unrequested_service) {
    /**
     * 0. router, server and client are started
     * 1. delay message processing on one of the servers to simulate it being temporarily offline
     * 2. break the client -> server connection to trigger a re-request of the service
     * 3. stop client application
     * 4. start new client application, with same client id
     * 5. verify that the new client does not receive the availability for the service requested by the previous client
     **/

    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    // Delay processing of messages from daemon->server to simulate a situation where the server is offline
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true, socket_role::server));

    // Clear previous received availabilities to wait for a new one after the re-request
    VSOMEIP_INFO << "Clearing availabilities";
    client_->availability_record_.clear();

    // Break client->server connection to simulate a re-request from the client
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::eof, server_name_, std::nullopt));

    ASSERT_TRUE(wait_for_last_command(client_name_, routingmanager_name_, socket_role::server, protocol::id_e::REQUEST_SERVICE_ID));

    stop_client(client_name_);

    create_app(client_name_);
    auto new_client_ = start_client(client_name_);
    ASSERT_NE(new_client_, nullptr);
    ASSERT_TRUE(new_client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // Re-enable delayed daemon->server communication. The daemon can now process the PONG from server,
    // and attempt to send availability for the previously re-requested service.
    // This tests that the new client (with the same client ID) will not be updated.
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, false, socket_role::server));

    // Ensure that the new client does not receive the availability for an unrequested service
    ASSERT_FALSE(new_client_->availability_record_.wait_for_last(service_availability::available(service_instance_),
                                                                 std::chrono::milliseconds(200)));
}

TEST_F(test_connection_restoration, both_clients_receive_availability_for_re_requested_services) {
    /**
     * 0. router, server and client are started
     * 1. client subscribes the service
     * 2. start second client application, that requests and subscribes to same service
     * 3. delay message processing on one of the servers to simulate it being temporarily offline
     * 4. break both client -> server connections to trigger a re-request of the service
     * 5. verify that both clients receive the availability for the service
     **/

    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    // Start second client application
    create_app(client_name_two_);
    auto new_client_ = start_client(client_name_two_);
    ASSERT_NE(new_client_, nullptr);
    ASSERT_TRUE(new_client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // Second client application requests and subscribes the service
    new_client_->request_service(service_instance_);
    ASSERT_TRUE(new_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    new_client_->subscribe_field(offered_field_);
    ASSERT_TRUE(new_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    // Delay processing of messages from daemon->server to simulate a situation where the server is offline
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true, socket_role::server));

    // Break both client->server connections
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::eof, server_name_, std::nullopt));
    ASSERT_TRUE(disconnect(client_name_two_, boost::asio::error::eof, server_name_, std::nullopt));

    // Clear previous received availabilities to wait for a new one after the re-request
    client_->availability_record_.clear();
    new_client_->availability_record_.clear();

    ASSERT_TRUE(wait_for_last_command(client_name_, routingmanager_name_, socket_role::server, protocol::id_e::REQUEST_SERVICE_ID));

    // Re-enable delayed daemon->server communication.
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, false, socket_role::server));

    // Ensure that both clients receive the availability
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(new_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
}

TEST_F(test_connection_restoration, routing_info_conflict_routingmanagerd) {
    /// simulate a situation where an (routing) application has multiple (client) client-ids to the same address/port
    /// which for IPv4 is a problem - can not connect twice to the same address/port, so one of the clients must be dropped!

    start_router();

    // start server with fixed client-id (using `server_name_`) that offers first service
    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    server_->offer(service_instance_);

    // start client with fixed client-id (using `client_name_`) that wants two services
    client_ = start_client(client_name_);
    ASSERT_NE(client_, nullptr);
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    client_->request_service(service_instance_);
    client_->request_service(service_instance_two_);

    // ensure client only sees first service available
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    EXPECT_FALSE(client_->availability_record_.wait_for_any(service_availability::available(service_instance_two_),
                                                            std::chrono::milliseconds(100)));
    client_->availability_record_.clear();

    // now it becomes tricky
    // we need to stop server, but such that routingmanagerd does not see it deregistering
    ASSERT_TRUE(set_ignore_inner_close(server_name_, true, routingmanager_name_, true)); // avoid ltsei connection closing
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true)); // avoid DEREGISTER_APPLICATION_ID

    // stimulate an error on server, so that it will not block on deregister
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::eof, routingmanager_name_, std::nullopt));
    stop_client(server_name_); // stop server

    // ensure client never sees first service becoming unavailable
    // the "sleep" is DISGUSTING! but I have no better idea
    EXPECT_FALSE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_),
                                                             std::chrono::milliseconds(100)));

    // start another server with same address/port (implicit!), but different client-id (using `server_name_two_`), and that offers
    // a second service

    // ensure client still did not see this other service
    EXPECT_FALSE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_two_),
                                                             std::chrono::milliseconds(100)));

    create_app(server_name_two_);
    auto* another_server = start_client(server_name_two_);
    ASSERT_NE(another_server, nullptr);
    ASSERT_TRUE(another_server->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    another_server->offer(service_instance_two_);

    // finally, we expect:
    // 1) client sees second service available
    EXPECT_TRUE(client_->availability_record_.wait_for_any(service_availability::available(service_instance_two_)));
    // 2) client sees first service unavailable - because routing understood it is gone!
    EXPECT_TRUE(client_->availability_record_.wait_for_any(service_availability::unavailable(service_instance_)));
}

TEST_F(test_connection_restoration, subscribe_eventgroup_connection_break) {
    /// check whether a subscription to an event group is restored after a connection break

    start_apps();

    send_field_message();

    client_->request_service(service_instance_);
    client_->subscribe_eventgroup_field(offered_field_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    ASSERT_TRUE(client_->message_record_.wait_for([](auto const& record) { return record.size() > 0; }));

    // need to increase the odds, hence the extra iterations
    for (size_t i = 0; i < 100; ++i) {
        TEST_LOG << "Iteration " << i;

        client_->availability_record_.clear();
        client_->subscription_record_.clear();
        client_->message_record_.clear();

        // client -> server connection breaks..
        std::ignore = disconnect(client_name_, boost::asio::error::timed_out, server_name_, std::nullopt);

        // we *MUST* see available/subscribed/message
        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
        ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
        ASSERT_TRUE(client_->message_record_.wait_for([](auto const& record) { return record.size() > 0; }));
    }
}

TEST_F(test_connection_restoration, ensure_the_correct_routing_info_is_kept) {

    // 1. Starts router
    // 2. Starts server and offers service
    // 3. routingmanagerd requests service and sees it available
    // 4. Server is suspended and router does not know about it
    // 5. Server is stopped due to eof
    // 6. Another server starts and offers another service
    // 7. Client starts and requests the service_instance_two_
    // 8. Client requests the service_instance_
    // 9. Client should see UNAVAILABLE for the first service, but AVAILABLE for the second service

    // 1.
    start_router();

    // 2.
    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    server_->offer(service_instance_);

    // 3.
    routingmanagerd_->request_service(service_instance_);
    ASSERT_TRUE(wait_for_last_available(routingmanagerd_, service_instance_));

    // 4.
    // Ensure that router does not see the server stopping
    ASSERT_TRUE(set_ignore_inner_close(server_name_, true, routingmanager_name_, true)); // avoid ltsei connection closing
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true)); // avoid DEREGISTER_APPLICATION_ID
    set_ignore_broken_pipe(routingmanager_name_,
                           true); // avoid broken pipe errors so that that router does not react to the broken connection
    set_ignore_nothing_to_read_from(server_name_, routingmanager_name_, socket_role::server, true);

    // 5.
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::eof, routingmanager_name_, std::nullopt));
    stop_client(server_name_); // stop server

    // 6.
    // At this point in time, router still believes that the first server exists and nothing has happened
    // Means, routing info is still there for the "broken server"
    // note: this new server will have the same ip/port, but different client-id
    create_app(server_name_two_);
    auto* another_server = start_client(server_name_two_);
    ASSERT_NE(another_server, nullptr);
    ASSERT_TRUE(another_server->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    another_server->offer(service_instance_two_);

    // 7.
    client_ = start_client(client_name_);
    ASSERT_NE(client_, nullptr);
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    client_->request_service(service_instance_two_);

    ASSERT_TRUE(wait_for_last_available(client_, service_instance_two_));

    // 8.
    client_->request_service(service_instance_);

    // 9.
    std::vector<service_availability> expected_availabilities = {
            service_availability::available(service_instance_two_),
    };

    ASSERT_TRUE(client_->availability_record_.wait_for([&expected_availabilities](const auto& record) {
        return record == expected_availabilities;
    })) << client_->availability_record_;

    // the clean-up of the ep is asynchronous therefore there is a chance that the client might see
    // an available of the first service, but if this happens we need to ensure that the last availability
    // for this very service is unavail.
    if (client_->availability_record_.wait_for_any(service_availability::available(service_instance_), std::chrono::milliseconds(100))) {
        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)))
                << client_->availability_record_;
    }
}

struct test_reoffer_on_connection_breaking : test_connection_restoration {

    void setup() {
        ASSERT_TRUE(setup_data_pipe(client_name_, routingmanager_name_, socket_role::client, router_to_client_gate_->get_data_pipe()));
        start_apps();
        request_service(); // do not subscribe to an event!
        answer_requests_with(expected_payload_);
        auto c = client_->get_application();
        // let the client always dispatch a request on availability
        c->register_availability_handler(
                service_instance_.service_, service_instance_.instance_,
                [this, weak_c = std::weak_ptr<vsomeip::application>(c)](service_t, instance_t, vsomeip::availability_state_e state) {
                    if (state == vsomeip::availability_state_e::AS_AVAILABLE) {
                        if (auto self = weak_c.lock(); self) {
                            TEST_LOG << "Client received service state: AVAILABLE";
                            client_->send_request(request_);
                        }
                    } else {
                        TEST_LOG << "Client received service state: UNAVAILABLE";
                    }
                });
    }

    // setup the control to be in control when the routing_info is received by the client
    // to increase chances of a concurrent routing_info reception + clean_up
    std::shared_ptr<command_gate> router_to_client_gate_ = command_gate::create();
    std::vector<unsigned char> expected_payload_ = {0x1, 0x2, 0x3};
    message_checker const response_checker_{std::nullopt, service_instance_, method_, vsomeip::message_type_e::MT_RESPONSE,
                                            expected_payload_};
};

TEST_F(test_reoffer_on_connection_breaking, client_can_dispatch_a_request_after_reoffer_during_connection_break) {
    /**
     * Ensure that when a client sees:
     * 1. routing info for a service that is re-offered (without a subscription)
     * 2. the connection breaking to the service that re-offers
     * 3. after the routing_info is received the connection breaks to the router
     *
     * that an "automatic" request (on availability) receives a response.
     **/
    setup();

    // the automatic request will ensure that we start the loop
    // with an established connection that can break.
    ASSERT_TRUE(await_service());
    ASSERT_TRUE(client_->message_record_.wait_for(response_checker_));

    for (int i = 0; i < 100; ++i) {
        TEST_LOG << "### Iteration: " << i << " START";

        // 0. Stop the offer
        client_->availability_record_.clear();
        stop_offer();
        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
        client_->message_record_.clear();
        client_->app_state_record_.clear();

        // 1. Hold back the routing_info at the clients "gate" to ensure it is received in close repetition with 2. and 3.
        router_to_client_gate_->block_at(vsomeip_v3::protocol::id_e::ROUTING_INFO_ID, 1);
        server_->offer(service_instance_);
        ASSERT_TRUE(router_to_client_gate_->wait_for_blocked());

        // 1.
        router_to_client_gate_->block(false);
        // 2.
        ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::timed_out));
        // 3.
        ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, routingmanager_name_, boost::asio::error::timed_out));

        // checking of sanity
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        ASSERT_TRUE(await_connection(client_name_, server_name_));
        ASSERT_TRUE(client_->message_record_.wait_for(response_checker_));
        ASSERT_TRUE(await_service());
        TEST_LOG << "### Iteration: " << i << " END";
    }
}
TEST_F(test_reoffer_on_connection_breaking, client_receives_response_after_reoffer_during_connection_break) {
    setup();
    /**
     * Ensure that when a client sees:
     * 1. routing info for a service that is re-offered (without a subscription)
     * 2. the connection breaking to the service that re-offers
     *
     * that an "automatic" request (on availability) receives a response.
     **/

    // the automatic request will ensure that we start the loop
    // with an established connection that can break.
    ASSERT_TRUE(await_service());
    ASSERT_TRUE(client_->message_record_.wait_for(response_checker_));
    client_->message_record_.clear();

    for (int i = 0; i < 100; ++i) {
        TEST_LOG << "### Iteration: " << i << " START";

        // 0. Stop the offer
        stop_offer();

        // 1. Hold back the routing_info at the clients "gate" to ensure it is received in close repetition with 2.
        router_to_client_gate_->block_at(vsomeip_v3::protocol::id_e::ROUTING_INFO_ID);
        server_->offer(service_instance_);
        ASSERT_TRUE(router_to_client_gate_->wait_for_blocked());

        // 1.
        router_to_client_gate_->block(false);
        // 2.
        ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::timed_out));

        // it is important to check as a first step that the connection was re-established,
        // and that then the service is getting available
        // and only then does the reply matter.
        // (If we would directly check the service and the reply, they might have been captured before the connection was disconnected,
        // leading to a false positive failure in the next iteration)
        ASSERT_TRUE(await_connection(client_name_, server_name_));
        ASSERT_TRUE(await_service());
        client_->availability_record_.clear();
        ASSERT_TRUE(client_->message_record_.wait_for(response_checker_));
        client_->message_record_.clear();
        TEST_LOG << "### Iteration: " << i << " END";
    }
}

TEST_F(test_connection_restoration, offer_and_stop_service_concurrency) {

    // 1. Setup applications (does not start second server just yet)
    // 2. Offer multiple services on the server (x11 just an arbitrary number)
    // 3. Request those services on the client, and wait for all availabilities
    // 4. Starts another server, and offer the same services on it
    // 5. Simulates a non responding server by disconnecting it from the router
    // 6. Second server offers the same services
    // 7. Wait for the client to recognize the availability of the services offered by
    // second server

    std::vector<availability_checker> availability_checkers_{[this]() {
        std::vector<availability_checker> checkers;
        for (const auto& si : service_instances) {
            checkers.emplace_back(service_instance{si}, vsomeip::availability_state_e::AS_AVAILABLE);
        }
        return checkers;
    }()};

    std::vector<availability_checker> un_availability_checkers_{[this]() {
        std::vector<availability_checker> checkers;
        for (const auto& si : service_instances) {
            checkers.emplace_back(service_instance{si}, vsomeip::availability_state_e::AS_UNAVAILABLE);
        }
        return checkers;
    }()};

    start_apps();
    create_app(server_name_two_);

    // Following loops are separated purposely.
    for (auto service : service_instances) {
        server_->offer(service);
    }

    for (auto service : service_instances) {
        client_->request_service(service);
    }

    for (const auto& checker : availability_checkers_) {
        ASSERT_TRUE(client_->availability_record_.wait_for(checker));
    }

    client_->availability_record_.clear();

    auto* another_server = start_client(server_name_two_);
    ASSERT_NE(another_server, nullptr);
    ASSERT_TRUE(another_server->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    ASSERT_TRUE(set_ignore_inner_close(server_name_, false, routingmanager_name_, true));
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true, socket_role::client));
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::eof, routingmanager_name_, boost::asio::error::connection_reset,
                           socket_role::client));

    for (auto service : service_instances) {
        another_server->offer(service);
    }

    // Unfortunately, we need to increase the timeout in the following operations, because the second server might be
    // waiting for the pong from the first server, which is still deregistering..thus, pong timeout needs to happen
    // and then the offer can be send once again by the second server.

    // TODO: this should be handled in a more efficient way, new offer/availability is being delayed by the pong timeout of the first
    // server, which is not ideal, but currently the best we can do to increase the odds of this test to be stable

    // Also, we wait for any because by the time we do the check the new availabily may be already processed, therefore
    // we cannot simply look for the last availability.

    // Note, the availabilities are not going to receive all at once, means, availabilities will be processed depending on
    // order of the offer commands
    for (auto checker : availability_checkers_) {
        ASSERT_TRUE(client_->availability_record_.wait_for(checker, std::chrono::seconds(6))) << client_->availability_record_;
    }

    for (const auto& checker : un_availability_checkers_) {
        // Here the default timeout should be more than enough, as before, we may have waited for some time already for
        // the availabilities
        ASSERT_TRUE(client_->availability_record_.wait_for(checker)) << client_->availability_record_;
    }
}

struct test_single_connection_breakdown : test_connection_restoration,
                                          ::testing::WithParamInterface<std::pair<std::string, std::string>> { };

TEST_P(test_single_connection_breakdown, ensure_that_every_dropped_connection_is_restored_with_request_reply) {

    auto [from, to] = GetParam();
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    answer_requests_with({0x2, 0x3});
    client_->send_request(request_);
    ASSERT_TRUE(server_->message_record_.wait_for_last(expected_request_));
    ASSERT_TRUE(client_->message_record_.wait_for_last(expected_reply_));

    // break one direction
    ASSERT_TRUE(disconnect(from, boost::asio::error::timed_out, to, boost::asio::error::connection_reset));

    for (int i = 0; i < 10; ++i) {
        client_->send_request(request_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Ensure connection is restored
    ASSERT_TRUE(await_connection(from, to));

    auto next_reply = expected_reply_;
    // It seems some messages are dropped, but the session bumped anyhow.
    next_reply.client_session_.session_ += 8;
    EXPECT_TRUE(client_->message_record_.wait_for_any(next_reply)) << next_reply;
}

TEST_P(test_single_connection_breakdown, ensure_that_every_dropped_connection_is_restored_with_events) {
    auto [from, to] = GetParam();

    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));

    // break single connection
    ASSERT_TRUE(disconnect(from, boost::asio::error::timed_out, to, boost::asio::error::connection_reset));

    for (int i = 0; i < 10; ++i) {
        send_first_message();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Ensure connection is restored
    ASSERT_TRUE(await_connection(from, to));

    auto some_expected_message = first_expected_message_;
    // It seems some messages are dropped, but the session bumped anyhow.
    some_expected_message.client_session_.session_ += 8;
    EXPECT_TRUE(client_->message_record_.wait_for_any(some_expected_message)) << some_expected_message;
}

INSTANTIATE_TEST_SUITE_P(test_all_permutations, test_single_connection_breakdown,
                         ::testing::Values(std::pair{client_name_, routingmanager_name_}, std::pair{server_name_, routingmanager_name_},
                                           std::pair{client_name_, server_name_}));
}

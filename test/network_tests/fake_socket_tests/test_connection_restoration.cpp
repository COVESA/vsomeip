// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/command_record.hpp"
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
static std::string const server_name_two_{"server_two"}; // without a fixed-id in config
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

    vsomeip_v3::method_t method_{0x1111};
    request request_{service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_request_{client_session{0x3490 /*client id*/, 1}, service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_reply_{client_session{0x3490 /*client id*/, 1}, service_instance_, method_, vsomeip::message_type_e::MT_RESPONSE, {}};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
};

struct test_protocol_messages : test_client_helper {

    std::shared_ptr<command_record> record_{command_record::create()};

    void track_client_server() {
        set_custom_command_handler(server_name_, client_name_, record_->create_collector(server_to_client_), socket_role::client);
        set_custom_command_handler(server_name_, client_name_, record_->create_collector(client_to_server_), socket_role::server);
        set_custom_command_handler(client_name_, server_name_, record_->create_collector(server_to_client_), socket_role::server);
        set_custom_command_handler(client_name_, server_name_, record_->create_collector(client_to_server_), socket_role::client);
    }
    void track_client_router() {
        set_custom_command_handler(routingmanager_name_, client_name_, record_->create_collector(router_to_client_), socket_role::client);
        set_custom_command_handler(routingmanager_name_, client_name_, record_->create_collector(client_to_router_), socket_role::server);
        set_custom_command_handler(client_name_, routingmanager_name_, record_->create_collector(router_to_client_), socket_role::server);
        set_custom_command_handler(client_name_, routingmanager_name_, record_->create_collector(client_to_router_), socket_role::client);
    }
    void track_server_router() {
        set_custom_command_handler(routingmanager_name_, server_name_, record_->create_collector(router_to_server_), socket_role::client);
        set_custom_command_handler(routingmanager_name_, server_name_, record_->create_collector(server_to_router_), socket_role::server);
        set_custom_command_handler(server_name_, routingmanager_name_, record_->create_collector(router_to_server_), socket_role::server);
        set_custom_command_handler(server_name_, routingmanager_name_, record_->create_collector(server_to_router_), socket_role::client);
    }

    std::string const router_to_client_{routingmanager_name_ + "-to-" + client_name_};
    std::string const router_to_server_{routingmanager_name_ + "-to-" + server_name_};
    std::string const server_to_client_{server_name_ + "-to-" + client_name_};
    std::string const server_to_router_{server_name_ + "-to-" + routingmanager_name_};
    std::string const client_to_server_{client_name_ + "-to-" + server_name_};
    std::string const client_to_router_{client_name_ + "-to-" + routingmanager_name_};
};

TEST_F(test_protocol_messages, ensure_sequence_of_registration_message) {
    track_client_router();

    start_router();
    start_client_app();

    using namespace vsomeip_v3::protocol;
    // REGISTER_APPLICATION_ID/CONFIG_ID are send in parallel and might appear in different orders in the record
    auto const expected_sequence1 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_router_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the router
            {client_to_router_, id_e::ASSIGN_CLIENT_ID}, // needed to be able to create a uds server
            {router_to_client_, id_e::ASSIGN_CLIENT_ACK_ID}, // only message currently received over the client
            {client_to_router_, id_e::REGISTER_APPLICATION_ID}, // 2. registration step -> ask router to connect back
            {router_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the client
            {router_to_client_, id_e::ROUTING_INFO_ID}, // received by client: application knows that the router is connected
            {client_to_router_, id_e::REGISTERED_ACK_ID}}; // router knows that it can successfully send message -> registration succeeded.
    auto const expected_sequence2 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_router_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the router
            {client_to_router_, id_e::ASSIGN_CLIENT_ID}, // needed to be able to create a uds server
            {router_to_client_, id_e::ASSIGN_CLIENT_ACK_ID}, // only message currently received over the client
            {router_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the client
            {client_to_router_, id_e::REGISTER_APPLICATION_ID}, // 2. registration step -> ask router to connect back
            {router_to_client_, id_e::ROUTING_INFO_ID}, // received by client: application knows that the router is connected
            {client_to_router_, id_e::REGISTERED_ACK_ID}}; // router knows that it can successfully send message -> registration succeeded.
    bool const is_any = expected_sequence1 == *record_ || expected_sequence2 == *record_;
    EXPECT_TRUE(is_any) << *record_;
}
TEST_F(test_protocol_messages, ensure_sequence_of_routing_info_request) {
    // collect all message over both connection between a client and the router

    start_apps();
    track_client_router();
    request_service();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    using namespace vsomeip_v3::protocol;
    auto const expected_sequence = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_router_, id_e::REQUEST_SERVICE_ID}, {router_to_client_, id_e::ROUTING_INFO_ID}};
    EXPECT_EQ(expected_sequence, *record_);
}
TEST_F(test_protocol_messages, ensure_sequence_of_event_registration) {
    // collect all message over both connection between a client and the router
    track_client_server();

    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    using namespace vsomeip_v3::protocol;
    // SUBSCRIBE_ID/CONFIG_ID are send in parallel and might appear in different orders in the record
    auto const expected_sequence1 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_server_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy
            {client_to_server_, id_e::SUBSCRIBE_ID},
            {server_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy for the service
            {server_to_client_, id_e::SUBSCRIBE_ACK_ID}};
    auto const expected_sequence2 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_server_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy
            {server_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy for the service
            {client_to_server_, id_e::SUBSCRIBE_ID},
            {server_to_client_, id_e::SUBSCRIBE_ACK_ID}};
    bool const is_any = expected_sequence1 == *record_ || expected_sequence2 == *record_;
    EXPECT_TRUE(is_any) << *record_;
}
TEST_F(test_protocol_messages, ensure_sequence_of_field_registration) {
    // collect all message over both connection between a client and the router
    track_client_server();

    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());

    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));

    using namespace vsomeip_v3::protocol;
    // SUBSCRIBE_ID/CONFIG_ID are send in parallel and might appear in different orders in the record
    auto const expected_sequence1 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_server_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the server
            {client_to_server_, id_e::SUBSCRIBE_ID},
            {server_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy for the service
            {server_to_client_, id_e::SUBSCRIBE_ACK_ID},
            {server_to_client_, id_e::SEND_ID} // initial event
    };
    auto const expected_sequence2 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_server_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the server
            {server_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy for the service
            {client_to_server_, id_e::SUBSCRIBE_ID},
            {server_to_client_, id_e::SUBSCRIBE_ACK_ID},
            {server_to_client_, id_e::SEND_ID} // initial event
    };
    bool const is_any = expected_sequence1 == *record_ || expected_sequence2 == *record_;
    EXPECT_TRUE(is_any) << *record_;
}

TEST_F(test_protocol_messages, ensure_sequence_of_request_reply) {
    // first start all the applications registration sequence is covered elsewhere
    start_apps();

    client_->request_service(service_instance_);
    answer_requests_with({0x2, 0x3});

    // wait for availability, before sending the request
    // or tracking the path of the request, otherwise no guarantee that it is sent out
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    track_client_server();
    track_client_router();
    track_server_router();

    client_->send_request(request_);
    ASSERT_TRUE(server_->message_record_.wait_for_last(expected_request_));
    EXPECT_TRUE(client_->message_record_.wait_for_last(expected_reply_));

    using namespace vsomeip_v3::protocol;
    // SEND_ID/CONFIG_ID are send in parallel and might appear in different orders in the record
    auto const expected_sequence1 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_server_, id_e::CONFIG_ID}, // lazy load security policy in the server
            {client_to_server_, id_e::SEND_ID}, // request
            {server_to_client_, id_e::CONFIG_ID}, // lazy load security policy in the client
            {server_to_client_, id_e::SEND_ID} // reply
    };
    auto const expected_sequence2 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_server_, id_e::CONFIG_ID}, // lazy load security policy in the server
            {server_to_client_, id_e::CONFIG_ID}, // lazy load security policy in the client
            {client_to_server_, id_e::SEND_ID}, // request
            {server_to_client_, id_e::SEND_ID} // reply
    };
    bool const is_any = expected_sequence1 == *record_ || expected_sequence2 == *record_;
    EXPECT_TRUE(is_any) << *record_;
}

TEST_F(test_client_helper, ensure_unavail_after_stop_offer) {
    start_apps();
    request_service();
    ASSERT_TRUE(await_service());
    client_->availability_record_.clear();

    stop_offer();
    EXPECT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
}

TEST_F(test_client_helper, ensure_unavail_after_stop_app) {
    start_apps();
    request_service();
    ASSERT_TRUE(await_service());
    client_->availability_record_.clear();
    stop_client(server_name_);

    EXPECT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
}

TEST_F(test_client_helper, field_subscription) {
    start_apps();

    send_field_message();
    ASSERT_TRUE(subscribe_to_field());

    EXPECT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
}

TEST_F(test_client_helper, router_offers_field) {
    start_router();
    start_client_app();
    routingmanagerd_->offer(service_instance_);
    routingmanagerd_->offer_event(offered_event_);
    routingmanagerd_->offer_field(offered_field_);
    routingmanagerd_->send_event(offered_field_, field_payload_);

    ASSERT_TRUE(subscribe_to_field());
    EXPECT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
}

TEST_F(test_client_helper, client_renews_connection_deletes_client_info) {
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
TEST_F(test_client_helper, re_registered_client_deletes_client_info) {
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

TEST_F(test_client_helper, when_a_client_connection_is_broken_just_after_accept_then_it_can_be_cleand_up) {

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

TEST_F(test_client_helper, request_reply_no_sub) {
    start_apps();

    client_->request_service(service_instance_);
    answer_requests_with({0x2, 0x3});

    // wait for availability, before sending the request
    // otherwise no guarantee that it is sent out
    EXPECT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    client_->send_request(request_);

    ASSERT_TRUE(server_->message_record_.wait_for_last(expected_request_));
    EXPECT_TRUE(client_->message_record_.wait_for_last(expected_reply_));
}

TEST_F(test_client_helper, request_reply_with_sub) {
    start_apps();

    // NOTE: subscription acknowledge happens after service availability, which is why this works!
    ASSERT_TRUE(subscribe_to_event());

    answer_requests_with({0x2, 0x3});
    client_->send_request(request_);

    ASSERT_TRUE(server_->message_record_.wait_for_last(expected_request_));
    EXPECT_TRUE(client_->message_record_.wait_for_last(expected_reply_));
}

TEST_F(test_client_helper, request_reply_routingd) {
    /// another boring request-reply, but this time the server is routingd

    start_router();
    start_client_app();
    // no server! router will be server

    // routingd offers service
    routingmanagerd_->offer(service_instance_);
    // ..and answers
    std::vector<uint8_t> payload{0x2, 0x3};
    routingmanagerd_->answer_request(request_, [payload] { return payload; });
    expected_reply_.payload_ = payload;

    client_->request_service(service_instance_);

    EXPECT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    client_->send_request(request_);

    ASSERT_TRUE(routingmanagerd_->message_record_.wait_for_last(expected_request_));
    EXPECT_TRUE(client_->message_record_.wait_for_last(expected_reply_));
}

TEST_F(test_client_helper, the_server_sends_subscribe_ack_when_the_routing_info_is_late) {
    start_apps();
    send_field_message();

    // ensure that the routing info is not received by the server before the subscription
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true));
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);

    // TODO what could we await here?
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, false));

    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    EXPECT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
}

TEST_F(test_client_helper, outdated_routing_info_will_not_cause_a_wrong_permanent_connection) {

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

TEST_F(test_client_helper, reproduction_allow_reconnects_on_first_try_between_router_and_client) {
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

TEST_F(test_client_helper, client_server_connection_breakdown_on_client_suspend_with_events) {
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

TEST_F(test_client_helper, server_suback_after_sub_insert) {
    /// check whether server sends SUBSCRIPTION ACK after it has inserted the subscription
    /// by forcing the server to send a notification after client receives SUBSCRIPTION ACK
    /// (this of course only makes for an event; a field has an initial notification)


    start_apps();

    // wait for client to receive SUBSCRIPTION_ACK
    ASSERT_TRUE(subscribe_to_event());

    // server sends notification
    send_first_message();

    // ... check that client has received it
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));
}

TEST_F(test_client_helper, client_server_connection_breakdown_on_server_suspend_with_events) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));

    // simulating a suspend of a server:
    set_ignore_connections(server_name_, true);
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::timed_out, server_name_, std::nullopt));

    client_->subscription_record_.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    // resume of the server:
    set_ignore_connections(server_name_, false);
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

TEST_F(test_client_helper, field_updates_are_resend_when_a_broken_routing_connection_enforces_a_resubscription) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));

    // simulating a suspend of a client:
    set_ignore_connections(client_name_, true);
    ASSERT_TRUE(disconnect(client_name_, std::nullopt, routingmanager_name_, boost::asio::error::timed_out));

    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    // resume of the client:
    set_ignore_connections(client_name_, false);
    // resume of the client:
    std::ignore = disconnect(client_name_, boost::asio::error::connection_reset, routingmanager_name_, std::nullopt);

    // TODO:
    // Because the re-connection needs to timeout before being successful, let's
    // await the reconnect before continueing with the test.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // because the client will re-subscribe it should receive another event after the subscription
    auto new_message = first_expected_field_message_;
    new_message.client_session_.session_ += 1;
    EXPECT_TRUE(client_->message_record_.wait_for_last(new_message));
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

TEST_F(test_client_helper, break_client_with_eof_before_finishing_registration) {

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

TEST_F(test_client_helper, break_host_side_only_with_broken_pipe) {
    start_apps();

    ASSERT_TRUE(
            disconnect(client_name_, boost::asio::error::eof, routingmanager_name_, boost::asio::error::broken_pipe, socket_role::server));

    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_));
}

TEST_F(test_client_helper, handle_early_routing_info_within_server_after_reconnect) {
    start_apps();
    send_field_message();
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
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
    ASSERT_TRUE(delay_message_processing(client_name_, routingmanager_name_, true, socket_role::server));
    // freeze close reception in server, for server -> client (to hold back close(), see 5.)
    ASSERT_TRUE(set_ignore_inner_close(client_name_, false, server_name_, true));
    // clear commands so we can peek
    clear_command_record(routingmanager_name_, server_name_);

    // trigger 1.
    ASSERT_TRUE(disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::eof));
    // 4.
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, protocol::id_e::CONFIG_ID, socket_role::client));
    // 5.
    ASSERT_TRUE(disconnect(client_name_, std::nullopt, server_name_, boost::asio::error::connection_reset));
    // 7.
    ASSERT_TRUE(delay_message_processing(client_name_, routingmanager_name_, false, socket_role::server));

    EXPECT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
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
    // ASSERT_TRUE(await_connectable(server_name_)); wouldn't work as the server is only started after client received a client_id
    // TODO at this point in time application::start() is called on a background thread and the server has tried to claim some port.
    // But it can happen that this port is not free, while we continue with the "sending" of the field.
    // Only after some port is claimed will the unguarded sec_client be set for the "send" preparation,
    // while the send preparation already requires the sec_client. This is a race condition
    // within the vsomeip application (to not protect against such a send preparation while not being set up properly yet),
    // but not the focuse of this tests
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    // 6. + 7.
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    // 8.
    fail_on_bind(server_name_, false);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    EXPECT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
}


TEST_F(test_client_helper, handle_late_clean_up_within_server_after_reconnect) {
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

TEST_F(test_client_helper, connection_break_vs_routing) {
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

TEST_F(test_client_helper, service_is_unavailable_after_clean_up_race) {
    /**
     * Regression test for the following scenario:
     * 0. router, server and client are started, client is only interested in the availability of the service
     * 1. the server is terminated
     * 2. the client handles the connection breakdown first
     *    2.a the client receives unavailable due to server's broken connection
     *    2.b the client sent a request for the service to the router
     * 3. the router receives the request of the client (2.b)
     * 4. the client receives the former routing info
     * 5. the client receives on_available
     * 6. the client can not connect to the server
     * 5. the router works on the deregistration of the server
     * 6. the server is restarted
     * 7. the router distributes the new routing_info
     * 8. the client receives on_available
     *
     * The problem is that the 8. "on_available" never reaches the application.
     * While this does not matter as subscriptions etc. are managed, it does
     * matter for requests.
     * Because at the moment the only signal a client application would receive,
     * would be an remote_error when the common api layer decides that the
     * request took too long.
     **/
    start_apps();
    request_service();
    ASSERT_TRUE(await_service());

    TEST_LOG << "[step] Stop Server";
    stop_client(server_name_);
    client_->availability_record_.clear();

    TEST_LOG << "[step] Restarting the Server";
    create_app(server_name_);
    start_server();
    EXPECT_TRUE(await_service());
}

/**
 * Ensures a service provider responds with a NACK when a subscription is sent by an already connected client
 * to a service it does not offer.
 */
TEST_F(test_client_helper, test_subscription_for_ghost_service) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    auto subscription_payload = construct_basic_raw_command(protocol::id_e::SUBSCRIBE_ID, // command
                                                            static_cast<uint16_t>(0), // version
                                                            static_cast<client_t>(0x3490), // client id
                                                            static_cast<uint32_t>(11), // size
                                                            static_cast<service_t>(0xAAAA), // service
                                                            static_cast<instance_t>(0x00), // instance
                                                            static_cast<eventgroup_t>(0x00), // event group
                                                            static_cast<major_version_t>(0x00), // major
                                                            static_cast<event_t>(0x00), // event
                                                            static_cast<uint16_t>(0) // pending id
    );
    inject_command(client_name_, server_name_, subscription_payload);
    ASSERT_TRUE(wait_for_command(client_name_, server_name_, protocol::id_e::SUBSCRIBE_NACK_ID, socket_role::client));
}

TEST_F(test_client_helper, routing_info_conflict_routingmanagerd) {
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
                                                            std::chrono::milliseconds(300)));
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
                                                             std::chrono::milliseconds(300)));

    // start another server with same address/port (implicit!), but different client-id (using `server_name_two_`), and that offers
    // a second service

    // ensure client still did not see this other service
    EXPECT_FALSE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_two_),
                                                             std::chrono::milliseconds(300)));

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

TEST_F(test_client_helper, subscribe_eventgroup_connection_break) {
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

struct test_restart_clients : test_client_helper {

    bool subscribe(app* client) {
        client->request_service(service_instance_);
        client->subscribe_field(offered_field_);
        return client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_));
    }

    std::string const client_one_{"client-one"};
    std::string const client_two_{"client-two"};
};

TEST_F(test_restart_clients, test_assignment_timeout_recover) {
    start_router();

    // avoid processing any message from the router to the client (via either connection)
    ASSERT_FALSE(delay_message_processing(client_name_, routingmanager_name_, true, socket_role::server));

    client_ = start_client(client_name_);
    ASSERT_NE(client_, nullptr);

    // At this point in time, the client should not be able to register due to assignment timeout
    ASSERT_FALSE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(4)));

    ASSERT_TRUE(delay_message_processing(client_name_, routingmanager_name_, false, socket_role::server));
    EXPECT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

TEST_F(test_restart_clients, test_restart_client_one_and_two_in_reverse_order) {
    start_router();
    start_server();
    {
        create_app(client_one_);
        create_app(client_two_);

        auto* one = start_client(client_one_);
        ASSERT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        ASSERT_TRUE(subscribe(one));
        auto* two = start_client(client_two_);
        ASSERT_TRUE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    // Hold back the routing info update from client_one for the server (including the remove_client
    // command)
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true, socket_role::server));
    TEST_LOG << "[step] stopping the apps";
    stop_client(client_one_);
    stop_client(client_two_);
    TEST_LOG << "[step] restarting the apps";
    {
        create_app(client_one_);
        create_app(client_two_);

        auto* two = start_client(client_two_);
        ASSERT_TRUE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        auto* one = start_client(client_one_);
        ASSERT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        one->request_service(service_instance_);
        one->subscribe_field(offered_field_);
        ASSERT_TRUE(await_connection(client_one_, server_name_));
        ASSERT_TRUE(wait_for_command(client_one_, server_name_, protocol::id_e::SUBSCRIBE_ID, socket_role::server));

        TEST_LOG << "[step] forward routing info";
        // ensure server receives updated routing info
        ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, false, socket_role::server));
        // now the new event should be send
        EXPECT_TRUE(one->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
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
        ASSERT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

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

        ASSERT_TRUE(one->availability_record_.wait_for_last(service_availability::available(service_instance_)));

        TEST_LOG << "[step] stopping the app";
        stop_client(client_one_);
    }
}

/**
 * Test blocking vsomeip messages.
 */
TEST_F(test_restart_clients, block_registration_process) {
    start_router();

    std::future<protocol::id_e> fut = drop_command_once(client_one_, routingmanager_name_, protocol::id_e::REGISTER_APPLICATION_ID);

    // start a client
    create_app(client_one_);
    auto* one = start_client(client_one_);
    ASSERT_TRUE(await_connection(client_one_, routingmanager_name_));

    ASSERT_FALSE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(1)));

    // sanity check that the right message was dropped
    ASSERT_TRUE(fut.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    ASSERT_EQ(fut.get(), protocol::id_e::REGISTER_APPLICATION_ID);

    // and that application eventually registers
    EXPECT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

struct test_single_connection_breakdown : test_client_helper, ::testing::WithParamInterface<std::pair<std::string, std::string>> { };

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

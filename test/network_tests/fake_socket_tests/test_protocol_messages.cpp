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

#include "../../../implementation/utility/include/utility.hpp"

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

struct protocol_messages_fixture : public base_fake_socket_fixture {
    protocol_messages_fixture() {
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

struct test_protocol_messages : protocol_messages_fixture {

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
    auto const expected_sequence1 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_router_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the router
            {client_to_router_, id_e::ASSIGN_CLIENT_ID}, // needed to be able to create a uds server
            {router_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the client
            {router_to_client_, id_e::ASSIGN_CLIENT_ACK_ID}}; // only message currently received over the client
    bool const is_any = expected_sequence1 == *record_;
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
TEST_F(test_protocol_messages, update_security_policy_configuration_calls) {
    /* There was a policy security test that would search for connected clients
    and send an UPDATE_SECURITY_POLICY_ID command. After a refactor, it was checking the wrong list,
    and not sending the command.
    */
    start_router();
    start_client_app();

    routingmanagerd_->update_security_policy_configuration(0, 0);
    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, protocol::id_e::UPDATE_SECURITY_POLICY_ID, socket_role::client,
                                 std::chrono::milliseconds(1)));
}
}

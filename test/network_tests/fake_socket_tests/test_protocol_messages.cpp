// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_gate.hpp"
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
static std::string const client_name_{"client"};

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

    // _before_offer runs after the registration, but still before the offer goes out.
    void start_server(std::function<void()> const& _before_offer = {}) {
        server_ = start_client(server_name_);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
        server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
        if (_before_offer) {
            _before_offer();
        }
        server_->offer(service_instance_);
    }

    void start_client_app() {
        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    void start_apps(std::function<void()> const& _before_offer = {}) {
        start_router();
        start_server(_before_offer);
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

    service_instance service_instance_{.service_ = 0x3344, .instance_ = 0x1, .major_ = 1, .minor_ = 0};
    event_ids offered_event_{service_instance_, 0x8002, 0x1};
    message first_expected_message_{
            client_session{0, 1}, service_instance_, offered_event_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, {}};
    event_ids offered_field_{service_instance_, 0x8003, 0x6};
    std::vector<unsigned char> field_payload_{0x42, 0x13};
    message first_expected_field_message_{client_session{0, 1}, service_instance_, offered_field_.event_id_,
                                          vsomeip::message_type_e::MT_NOTIFICATION, field_payload_};
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
TEST_F(test_protocol_messages, ensure_sequence_of_client_assign_ack) {
    track_client_router();

    start_router();
    // cached so it gets forwarded to the client while it registers
    // so that it triggers send_cached_security_policies()
    routingmanagerd_->update_security_policy_configuration(0, 0);
    start_client_app();

    using namespace vsomeip_v3::protocol;
    auto const expected_sequence1 = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {client_to_router_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the router
            {client_to_router_, id_e::ASSIGN_CLIENT_ID}, // needed to be able to create a uds server
            {router_to_client_, id_e::CONFIG_ID}, // needed to trigger lazy-load of the security policy in the client
            {router_to_client_, id_e::DISTRIBUTE_SECURITY_POLICIES_ID}, // must arrive before the ack, so the client already has its
                                                                        // security policies by the time it processes the assignment
            {router_to_client_, id_e::ASSIGN_CLIENT_ACK_ID}};
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

TEST_F(test_protocol_messages, ensure_sequence_service_offer_after_event_registration) {
    // collect all message over both connection between a client and the router
    track_server_router();

    start_router();
    start_client_app();
    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    request_service();
    server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
    server_->offer(service_instance_);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    using namespace vsomeip_v3::protocol;
    auto const expected_sequence = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {server_to_router_, id_e::CONFIG_ID},         {server_to_router_, id_e::ASSIGN_CLIENT_ID},
            {router_to_server_, id_e::CONFIG_ID},         {router_to_server_, id_e::ASSIGN_CLIENT_ACK_ID},
            {server_to_router_, id_e::REGISTER_EVENT_ID}, {server_to_router_, id_e::OFFER_SERVICE_ID}};

    EXPECT_EQ(expected_sequence, *record_);
}

TEST_F(test_protocol_messages, ensure_sequence_service_offer) {
    // collect all message over both connection between a client and the router
    track_server_router();

    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    using namespace vsomeip_v3::protocol;
    auto const expected_sequence = std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>>{
            {server_to_router_, id_e::CONFIG_ID},         {server_to_router_, id_e::ASSIGN_CLIENT_ID},
            {router_to_server_, id_e::CONFIG_ID},         {router_to_server_, id_e::ASSIGN_CLIENT_ACK_ID},
            {server_to_router_, id_e::REGISTER_EVENT_ID}, {server_to_router_, id_e::OFFER_SERVICE_ID}};

    EXPECT_EQ(expected_sequence, *record_);
}

TEST_F(test_protocol_messages, ensure_sequence_of_release_service) {
    // collect all message over both connection between a client and the router
    std::shared_ptr<command_gate> client_to_router_gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(client_name_, routingmanager_name_, socket_role::server, client_to_router_gate->get_data_pipe()));
    track_client_router();
    track_client_server();

    start_apps();

    using namespace vsomeip_v3::protocol;
    client_to_router_gate->block_at(vsomeip_v3::protocol::id_e::RELEASE_SERVICE_ID);
    request_service();
    ASSERT_TRUE(await_service());

    client_->release_service(service_instance_);
    EXPECT_TRUE(client_to_router_gate->wait_for_blocked());
    client_to_router_gate->block(false);
    auto const expected_sequence = std::vector<std::pair<std::string, id_e>>{
            {client_to_router_, id_e::CONFIG_ID},          {client_to_router_, id_e::ASSIGN_CLIENT_ID},
            {router_to_client_, id_e::CONFIG_ID},          {router_to_client_, id_e::ASSIGN_CLIENT_ACK_ID},
            {client_to_router_, id_e::REQUEST_SERVICE_ID}, {router_to_client_, id_e::ROUTING_INFO_ID},
            {client_to_router_, id_e::RELEASE_SERVICE_ID}};
    EXPECT_EQ(expected_sequence, *record_);
}

TEST_F(test_protocol_messages, ensure_sequence_of_unsubscribe_event) {
    std::shared_ptr<command_gate> client_to_router_gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(client_name_, server_name_, socket_role::server, client_to_router_gate->get_data_pipe()));
    track_client_server();

    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    using namespace vsomeip_v3::protocol;
    client_to_router_gate->block_at(vsomeip_v3::protocol::id_e::UNSUBSCRIBE_ID);
    client_->unsubscribe_from_group(offered_event_);
    EXPECT_TRUE(client_to_router_gate->wait_for_blocked());
    client_to_router_gate->block(false);

    auto const expected_sequence1 = std::vector<std::pair<std::string, id_e>>{{client_to_server_, id_e::CONFIG_ID},
                                                                              {client_to_server_, id_e::SUBSCRIBE_ID},
                                                                              {server_to_client_, id_e::CONFIG_ID},
                                                                              {server_to_client_, id_e::SUBSCRIBE_ACK_ID},
                                                                              {client_to_server_, id_e::UNSUBSCRIBE_ID}};
    auto const expected_sequence2 = std::vector<std::pair<std::string, id_e>>{{client_to_server_, id_e::CONFIG_ID},
                                                                              {server_to_client_, id_e::CONFIG_ID},
                                                                              {client_to_server_, id_e::SUBSCRIBE_ID},
                                                                              {server_to_client_, id_e::SUBSCRIBE_ACK_ID},
                                                                              {client_to_server_, id_e::UNSUBSCRIBE_ID}};

    bool const is_any = expected_sequence1 == *record_ || expected_sequence2 == *record_;
    EXPECT_TRUE(is_any) << *record_;
}

/**
 * The rmc must fill the major version into every command that carries a service key. The routing
 * manager still discards that field, so the wire content is the only observable effect - each case
 * asserts on the recorded command rather than on any behaviour change.
 */
struct test_command_major_version : test_protocol_messages {
    /// Fetches the last recorded command with _id and returns its typed payload.
    template<typename T>
    T const* last_payload(vsomeip_v3::protocol::id_e _id) {
        auto const its_msg = record_->find_last(_id);
        if (!its_msg) {
            ADD_FAILURE() << "no command with id " << static_cast<int>(_id) << " was recorded: " << *record_;
            return nullptr;
        }
        auto const* its_payload = its_msg->payload_.template get_if<T>();
        if (!its_payload) {
            ADD_FAILURE() << "command " << static_cast<int>(_id) << " did not carry the expected payload type";
        }
        return its_payload;
    }
};

TEST_F(test_command_major_version, provided_event_registration_is_held_until_the_service_is_offered) {
    track_server_router();

    start_router();
    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // The major only exists on the offer, so registering an event must not announce it yet.
    server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
    EXPECT_FALSE(wait_for_command(server_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID, socket_role::server,
                                  std::chrono::milliseconds(100)))
            << "REGISTER_EVENT was sent before the service was offered, so it could only carry ANY_MAJOR";

    server_->offer(service_instance_);
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::OFFER_SERVICE_ID, socket_role::server,
                                 std::chrono::milliseconds(500)));

    auto const* its_registrations =
            last_payload<std::vector<vsomeip_v3::protocol::register_event_data>>(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    ASSERT_NE(its_registrations, nullptr);
    ASSERT_EQ(its_registrations->size(), 1u);
    EXPECT_EQ(its_registrations->front().event_, offered_event_.event_id_);
    EXPECT_EQ(its_registrations->front().service_key_.major_, service_instance_.major_)
            << "the held registration must be stamped with the major the service is offered with";

    // The router has to know the event before the service is advertised, so the flush must not slip
    // behind the offer it was batched with.
    auto const its_record = record_->as_sequence();
    auto const its_offer = std::find_if(its_record.begin(), its_record.end(), [this](auto const& _entry) {
        return _entry.first == server_to_router_ && _entry.second == vsomeip_v3::protocol::id_e::OFFER_SERVICE_ID;
    });
    ASSERT_NE(its_offer, its_record.end());
    EXPECT_NE(std::find_if(its_record.begin(), its_offer,
                           [this](auto const& _entry) {
                               return _entry.first == server_to_router_ && _entry.second == vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID;
                           }),
              its_offer)
            << "no REGISTER_EVENT ahead of OFFER_SERVICE: " << *record_;
}

TEST_F(test_command_major_version, re_offering_under_a_different_major_re_registers_the_events) {
    std::shared_ptr<command_gate> server_to_router_gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_name_, routingmanager_name_, socket_role::server, server_to_router_gate->get_data_pipe()));
    track_server_router();

    start_apps();
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::OFFER_SERVICE_ID, socket_role::server,
                                 std::chrono::milliseconds(500)));
    auto const its_first_offer_registrations = record_->count(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);

    server_->stop_offer(service_instance_);
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::STOP_OFFER_SERVICE_ID, socket_role::server,
                                 std::chrono::milliseconds(500)));

    // Same events, new major: the router still holds them under the withdrawn major, so they have to
    // be announced again rather than left stale.
    auto its_second_version = service_instance_;
    its_second_version.major_ = static_cast<vsomeip::major_version_t>(service_instance_.major_ + 1);
    // Arm before the trigger: wait_for_command() matches the whole history, so it would return on the
    // registration the first offer already produced instead of waiting for this one.
    server_to_router_gate->block_at(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    server_->offer(its_second_version);
    ASSERT_TRUE(server_to_router_gate->wait_for_blocked());
    server_to_router_gate->block(false);

    EXPECT_GT(record_->count(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID), its_first_offer_registrations);

    auto const* its_registrations =
            last_payload<std::vector<vsomeip_v3::protocol::register_event_data>>(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    ASSERT_NE(its_registrations, nullptr);
    ASSERT_FALSE(its_registrations->empty());
    EXPECT_EQ(its_registrations->front().service_key_.major_, its_second_version.major_)
            << "the re-offer must re-stamp the registrations with the new major";
}

TEST_F(test_command_major_version, re_registering_an_event_while_not_offered_must_not_duplicate_it) {
    std::shared_ptr<command_gate> server_to_router_gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_name_, routingmanager_name_, socket_role::server, server_to_router_gate->get_data_pipe()));
    track_server_router();

    start_apps();
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::OFFER_SERVICE_ID, socket_role::server,
                                 std::chrono::milliseconds(500)));

    server_->stop_offer(service_instance_);
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::STOP_OFFER_SERVICE_ID, socket_role::server,
                                 std::chrono::milliseconds(500)));

    // The stored registration still carries the withdrawn major, so this one is built with ANY_MAJOR
    // and must not be mistaken for a different registration.
    server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());

    auto its_second_version = service_instance_;
    its_second_version.major_ = static_cast<vsomeip::major_version_t>(service_instance_.major_ + 1);
    server_to_router_gate->block_at(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    server_->offer(its_second_version);
    ASSERT_TRUE(server_to_router_gate->wait_for_blocked());
    server_to_router_gate->block(false);

    auto const* its_registrations =
            last_payload<std::vector<vsomeip_v3::protocol::register_event_data>>(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    ASSERT_NE(its_registrations, nullptr);
    EXPECT_EQ(std::count_if(its_registrations->begin(), its_registrations->end(),
                            [this](auto const& _reg) { return _reg.event_ == offered_event_.event_id_; }),
              1)
            << "the re-offer announced the same event more than once: " << *record_;
}

TEST_F(test_command_major_version, consumed_event_registration_carries_the_requested_major) {
    track_client_router();

    start_apps();
    // Expected consumer order: the service is requested first, so the major is already known when
    // the event is registered and the registration goes straight out carrying it.
    ASSERT_TRUE(subscribe_to_event());

    auto const* its_registrations =
            last_payload<std::vector<vsomeip_v3::protocol::register_event_data>>(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    ASSERT_NE(its_registrations, nullptr);
    ASSERT_FALSE(its_registrations->empty());
    EXPECT_FALSE(its_registrations->front().is_provided_);
    EXPECT_EQ(its_registrations->front().service_key_.major_, service_instance_.major_)
            << "a consumed registration must carry the major the service was requested with";
}

TEST_F(test_command_major_version, consumed_event_registration_is_held_until_the_service_is_requested) {
    track_client_router();

    start_apps();

    // Reversed order: without a request there is no major to fill in, so the registration waits.
    client_->request_event(offered_event_);
    EXPECT_FALSE(wait_for_command(client_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID, socket_role::server,
                                  std::chrono::milliseconds(100)))
            << "REGISTER_EVENT was sent before the service was requested, so it could only carry ANY_MAJOR";

    client_->request_service(service_instance_);
    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID, socket_role::server,
                                 std::chrono::milliseconds(500)));

    auto const* its_registrations =
            last_payload<std::vector<vsomeip_v3::protocol::register_event_data>>(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    ASSERT_NE(its_registrations, nullptr);
    ASSERT_EQ(its_registrations->size(), 1u);
    EXPECT_EQ(its_registrations->front().event_, offered_event_.event_id_);
    EXPECT_EQ(its_registrations->front().service_key_.major_, service_instance_.major_)
            << "the held registration must be stamped with the major the service is requested with";
}

TEST_F(test_command_major_version, consumed_event_registration_is_released_by_a_wildcard_request) {

    track_client_router();

    start_apps();

    client_->request_service({.service_ = service_instance_.service_,
                              .instance_ = vsomeip::ANY_INSTANCE,
                              .major_ = service_instance_.major_,
                              .minor_ = service_instance_.minor_});
    client_->request_event(offered_event_);

    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID, socket_role::server,
                                 std::chrono::milliseconds(500)))
            << "the registration stayed held: a wildcard request never releases it: " << *record_;

    auto const* its_registrations =
            last_payload<std::vector<vsomeip_v3::protocol::register_event_data>>(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    ASSERT_NE(its_registrations, nullptr);
    ASSERT_EQ(its_registrations->size(), 1u);
    EXPECT_EQ(its_registrations->front().event_, offered_event_.event_id_);
    EXPECT_EQ(its_registrations->front().service_key_.major_, service_instance_.major_)
            << "the wildcard request carries the major, so the released registration must be stamped with it";
}

TEST_F(test_command_major_version, pending_consumed_event_registration_is_released_by_a_later_wildcard_request) {
    // Reverse of the case above: the event is registered first, so it is genuinely held and the
    // wildcard request has to be what flushes it.
    track_client_router();

    start_apps();

    client_->request_event(offered_event_);
    EXPECT_FALSE(wait_for_command(client_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID, socket_role::server,
                                  std::chrono::milliseconds(100)))
            << "REGISTER_EVENT was sent before the service was requested, so it could only carry ANY_MAJOR";

    client_->request_service({.service_ = service_instance_.service_,
                              .instance_ = vsomeip::ANY_INSTANCE,
                              .major_ = service_instance_.major_,
                              .minor_ = service_instance_.minor_});

    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID, socket_role::server,
                                 std::chrono::milliseconds(500)))
            << "the registration stayed held: a wildcard request never releases it: " << *record_;

    auto const* its_registrations =
            last_payload<std::vector<vsomeip_v3::protocol::register_event_data>>(vsomeip_v3::protocol::id_e::REGISTER_EVENT_ID);
    ASSERT_NE(its_registrations, nullptr);
    ASSERT_EQ(its_registrations->size(), 1u);
    EXPECT_EQ(its_registrations->front().event_, offered_event_.event_id_);
    EXPECT_EQ(its_registrations->front().service_key_.major_, service_instance_.major_);
}

TEST_F(test_command_major_version, release_service_carries_the_requested_major) {
    std::shared_ptr<command_gate> client_to_router_gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(client_name_, routingmanager_name_, socket_role::server, client_to_router_gate->get_data_pipe()));
    track_client_router();

    start_apps();

    client_to_router_gate->block_at(vsomeip_v3::protocol::id_e::RELEASE_SERVICE_ID);
    request_service();
    ASSERT_TRUE(await_service());
    client_->release_service(service_instance_);
    ASSERT_TRUE(client_to_router_gate->wait_for_blocked());
    client_to_router_gate->block(false);

    auto const* its_release = last_payload<vsomeip_v3::protocol::service_key>(vsomeip_v3::protocol::id_e::RELEASE_SERVICE_ID);
    ASSERT_NE(its_release, nullptr);
    EXPECT_EQ(its_release->major_, service_instance_.major_) << "RELEASE_SERVICE must carry the major the service was requested with";
}

TEST_F(test_command_major_version, unregister_event_carries_the_offered_major) {
    std::shared_ptr<command_gate> server_to_router_gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_name_, routingmanager_name_, socket_role::server, server_to_router_gate->get_data_pipe()));
    track_server_router();

    start_apps();

    server_to_router_gate->block_at(vsomeip_v3::protocol::id_e::UNREGISTER_EVENT_ID);
    server_->stop_offer_event(offered_event_);
    ASSERT_TRUE(server_to_router_gate->wait_for_blocked());
    server_to_router_gate->block(false);

    auto const* its_unregister = last_payload<vsomeip_v3::protocol::unregister_event_data>(vsomeip_v3::protocol::id_e::UNREGISTER_EVENT_ID);
    ASSERT_NE(its_unregister, nullptr);
    EXPECT_EQ(its_unregister->service_key_.major_, service_instance_.major_)
            << "UNREGISTER_EVENT must carry the major the providing service is offered with";
}

TEST_F(test_command_major_version, unsubscribe_carries_the_subscribed_major) {
    std::shared_ptr<command_gate> client_to_server_gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(client_name_, server_name_, socket_role::server, client_to_server_gate->get_data_pipe()));
    track_client_server();

    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    client_to_server_gate->block_at(vsomeip_v3::protocol::id_e::UNSUBSCRIBE_ID);
    client_->unsubscribe_from_group(offered_event_);
    ASSERT_TRUE(client_to_server_gate->wait_for_blocked());
    client_to_server_gate->block(false);

    auto const* its_unsubscribe = last_payload<vsomeip_v3::protocol::subscribe_data>(vsomeip_v3::protocol::id_e::UNSUBSCRIBE_ID);
    ASSERT_NE(its_unsubscribe, nullptr);
    EXPECT_EQ(its_unsubscribe->service_key_.major_, service_instance_.major_)
            << "UNSUBSCRIBE must carry the same major as the SUBSCRIBE it withdraws";
}

TEST_F(test_command_major_version, subscribe_ack_carries_the_offered_major) {
    track_client_server();

    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    auto const* its_ack = last_payload<vsomeip_v3::protocol::subscribe_answer_data>(vsomeip_v3::protocol::id_e::SUBSCRIBE_ACK_ID);
    ASSERT_NE(its_ack, nullptr);
    EXPECT_EQ(its_ack->service_key_.major_, service_instance_.major_)
            << "SUBSCRIBE_ACK must carry the major the acknowledging provider offers";
}

TEST_F(test_protocol_messages, ensure_sequence_failover_service_reoffer) {
    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    track_server_router();
    track_client_router();

    // Stop offering
    stop_offer();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // Re-offer the service
    server_->offer(service_instance_);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    using namespace vsomeip_v3::protocol;
    auto const expected_sequence = std::vector<std::pair<std::string, id_e>>{{server_to_router_, id_e::STOP_OFFER_SERVICE_ID},
                                                                             {router_to_client_, id_e::ROUTING_INFO_ID},
                                                                             {server_to_router_, id_e::OFFER_SERVICE_ID},
                                                                             {router_to_client_, id_e::ROUTING_INFO_ID}};
    EXPECT_EQ(expected_sequence, *record_);
}

TEST_F(test_protocol_messages, ensure_sequence_of_stop_offer_event) {
    // Server stops offering an event but keeps the service active
    start_apps();
    ASSERT_TRUE(subscribe_to_event());
    track_server_router();

    server_->stop_offer_event(offered_event_);

    using namespace vsomeip_v3::protocol;
    auto const expected_sequence = std::vector<std::pair<std::string, id_e>>{{server_to_router_, id_e::UNREGISTER_EVENT_ID}};
    EXPECT_EQ(expected_sequence, *record_);
}

TEST_F(test_protocol_messages, ensure_sequence_of_request_reply) {
    // first start all the applications registration sequence is covered elsewhere
    start_apps([this] { answer_requests_with({0x2, 0x3}); });

    client_->request_service(service_instance_);

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

// Regression coverage: on RIE_DELETE_SERVICE_INSTANCE the routing_manager_client must
// remove the service from available_services_. on_stop_offer_service() only fires AS_UNAVAILABLE, it
// does not purge the table; a stale entry keeps is_available() true and find_local_client() pointing at
// the gone provider, so a post-stop_offer request is mis-routed to it. The service is not re-offered.
TEST_F(test_protocol_messages, request_after_stop_offer_is_not_routed_to_defunct_provider) {

    message_checker const request_checker{std::nullopt, service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, std::nullopt};
    message_checker const reply_checker{std::nullopt, service_instance_, method_, vsomeip::message_type_e::MT_RESPONSE, std::nullopt};

    start_apps([this] { answer_requests_with({0x2, 0x3}); });
    client_->request_service(service_instance_);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // Baseline: the request reaches the provider and is answered, proving the consumer routing table
    // points at the server.
    client_->send_request(request_);
    ASSERT_TRUE(server_->message_record_.wait_for(request_checker));
    ASSERT_TRUE(client_->message_record_.wait_for(reply_checker));

    // Provider stops offering -> router announces RIE_DELETE_SERVICE_INSTANCE. The service is
    // intentionally NOT re-offered (a re-offer would overwrite the stale entry and hide the bug).
    server_->message_record_.clear();
    client_->message_record_.clear();
    stop_offer();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // The service is gone. A request issued now must not be routed to the defunct provider. Without
    // the available_services_.remove() fix the stale entry sends it straight back to the server (which
    // still has its message handler registered), and the client even receives a reply.
    client_->send_request(request_);
    EXPECT_FALSE(server_->message_record_.wait_for(request_checker, std::chrono::milliseconds(100)))
            << "request was routed to a provider that stopped offering: stale available_services_ entry "
               "(missing available_services_.remove() on RIE_DELETE_SERVICE_INSTANCE)";
    EXPECT_FALSE(client_->message_record_.wait_for(reply_checker, std::chrono::milliseconds(500)))
            << "client received a reply from a provider that stopped offering";
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

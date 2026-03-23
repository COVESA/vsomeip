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

struct test_client_lifecycle : public base_fake_socket_fixture {
    test_client_lifecycle() {
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

struct test_restart_clients : test_client_lifecycle {

    bool subscribe(app* client) {
        client->request_service(service_instance_);
        client->subscribe_field(offered_field_);
        return client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_));
    }

    std::string const client_one_{"client-one"};
    std::string const client_two_{"client-two"};
};

/**
 * Test fixture for concurrent registration scenario.
 * Two application instances with the same ID start/stop simultaneously.
 * The scenario tests that the deregister command from the first instance is still
 * in the routing manager's queue when the register command from a newer instance arrives.
 */
struct test_concurrent_registration : base_fake_socket_fixture {
    test_concurrent_registration() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
    }

    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));
    }

    // Both instances use the same application name to simulate concurrent registration
    std::string const routingmanager_name_{"routingmanagerd"};
    std::string const app_name_{"concurrent_app"};

    service_instance service_{0x1387, 0x1};
    vsomeip_v3::method_t method_{0x1221};
    request req_{service_, method_, vsomeip::message_type_e::MT_REQUEST, {}};

    app* routingmanagerd_{};
};

TEST_F(test_client_lifecycle, router_consumes_field_before_service_tries_to_offer_field_is_updated_before_registration) {
    GTEST_SKIP() << "Provokes a race in application_impl regarding the usage of sec_client_";

    start_router();
    routingmanagerd_->request_service(service_instance_);
    routingmanagerd_->subscribe_field(offered_field_);

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    server_->offer(service_instance_);
    server_->offer_field(offered_field_);
    send_field_message();

    ASSERT_TRUE(routingmanagerd_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    EXPECT_TRUE(routingmanagerd_->message_record_.wait_for(field_checker_));
}
TEST_F(test_client_lifecycle, router_consumes_field_before_service_tries_to_offer_field_is_updated_after_registration) {
    GTEST_SKIP() << "Provokes a race in application_impl regarding the usage of sec_client_";

    start_router();
    routingmanagerd_->request_service(service_instance_);
    routingmanagerd_->subscribe_field(offered_field_);

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    server_->offer(service_instance_);
    server_->offer_field(offered_field_);
    ASSERT_TRUE(routingmanagerd_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    send_field_message();
    EXPECT_TRUE(routingmanagerd_->message_record_.wait_for(field_checker_));
}
TEST_F(test_client_lifecycle, router_consumes_field_after_service_tries_to_offer_before_registration) {
    GTEST_SKIP() << "Provokes a race in application_impl regarding the usage of sec_client_";

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    server_->offer(service_instance_);
    server_->offer_field(offered_field_);
    send_field_message();

    start_router();

    routingmanagerd_->request_service(service_instance_);
    routingmanagerd_->subscribe_field(offered_field_);
    ASSERT_TRUE(routingmanagerd_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    EXPECT_TRUE(routingmanagerd_->message_record_.wait_for(field_checker_));
}
TEST_F(test_client_lifecycle, router_consumes_field_after_service_tries_to_offer_after_registration) {

    start_apps();
    send_field_message();

    // ensure that everything is fully set up...
    ASSERT_TRUE(subscribe_to_field());
    ASSERT_TRUE(client_->message_record_.wait_for(field_checker_));

    // ... before subscribing from the router itself
    routingmanagerd_->request_service(service_instance_);
    routingmanagerd_->subscribe_field(offered_field_);
    ASSERT_TRUE(routingmanagerd_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    EXPECT_TRUE(routingmanagerd_->message_record_.wait_for(field_checker_));
}

TEST_F(test_client_lifecycle, mutual_offerings_and_consumptions_with_router) {
    // helper structs
    auto beef_payload = std::vector<unsigned char>{0xf, 0xe, 0xe, 0xd};
    auto beef_checker = message_checker{std::nullopt, interfaces::beef.instance_, interfaces::beef.fields_[0].event_id_,
                                        vsomeip::message_type_e::MT_NOTIFICATION, beef_payload};

    auto cafe_payload = std::vector<unsigned char>{0xf, 0xa, 0xd, 0xe};
    auto cafe_checker = message_checker{std::nullopt, interfaces::cafe.instance_, interfaces::cafe.fields_[0].event_id_,
                                        vsomeip::message_type_e::MT_NOTIFICATION, cafe_payload};

    // offering setup
    start_router();
    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // only offer now, otherwise the routing_manager will encounter
    // data race in the sec_client usage :/
    routingmanagerd_->offer(interfaces::beef);
    routingmanagerd_->send_event(interfaces::beef.fields_[0], beef_payload);
    server_->offer(interfaces::cafe);
    server_->send_event(interfaces::cafe.fields_[0], cafe_payload);

    // ensure setup is fully operational
    client_ = start_client(client_name_);
    ASSERT_NE(client_, nullptr);
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    client_->subscribe(interfaces::cafe);
    client_->subscribe(interfaces::beef);
    ASSERT_TRUE(client_->message_record_.wait_for(cafe_checker));
    ASSERT_TRUE(client_->message_record_.wait_for(beef_checker));

    // do the actual subscription and ensure subscription is working
    routingmanagerd_->subscribe(interfaces::cafe);
    server_->subscribe(interfaces::beef);
    EXPECT_TRUE(routingmanagerd_->message_record_.wait_for(cafe_checker));
    EXPECT_TRUE(server_->message_record_.wait_for(beef_checker));
}

TEST_F(test_client_lifecycle, ensure_unavail_after_stop_offer) {
    start_apps();
    request_service();
    ASSERT_TRUE(await_service());
    client_->availability_record_.clear();

    stop_offer();
    EXPECT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
}

TEST_F(test_client_lifecycle, ensure_unavail_after_stop_app) {
    start_apps();
    request_service();
    ASSERT_TRUE(await_service());
    client_->availability_record_.clear();
    stop_client(server_name_);

    EXPECT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
}

TEST_F(test_client_lifecycle, field_subscription) {
    start_apps();

    send_field_message();
    ASSERT_TRUE(subscribe_to_field());

    EXPECT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
}
TEST_F(test_client_lifecycle, field_subscription_before_field_offering) {
    start_router();
    start_client_app();
    request_service();
    client_->subscribe_field(offered_field_);

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    server_->offer_event(offered_event_);
    server_->offer_field(offered_field_);
    server_->offer(service_instance_);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    send_field_message();

    EXPECT_TRUE(client_->message_record_.wait_for(field_checker_)) << client_->message_record_;
}

TEST_F(test_client_lifecycle, field_subscription_between_service_and_field_offering) {
    start_router();
    start_client_app();
    request_service();
    client_->subscribe_field(offered_field_);

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    server_->offer(service_instance_);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    server_->offer_event(offered_event_);
    server_->offer_field(offered_field_);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    send_field_message();

    EXPECT_TRUE(client_->message_record_.wait_for(field_checker_)) << client_->message_record_;
}

TEST_F(test_client_lifecycle, router_offers_field) {
    start_router();
    start_client_app();
    routingmanagerd_->offer(service_instance_);
    routingmanagerd_->offer_event(offered_event_);
    routingmanagerd_->offer_field(offered_field_);
    routingmanagerd_->send_event(offered_field_, field_payload_);

    ASSERT_TRUE(subscribe_to_field());
    EXPECT_TRUE(client_->message_record_.wait_for_last(first_expected_field_message_));
}

TEST_F(test_client_lifecycle, request_reply_no_sub) {
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

TEST_F(test_client_lifecycle, request_reply_with_sub) {
    start_apps();

    // NOTE: subscription acknowledge happens after service availability, which is why this works!
    ASSERT_TRUE(subscribe_to_event());

    answer_requests_with({0x2, 0x3});
    client_->send_request(request_);

    ASSERT_TRUE(server_->message_record_.wait_for_last(expected_request_));
    EXPECT_TRUE(client_->message_record_.wait_for_last(expected_reply_));
}

TEST_F(test_client_lifecycle, request_reply_routingd) {
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

TEST_F(test_client_lifecycle, the_server_sends_subscribe_ack_when_the_routing_info_is_late) {
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

TEST_F(test_client_lifecycle, switch_client_id) {

    // This test ensures that when a service is offered by an app that
    // switched its client id, the applications subscribed to it
    // are able to receive notifications for it after the client id switch
    // 1. Service is offered
    // 2. Client requests service
    // 3. Service is stopped
    // 4. A new application is started claiming the former client id of service app
    // 5. The previous service is started again with a new client id
    // 6. Ensure that the client is still able to receive notifications

    // Use specific configuration where apps don't have fixed client id
    use_configuration("switch_client_id.json");

    // 1.
    start_apps();
    auto old_server_client_id = server_->get_client();

    // 2.
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));

    // 3.
    stop_client(server_name_);
    client_->subscription_record_.clear();
    while (utility::get_used_client_ids("vsomeip").size() > 2) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    } // only continue after old server client id is free

    // 4.
    auto new_app_name = "new_application";
    create_app(new_app_name);
    auto new_app = start_client(new_app_name);
    ASSERT_NE(new_app, nullptr);
    ASSERT_TRUE(new_app->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // 5.
    create_app(server_name_);
    start_server();
    auto new_server_client_id = server_->get_client();
    // Verify that server client id has switched
    ASSERT_NE(old_server_client_id, new_server_client_id);

    // 6.
    ASSERT_TRUE(subscribe_to_event());
    send_first_message();
    ASSERT_TRUE(client_->message_record_.wait_for_last(first_expected_message_));
}

TEST_F(test_client_lifecycle, server_suback_after_sub_insert) {
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

TEST_F(test_client_lifecycle, missing_initial_events) {
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

TEST_F(test_client_lifecycle, service_is_unavailable_after_clean_up_race) {
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
TEST_F(test_client_lifecycle, test_subscription_for_ghost_service) {
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

TEST_F(test_client_lifecycle, availability_callback_is_only_called_once_on_stop) {
    /**
     * Regression test for the following scenario:
     * 0. router, server and client are started
     * 1. client subscribes the service
     * 2. stop the server
     * 3. verify that the client only received 1 ON_AVAILABLE and 1 ON_UNAVAILABLE
     **/

    std::vector<service_availability> expected_availabilities = {service_availability::available(service_instance_),
                                                                 service_availability::unavailable(service_instance_)};

    std::vector<service_availability> unexpected_availabilities = {service_availability::available(service_instance_),
                                                                   service_availability::unavailable(service_instance_),
                                                                   service_availability::available(service_instance_)};

    start_apps();
    ASSERT_TRUE(subscribe_to_field());

    stop_client(server_name_);

    // Wait for some time to ensure that the availabilities are not checked too early
    ASSERT_FALSE(client_->availability_record_.wait_for(
            [&unexpected_availabilities](const auto& record) { return record == unexpected_availabilities; },
            std::chrono::milliseconds(300)))
            << client_->availability_record_;

    ASSERT_TRUE(client_->availability_record_.wait_for([&expected_availabilities](const auto& record) {
        return record == expected_availabilities;
    })) << client_->availability_record_;
}

TEST_F(test_client_lifecycle, service_re_request_does_not_block_new_request) {
    /**
     * 0. router, server and client are started
     * 1. start a new server that will offer a different service
     * 2. delay message processing on one of the servers to simulate it being offline
     * 3. client re-requests original service and requests new service
     * 4. verify that the client received the availability only for the new requested service
     **/

    start_apps();
    request_service();
    ASSERT_TRUE(await_service());

    create_app(server_name_two_);
    auto* another_server = start_client(server_name_two_);
    another_server->offer(service_instance_two_);

    // Delay processing of messages from daemon->server to simulate a situation where one of the servers is offline
    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, true, socket_role::server));

    client_->request_service(service_instance_);
    client_->request_service(service_instance_two_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_two_)));

    ASSERT_TRUE(delay_message_processing(server_name_, routingmanager_name_, false, socket_role::server));
}

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

    std::future<protocol::id_e> fut = drop_command_once(client_one_, routingmanager_name_, protocol::id_e::ASSIGN_CLIENT_ID);

    // start a client
    create_app(client_one_);
    auto* one = start_client(client_one_);
    ASSERT_TRUE(await_connection(client_one_, routingmanager_name_));

    ASSERT_FALSE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(1)));

    // sanity check that the right message was dropped
    ASSERT_TRUE(fut.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    ASSERT_EQ(fut.get(), protocol::id_e::ASSIGN_CLIENT_ID);

    // and that application eventually registers
    EXPECT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

/**
 * Test concurrent registration: deregister from first instance is in queue when
 * register from second instance arrives.
 *
 * Scenario:
 * 1. Start router
 * 2. Start first instance, wait for registration
 * 3. First instance offers and requests service
 * 4. Delay deregister processing to simulate queue buildup
 * 5. Stop first instance (deregister queued)
 * 6. Immediately start second instance (register queued after deregister)
 * 7. Process all queued commands
 * 8. Verify second instance can offer/request and communicate
 */
TEST_F(test_concurrent_registration, deregister_register_queue_ordering) {
    // 1. Start the routing manager
    start_router();

    // 2. Start first instance
    create_app(app_name_);
    auto* first_instance = start_client(app_name_);
    ASSERT_NE(first_instance, nullptr);
    ASSERT_TRUE(first_instance->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // 3. First instance offers and requests service
    first_instance->offer(service_);
    first_instance->request_service(service_);
    ASSERT_TRUE(first_instance->availability_record_.wait_for_last(service_availability::available(service_)));

    // 4. Hold back the deregister processing in the routing manager
    // This ensures the deregister command stays in the queue
    ASSERT_TRUE(delay_message_processing(app_name_, routingmanager_name_, true));

    // 5. Stop first instance - this queues the DEREGISTER_APPLICATION command
    TEST_LOG << "[step] Stopping first instance (deregister will be queued)";
    stop_client(app_name_);

    // 6. Immediately start second instance - this queues REGISTER_APPLICATION after DEREGISTER
    TEST_LOG << "[step] Starting second instance (register queued after deregister)";
    create_app(app_name_);
    auto* second_instance = start_client(app_name_);
    ASSERT_NE(second_instance, nullptr);

    // Wait for the second instance's socket to be established before releasing the delay.
    // (start_client only waits for io_context assignment, not for the TCP connection)
    ASSERT_TRUE(await_connection(app_name_, routingmanager_name_));

    // Allow the routing manager to process queued messages (DEREGISTER then REGISTER)
    ASSERT_TRUE(delay_message_processing(app_name_, routingmanager_name_, false));

    // 7. Second instance should register successfully (new connection has no delay)
    ASSERT_TRUE(second_instance->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // Second instance offers and requests the service
    second_instance->offer(service_);
    second_instance->request_service(service_);
    EXPECT_TRUE(second_instance->availability_record_.wait_for_last(service_availability::available(service_)));
}

/**
 * Test concurrent registration with a client verifying service reachability.
 *
 * Scenario similar to above but with an additional client that
 * verifies the service offered by the second instance is reachable.
 */
TEST_F(test_concurrent_registration, deregister_register_service_reachable_by_client) {
    // Start router
    start_router();

    // Create and start a client that will request the service
    std::string const client_name{"client"};
    create_app(client_name);
    auto* client = start_client(client_name);
    ASSERT_NE(client, nullptr);
    ASSERT_TRUE(client->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // Client requests the service
    client->request_service(service_);

    // Start first instance that offers the service
    create_app(app_name_);
    auto* first_instance = start_client(app_name_);
    ASSERT_NE(first_instance, nullptr);
    ASSERT_TRUE(first_instance->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // First instance offers service
    first_instance->offer(service_);
    std::vector<uint8_t> payload_v1{0x01, 0x02};
    first_instance->answer_request(req_, [payload_v1] { return payload_v1; });

    // Wait for client to see the service available
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(service_)));

    // Client sends request and verifies the reply from the first instance
    client->send_request(req_);
    ASSERT_TRUE(client->wait_for_messages({payload_v1}));

    client->availability_record_.clear();
    client->message_record_.clear();

    // Hold back deregister processing
    ASSERT_TRUE(delay_message_processing(app_name_, routingmanager_name_, true));

    // Stop first instance
    TEST_LOG << "[step] Stopping first instance";
    stop_client(app_name_);

    // Immediately start second instance
    TEST_LOG << "[step] Starting second instance";
    create_app(app_name_);
    auto* second_instance = start_client(app_name_);
    ASSERT_NE(second_instance, nullptr);

    // Wait for the second instance's socket to be established before releasing the delay.
    ASSERT_TRUE(await_connection(app_name_, routingmanager_name_));

    // Allow the routing manager to process queued messages (DEREGISTER then REGISTER)
    ASSERT_TRUE(delay_message_processing(app_name_, routingmanager_name_, false));

    // Second instance should register (new connection has no delay)
    ASSERT_TRUE(second_instance->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // Second instance offers service with different payload
    second_instance->offer(service_);
    std::vector<uint8_t> payload_v2{0x03, 0x04};
    second_instance->answer_request(req_, [payload_v2] { return payload_v2; });

    // Client should see service available again (after potential unavailable)
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(service_)));

    // Verify the second instance is actually serving by checking the v2 payload
    client->send_request(req_);
    EXPECT_TRUE(client->wait_for_messages({payload_v2}));
}

/**
 * Smoke test: repeated start/stop of the same application name.
 */
TEST_F(test_concurrent_registration, repeated_start_stop) {
    start_router();

    for (size_t iteration = 0; iteration < 10; ++iteration) {
        TEST_LOG << "[iteration] " << iteration;

        // Start instance
        create_app(app_name_);
        auto* instance = start_client(app_name_);
        ASSERT_NE(instance, nullptr);
        ASSERT_TRUE(instance->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

        // Offer and request service
        instance->offer(service_);
        instance->request_service(service_);
        ASSERT_TRUE(instance->availability_record_.wait_for_last(service_availability::available(service_)));

        // Stop instance
        stop_client(app_name_);
    }
    stop_client(routingmanager_name_);
}

/**
 * Concurrent registration with deregister/register queuing on alternating iterations.
 */
TEST_F(test_concurrent_registration, repeated_concurrent_registration_with_queuing) {
    start_router();

    for (size_t iteration = 0; iteration < 5; ++iteration) {
        TEST_LOG << "[iteration] " << iteration;

        // Start instance
        create_app(app_name_);
        auto* instance = start_client(app_name_);
        ASSERT_NE(instance, nullptr);
        ASSERT_TRUE(instance->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

        // Offer and request service
        instance->offer(service_);
        instance->request_service(service_);
        ASSERT_TRUE(instance->availability_record_.wait_for_last(service_availability::available(service_)));

        // Hold back deregister to create queue scenario
        ASSERT_TRUE(delay_message_processing(app_name_, routingmanager_name_, true));

        // Stop instance - deregister is queued
        stop_client(app_name_);

        // Create next instance before releasing queue - register is queued after deregister
        create_app(app_name_);
        auto* next_instance = start_client(app_name_);
        ASSERT_NE(next_instance, nullptr);

        // Wait for the second instance's socket to be established before releasing the delay.
        ASSERT_TRUE(await_connection(app_name_, routingmanager_name_));

        // Release queued routing manager messages to process deregister/register in order
        ASSERT_TRUE(delay_message_processing(app_name_, routingmanager_name_, false));

        // Next instance should register successfully (new connection has no delay)
        ASSERT_TRUE(next_instance->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

        // Stop this instance normally
        stop_client(app_name_);
    }
    stop_client(routingmanager_name_);
}
}

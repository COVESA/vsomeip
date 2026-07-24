// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_gate.hpp"
#include "helpers/command_record.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/sockets/fake_tcp_socket_handle.hpp"
#include "helpers/service_state.hpp"
#include "helpers/availability_checker.hpp"
#include "common/timeout_scale.hpp" // common::scaled_timeout

#include "../../../implementation/utility/include/utility.hpp"
#include "../../../implementation/protocol/include/command_types.hpp"
#include "../../../implementation/protocol/include/serialize.hpp"

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
        server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
        server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
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
    message_checker const event_checker_{std::nullopt, service_instance_, offered_event_.event_id_,
                                         vsomeip::message_type_e::MT_NOTIFICATION, std::vector<unsigned char>{}};

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
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
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
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
    ASSERT_TRUE(routingmanagerd_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    send_field_message();
    EXPECT_TRUE(routingmanagerd_->message_record_.wait_for(field_checker_));
}
TEST_F(test_client_lifecycle, router_consumes_field_after_service_tries_to_offer_before_registration) {
    GTEST_SKIP() << "Provokes a race in application_impl regarding the usage of sec_client_";

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    server_->offer(service_instance_);
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
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
    routingmanagerd_->send_event({interfaces::beef.instance_, interfaces::beef.fields_[0]}, beef_payload);
    server_->offer(interfaces::cafe);
    server_->send_event({interfaces::cafe.instance_, interfaces::cafe.fields_[0]}, cafe_payload);

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
TEST_F(test_client_lifecycle, cached_field) {
    start_apps();

    // offering
    service_instance other_service = service_instance_;
    other_service.service_ += 2;
    event_ids field_one{other_service, 0x8010, 0x2};
    event_ids field_two{other_service, 0x8011, 0x2};
    server_->offer_event(field_one.si_, field_one.to_event_spec());
    server_->offer_field(field_two.si_, field_two.to_event_spec());
    server_->offer(other_service);

    // subscribing on the server side for both fields,
    // but only handling the first
    client_->request_service(other_service);
    client_->subscribe_eventgroup_field(field_one);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_one)));

    std::vector<unsigned char> p1 = {0x1, 0x2};
    std::vector<unsigned char> p2 = {0x2, 0x3};
    std::vector<unsigned char> p3 = {0x2, 0x7};

    server_->send_event(field_two, p2);
    server_->send_event(field_two, p3);
    server_->send_event(field_one, p1);

    message_checker const field_checker1{std::nullopt, other_service, field_one.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, p1};
    message_checker const field_checker3{std::nullopt, other_service, field_two.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, p3};

    ASSERT_TRUE(client_->message_record_.wait_for(field_checker1)) << client_->message_record_;
    client_->subscribe_eventgroup_field(field_two);

    EXPECT_TRUE(client_->message_record_.wait_for(field_checker3)) << client_->message_record_;
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

    server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
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

    server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    send_field_message();

    EXPECT_TRUE(client_->message_record_.wait_for(field_checker_)) << client_->message_record_;
}

TEST_F(test_client_lifecycle, router_offers_field) {
    start_router();
    start_client_app();
    routingmanagerd_->offer(service_instance_);
    routingmanagerd_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
    routingmanagerd_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
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
    server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
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
    inject_command_tcp(client_name_, server_name_, subscription_payload);
    ASSERT_TRUE(wait_for_command(client_name_, server_name_, protocol::id_e::SUBSCRIBE_NACK_ID, socket_role::client));
}

TEST_F(test_client_lifecycle, test_partial_read_leads_to_connection_drop) {
    start_apps();
    ASSERT_TRUE(subscribe_to_event());

    // Watch before injecting: the consumer reconnects within ~1ms of the drop, so a level-based
    // check can miss it. The watch latches the drop regardless of reconnect timing.
    auto drop_watch = watch_connection_drop(client_name_, server_name_);

    auto subscription_payload = construct_basic_raw_command(protocol::id_e::SUBSCRIBE_ID, // command
                                                            static_cast<uint16_t>(0), // version
                                                            static_cast<client_t>(0x3490), // client id
                                                            static_cast<uint32_t>(20), // size
                                                            static_cast<service_t>(0xAAAA), // service
                                                            static_cast<instance_t>(0x00)
                                                            // to not finish the message
    );
    inject_command_tcp(client_name_, server_name_, subscription_payload);
    EXPECT_TRUE(drop_watch.wait()) << "connection drop was not detected";
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

TEST_F(test_client_lifecycle, release_then_request_blocks_duplicate_available) {
    /**
     * After release_service(), the handler shadow remains AS_AVAILABLE.
     * When request_service() is called again (service still offered), replay_availability()
     * and the subsequent RIE_ADD from the routing manager both hit on_availability(AS_AVAILABLE).
     * The shadow check (shadow == state) suppresses both — no second callback fires.
     **/
    start_apps();
    request_service();
    ASSERT_TRUE(await_service());

    clear_command_record(client_name_, routingmanager_name_);
    client_->release_service(service_instance_);
    client_->request_service(service_instance_);

    // Wait for the routing manager to receive the re-registration, ensuring RIE_ADD has had time to arrive
    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, protocol::id_e::REQUEST_SERVICE_ID, socket_role::server));

    // The dedup guard must suppress the duplicate AS_AVAILABLE — no second entry must appear
    ASSERT_FALSE(client_->availability_record_.wait_for([](const auto& _r) { return _r.size() > 1; }, std::chrono::milliseconds(300)))
            << client_->availability_record_;
    ASSERT_TRUE(client_->availability_record_.equals({service_availability::available(service_instance_)}));
}

TEST_F(test_client_lifecycle, reoffer_after_stop_fires_available) {
    /**
     * Dedup guard test: full offer → stop_offer → re-offer cycle.
     * The shadow transitions AS_UNKNOWN → AS_AVAILABLE → AS_UNAVAILABLE → AS_AVAILABLE.
     * Each step satisfies shadow != state so each handler fires exactly once.
     * Expected sequence: [AVAILABLE, UNAVAILABLE, AVAILABLE] — no entry omitted, no duplicates.
     **/
    std::vector<service_availability> expected_availabilities = {
            service_availability::available(service_instance_),
            service_availability::unavailable(service_instance_),
            service_availability::available(service_instance_),
    };

    start_apps();
    request_service();
    ASSERT_TRUE(await_service());

    stop_offer();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    server_->offer(service_instance_);

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

TEST_F(test_client_lifecycle, subscribe_before_event_offering) {
    /**
     * 0. start router and client
     * 1. client requests service and subscribes to field and event
     * 2. start server and offer service
     * 3. ensure server received the subscription and sent initial event
     * 4. only after, offer and send field
     * 5. ensure client receives field message
     */

    start_router();
    start_client_app();

    // first subscribe to the field that will be offered late
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);
    client_->subscribe_field(offered_event_);

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    server_->offer(service_instance_);
    // ensure server received the subscription of the first event
    ASSERT_TRUE(client_->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(offered_field_)));
    // and tried to send the initial event out already
    ASSERT_TRUE(client_->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(offered_event_)));

    // only now offer the field
    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
    send_field_message();
    EXPECT_TRUE(client_->message_record_.wait_for(field_checker_)) << client_->message_record_;
}

TEST_F(test_client_lifecycle, self_subscribe) {
    /**
     * 0. start router and server
     * 1. server offers and requests service and subscribes to field and event
     * 2. ensure server received the subscription and sent initial event
     */

    start_router();
    start_server();

    server_->request_service(service_instance_);
    server_->subscribe_field(offered_field_);
    server_->subscribe_field(offered_event_);

    ASSERT_TRUE(server_->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(offered_field_)));
    ASSERT_TRUE(server_->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(offered_event_)));

    send_field_message();
    send_first_message();
    EXPECT_TRUE(server_->message_record_.wait_for(field_checker_)) << server_->message_record_;
    EXPECT_TRUE(server_->message_record_.wait_for(event_checker_)) << server_->message_record_;
}

TEST_F(test_client_lifecycle, late_self_subscribe) {
    start_router();
    start_server();

    send_field_message();

    server_->request_service(service_instance_);
    server_->subscribe_field(offered_field_);

    ASSERT_TRUE(server_->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(offered_field_)));
    EXPECT_TRUE(server_->message_record_.wait_for(field_checker_)) << server_->message_record_;
}

TEST_F(test_client_lifecycle, empty_field_is_received) {
    start_apps();

    ASSERT_TRUE(subscribe_to_field());

    message_checker checker{std::nullopt, service_instance_, offered_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                            std::vector<unsigned char>{}};
    ASSERT_FALSE(client_->message_record_.wait_for(checker, std::chrono::milliseconds(200))) << client_->message_record_;
    // Send empty field, should be received even if the value is the same as before (default payload) as event is still not set.
    server_->send_event(offered_field_, {});
    EXPECT_TRUE(client_->message_record_.wait_for(checker)) << client_->message_record_;
    client_->message_record_.clear();
    // Try to resend empty field, should fail due to caching, as the value is the same as before and event is already set.
    server_->send_event(offered_field_, {});
    EXPECT_FALSE(client_->message_record_.wait_for(checker, std::chrono::milliseconds(200))) << client_->message_record_;
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
    ASSERT_TRUE(fut.wait_for(common::scaled_timeout(std::chrono::seconds(5))) == std::future_status::ready);
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

TEST_F(test_client_lifecycle, resubscribe_while_acked_replays_cache_without_duplicate_subscribe) {
    start_apps();

    // Wait for the client to subscribe and receive the initial field value from the server.
    ASSERT_TRUE(subscribe_to_field());
    send_field_message();
    ASSERT_TRUE(client_->message_record_.wait_for(field_checker_));

    // Clear records so only activity from the re-subscribe onward is visible.
    client_->message_record_.clear();
    clear_command_record(server_name_, routingmanager_name_);

    // Re-subscribe without the server sending any new notification. The routing_manager_client
    // must replay the cached field value locally via send_back_cached_event_unlocked().
    client_->subscribe_field(offered_field_);
    EXPECT_FALSE(wait_for_command(client_name_, routingmanager_name_, protocol::id_e::SUBSCRIBE_ID, socket_role::client,
                                  std::chrono::milliseconds(200)));

    // The replayed notification must arrive on the client.
    EXPECT_TRUE(client_->message_record_.wait_for(field_checker_));

    // No NOTIFY_ID must have been sent by the server during this window —
    // proving the notification came from local cache replay, not a fresh server emission.
    EXPECT_FALSE(wait_for_command(server_name_, routingmanager_name_, protocol::id_e::NOTIFY_ID, socket_role::server));
}

TEST_F(test_client_lifecycle, resubscribe_after_service_restart_delivers_fresh_notification_not_cache) {
    start_apps();

    ASSERT_TRUE(subscribe_to_field());
    send_field_message();
    ASSERT_TRUE(client_->message_record_.wait_for(field_checker_));

    // Stop the service offer. Client must see the service go unavailable.
    stop_offer();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // Confirm that re-subscribing while the service is unavailable does NOT replay the stale v1 value.
    // on_stop_offer_service() must have reset initial_notification_received_, so no cache replay fires.
    client_->message_record_.clear();
    client_->subscribe_field(offered_field_);
    EXPECT_FALSE(client_->message_record_.wait_for(field_checker_, std::chrono::milliseconds(200)));

    // Re-offer the service with a new field payload (v2).
    std::vector<unsigned char> const payload_v2{0xBB};
    message_checker const field_checker_v2{std::nullopt, service_instance_, offered_field_.event_id_,
                                           vsomeip::message_type_e::MT_NOTIFICATION, payload_v2};

    server_->offer_field(offered_field_.si_, offered_field_.to_event_spec());
    server_->send_event(offered_field_, payload_v2);
    server_->offer(service_instance_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // Re-subscription is automatic: when the service re-offers, send_pending_commands() replays
    // pending_subscriptions_. Because on_stop_offer_service() reset initial_notification_received_,
    // no stale cache replay fires — the client must wait for the fresh v2 notification from the server.

    EXPECT_TRUE(client_->message_record_.wait_for(field_checker_v2));
    // The last recorded message must be v2, not the stale v1.
    EXPECT_TRUE(client_->message_record_.wait_for_last(field_checker_v2));
}

/**
 * Test fixture for verifying that a single IO thread does not deadlock during stop.
 *
 * With the default configuration (2 IO threads), the stop sequence could rely on
 * a second thread to complete asynchronous work. With only 1 IO thread, the stop
 * continuation must not block the sole thread — otherwise the application hangs.
 */
struct test_single_io_thread : public base_fake_socket_fixture {
    static constexpr auto router_name_ = "routingmanagerd";
    static constexpr auto server_name_ = "server";
    static constexpr auto client_name_ = "client";

    ecu_config config_ = [] {
        ecu_config cfg;
        cfg.apps_ = {application_config{router_name_, 0x0100, 1}, application_config{server_name_, 0x3489, 1},
                     application_config{client_name_, 0x3490, 1}};
        cfg.routing_config_ = local_tcp_config{.router_name_ = router_name_,
                                               .host_ = boost::asio::ip::make_address("127.0.0.1"),
                                               .guest_ = boost::asio::ip::make_address("127.0.0.1")};
        cfg.sd_ = false;
        return cfg;
    }();

    ecu_setup ecu_{"single_io", config_, *socket_manager_};

    service_instance service_{0x3344, 0x1};
    event_ids field_{service_, 0x8003, 0x6};

    void start_apps() {
        ecu_.prepare();
        ecu_.start_apps();
    }
};

TEST_F(test_single_io_thread, stop_flushes_queued_messages_with_one_io_thread) {
    /**
     * Verify that endpoint flushing during stop works correctly with a single
     * IO thread and that queued messages are delivered to the receiver.
     *
     * 1. Start all apps (router, server, client) each with threads=1
     * 2. Offer a service and subscribe to a field
     * 3. Delay the send-completion callback on the server→client connection
     *    so messages pile up in the server's local endpoint send queue
     * 4. Send several event notifications from the server
     * 5. Stop the server — this triggers endpoint flushing
     * 6. Release the send delay so the queued data can flow
     * 7. Verify the client received all notifications
     */
    start_apps();

    auto* server = ecu_.apps_[server_name_];
    auto* client = ecu_.apps_[client_name_];
    ASSERT_NE(server, nullptr);
    ASSERT_NE(client, nullptr);

    ASSERT_TRUE(server->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    ASSERT_TRUE(client->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    server->offer_field(field_.si_, field_.to_event_spec());
    server->offer(service_);

    client->request_service(service_);
    client->subscribe_field(field_);
    ASSERT_TRUE(client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_)));

    // Hold back send-completion callbacks from the server so that notifications
    // pile up in the server's local endpoint send queue.
    ASSERT_TRUE(delay_sending(client_name_, server_name_, true, socket_role::server));

    server->send_event(field_, {0x01});
    server->send_event(field_, {0x02});
    server->send_event(field_, {0x03});

    // Stop the server while messages are still queued.
    // With 1 IO thread this must not deadlock — the async stop continuation
    // allows the flushing work to complete on the same thread.
    // (use vsomeip application directly to avoid joining the io thread)
    server->get_application()->stop();

    // Release the queued send completions so the endpoint can finish flushing
    // and the routing manager forwards the notifications.
    ASSERT_TRUE(delay_sending(client_name_, server_name_, false, socket_role::server));

    // The client should eventually receive the last notification.
    message_checker const field_checker{std::nullopt, service_, field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                                        std::vector<unsigned char>{0x03}};
    EXPECT_TRUE(client->message_record_.wait_for(field_checker, std::chrono::seconds(5)))
            << "Client did not receive all flushed notifications\n"
            << client->message_record_;

    ecu_.stop_one(client_name_);
    ecu_.stop_one(router_name_);
}

// Regression tests "Decouple consumer vs. provider error handler".
// A routing_manager_client can be, toward the same peer, both PROVIDER (accepted
// socket for a service it offers) and CONSUMER (outbound socket for one it consumes),
// on two distinct sockets keyed by client_t. A failure on one must tear down only
// that role. Here A offers S1 (B subscribes) and consumes S2 (B offers); we fail one
// socket and assert the other role of A survives.
struct test_provider_consumer_error_isolation : public base_fake_socket_fixture {
    test_provider_consumer_error_isolation() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(a_name_);
        create_app(b_name_);
        create_app(c_name_);
    }

    // A: offers S1, consumes S2 (bound to the file-scope server/client names).
    std::string const& a_name_{server_name_};
    // B: offers S2, consumes S1.
    std::string const& b_name_{client_name_};
    // C: a second consumer of S1 that offers nothing (models a new, live provider peer).
    std::string const& c_name_{client_name_two_};

    // S1 is offered by A and consumed by B.
    service_instance s1_{0x3344, 0x1};
    event_ids ev1_{s1_, 0x8002, 0x1};
    // S2 is offered by B and consumed by A.
    service_instance s2_{0x3345, 0x1};
    event_ids ev2_{s2_, 0x8002, 0x1};

    std::vector<unsigned char> s1_payload_{0x11, 0x22};
    std::vector<unsigned char> s2_payload_{0x33, 0x44};

    static message_checker notification_checker(service_instance const& _si, vsomeip::event_t _event,
                                                std::vector<unsigned char> const& _payload) {
        return message_checker{std::nullopt, _si, _event, vsomeip::message_type_e::MT_NOTIFICATION, _payload};
    }

    app* rm_{};
    app* a_{};
    app* b_{};
    app* c_{};

    // Brings the bidirectional provider/consumer relationship into a verified
    // steady state: both directed local sockets exist and both directions
    // actually carry a notification.
    void bring_up_bidirectional() {
        rm_ = start_client(routingmanager_name_);
        ASSERT_NE(rm_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));

        a_ = start_client(a_name_);
        ASSERT_NE(a_, nullptr);
        ASSERT_TRUE(a_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        b_ = start_client(b_name_);
        ASSERT_NE(b_, nullptr);
        ASSERT_TRUE(b_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

        // A offers S1, B offers S2.
        a_->offer(s1_);
        a_->offer_event(ev1_.si_, ev1_.to_event_spec());
        b_->offer(s2_);
        b_->offer_event(ev2_.si_, ev2_.to_event_spec());

        // B consumes S1 (provider connection B -> A).
        b_->request_service(s1_);
        ASSERT_TRUE(b_->availability_record_.wait_for_last(service_availability::available(s1_)));
        b_->subscribe_event(ev1_);
        ASSERT_TRUE(b_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(ev1_)));

        // A consumes S2 (consumer connection A -> B).
        a_->request_service(s2_);
        ASSERT_TRUE(a_->availability_record_.wait_for_last(service_availability::available(s2_)));
        a_->subscribe_event(ev2_);
        ASSERT_TRUE(a_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(ev2_)));

        // Both directed local sockets between A and B must exist before we
        // fault-inject one of them.
        ASSERT_TRUE(await_connection(b_name_, a_name_)); // provider: B -> A
        ASSERT_TRUE(await_connection(a_name_, b_name_)); // consumer: A -> B
        // Confirm they really are two independent directed connections.
        ASSERT_GE(connection_count(b_name_, a_name_), 1u);
        ASSERT_GE(connection_count(a_name_, b_name_), 1u);

        // Baseline: both directions actually deliver a notification.
        a_->send_event(ev1_, s1_payload_);
        b_->send_event(ev2_, s2_payload_);
        ASSERT_TRUE(b_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, s1_payload_)))
                << "baseline S1 notification not received by B: " << b_->message_record_;
        ASSERT_TRUE(a_->message_record_.wait_for(notification_checker(s2_, ev2_.event_id_, s2_payload_)))
                << "baseline S2 notification not received by A: " << a_->message_record_;
    }

    // Injects into @p _target a routing_info RIE_ADD as if the routing manager announced that
    // @p _client offers @p _si at (@p _address, @p _port).
    [[nodiscard]] bool inject_routing_info_add(std::string const& _target, client_t _client, service_instance const& _si,
                                               boost::asio::ip::address_v4 const& _address, port_t _port) {
        protocol::routing_info_entry_data entry;
        entry.type_ = protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE;
        entry.client_ = _client;
        entry.address_ = _address;
        entry.port_ = _port;
        entry.services_.push_back({_si.service_, _si.instance_, major_version_t{0x1}, minor_version_t{0x0}});
        auto const cmd = protocol::create_routing_info_cmd(client_t{0x0} /* VSOMEIP_ROUTING_CLIENT */, {entry});
        std::vector<unsigned char> payload(protocol::wire_size(cmd));
        protocol::serialize(cmd, payload.data());
        return inject_command_tcp(_target, routingmanager_name_, payload, socket_role::client);
    }
};

// Test 1 (happy path, provider state intact):
// Failing ONLY A's CONSUMER socket (the outbound A -> B endpoint) must NOT drop
// B's subscription to the service A offers. A must keep serving S1 to B.
TEST_F(test_provider_consumer_error_isolation, consumer_socket_failure_keeps_provided_service_alive) {
    bring_up_bidirectional();

    // A is the connector in A -> B, so socket_role::client targets exactly A's
    // outbound consumer endpoint and leaves the B -> A provider socket up.
    ASSERT_TRUE(disconnect(a_name_, boost::asio::error::connection_reset, b_name_, std::nullopt, socket_role::client));

    // Barrier: the consumer-side cleanup for the failed A -> B socket marks the
    // peer-offered service S2 unavailable at A. Waiting for it guarantees the
    // error handler has run before we probe the provider role.
    ASSERT_TRUE(a_->availability_record_.wait_for_any(service_availability::unavailable(s2_)))
            << "consumer cleanup barrier not reached: " << a_->availability_record_;

    // Provider path (B -> A) must be untouched: B keeps receiving fresh S1.
    b_->message_record_.clear();
    std::vector<unsigned char> const next_s1{0x55, 0x66};
    a_->send_event(ev1_, next_s1);
    // Receiving the fresh S1 notification above already proves A's accepted
    // provider connection to B is still alive.
    EXPECT_TRUE(b_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, next_s1)))
            << "B stopped receiving S1 after A's consumer socket failed: " << b_->message_record_;
}

// Test 2 (different code path, consumer state intact):
// Failing ONLY A's PROVIDER socket (the accepted B -> A endpoint) must NOT tear
// down A's outbound consumer endpoint to B. We assert the consumer subscription
// is NOT disturbed: A must not be forced to re-subscribe to S2.
TEST_F(test_provider_consumer_error_isolation, provider_socket_failure_does_not_disturb_consumer_subscription) {
    bring_up_bidirectional();

    // Only observe subscription activity that happens AFTER the fault.
    a_->subscription_record_.clear();

    // Fail the whole B -> A connection (A's accepted provider endpoint); A's
    // outbound A -> B consumer socket stays up.
    ASSERT_TRUE(disconnect(b_name_, boost::asio::error::connection_reset, a_name_, boost::asio::error::connection_reset));

    // Provider cleanup barrier: wait for B to mark S1 unavailable so the provider-side error handler has run before probing A's consumer
    // role.
    ASSERT_TRUE(b_->availability_record_.wait_for_any(service_availability::unavailable(s1_)))
            << "provider cleanup barrier not reached: " << b_->availability_record_;

    // The consumer endpoint to B must stay intact: A must NOT be driven to
    // re-subscribe to S2. A consumer teardown + re-request.
    EXPECT_FALSE(a_->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(ev2_),
                                                       common::scaled_timeout(std::chrono::milliseconds(500))))
            << "A re-subscribed to S2 after a provider-socket failure (consumer endpoint was torn down): " << a_->subscription_record_;

    // And the untouched consumer socket still delivers fresh S2 notifications.
    a_->message_record_.clear();
    std::vector<unsigned char> const next_s2{0x77, 0x88};
    b_->send_event(ev2_, next_s2);
    // Receiving the fresh S2 notification above already proves A's outbound
    // consumer connection to B is still alive.
    EXPECT_TRUE(a_->message_record_.wait_for(notification_checker(s2_, ev2_.event_id_, next_s2)))
            << "A stopped receiving S2 after A's provider socket failed: " << a_->message_record_;
}

// Test 3 (edge case, no cross-role side effects):
// A PROVIDER-socket failure must NOT mark the peer-offered service unavailable
// on the consumer side, and must NOT trigger the consumer re-request path.
TEST_F(test_provider_consumer_error_isolation, provider_socket_failure_does_not_mark_consumed_service_unavailable) {
    bring_up_bidirectional();

    // Only look at availability transitions that happen AFTER the fault.
    a_->availability_record_.clear();

    // Fail the whole B -> A connection (A's accepted provider endpoint); A's
    // outbound A -> B consumer socket stays up.
    ASSERT_TRUE(disconnect(b_name_, boost::asio::error::connection_reset, a_name_, boost::asio::error::connection_reset));

    // Provider cleanup barrier: wait for B to mark S1 unavailable so the provider-side teardown ran before we assert the negative on A's
    // consumer side.
    ASSERT_TRUE(b_->availability_record_.wait_for_any(service_availability::unavailable(s1_)))
            << "provider cleanup barrier not reached: " << b_->availability_record_;

    // Before the change, the provider error also ran the consumer cleanup block,
    // which marked S2 unavailable and re-requested it. It must not happen now.
    EXPECT_FALSE(a_->availability_record_.wait_for_any(service_availability::unavailable(s2_),
                                                       common::scaled_timeout(std::chrono::milliseconds(200))))
            << "A wrongly marked the consumed service S2 unavailable on a provider-socket failure: " << a_->availability_record_;
}

// Test 4 (routing-info path, live provider of a NEW client survives):
// The old client is stale on the CONSUMER side only; its provider slot at address:(port + 1) has
// been taken over by a new, live client (here C, which only consumes our S1, so A keeps no consumer
// entry for it). A new routing_info at address:port must drop ONLY the stale consumer entry and
// MUST NOT tear down the live provider connection of the new client.
TEST_F(test_provider_consumer_error_isolation, stale_consumer_does_not_tear_down_live_provider_of_new_client) {

    rm_ = start_client(routingmanager_name_);
    ASSERT_NE(rm_, nullptr);
    ASSERT_TRUE(await_connectable(routingmanager_name_));
    a_ = start_client(a_name_);
    ASSERT_NE(a_, nullptr);
    ASSERT_TRUE(a_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    a_->offer(s1_);
    a_->offer_event(ev1_.si_, ev1_.to_event_spec());

    // C is the new, live client: it only CONSUMES S1 (offers nothing), so A holds no consumer_
    // entry for C — only its accepted provider endpoint C -> A at (127.0.0.1, C_port + 1).
    c_ = start_client(c_name_);
    ASSERT_NE(c_, nullptr);
    ASSERT_TRUE(c_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    c_->request_service(s1_);
    ASSERT_TRUE(c_->availability_record_.wait_for_last(service_availability::available(s1_)));
    c_->subscribe_event(ev1_);
    ASSERT_TRUE(c_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(ev1_)));
    ASSERT_TRUE(await_connection(c_name_, a_name_)); // provider connection C -> A

    a_->send_event(ev1_, s1_payload_);
    ASSERT_TRUE(c_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, s1_payload_)))
            << "baseline S1 notification not received by C: " << c_->message_record_;

    auto const c_port = server_port(c_name_);
    ASSERT_TRUE(c_port.has_value()) << "could not resolve C's server port";
    auto const localhost = boost::asio::ip::make_address_v4("127.0.0.1");

    // Fabricated stale old client and the new client; keep both distinct from C's real id.
    client_t const old_client = 0x6001;
    client_t const new_client = 0x7777;
    ASSERT_NE(c_->get_client_id(), old_client);
    ASSERT_NE(c_->get_client_id(), new_client);
    service_instance const stale_service{0x7A01, 0x1};
    service_instance const new_service{0x7A02, 0x1};

    // 1. Plant a STALE consumer mapping on A: the old client "offers" S_STALE at (127.0.0.1, C_port).
    //    C offers nothing, so this is the only consumer_ entry at that address:port -> unambiguous.
    ASSERT_TRUE(inject_routing_info_add(a_name_, old_client, stale_service, localhost, *c_port));
    ASSERT_TRUE(a_->availability_record_.wait_for_any(service_availability::available(stale_service)))
            << "stale consumer mapping was not installed on A: " << a_->availability_record_;

    // Only observe post-trigger effects.
    c_->availability_record_.clear();
    c_->message_record_.clear();
    a_->availability_record_.clear();

    // 2. A new client id shows up at that same address:port. get_client_by_address resolves the OLD
    //    stale client => its consumer entry is dropped; but the provider endpoint at C_port + 1 is
    //    bound to C (a different, live client) => it must survive.
    ASSERT_TRUE(inject_routing_info_add(a_name_, new_client, new_service, localhost, *c_port));

    // Barrier: the stale consumer mapping is gone => the old-client cleanup ran.
    ASSERT_TRUE(a_->availability_record_.wait_for_any(service_availability::unavailable(stale_service)))
            << "old-client consumer cleanup barrier not reached on A: " << a_->availability_record_;

    // The live provider connection C -> A must be untouched: C must NOT see S1 go unavailable.
    EXPECT_FALSE(c_->availability_record_.wait_for_any(service_availability::unavailable(s1_),
                                                       common::scaled_timeout(std::chrono::milliseconds(300))))
            << "C's live provider connection was wrongly torn down (S1 went unavailable): " << c_->availability_record_;

    // And the untouched provider connection still delivers fresh S1 to C.
    std::vector<unsigned char> const fresh{0xAB, 0xCD};
    a_->send_event(ev1_, fresh);
    EXPECT_TRUE(c_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, fresh)))
            << "C stopped receiving S1 after an unrelated new-client routing_info: " << c_->message_record_;
}

// Test 5 (routing-info path, stale provider of the OLD client is torn down):
// The old client (B) is stale on BOTH roles as A sees it: A still has a consumer entry for it (A
// consumes S2 from B) AND its accepted provider endpoint B -> A at address:(port + 1) is still
// bound to B. A new routing_info at address:port must drop the consumer entry AND tear down that
// stale provider endpoint.
TEST_F(test_provider_consumer_error_isolation, stale_consumer_and_provider_of_old_client_are_both_dropped) {
    bring_up_bidirectional();

    auto const b_port = server_port(b_name_);
    ASSERT_TRUE(b_port.has_value()) << "could not resolve B's server port";
    auto const localhost = boost::asio::ip::make_address_v4("127.0.0.1");

    client_t const new_client = 0x7777;
    ASSERT_NE(b_->get_client_id(), new_client);
    service_instance const new_service{0x7A02, 0x1};

    // Only observe post-trigger effects.
    a_->availability_record_.clear();
    b_->availability_record_.clear();

    // A new client id shows up at B's exact address:port (B gone, a new app took its slot, while B's
    // stale routing state on A has not been cleaned yet).
    ASSERT_TRUE(inject_routing_info_add(a_name_, new_client, new_service, localhost, *b_port));

    // Consumer cleanup barrier: A drops the service B offered (S2) => the old-client block ran.
    ASSERT_TRUE(a_->availability_record_.wait_for_any(service_availability::unavailable(s2_)))
            << "old-client consumer cleanup barrier not reached on A: " << a_->availability_record_;

    // The stale provider endpoint B -> A was torn down by the guard's trigger_error(): B's consumer
    // connection to A breaks, so B sees the service it consumes there (S1) go unavailable.
    EXPECT_TRUE(b_->availability_record_.wait_for_any(service_availability::unavailable(s1_)))
            << "stale provider endpoint of old client B was not torn down (B kept S1): " << b_->availability_record_;
}

// Regression tests "Split the event registration set into producer vs. consumer sets"
// Pending_event_registrations_ was split into a PROVIDER set (provider_mutex_) and a CONSUMER set
// (consumer_mutex_). Each app is both provider and consumer, so both sets are populated, covering
// register_event() (push + direct send), unregister_event() (erase), and resend_provided_event_registrations()
// (resends only the provider set). A offers S1/ev1 + consumes S2/ev2; B offers S2/ev2 + consumes S1/ev1.
struct test_pending_event_registration_split : public base_fake_socket_fixture {
    test_pending_event_registration_split() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(a_name_);
        create_app(b_name_);
    }

    std::string const& a_name_{server_name_};
    std::string const& b_name_{client_name_};

    // S1 is offered by A and consumed by B; S2 is offered by B and consumed by A.
    service_instance s1_{0x3344, 0x1};
    event_ids ev1_{s1_, 0x8002, 0x1};
    service_instance s2_{0x3345, 0x1};
    event_ids ev2_{s2_, 0x8002, 0x1};

    std::vector<unsigned char> s1_payload_{0x11, 0x22};
    std::vector<unsigned char> s2_payload_{0x33, 0x44};

    static message_checker notification_checker(service_instance const& _si, vsomeip::event_t _event,
                                                std::vector<unsigned char> const& _payload) {
        return message_checker{std::nullopt, _si, _event, vsomeip::message_type_e::MT_NOTIFICATION, _payload};
    }

    app* rm_{};
    app* a_{};
    app* b_{};

    // Builds a raw RESEND_PROVIDED_EVENTS command as the routing manager would send it, so it can be
    // injected onto A's connection to drive resend_provided_event_registrations().
    static std::vector<unsigned char> make_resend_provided_events_command(vsomeip::client_t _sender) {
        return construct_basic_raw_command(protocol::id_e::RESEND_PROVIDED_EVENTS_ID,
                                           static_cast<uint16_t>(0), // command version
                                           _sender, // sender (routing manager)
                                           static_cast<uint32_t>(sizeof(vsomeip::pending_remote_offer_id_t)), // payload size
                                           static_cast<vsomeip::pending_remote_offer_id_t>(0x0000ABCD)); // pending remote offer id
    }

    // Brings A into a state where it holds BOTH a provided event registration (ev1) and a
    // consumed event registration (ev2), each verified to work.
    void bring_up() {
        rm_ = start_client(routingmanager_name_);
        ASSERT_NE(rm_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));

        a_ = start_client(a_name_);
        ASSERT_NE(a_, nullptr);
        ASSERT_TRUE(a_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        b_ = start_client(b_name_);
        ASSERT_NE(b_, nullptr);
        ASSERT_TRUE(b_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

        a_->offer(s1_);
        a_->offer_event(ev1_.si_, ev1_.to_event_spec());
        b_->offer(s2_);
        b_->offer_event(ev2_.si_, ev2_.to_event_spec());

        b_->request_service(s1_);
        ASSERT_TRUE(b_->availability_record_.wait_for_last(service_availability::available(s1_)));
        b_->subscribe_event(ev1_);
        ASSERT_TRUE(b_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(ev1_)));

        a_->request_service(s2_);
        ASSERT_TRUE(a_->availability_record_.wait_for_last(service_availability::available(s2_)));
        a_->subscribe_event(ev2_);
        ASSERT_TRUE(a_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(ev2_)));
    }

    // Asserts a fresh notification currently flows in BOTH directions: A -> B for the
    // provided event (ev1) and B -> A for the consumed event (ev2).
    void assert_both_directions_deliver(std::vector<unsigned char> const& _s1, std::vector<unsigned char> const& _s2) {
        a_->send_event(ev1_, _s1);
        b_->send_event(ev2_, _s2);
        ASSERT_TRUE(b_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, _s1)))
                << "provided event ev1 not delivered to B: " << b_->message_record_;
        ASSERT_TRUE(a_->message_record_.wait_for(notification_checker(s2_, ev2_.event_id_, _s2)))
                << "consumed event ev2 not delivered to A: " << a_->message_record_;
    }
};

// Test 1 (happy path, both sets populated via register_event):
// An app that both provides ev1 and consumes ev2 registers each event into its role-matching
// pending set; both registrations are honored end-to-end.
TEST_F(test_pending_event_registration_split, provider_and_consumer_registrations_are_both_honored) {
    bring_up();
    assert_both_directions_deliver(s1_payload_, s2_payload_);
}

// Test 2 (edge case, role-scoped unregister_event):
// Stopping the OFFER of ev1 (is_provided=true) must erase it from the provider set ONLY, leaving
// the consumed ev2 registration untouched and still delivering.
TEST_F(test_pending_event_registration_split, stopping_provided_event_does_not_disturb_consumed_registration) {
    bring_up();
    assert_both_directions_deliver(s1_payload_, s2_payload_);

    // Provider-side unregister -> erases ev1 from pending_provided_event_registrations_.
    a_->get_application()->stop_offer_event(s1_.service_, s1_.instance_, ev1_.event_id_);

    // The consumer-side registration (ev2) must be unaffected: A still receives fresh S2.
    a_->message_record_.clear();
    std::vector<unsigned char> const next_s2{0x9A, 0xBC};
    b_->send_event(ev2_, next_s2);
    EXPECT_TRUE(a_->message_record_.wait_for(notification_checker(s2_, ev2_.event_id_, next_s2)))
            << "A stopped receiving consumed S2 after stopping its provided ev1: " << a_->message_record_;
}

// Test 3 (different code path, the consumer branch of unregister_event):
// Mirror of Test 2: releasing the CONSUMED event ev2 (is_provided=false) must erase it from the
// consumer set ONLY, leaving the provided ev1 registration untouched and still delivering to B.
TEST_F(test_pending_event_registration_split, releasing_consumed_event_does_not_disturb_provided_registration) {
    bring_up();
    assert_both_directions_deliver(s1_payload_, s2_payload_);

    // Consumer-side unregister -> erases ev2 from pending_consumed_event_registrations_.
    a_->get_application()->release_event(s2_.service_, s2_.instance_, ev2_.event_id_);

    // The provider-side registration (ev1) must be unaffected: B still receives fresh S1.
    b_->message_record_.clear();
    std::vector<unsigned char> const next_s1{0xDE, 0xF0};
    a_->send_event(ev1_, next_s1);
    EXPECT_TRUE(b_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, next_s1)))
            << "B stopped receiving provided S1 after A released its consumed ev2: " << b_->message_record_;
}

// Test 4 (resend path + observable erase):
// A RESEND_PROVIDED_EVENTS command drives resend_provided_event_registrations(), which resends the
// provided registration (ev1) as REGISTER_EVENT. After the offer stops the provider set is empty, so a
// second resend emits nothing — proving both the resend and the provider-side erase read the split set.
TEST_F(test_pending_event_registration_split, resend_provided_events_resends_only_the_provider_set) {
    bring_up();

    // Observe only what A sends to the routing manager from here on.
    clear_command_record(a_name_, routingmanager_name_);
    auto resend = make_resend_provided_events_command(rm_->get_client());
    ASSERT_TRUE(inject_command_tcp(a_name_, routingmanager_name_, resend, socket_role::client));
    EXPECT_TRUE(wait_for_command(a_name_, routingmanager_name_, protocol::id_e::REGISTER_EVENT_ID, socket_role::server))
            << "A did not resend its provided event registration on RESEND_PROVIDED_EVENTS";

    // Stop offering ev1 -> pending_provided_event_registrations_ becomes empty.
    a_->get_application()->stop_offer_event(s1_.service_, s1_.instance_, ev1_.event_id_);
    clear_command_record(a_name_, routingmanager_name_);
    auto resend_again = make_resend_provided_events_command(rm_->get_client());
    ASSERT_TRUE(inject_command_tcp(a_name_, routingmanager_name_, resend_again, socket_role::client));
    EXPECT_FALSE(wait_for_command(a_name_, routingmanager_name_, protocol::id_e::REGISTER_EVENT_ID, socket_role::server,
                                  common::scaled_timeout(std::chrono::milliseconds(300))))
            << "A resent a provided registration that had been stopped (provider-set erase failed)";
}

// Test 5 (same event in BOTH split sets — the CommonAPI "stub + proxy in one process" pattern):
// A single app A offers ev1 (populating the PROVIDER set) and also subscribes to its OWN ev1
// (populating the CONSUMER set), so the identical (service, instance, event) lives in both split sets
// at once. An external consumer B subscribes to the same ev1. A single notification from A must reach
// BOTH A (self-consumption) and B (regular provider -> consumer delivery).
TEST_F(test_pending_event_registration_split, same_event_offered_and_consumed_by_one_app) {
    rm_ = start_client(routingmanager_name_);
    ASSERT_NE(rm_, nullptr);
    ASSERT_TRUE(await_connectable(routingmanager_name_));

    a_ = start_client(a_name_);
    ASSERT_NE(a_, nullptr);
    ASSERT_TRUE(a_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    b_ = start_client(b_name_);
    ASSERT_NE(b_, nullptr);
    ASSERT_TRUE(b_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // A offers ev1 (provider set) and subscribes to its OWN ev1 (consumer set): same event, both sets.
    a_->offer(s1_);
    a_->offer_event(ev1_.si_, ev1_.to_event_spec());
    a_->request_service(s1_);
    a_->subscribe_event(ev1_);
    ASSERT_TRUE(a_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(ev1_)))
            << "A failed to subscribe to its own offered ev1";

    // B consumes the same ev1 from A.
    b_->request_service(s1_);
    ASSERT_TRUE(b_->availability_record_.wait_for_last(service_availability::available(s1_)));
    b_->subscribe_event(ev1_);
    ASSERT_TRUE(b_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(ev1_)))
            << "B failed to subscribe to A's ev1";

    // One notification must reach both the self-subscriber (A) and the external subscriber (B).
    a_->send_event(ev1_, s1_payload_);
    EXPECT_TRUE(a_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, s1_payload_)))
            << "A did not receive its own offered+consumed ev1: " << a_->message_record_;
    EXPECT_TRUE(b_->message_record_.wait_for(notification_checker(s1_, ev1_.event_id_, s1_payload_)))
            << "B did not receive A's ev1: " << b_->message_record_;
}
}

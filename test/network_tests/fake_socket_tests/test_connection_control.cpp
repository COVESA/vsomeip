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
static std::string const client_name_{"client"};
static std::string const client_one_{"client-one"};
static std::string const client_two_{"client-two"};

struct test_connection_control : public base_fake_socket_fixture {
    test_connection_control() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(client_name_);
    }
    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));
    }

    void start_client_app() {
        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    service_instance service_instance_{0x3344, 0x1};
    event_ids offered_event_{service_instance_, 0x8002, 0x1};
    event_ids offered_field_{service_instance_, 0x8003, 0x6};

    app* routingmanagerd_{};
    app* client_{};
};

TEST_F(test_connection_control, basic_change) {
    /// basic check behavior of `change_connection_control`
    /// which will cause the router to drop+block/accept connections from a given host

    start_router();
    start_client_app();

    // no blockage, client should become registered
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    client_->app_state_record_.clear();

    // block connections, clients is using host 127.0.0.1 (see multiple_client_one_process.json)
    auto router = routingmanagerd_->get_application();
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    // existing client should be disconnected, become deregistered
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    // if the connections are not blocked 30ms should suffice to at least fail sometimes.
    ASSERT_FALSE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));
}

TEST_F(test_connection_control, no_clients) {
    /// check behavior of `change_connection_control` when no clients are connected

    start_router();

    auto router = routingmanagerd_->get_application();
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    // can also block other addresses
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.2"),
              connection_control_response_e::CCR_OK);

    // and allow them again
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_ACCEPT, "127.0.0.2"),
              connection_control_response_e::CCR_OK);
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_ACCEPT, "127.0.0.1"),
              connection_control_response_e::CCR_OK);
}

TEST_F(test_connection_control, toggle) {
    /// check drop+block then accept behavior of `change_connection_control`
    /// after accepting, client should be able to reconnect

    start_router();
    start_client_app();

    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    client_->app_state_record_.clear();

    auto router = routingmanagerd_->get_application();
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));

    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_ACCEPT, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    // client must be able to reconnect
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

TEST_F(test_connection_control, block_before) {
    /// check blocking behavior of `change_connection_control` before an application starts

    start_router();

    auto router = routingmanagerd_->get_application();
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    client_ = start_client(client_name_);

    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    // client never manages to connect
    ASSERT_FALSE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));
    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));

    // until the state change
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_ACCEPT, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

TEST_F(test_connection_control, multiple_clients) {
    /// check behavior of `change_connection_control` with multiple clients
    /// which will cause the router to drop+block/accept connections from a given host

    start_router();

    // start first client
    create_app(client_one_);
    auto* one = start_client(client_one_);

    ASSERT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // block connections, all clients are using host 127.0.0.1 (see multiple_client_one_process.json)
    auto router = routingmanagerd_->get_application();
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    // existing client should be disconnected, become deregistered
    ASSERT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));

    // second client should be able to register
    create_app(client_two_);
    auto* two = start_client(client_two_);
    // somewhat dislike these "see that nothing happens for X time" tests, but do not have a better idea
    ASSERT_FALSE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));

    // block connections again; should have no impact
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    // nothing changes
    ASSERT_FALSE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));
    ASSERT_FALSE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));

    // block connections for another host; should have no impact
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.2"),
              connection_control_response_e::CCR_OK);

    ASSERT_FALSE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));
    ASSERT_FALSE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));

    // allow connections for another host; should have no impact
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_ACCEPT, "127.0.0.2"),
              connection_control_response_e::CCR_OK);

    ASSERT_FALSE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));
    ASSERT_FALSE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));

    // allow connections for host; clients should be able to connect! and become registered
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_ACCEPT, "127.0.0.1"),
              connection_control_response_e::CCR_OK);

    ASSERT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    ASSERT_TRUE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
}

TEST_F(test_connection_control, guest_service) {
    /// check behavior of `change_connection_control` when a guest
    ///  application is offering a service

    {
        // we need a bit of a trick to have an application that uses another config
        // NOTE: it is safe, because no libvsomeip application is running, there is nothing doing ::getenv
        ::setenv("VSOMEIP_CONFIGURATION_guest", "guest.json", 1);
    }

    std::string const guest_name = "guest";

    start_router();
    start_client_app();

    // start guest app
    create_app(guest_name);
    auto* guest = start_client(guest_name);
    ASSERT_TRUE(guest->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // guest offer, client requests
    guest->offer(service_instance_);
    guest->offer_event(offered_event_);
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_event_);
    // client must see service available, subscription must work
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    // block guest
    auto router = routingmanagerd_->get_application();
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.2"),
              connection_control_response_e::CCR_OK);

    ASSERT_TRUE(guest->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    // client must see service unavailable, and not see a change until something else happens
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
    ASSERT_FALSE(guest->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(100)));

    // unblock guest
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_ACCEPT, "127.0.0.2"),
              connection_control_response_e::CCR_OK);

    ASSERT_TRUE(guest->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    // client must see the service available again
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    {
        // need to manually stop all clients, in order to ::unsetenv later
        // there can be nothing running from libvsomeip in order to avoid ::getenv races!
        for (auto& app : {routingmanagerd_, client_, guest}) {
            app->stop();
        }

        ::unsetenv("VSOMEIP_CONFIGURATION_guest");
    }
}

TEST_F(test_connection_control, guest_suspended) {
    /// check behavior when `change_connection_control` is done, but guest is "suspended"
    /// especially the router state

    // NOTE: it is safe, because no libvsomeip application is running, there is nothing doing ::getenv
    ::setenv("VSOMEIP_CONFIGURATION_guest", "guest.json", 1);

    std::string const guest_name = "guest";

    start_router();

    // start guest app
    create_app(guest_name);
    auto* guest = start_client(guest_name);
    ASSERT_TRUE(guest->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // guest offer, router requests
    guest->offer(service_instance_);
    routingmanagerd_->request_service(service_instance_);
    // router must see service available
    ASSERT_TRUE(routingmanagerd_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // freeze guest, so that it does not react to connection closure
    ASSERT_TRUE(set_ignore_inner_close(guest_name, true, routingmanager_name_, false));
    ASSERT_TRUE(delay_message_processing(guest_name, routingmanager_name_, true));

    // block guest
    auto router = routingmanagerd_->get_application();
    ASSERT_EQ(router->change_connection_control(connection_control_request_e::CCR_RESET_AND_BLOCK, "127.0.0.2"),
              connection_control_response_e::CCR_OK);

    // guest is frozen - so will not see the deregistration
    ASSERT_FALSE(guest->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED, std::chrono::milliseconds(100)));
    // router MUST however see service unavailable
    ASSERT_TRUE(routingmanagerd_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // now inform guest, so that test does not block due to deregistration
    ASSERT_TRUE(disconnect(guest_name, boost::asio::error::connection_reset, routingmanager_name_, std::nullopt));

    {
        // need to manually stop all clients, in order to ::unsetenv later
        // there can be nothing running from libvsomeip in order to avoid ::getenv races!
        for (auto& app : {guest, routingmanagerd_}) {
            app->stop();
        }

        ::unsetenv("VSOMEIP_CONFIGURATION_guest");
    }
}

TEST_F(test_connection_control, set_routing_state) {
    /// check that set_routing_state triggers the registered routing_state_handler

    // NOTE: it is safe, because no libvsomeip application is running, there is nothing doing ::getenv
    static std::string const routingmanagerd_name_{"router_one"};
    ::setenv("VSOMEIP_CONFIGURATION_router_one", "ecu_one.json", 1);

    create_app(routingmanagerd_name_);
    auto* routingmanagerd_ = start_client(routingmanagerd_name_);
    ASSERT_NE(routingmanagerd_, nullptr);
    ASSERT_TRUE(await_connectable(routingmanagerd_name_));

    // register a routing state handler on the router application
    attribute_recorder<vsomeip::routing_state_e> routing_state_record;
    routingmanagerd_->get_application()->register_routing_state_handler(
            [&routing_state_record](vsomeip::routing_state_e _state) { routing_state_record.record(_state); });

    // set routing state to suspended
    routingmanagerd_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    // handler must have been called with RS_SUSPENDED
    ASSERT_TRUE(routing_state_record.wait_for_last(vsomeip::routing_state_e::RS_SUSPENDED));

    // set routing state to delayed resume
    routingmanagerd_->set_routing_state(vsomeip::routing_state_e::RS_DELAYED_RESUME);

    // handler must have been called with RS_DELAYED_RESUME
    ASSERT_TRUE(routing_state_record.wait_for_last(vsomeip::routing_state_e::RS_DELAYED_RESUME));

    // set routing state to resumed
    routingmanagerd_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    // handler must have been called with RS_RESUMED
    ASSERT_TRUE(routing_state_record.wait_for_last(vsomeip::routing_state_e::RS_RESUMED));

    // set routing state to running
    routingmanagerd_->set_routing_state(vsomeip::routing_state_e::RS_RUNNING);

    // handler must have been called with RS_RUNNING
    ASSERT_TRUE(routing_state_record.wait_for_last(vsomeip::routing_state_e::RS_RUNNING));

    {
        // need to manually stop all clients, in order to ::unsetenv later
        // there can be nothing running from libvsomeip in order to avoid ::getenv races!
        routingmanagerd_->stop();
        ::unsetenv("VSOMEIP_CONFIGURATION_router_one");
    }
}
}

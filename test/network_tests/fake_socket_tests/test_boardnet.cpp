// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/service_state.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {

std::string const router_one_name = "router_one";
std::string const router_two_name_ = "router_two";
std::string const ecu_two_server_name_ = "ecu_two_server";
std::string const ecu_one_client_name_ = "ecu_one_client";

struct test_boardnet_helper : public base_fake_socket_fixture {
    test_boardnet_helper() { }
    ~test_boardnet_helper() {
        for (auto [name, vsip_app] : apps_) {
            stop_client(name);
        }
        for (auto env : env_vars_) {
            ::unsetenv(env.c_str());
        }
    }

    app* start_application(std::string _app_name, std::string _config) {
        auto env{"VSOMEIP_CONFIGURATION_" + _app_name};
        if (!env_vars_.count(env)) {
            ::setenv(env.c_str(), _config.c_str(), 1);
            env_vars_.insert(env);
        }
        create_app(_app_name);
        auto* vsip_app = start_client(_app_name);
        apps_[_app_name] = vsip_app;

        return vsip_app;
    }

    void stop_application(std::string _app_name) {
        stop_client(_app_name);
        apps_.erase(_app_name);
    }

    [[nodiscard]] bool successfully_registered(app* _app) {
        return _app && _app->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED);
    }

    void start_all_apps() {
        router_one = start_application(router_one_name, "ecu_one.json");
        ecu_one_client = start_application(ecu_one_client_name_, "ecu_one.json");
        router_two = start_application(router_two_name_, "ecu_two.json");
        ecu_two_server = start_application(ecu_two_server_name_, "ecu_two.json");
        wait_for_all_registrations(router_two, router_one, ecu_two_server, ecu_one_client);
    }

    void wait_for_all_registrations(app* _router_two, app* _router_one, app* _ecu_two_server, app* _ecu_one_client) {
        ASSERT_TRUE(successfully_registered(_router_two));
        ASSERT_TRUE(successfully_registered(_router_one));
        ASSERT_TRUE(successfully_registered(_ecu_two_server));
        ASSERT_TRUE(successfully_registered(_ecu_one_client));
    }

    interface boardnet_interface_{0x3344, vsomeip::reliability_type_e::RT_UNRELIABLE};
    service_instance service_instance_{boardnet_interface_.instance_};
    event_ids offered_field_{boardnet_interface_.field_two_};

    message first_expected_message_{client_session{0, 1},
                                    boardnet_interface_.instance_,
                                    boardnet_interface_.field_two_.event_id_,
                                    vsomeip::message_type_e::MT_NOTIFICATION,
                                    {}};

    std::map<std::string, app*> apps_;
    std::set<std::string> env_vars_;

    app* router_one{};
    app* router_two{};
    app* ecu_one_client{};
    app* ecu_two_server{};
};

TEST_F(test_boardnet_helper, test_boardnet_service_availability) {
    // start 1st daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one));

    // start 2nd daemon on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two));

    router_one->request_service(service_instance_);
    router_two->offer(boardnet_interface_);

    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    router_two->stop_offer(service_instance_);
    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
}

TEST_F(test_boardnet_helper, test_boardnet_initial_event) {
    // start 1st daemon and client on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two));

    auto ecu_two_server = start_application(ecu_two_server_name_, "ecu_two.json");

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one));

    router_one->request_service(service_instance_);
    ecu_two_server->offer(boardnet_interface_);
    ecu_two_server->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    router_one->subscribe_event(offered_field_);

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one->message_record_.wait_for_last(next_expected_message));

    // restarting ecu_blue
    TEST_LOG << "Stopping router two";
    stop_application(router_two_name_);
    router_one->availability_record_.clear();
    router_one->message_record_.clear();
    TEST_LOG << "Restarting router two";
    start_application(router_two_name_, "ecu_two.json");

    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    EXPECT_TRUE(router_one->message_record_.wait_for_last(next_expected_message));
}

struct test_field_routing : test_boardnet_helper { };

TEST_F(test_field_routing, test_routing_with_str_on_server_side) {

    // 1. start applications (ECU one (Router+Client) , ECU two (Router+Server))
    // 2. Server offers the boardnet interface
    // 3. Client subscribes to the boardnet interface
    // 4. ECU two is suspended
    // 5. Client should see the service as unavailable
    // 6. ECU two is suspended for 250ms
    // 7. ECU two is resumed
    // 8. ECU one should see the service as available again
    // 9. Client should auto subscribe again

    // 1.
    start_all_apps();

    // 2.
    ecu_two_server->offer(boardnet_interface_);

    // 3.
    ecu_one_client->subscribe(boardnet_interface_);
    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(ecu_one_client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    // 4.
    router_two->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    // 5.
    // once we are suspended, ECU two sends STOP OFFERs to the 'boardnet'
    // ECU one, will process these offers, the routing info is removed by the router (ECU one)
    // and therefore, it is expected the router to notify the client about the service unavailability
    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // 6.
    // This delay is purposely added to simulate a 'suspend'
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 7.
    router_two->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    // 8.
    // Note there is no need to re-offer the service
    // Once ECU two is resumed, it will re-offer the 'boardnet' service
    // The service should be automatically available without even to re-request the client side
    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // 9.
    // Once the service is available again, the client should auto-subscribe again to the offered field
    ASSERT_TRUE(ecu_one_client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
}

TEST_F(test_field_routing, test_routing_with_str_on_client_side) {

    // 1. start applications (ECU one (Router+Client) , ECU two (Router+Server))
    // 2. Server offers the boardnet interface
    // 3. Client subscribes to the boardnet interface
    // 4. ECU one is suspended
    // 5. Client should see the service as unavailable
    // 6. ECU one is suspended for 250ms
    // 7. ECU one is resumed
    // 8. ECU one should see the service as available again
    // 9. Client should auto subscribe again

    // 1.
    start_all_apps();

    // 2.
    ecu_two_server->offer(boardnet_interface_);

    // 3.
    ecu_one_client->subscribe(boardnet_interface_);
    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(ecu_one_client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    // 4.
    router_one->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    // 5.
    // even though ECU two did not suspend, the client should regardless see the service as unavailable
    // once we are suspended, del_routing_info is called for the 'boardnet' services
    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // 6.
    // This delay is purposely added to simulate a 'suspend'
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 7.
    router_one->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    // 8.
    // Boardnet offers by ECU two, will now process normally once resumed and service discovery is started again
    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // 9.
    // Once the service is available again, the client should now be able to subscribe again
    ASSERT_TRUE(ecu_one_client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
}

TEST_F(test_field_routing, server_router_router_client) {
    // start 1st daemon and client on 127.0.0.2 interface
    start_all_apps();

    ecu_one_client->subscribe(boardnet_interface_);
    ecu_two_server->offer(boardnet_interface_);
    ecu_two_server->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(ecu_one_client->message_record_.wait_for_last(next_expected_message));
}
TEST_F(test_field_routing, server_router_router) {
    // start 1st daemon and client on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two));

    auto ecu_two_server = start_application(ecu_two_server_name_, "ecu_two.json");

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one));

    router_one->subscribe(boardnet_interface_);
    ecu_two_server->offer(boardnet_interface_);
    ecu_two_server->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one->message_record_.wait_for_last(next_expected_message));
}
TEST_F(test_field_routing, router_router_client) {
    // start 1st daemon and client on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two));

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one));
    auto ecu_one_client = start_application(ecu_one_client_name_, "ecu_one.json");

    ecu_one_client->subscribe(boardnet_interface_);
    router_two->offer(boardnet_interface_);
    router_two->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(ecu_one_client->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    next_expected_message.client_session_.session_ += 1; // interesting
    EXPECT_TRUE(ecu_one_client->message_record_.wait_for_last(next_expected_message)) << next_expected_message;
}
TEST_F(test_field_routing, router_router) {
    // start 1st daemon and client on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two));

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one));

    router_one->subscribe(boardnet_interface_);
    router_two->offer(boardnet_interface_);
    router_two->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    next_expected_message.client_session_.session_ += 1; // interesting
    EXPECT_TRUE(router_one->message_record_.wait_for_last(next_expected_message)) << next_expected_message;
}
}

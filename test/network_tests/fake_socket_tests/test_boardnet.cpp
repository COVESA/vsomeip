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

    interface boardnet_interface_{0x3344, vsomeip::reliability_type_e::RT_UNRELIABLE};
    service_instance service_instance_{boardnet_interface_.instance_};
    event_ids offered_field_{boardnet_interface_.field_two_};

    message first_expected_message_{client_session{0, 1},
                                    boardnet_interface_.instance_,
                                    boardnet_interface_.field_two_.event_id_,
                                    vsomeip::message_type_e::MT_NOTIFICATION,
                                    {}};

    std::string const router_one_name = "router_one";
    std::string const router_two_name_ = "router_two";
    std::string const ecu_two_server_name_ = "ecu_two_server";
    std::string const ecu_one_client_name_ = "ecu_one_client";

    std::map<std::string, app*> apps_;
    std::set<std::string> env_vars_;
};

TEST_F(test_boardnet_helper, test_boardnet_service_availability) {
    // start 1st daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(router_one && router_one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // start 2nd daemon on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(router_two && router_two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    router_one->request_service(service_instance_);
    router_two->offer(boardnet_interface_);

    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    router_two->stop_offer(service_instance_);
    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));
}

TEST_F(test_boardnet_helper, test_boardnet_initial_event) {
    // start 1st daemon and client on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(router_two && router_two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    auto ecu_two_server = start_application(ecu_two_server_name_, "ecu_two.json");

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(router_one && router_one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

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

TEST_F(test_field_routing, server_router_router_client) {
    // start 1st daemon and client on 127.0.0.2 interface
    auto router_two = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(router_two && router_two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    auto ecu_two_server = start_application(ecu_two_server_name_, "ecu_two.json");

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(router_one && router_one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    auto ecu_one_client = start_application(ecu_one_client_name_, "ecu_one.json");

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
    ASSERT_TRUE(router_two && router_two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    auto ecu_two_server = start_application(ecu_two_server_name_, "ecu_two.json");

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(router_one && router_one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

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
    ASSERT_TRUE(router_two && router_two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(router_one && router_one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
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
    ASSERT_TRUE(router_two && router_two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // start 2nd daemon on 127.0.0.1 interface
    auto router_one = start_application(router_one_name, "ecu_one.json");
    ASSERT_TRUE(router_one && router_one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

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

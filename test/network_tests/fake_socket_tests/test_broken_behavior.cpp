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
static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct test_broken_behavior : public base_fake_socket_fixture {
    test_broken_behavior() {
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

    service_instance service_instance_{0x3344, 0x1};
    event_ids offered_event_{service_instance_, 0x8002, 0x1};
    event_ids offered_field_{service_instance_, 0x8003, 0x6};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
};

TEST_F(test_broken_behavior, available_versions) {
    /// check that client receives ON_AVAILABLE even for non-matching major version
    /// unfortunate behavior, but it is sufficiently visible that we should not break it (until next major lib version)

    start_router();
    start_server();

    // want a specific service for this test
    service_instance si{static_cast<service_t>(service_instance_.service_ + 1), service_instance_.instance_};
    major_version_t major = 0x2; // with a specific major version
    // .. offered by the `server`
    server_->get_application()->offer_service(si.service_, si.instance_, major);

    // start a few clients
    std::string const client_one{"client-one"};
    std::string const client_two{"client-two"};
    std::string const client_three{"client-three"};

    create_app(client_one);
    create_app(client_two);
    create_app(client_three);
    auto* one = start_client(client_one);
    auto* two = start_client(client_two);
    auto* three = start_client(client_three);
    ASSERT_TRUE(one->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    ASSERT_TRUE(two->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    ASSERT_TRUE(three->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // clients want different major versions
    one->get_application()->request_service(si.service_, si.instance_, major - 1); // wants smaller version
    two->get_application()->request_service(si.service_, si.instance_, major); // wants offered version
    three->get_application()->request_service(si.service_, si.instance_, major + 1); // wants bigger version

    // and here is the broken behavior: all clients see availability...!
    EXPECT_TRUE(one->availability_record_.wait_for_last(service_availability::available(si)));
    EXPECT_TRUE(two->availability_record_.wait_for_last(service_availability::available(si)));
    EXPECT_TRUE(three->availability_record_.wait_for_last(service_availability::available(si)));
}
}

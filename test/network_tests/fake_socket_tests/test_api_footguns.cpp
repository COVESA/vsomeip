// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/**
 * This file contains API behavior that we need to preserve, but that is really, really, really, really, really questionable
 */

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/service_state.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

using namespace vsomeip_v3;
using namespace vsomeip_v3::testing;

static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct test_api_footgun : public base_fake_socket_fixture {
    test_api_footgun() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(server_name_);
        create_app(client_name_);
    }

    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_TRUE(routingmanagerd_ && await_connectable(routingmanager_name_));
    }

    void start_server() {
        server_ = start_client(server_name_);
        ASSERT_TRUE(server_ && server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer_event(service_instance_, offered_event_.to_event_spec());
        server_->offer_field(service_instance_, offered_field_.to_event_spec());
        server_->offer(service_instance_);
    }

    void start_client_app() {
        client_ = start_client(client_name_);
        ASSERT_TRUE(client_ && client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    void start_apps() {
        start_router();
        start_server();
        start_client_app();
    }

    service_instance service_instance_{.service_ = 0x3344, .instance_ = 0x1, .major_ = 0x1};
    event_ids offered_event_{service_instance_, 0x8002, 0x1};
    event_ids offered_field_{service_instance_, 0x8003, 0x6};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
};

TEST_F(test_api_footgun, notify_one_field) {
    /// what happens if a server does `notify_one` of a field?
    /// we want to guarantee that.. it does only send an update to the specified client, but it is a footgun, because it is a field, so
    /// `notify_one` also updates the initial value for new subscribing clients

    start_apps();

    server_->send_event(offered_field_, {0x01}); // initial value

    // we need several clients
    create_app("client2");
    create_app("client3");
    app* client2 = start_client("client2");
    app* client3 = start_client("client3");
    ASSERT_TRUE(client2 && client2->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    ASSERT_TRUE(client3 && client3->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    // only 1+2 subscribe to the field
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);
    client2->request_service(service_instance_);
    client2->subscribe_field(offered_field_);

    // and see it, of course
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    ASSERT_TRUE(client2->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client2->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    // as well as the initial value
    message_checker initial_value{std::nullopt, service_instance_, offered_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                                  std::vector<uint8_t>{0x01}};
    EXPECT_TRUE(client_->message_record_.wait_for(initial_value));
    EXPECT_TRUE(client2->message_record_.wait_for(initial_value));

    // now, the server does a notify_one to client
    server_->send_event_one(offered_field_, {0x02}, client_->get_client_id());
    // client should see the update
    message_checker field_update{std::nullopt, service_instance_, offered_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                                 std::vector<uint8_t>{0x02}};
    EXPECT_TRUE(client_->message_record_.wait_for(field_update));
    // client2 should NOT see the update (because `notify_one`)
    EXPECT_FALSE(client2->message_record_.wait_for(field_update, std::chrono::milliseconds(50)));

    // new subscribing client sees also see the update, because of the initial value
    client3->request_service(service_instance_);
    client3->subscribe_field(offered_field_);

    ASSERT_TRUE(client3->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client3->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    EXPECT_TRUE(client3->message_record_.wait_for(field_update));
    EXPECT_FALSE(client2->message_record_.wait_for(field_update, std::chrono::milliseconds(50)));
}

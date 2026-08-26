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
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

using namespace vsomeip_v3;
using namespace vsomeip_v3::testing;

static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct test_api_helper : public base_fake_socket_fixture {
    test_api_helper() {
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

TEST_F(test_api_helper, subscribe_after_release) {
    /// what happens if a client releases a service... and then subscribes?

    start_apps();

    client_->request_service(service_instance_);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    client_->release_service(service_instance_);
    client_->subscribe_event(offered_event_);

    // we should *NOT*! see a successful subscription, we do not care about the service anymore
    ASSERT_FALSE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_),
                                                             std::chrono::milliseconds(50)));

    client_->subscription_record_.clear();
    client_->request_service(service_instance_);
    // only now should we see a successful subscription
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));
}

TEST_F(test_api_helper, availability_after_release) {
    /// check what happens if a client releases a service and _then_ said service goes unavailable

    start_apps();

    client_->request_service(service_instance_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // client does not care anymore
    // (but keeps availability handler; could have been one for ANY_SERVICE!)
    client_->release_service(service_instance_);

    // sigh, need to wait for routing to process the release
    // TODO: do better, perhaps some sort of client -> routing ping/pong?
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // then server stops offering..
    server_->stop_offer(service_instance_);
    // client does not care, client should *NOT* receive the unavailable!
    ASSERT_FALSE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_),
                                                             std::chrono::milliseconds(50)));

    client_->availability_record_.clear();
    server_->offer(service_instance_);
    // client still does not care
    ASSERT_FALSE(
            client_->availability_record_.wait_for_last(service_availability::available(service_instance_), std::chrono::milliseconds(50)));
}

TEST_F(test_api_helper, subscribe_service_toggle) {
    /// check whether a client receives the new values for a newly offered_service

    start_apps();

    server_->send_event(offered_field_, {0x01});

    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    ASSERT_TRUE(client_->wait_for_messages({{0x01}}));

    client_->unsubscribe(offered_field_);
    client_->release_service(service_instance_);

    client_->availability_record_.clear();
    client_->subscription_record_.clear();

    // then subscribe again
    client_->request_service(service_instance_);
    client_->subscribe_field(offered_field_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    ASSERT_TRUE(client_->wait_for_messages({{0x01}, {0x01}}));

    // then stop offer/offer service again
    // and send a different payload
    server_->stop_offer(service_instance_);
    server_->stop_offer_event(offered_field_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    client_->availability_record_.clear();
    client_->subscription_record_.clear();

    // now the order is a bit funny, be careful here
    // offer event first.. then send the payload
    server_->offer_field(service_instance_, offered_field_.to_event_spec());
    server_->send_event(offered_field_, {0x02});
    server_->offer(service_instance_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    ASSERT_TRUE(client_->wait_for_messages({{0x01}, {0x01}, {0x02}})) << client_->message_record_;
}

TEST_F(test_api_helper, interface_version_when_setting_the_initial_value_after_service_offering) {
    start_router();
    start_client_app();
    server_ = start_client(server_name_);
    ASSERT_TRUE(server_ && server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    server_->offer_field(service_instance_, offered_field_.to_event_spec());
    server_->offer(service_instance_);
    server_->send_event(offered_field_, {0x01});

    client_->request_service(service_instance_);
    client_->subscribe_event(offered_field_);

    auto const contains_field = [&](auto const& _record) {
        return std::any_of(_record.begin(), _record.end(), [&](auto const& _message) {
            return _message.service_instance_.service_ == service_instance_.service_
                    && _message.service_instance_.instance_ == service_instance_.instance_
                    && _message.service_instance_.major_ == service_instance_.major_
                    && _message.message_type_ == vsomeip::message_type_e::MT_NOTIFICATION
                    && _message.payload_ == std::vector<unsigned char>{0x01};
        });
    };

    EXPECT_TRUE(client_->message_record_.wait_for(contains_field));
}

TEST_F(test_api_helper, interface_version_when_setting_the_initial_value_before_service_offering) {
    start_router();
    start_client_app();
    server_ = start_client(server_name_);
    ASSERT_TRUE(server_ && server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    server_->offer_field(service_instance_, offered_field_.to_event_spec());
    server_->send_event(offered_field_, {0x01});
    server_->offer(service_instance_);

    client_->request_service(service_instance_);
    client_->subscribe_event(offered_field_);

    auto const contains_field = [&](auto const& _record) {
        return std::any_of(_record.begin(), _record.end(), [&](auto const& _message) {
            return _message.service_instance_.service_ == service_instance_.service_
                    && _message.service_instance_.instance_ == service_instance_.instance_
                    && _message.service_instance_.major_ == service_instance_.major_
                    && _message.message_type_ == vsomeip::message_type_e::MT_NOTIFICATION
                    && _message.payload_ == std::vector<unsigned char>{0x01};
        });
    };

    EXPECT_TRUE(client_->message_record_.wait_for(contains_field));
}

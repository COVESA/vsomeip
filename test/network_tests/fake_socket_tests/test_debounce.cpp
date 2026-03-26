// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/fake_tcp_socket_handle.hpp"
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

struct test_debounce_helper : public base_fake_socket_fixture {
    test_debounce_helper() {
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
        server_->offer(service_instance_);
        server_->offer_event(offered_event_);
        server_->offer_field(offered_field_);
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

    [[nodiscard]] bool subscribe_to_event(debounce_filter_t const& filter) {
        client_->request_service(service_instance_);
        client_->subscribe_event_debounce(offered_event_, filter);
        return client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_));
    }
    [[nodiscard]] bool subscribe_to_field(debounce_filter_t const& filter) {
        client_->request_service(service_instance_);
        client_->subscribe_field_debounce(offered_field_, filter);
        return client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_));
    }

    service_instance service_instance_{0x3344, 0x1};
    service_instance service_instance_two_{0x3345, 0x1};
    event_ids offered_event_{service_instance_, 0x8002, 0x1};
    event_ids offered_field_{service_instance_, 0x8003, 0x6};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
};

TEST_F(test_debounce_helper, debounce_on_change) {
    /// `subscribe_with_debounce`, server offers, client subscribes such that it only waits changes

    start_apps();

    debounce_filter_t filter;
    filter.on_change_ = true;
    filter.interval_ = -1;

    ASSERT_TRUE(subscribe_to_field(filter));
    // can't receive a notification the server did not send!
    ASSERT_TRUE(client_->wait_for_messages({}));

    server_->send_event(offered_field_, {0x00});
    ASSERT_TRUE(client_->wait_for_messages({{0x00}}));

    // client only sees changes
    server_->send_event(offered_field_, {0x00});
    server_->send_event(offered_field_, {0x01});
    ASSERT_TRUE(client_->wait_for_messages({{0x00}, {0x01}}));

    server_->send_event(offered_field_, {0x00});
    server_->send_event(offered_field_, {0x02, 0x02});
    ASSERT_TRUE(client_->wait_for_messages({{0x00}, {0x01}, {0x00}, {0x02, 0x02}}));
}

TEST_F(test_debounce_helper, router_debounce_on_change) {
    /// sanity test for `subscribe_with_debounce` with configuration that only waits changes
    /// router is the one offering the service, so that we check the *router* debouncing behavior

    start_router();
    start_client_app();

    // router offers several events, which happen to have the same event-id
    event_ids offered_field{service_instance_, 0x8001, 0x1};
    event_ids offered_field_two{service_instance_two_, 0x8001, 0x02};
    routingmanagerd_->offer(service_instance_);
    routingmanagerd_->offer(service_instance_two_);
    routingmanagerd_->offer_field(offered_field);
    routingmanagerd_->offer_event(offered_field_two);

    routingmanagerd_->send_event(offered_field, {0x00});
    routingmanagerd_->send_event(offered_field_two, {0xF0});

    // client only requests *one* of the services
    // and does a subscribe_with_debounce, with parameters that *ONLY* want changes
    debounce_filter_t filter;
    filter.on_change_ = true;
    filter.interval_ = -1;
    filter.send_current_value_after_ = true;

    client_->request_service(service_instance_);
    client_->subscribe_field_debounce(offered_field, filter);

    // ensure client only sees first service available
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field)));

    ASSERT_TRUE(client_->wait_for_messages({{0x00}}));
    routingmanagerd_->send_event(offered_field, {0x00});
    routingmanagerd_->send_event(offered_field, {0x01});
    ASSERT_TRUE(client_->wait_for_messages({{0x00}, {0x01}}));
    routingmanagerd_->send_event(offered_field, {0x01});
    routingmanagerd_->send_event(offered_field_two, {0xF1});
    routingmanagerd_->send_event(offered_field, {0x01});
    routingmanagerd_->send_event(offered_field_two, {0xF1});
    routingmanagerd_->send_event(offered_field, {0x01});

    // give the other notifications a chance to do something..
    // TODO: not good..
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    routingmanagerd_->send_event(offered_field, {0x02});
    ASSERT_TRUE(client_->wait_for_messages({{0x00}, {0x01}, {0x02}}));
}

TEST_F(test_debounce_helper, debounce_default) {
    /// `subscribe_with_debounce`, check default values

    start_apps();

    // default `filter`, so.. essentially the same as a normal subscribe
    // no "on-change", no interval, nothing
    debounce_filter_t filter;

    ASSERT_TRUE(subscribe_to_event(filter));
    ASSERT_TRUE(client_->wait_for_messages({}));

    server_->send_event(offered_event_, {0x00});
    server_->send_event(offered_event_, {0x00});
    server_->send_event(offered_event_, {0x01, 0x01});
    server_->send_event(offered_event_, {0x02});
    // currently receive nothing
    // TODO: FIXME!
    ASSERT_FALSE(client_->wait_for_messages({{0x00}, {0x00}, {0x01, 0x01}, {0x02}}, std::chrono::milliseconds(10)));
    ASSERT_TRUE(client_->wait_for_messages({}));
}

TEST_F(test_debounce_helper, debounce_interval) {
    /// `subscribe_with_debounce`, check interval behavior

    start_apps();

    debounce_filter_t filter;
    filter.interval_ = 1000; // only accept messages every sec

    ASSERT_TRUE(subscribe_to_event(filter));
    ASSERT_TRUE(client_->wait_for_messages({}));

    // immediately send 3 events
    server_->send_event(offered_event_, {0x00});
    server_->send_event(offered_event_, {0x01});
    server_->send_event(offered_event_, {0x02});

    // definitely shouldn't ever, ever see the 3 events
    // TODO: not good, this is essentially a 10ms sleep..
    ASSERT_FALSE(client_->wait_for_messages({{0x00}, {0x01}, {0x02}}, std::chrono::milliseconds(10)));
    ASSERT_TRUE(client_->wait_for_messages({{0x00}})) << client_->message_record_;
}

TEST_F(test_debounce_helper, debounce_on_change_interval) {
    /// `subscribe_with_debounce`, check on_change + interval
    /// where the expectation is that changed values will bypass interval

    start_apps();

    debounce_filter_t filter_event;
    filter_event.on_change_ = true;
    filter_event.interval_ = 1000; // only accept messages every sec

    debounce_filter_t filter_field;
    filter_field.on_change_resets_interval_ = true; // changes `on_change_resets_interval_` instead of `on_change`!
    filter_field.interval_ = 1000; // only accept messages every sec

    // NOTE: careful, do not want to do `request_service` twice
    client_->request_service(service_instance_);
    client_->subscribe_event_debounce(offered_event_, filter_event);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));
    client_->subscribe_field_debounce(offered_field_, filter_field);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));
    ASSERT_TRUE(client_->wait_for_messages({}));

    // immediately send several events
    for (std::vector<uint8_t> const& payload : std::vector<std::vector<uint8_t>>{{0x00}, {0x01}, {0x01}, {0x02}}) {
        server_->send_event(offered_event_, payload);
        server_->send_event(offered_field_, payload);
    }

    // only `on_change` bypasses interval, not `on_change_resets_interval_`
    // hence why 0x00 is seen twice
    ASSERT_TRUE(client_->wait_for_messages({{0x00}, {0x00}, {0x01}, {0x02}})) << client_->message_record_;
    // and of course, last notification is from event (which uses `on_change`)
    ASSERT_TRUE(client_->message_record_.last()->method_ = offered_event_.event_id_) << client_->message_record_;
}

TEST_F(test_debounce_helper, debounce_send_after) {
    /// `subscribe_with_debounce`, check `send_current_value_after_` behavior

    start_apps();

    debounce_filter_t filter;
    filter.interval_ = 100;
    filter.send_current_value_after_ = true;

    ASSERT_TRUE(subscribe_to_event(filter));
    ASSERT_TRUE(client_->wait_for_messages({}));

    // immediately send 3 messages
    server_->send_event(offered_event_, {0x00});
    server_->send_event(offered_event_, {0x01});
    server_->send_event(offered_event_, {0x02});

    // will only receive first, because `send_current_value_after_` is broken and does absolutely nothing
    ASSERT_FALSE(client_->wait_for_messages({{0x00, 0x02}}, std::chrono::milliseconds(150)));
    ASSERT_TRUE(client_->wait_for_messages({{0x00}})) << client_->message_record_;
}

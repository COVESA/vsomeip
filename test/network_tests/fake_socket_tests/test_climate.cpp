// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/service_state.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

using namespace vsomeip_v3;
using namespace vsomeip_v3::testing;

static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct test_climate : public base_fake_socket_fixture {
    test_climate() {
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
        server_->offer_field(field_.si_, field_.to_event_spec());
    }

    void start_client_app() {
        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        client_->subscribe_field(field_);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        client_->request_service(service_instance_);
        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    }

    void start_apps() {
        start_router();
        start_server();
        start_client_app();
    }

    void client_release() {
        client_->unsubscribe(field_); // -> vsomeip::application::unsubscribe() + release_event()
        client_->release_service(service_instance_);
    }

    void client_reacquire() {
        client_->request_service(service_instance_);
        client_->subscribe_field(field_); // -> request_event() + subscribe()
    }

    void expect_notification(std::vector<std::vector<unsigned char>> const& _cumulative_expected_payloads) {
        ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_)));
        client_->subscription_record_.clear();
        server_->send_event(field_, field_payload_);
        EXPECT_TRUE(client_->wait_for_messages(_cumulative_expected_payloads));
    }

    service_instance service_instance_{0x1234, 0x5678};
    event_ids field_{service_instance_, 0x8778, 0x4465, vsomeip::reliability_type_e::RT_UNRELIABLE};
    std::vector<unsigned char> const field_payload_{0x00, 0x01, 0x02, 0x03, 0x04};

    app* routingmanagerd_{};
    app* server_{};
    app* client_{};
};

TEST_F(test_climate, notification_delivery_survives_sequential_availability_flip) {
    start_apps();
    expect_notification({field_payload_});

    client_->availability_record_.clear();

    client_release();
    client_reacquire();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    expect_notification({field_payload_, field_payload_});

    client_->availability_record_.clear();

    server_->stop_offer(service_instance_);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    client_->release_service(service_instance_);
    clear_command_record(client_name_, routingmanager_name_);
    client_->request_service(service_instance_);
    ASSERT_TRUE(wait_for_command(client_name_, routingmanager_name_, protocol::id_e::REQUEST_SERVICE_ID, socket_role::server));

    // have to wait for request to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    server_->offer(service_instance_);
    server_->stop_offer(service_instance_);
    server_->offer(service_instance_);
    std::vector<service_availability> const expected_availability = {
            service_availability::available(service_instance_),
            service_availability::unavailable(service_instance_),
            service_availability::available(service_instance_),
    };
    ASSERT_TRUE(client_->availability_record_.wait_for_sequence(expected_availability)) << client_->availability_record_.to_string();

    client_release();
    client_reacquire();
    expect_notification({field_payload_, field_payload_, field_payload_});
}

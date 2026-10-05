// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_gate.hpp"
#include "helpers/command_record.hpp"
#include "helpers/ecu_config.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/service_state.hpp"
#include "helpers/someip_gate.hpp"

#include "sample_configurations.hpp"

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/vsomeip.hpp>

#include <gtest/gtest.h>

#include <boost/asio/error.hpp>

#include <cstdint>
#include <cstdlib>
#include <utility>

namespace vsomeip_v3::testing {

struct test_multiversion_support_single_ecu : public base_fake_socket_fixture {
    void start_apps() {
        ecu_.add_app(client_v1_name_);
        ecu_.add_app(client_v2_name_);
        ecu_.add_app(server_v1_name_);
        ecu_.add_app(server_v2_name_);
        ecu_.add_app(prope_client_name_);
        ecu_.add_app(extra_app_one_name_);
        ecu_.add_app(extra_app_two_name_);

        ecu_.prepare();
        ecu_.start_apps();

        server_v1_ = ecu_.apps_[server_v1_name_];
        server_v2_ = ecu_.apps_[server_v2_name_];
        client_v1_ = ecu_.apps_[client_v1_name_];
        client_v2_ = ecu_.apps_[client_v2_name_];
        prope_client_ = ecu_.apps_[prope_client_name_];
        extra_app_one_ = ecu_.apps_[extra_app_one_name_];
        extra_app_two_ = ecu_.apps_[extra_app_two_name_];
    }

    std::string const prope_client_name_{"prope_client"};
    std::string const client_v1_name_{"client_v1"};
    std::string const client_v2_name_{"client_v2"};
    std::string const server_v1_name_{"server_v1"};
    std::string const server_v2_name_{"server_v2"};
    std::string const extra_app_one_name_{"extra_app_one"};
    std::string const extra_app_two_name_{"extra_app_two"};

    interface interface_v1_{service_instance{.service_ = 0x1000, .instance_ = 0x1, .major_ = 0x1, .minor_ = 0x0},
                            {},
                            {event_spec{0x8001, {0x8001}, vsomeip::reliability_type_e::RT_UNRELIABLE}}};
    interface interface_v2_{service_instance{.service_ = 0x1000, .instance_ = 0x1, .major_ = 0x2, .minor_ = 0x0},
                            {},
                            {event_spec{0x8002, {0x8002}, vsomeip::reliability_type_e::RT_UNRELIABLE}}};

    ecu_setup ecu_{"single-ecu", ecu_config{boardnet::ecu_one_config}, *socket_manager_};

    app* server_v1_;
    app* server_v2_;
    app* prope_client_;
    app* client_v1_;
    app* client_v2_;
    app* extra_app_one_;
    app* extra_app_two_;
};

TEST_F(test_multiversion_support_single_ecu, when_v1_is_offered_first_then_only_client_v1_receives_availability) {
    start_apps();

    server_v1_->offer(interface_v1_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));
}
TEST_F(test_multiversion_support_single_ecu, when_v1_is_offered_last_then_only_client_v1_receives_availability) {
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    server_v1_->offer(interface_v1_.instance_);

    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));
}
TEST_F(test_multiversion_support_single_ecu, when_v2_is_offered_first_then_only_client_v2_receives_availability) {
    start_apps();

    server_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_),
                                                                std::chrono::milliseconds(10)));
}
TEST_F(test_multiversion_support_single_ecu, when_v2_is_offered_last_then_only_client_v2_receives_availability) {
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    server_v2_->offer(interface_v2_.instance_);

    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_),
                                                                std::chrono::milliseconds(10)));
}

TEST_F(test_multiversion_support_single_ecu, when_v1_and_v2_are_offered_first_both_become_available) {
    start_apps();

    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
}
TEST_F(test_multiversion_support_single_ecu, when_v1_and_v2_are_offered_last_both_become_available) {
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);

    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
}

TEST_F(test_multiversion_support_single_ecu, v1_and_v2_offered_when_v1_is_withdrawn_only_v2_is_available) {
    start_apps();
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);

    prope_client_->request_service(interface_v1_.instance_);
    ASSERT_TRUE(prope_client_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    server_v1_->stop_offer(interface_v1_.instance_);
    ASSERT_TRUE(prope_client_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_)));

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_),
                                                                std::chrono::milliseconds(10)));
}

TEST_F(test_multiversion_support_single_ecu, v1_and_v2_offered_when_v2_is_withdrawn_only_v1_is_available) {
    start_apps();
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);

    prope_client_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(prope_client_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    server_v2_->stop_offer(interface_v2_.instance_);
    ASSERT_TRUE(prope_client_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_)));

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));
}

TEST_F(test_multiversion_support_single_ecu, when_v1_withdraws_only_v1_becomes_unavailable) {
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    server_v1_->stop_offer(interface_v1_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_)));
    ASSERT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));

    server_v1_->offer(interface_v1_.instance_);
    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));
}
TEST_F(test_multiversion_support_single_ecu, when_v2_withdraws_only_v2_becomes_unavailable) {
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    server_v2_->stop_offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_)));
    ASSERT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_),
                                                                std::chrono::milliseconds(10)));

    server_v2_->offer(interface_v2_.instance_);
    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_),
                                                                std::chrono::milliseconds(10)));
}

TEST_F(test_multiversion_support_single_ecu, when_v1_client_connection_breaks_then_v1_service_is_pinged) {
    std::shared_ptr<command_gate> gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_v1_name_, ecu_.router_name_, socket_role::client, gate->get_data_pipe()));
    start_apps();

    client_v1_->subscribe(interface_v1_);
    client_v2_->request_service(interface_v2_.instance_);
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v1_->subscription_record_.wait_for_last(
            event_subscription::successfully_subscribed_to({interface_v1_.instance_, interface_v1_.fields_[0]})));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    gate->block_at(vsomeip_v3::protocol::id_e::PING_ID);
    ASSERT_TRUE(disconnect(client_v1_name_, boost::asio::error::timed_out, server_v1_name_, boost::asio::error::connection_reset));

    EXPECT_TRUE(gate->wait_for_blocked());
}
TEST_F(test_multiversion_support_single_ecu, when_v1_client_connection_breaks_then_v1_service_returns_available) {
    start_apps();

    client_v1_->subscribe(interface_v1_);
    server_v1_->offer(interface_v1_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v1_->subscription_record_.wait_for_last(
            event_subscription::successfully_subscribed_to({interface_v1_.instance_, interface_v1_.fields_[0]})));
    client_v1_->availability_record_.clear();

    ASSERT_TRUE(disconnect(client_v1_name_, boost::asio::error::timed_out, server_v1_name_, boost::asio::error::connection_reset));

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    // the availability handler can not tell us what version is available, because we don't receive it. But if the "wrong" version is noted
    // in the router, we would be told about a different version turning available
    client_v1_->availability_record_.clear();
    server_v1_->stop_offer(interface_v1_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_)));

    server_v2_->offer(interface_v2_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));
}
TEST_F(test_multiversion_support_single_ecu, when_v2_client_connection_breaks_then_v2_service_is_pinged) {
    std::shared_ptr<command_gate> gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_v2_name_, ecu_.router_name_, socket_role::client, gate->get_data_pipe()));
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->subscribe(interface_v2_);
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    ASSERT_TRUE(client_v2_->subscription_record_.wait_for_last(
            event_subscription::successfully_subscribed_to({interface_v2_.instance_, interface_v2_.fields_[0]})));
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    gate->block_at(vsomeip_v3::protocol::id_e::PING_ID);
    ASSERT_TRUE(disconnect(client_v2_name_, boost::asio::error::timed_out, server_v2_name_, boost::asio::error::connection_reset));

    EXPECT_TRUE(gate->wait_for_blocked());
}
TEST_F(test_multiversion_support_single_ecu, when_v2_client_connection_breaks_then_v2_service_returns_available) {
    start_apps();

    client_v2_->subscribe(interface_v2_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    ASSERT_TRUE(client_v2_->subscription_record_.wait_for_last(
            event_subscription::successfully_subscribed_to({interface_v2_.instance_, interface_v2_.fields_[0]})));
    client_v2_->availability_record_.clear();

    ASSERT_TRUE(disconnect(client_v2_name_, boost::asio::error::timed_out, server_v2_name_, boost::asio::error::connection_reset));

    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    // the availability handler can not tell us what version is available, because we don't receive it. But if the "wrong" version is noted
    // in the router, we would be told about a different version turning available
    client_v2_->availability_record_.clear();
    server_v2_->stop_offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_)));

    server_v1_->offer(interface_v1_.instance_);
    client_v1_->request_service(interface_v1_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_),
                                                                std::chrono::milliseconds(10)));
}

TEST_F(test_multiversion_support_single_ecu, when_another_service_tries_to_also_offer_v1_then_v1_is_pinged) {
    std::shared_ptr<command_gate> gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_v1_name_, ecu_.router_name_, socket_role::client, gate->get_data_pipe()));
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    gate->block_at(vsomeip_v3::protocol::id_e::PING_ID);
    prope_client_->offer(interface_v1_.instance_);

    EXPECT_TRUE(gate->wait_for_blocked());
}
TEST_F(test_multiversion_support_single_ecu, when_another_service_tries_to_also_offer_v2_then_v2_is_pinged) {
    std::shared_ptr<command_gate> gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_v2_name_, ecu_.router_name_, socket_role::client, gate->get_data_pipe()));
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    gate->block_at(vsomeip_v3::protocol::id_e::PING_ID);
    prope_client_->offer(interface_v2_.instance_);

    EXPECT_TRUE(gate->wait_for_blocked());
}

TEST_F(test_multiversion_support_single_ecu, when_v1_and_v2_are_reofferd_both_incumbents_are_pinged) {
    std::shared_ptr<command_gate> gate_v1 = command_gate::create();
    std::shared_ptr<command_gate> gate_v2 = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_v1_name_, ecu_.router_name_, socket_role::client, gate_v1->get_data_pipe()));
    ASSERT_TRUE(setup_data_pipe(server_v2_name_, ecu_.router_name_, socket_role::client, gate_v2->get_data_pipe()));
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    server_v1_->offer(interface_v1_.instance_);
    server_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    gate_v1->block_at(vsomeip_v3::protocol::id_e::PING_ID);
    gate_v2->block_at(vsomeip_v3::protocol::id_e::PING_ID);

    // Note by firing these two in a row in some test executions the order is swapped at the router side
    // this is "by-design", because if the test would fail orderly dependent we would have a flaky test
    extra_app_one_->offer(interface_v1_.instance_);
    extra_app_two_->offer(interface_v2_.instance_);

    EXPECT_TRUE(gate_v1->wait_for_blocked());
    EXPECT_TRUE(gate_v2->wait_for_blocked());
}

struct test_multiversion_support_limited_support : public base_fake_socket_fixture {
    void start_apps() {
        ecu_.add_app(client_v1_name_);
        ecu_.add_app(client_v2_name_);
        ecu_.add_app(server_v1_name_);
        ecu_.add_app(server_v2_name_);

        ecu_.prepare();
        ecu_.start_apps();

        server_v1_ = ecu_.apps_[server_v1_name_];
        server_v2_ = ecu_.apps_[server_v2_name_];
        client_v1_ = ecu_.apps_[client_v1_name_];
        client_v2_ = ecu_.apps_[client_v2_name_];
    }

    std::string const client_v1_name_{"client_v1"};
    std::string const client_v2_name_{"client_v2"};
    std::string const server_v1_name_{"server_v1"};
    std::string const server_v2_name_{"server_v2"};

    interface interface_v1_{service_instance{.service_ = 0x1000, .instance_ = 0x1, .major_ = 0x1, .minor_ = 0x0},
                            {},
                            {event_spec{0x8001, {0x8001}, vsomeip::reliability_type_e::RT_UNRELIABLE}}};
    interface interface_v2_{service_instance{.service_ = 0x1000, .instance_ = 0x1, .major_ = 0x2, .minor_ = 0x0},
                            {},
                            {event_spec{0x8002, {0x8002}, vsomeip::reliability_type_e::RT_UNRELIABLE}}};

    ecu_setup ecu_{"single-ecu", ecu_config{boardnet::ecu_one_config}.add_interface({interface_v1_, interface_v2_}), *socket_manager_};

    app* server_v1_;
    app* server_v2_;
    app* client_v1_;
    app* client_v2_;
};

TEST_F(test_multiversion_support_limited_support, interfaces_for_boardnet_are_not_supported_with_multiple_versions) {
    // This is a test that is expected to be removed once we support multiple versions for the boardnet.
    // But as long as we don't do this, we should reject such attempts
    start_apps();

    // first offering is fine
    server_v1_->offer(interface_v1_.instance_);
    client_v1_->request_service(interface_v1_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    // second one is going to be rejected
    server_v2_->offer(interface_v2_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));
}
}

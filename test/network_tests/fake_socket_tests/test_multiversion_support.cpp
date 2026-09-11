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

#include "common/timeout_scale.hpp"

#include <chrono>
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

/**
 * Three-ECU topology isolating the *consuming* ECU's ability to independently track two
 * remote offers of the same (service, instance) discovered from two different hosts:
 *
 *   - provider_v1_ecu_ offers major v1 with real boardnet ports (never offers v2 itself).
 *   - provider_v2_ecu_ offers major v2 with real boardnet ports (never offers v1 itself).
 *   - consumer_ecu_ only ever requests/consumes, never offers.
 *
 * Neither offering ECU offers more than one major itself, so this does not exercise a
 * single host announcing multiple majors of the same (service, instance) via SD (a
 * separate, not yet supported concern) -- it purely tests the consumer's bookkeeping of
 * two independently-discovered remote offers.
 */
struct test_multiversion_support_aggregate_consumption : public base_fake_socket_fixture {
    void start_apps() {
        provider_v1_ecu_.add_app(provider_v1_name_);
        provider_v2_ecu_.add_app(provider_v2_name_);
        consumer_ecu_.add_app(client_v1_name_);
        consumer_ecu_.add_app(client_v2_name_);
        consumer_ecu_.add_app("client_any");

        provider_v1_ecu_.prepare();
        provider_v2_ecu_.prepare();
        consumer_ecu_.prepare();

        provider_v1_ecu_.start_apps();
        provider_v2_ecu_.start_apps();
        consumer_ecu_.start_apps();

        provider_v1_ = provider_v1_ecu_.apps_[provider_v1_name_];
        provider_v2_ = provider_v2_ecu_.apps_[provider_v2_name_];
        client_v1_ = consumer_ecu_.apps_[client_v1_name_];
        client_v2_ = consumer_ecu_.apps_[client_v2_name_];
        client_any_ = consumer_ecu_.apps_["client_any"];

        ASSERT_TRUE(provider_v1_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        ASSERT_TRUE(provider_v2_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        ASSERT_TRUE(client_v1_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        ASSERT_TRUE(client_v2_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        ASSERT_TRUE(client_any_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    std::string const provider_v1_name_{"provider_v1"};
    std::string const provider_v2_name_{"provider_v2"};
    std::string const client_v1_name_{"client_v1"};
    std::string const client_v2_name_{"client_v2"};

    interface interface_v1_{service_instance{.service_ = 0x1000, .instance_ = 0x1, .major_ = 0x1, .minor_ = 0x0},
                            {},
                            {event_spec{0x8001, {0x8001}, vsomeip::reliability_type_e::RT_RELIABLE}}};
    interface interface_v2_{service_instance{.service_ = 0x1000, .instance_ = 0x1, .major_ = 0x2, .minor_ = 0x0},
                            {},
                            {event_spec{0x8002, {0x8002}, vsomeip::reliability_type_e::RT_RELIABLE}}};

    vsomeip::method_t const method_{0x1111};
    request request_v1_{interface_v1_.instance_, method_, vsomeip::message_type_e::MT_REQUEST, false, {}};
    request request_v2_{interface_v2_.instance_, method_, vsomeip::message_type_e::MT_REQUEST, false, {}};

    // The SD multicast group address (VSOMEIP_SD_DEFAULT_MULTICAST); none of the ecu_configs used
    // here override it. FindService/OfferService sent to this address still go out as an ordinary
    // sendto() on the sender's own unicast SD socket -- joining the group is only needed to
    // *receive* multicast traffic, so gating on this as the receiver party does not depend on
    // whether the multicast join has completed.
    boost::asio::ip::udp::endpoint const sd_multicast_ep_{boost::asio::ip::make_address("224.224.224.0"), 30490};

    // provider_v1_ecu_/provider_v2_ecu_ each offer only their own major -- reusing the
    // client-only boardnet::ecu_one_config/ecu_two_config bases (stripped of the unrelated
    // service_3344 baked into ecu_two_config) purely for their distinct unicast IP/router
    // name/network; consumer_ecu_ never offers anything.
    //
    // Each ECU gets its own diagnosis address so their auto-assigned client ids don't collide --
    // utility::request_client_id() reads diagnosis/mask fresh from _config per call (no longer
    // function-local-static-cached), so this now gives each ECU its own non-overlapping range.
    ecu_setup provider_v1_ecu_{"provider_v1", ecu_config{boardnet::ecu_one_config}.add_interface({interface_v1_}), *socket_manager_};
    ecu_setup provider_v2_ecu_{"provider_v2", ecu_config{boardnet::ecu_two_config}.add_interface({interface_v2_}), *socket_manager_};
    // consumer_ecu_ never offers anything.
    ecu_setup consumer_ecu_{"consumer", boardnet::ecu_three_config, *socket_manager_};

    app* provider_v1_;
    app* provider_v2_;
    app* client_v1_;
    app* client_v2_;
    app* client_any_;
};

TEST_F(test_multiversion_support_aggregate_consumption, both_majors_offered_from_different_ecus_become_independently_available) {
    start_apps();

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_)));

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
}

TEST_F(test_multiversion_support_aggregate_consumption, releasing_v2_then_any_major_eventually_drops_v2s_connection_but_never_v1s) {
    start_apps();

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    service_instance const any_major{.service_ = interface_v1_.instance_.service_, .instance_ = interface_v1_.instance_.instance_};
    client_any_->request_service(any_major);

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    ASSERT_TRUE(client_any_->availability_record_.wait_for_last(service_availability::available(any_major)));

    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_));
    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_));

    auto drop_watch_v1 = watch_connection_drop(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_);
    auto drop_watch_v2 = watch_connection_drop(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_);

    client_v2_->release_service(interface_v2_.instance_);
    EXPECT_FALSE(drop_watch_v2.wait(std::chrono::milliseconds(500)))
            << "v2's connection dropped even though client_any_ (ANY_MAJOR) could still need it";
    EXPECT_FALSE(drop_watch_v1.wait(std::chrono::milliseconds(0))) << "v1's connection must never be touched by releasing v2";

    client_any_->release_service(any_major);
    EXPECT_TRUE(drop_watch_v2.wait(std::chrono::milliseconds(500)))
            << "v2's connection was never dropped after its last possible requester (client_any_) released -- "
               "release_service() only re-checked the one major client_any_ happened to be bound to, leaking v2's "
               "already-abandoned connection forever";
    EXPECT_FALSE(drop_watch_v1.wait(std::chrono::milliseconds(0)))
            << "v1's connection must survive: client_v1_ still concretely requests it";
}

// Mirror of releasing_v2_then_any_major_eventually_drops_v2s_connection_but_never_v1s with the
// roles of v1 and v2 swapped, to guard against a fix that happens to only work for whichever
// major find_service()'s ANY_MAJOR resolution (lowest offered major) happens to bind to.
TEST_F(test_multiversion_support_aggregate_consumption, releasing_v1_then_any_major_eventually_drops_v1s_connection_but_never_v2s) {
    start_apps();

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    service_instance const any_major{.service_ = interface_v1_.instance_.service_, .instance_ = interface_v1_.instance_.instance_};
    client_any_->request_service(any_major);

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    ASSERT_TRUE(client_any_->availability_record_.wait_for_last(service_availability::available(any_major)));

    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_));
    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_));

    auto drop_watch_v1 = watch_connection_drop(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_);
    auto drop_watch_v2 = watch_connection_drop(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_);

    client_v1_->release_service(interface_v1_.instance_);
    EXPECT_FALSE(drop_watch_v1.wait(std::chrono::milliseconds(500)))
            << "v1's connection dropped even though client_any_ (ANY_MAJOR) could still need it";
    EXPECT_FALSE(drop_watch_v2.wait(std::chrono::milliseconds(0))) << "v2's connection must never be touched by releasing v1";

    client_any_->release_service(any_major);
    EXPECT_TRUE(drop_watch_v1.wait(std::chrono::milliseconds(500)))
            << "v1's connection was never dropped after its last possible requester (client_any_) released -- "
               "release_service() only re-checked the one major client_any_ happened to be bound to, leaking v1's "
               "already-abandoned connection forever";
    EXPECT_FALSE(drop_watch_v2.wait(std::chrono::milliseconds(0)))
            << "v2's connection must survive: client_v2_ still concretely requests it";
}

TEST_F(test_multiversion_support_aggregate_consumption, stopping_v1_offer_does_not_disturb_v2_in_aggregate_setup) {
    start_apps();

    std::vector<unsigned char> const payload_v2{0xA1, 0xB2};
    provider_v2_->answer_request(request_v2_, [payload_v2] { return payload_v2; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    provider_v1_->stop_offer(interface_v1_.instance_);

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_)));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_),
                                                                std::chrono::milliseconds(250)));

    client_v2_->send_request(request_v2_);
    EXPECT_TRUE(client_v2_->wait_for_messages({payload_v2}));
}

TEST_F(test_multiversion_support_aggregate_consumption, stopping_v2_offer_does_not_disturb_v1_in_aggregate_setup) {
    start_apps();

    std::vector<unsigned char> const payload_v1{0xC3, 0xD4};
    provider_v1_->answer_request(request_v1_, [payload_v1] { return payload_v1; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    provider_v2_->stop_offer(interface_v2_.instance_);

    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_)));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_),
                                                                std::chrono::milliseconds(250)));

    client_v1_->send_request(request_v1_);
    EXPECT_TRUE(client_v1_->wait_for_messages({payload_v1}));
}

TEST_F(test_multiversion_support_aggregate_consumption, request_to_each_major_is_routed_to_the_matching_remote_provider) {
    start_apps();

    std::vector<unsigned char> const payload_v1{0x01, 0x02};
    std::vector<unsigned char> const payload_v2{0x03, 0x04};
    provider_v1_->answer_request(request_v1_, [payload_v1] { return payload_v1; });
    provider_v2_->answer_request(request_v2_, [payload_v2] { return payload_v2; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->send_request(request_v1_);
    client_v2_->send_request(request_v2_);

    EXPECT_TRUE(client_v1_->wait_for_messages({payload_v1}));
    EXPECT_TRUE(client_v2_->wait_for_messages({payload_v2}));
}

TEST_F(test_multiversion_support_aggregate_consumption, bringing_v1_provider_ecu_routing_down_expires_only_v1_and_keeps_v2_available) {
    start_apps();

    // Bring one offering ECU fully off-boardnet. This models "one ECU goes down" regardless of
    // service reliability and avoids depending on router-name TCP connection wiring in the test.

    std::vector<unsigned char> const payload_v2{0x33, 0x44};
    provider_v2_->answer_request(request_v2_, [payload_v2] { return payload_v2; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    ASSERT_TRUE(provider_v1_ecu_.set_routing(fake_netlink_connector::state_e::DOWN));

    // note we are using ttl expiry here to ensure the path in which a STOP_OFFER had not been send out.
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(30))));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_),
                                                                std::chrono::milliseconds(250)));

    client_v2_->send_request(request_v2_);
    EXPECT_TRUE(client_v2_->wait_for_messages({payload_v2}));
}

// Mirror of bringing_v1_provider_ecu_routing_down_expires_only_v1_and_keeps_v2_available with the
// roles of v1 and v2 swapped, to guard against a fix that happens to only work for whichever major
// sorts first.
TEST_F(test_multiversion_support_aggregate_consumption, bringing_v2_provider_ecu_routing_down_expires_only_v2_and_keeps_v1_available) {
    start_apps();

    std::vector<unsigned char> const payload_v1{0x55, 0x66};
    provider_v1_->answer_request(request_v1_, [payload_v1] { return payload_v1; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    ASSERT_TRUE(provider_v2_ecu_.set_routing(fake_netlink_connector::state_e::DOWN));

    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(30))));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_),
                                                                std::chrono::milliseconds(250)));

    client_v1_->send_request(request_v1_);
    EXPECT_TRUE(client_v1_->wait_for_messages({payload_v1}));
}

TEST_F(test_multiversion_support_aggregate_consumption,
       bringing_v1_provider_ecu_routing_down_does_not_corrupt_v2_client_endpoint_bookkeeping) {
    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe(udp_party::any(), {sd_multicast_ep_, consumer_ecu_.router_name_}, gate->get_data_pipe()));

    provider_v2_ecu_.config().service_discovery_.ttl_ = 60;

    start_apps();

    std::vector<unsigned char> const payload_v2{0x77, 0x88};
    provider_v2_->answer_request(request_v2_, [payload_v2] { return payload_v2; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_));
    auto drop_watch_v2 = watch_connection_drop(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_);

    // Block every incoming SD message to consumer_ecu_ from here on: this does not stop v1's TTL
    // from expiring (that is a purely time-based decrement, independent of incoming traffic), but
    // it does prevent provider_v2's next cyclic re-offer from silently repairing v2's bookkeeping
    // before we get to probe it below.
    gate->block_at(someip_gate::sd_predicate{[](auto const&) { return true; }});

    ASSERT_TRUE(provider_v1_ecu_.set_routing(fake_netlink_connector::state_e::DOWN));

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(30))));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_),
                                                                std::chrono::milliseconds(250)));

    EXPECT_FALSE(drop_watch_v2.wait(std::chrono::milliseconds(0)))
            << "v2's reliable boardnet connection was dropped by v1's TTL expiry -- clear_client_endpoints() matched "
               "v2's entry purely by (service, instance), ignoring major";

    client_v2_->send_request(request_v2_);
    EXPECT_TRUE(client_v2_->wait_for_messages({payload_v2}));
}

TEST_F(test_multiversion_support_aggregate_consumption,
       bringing_v2_provider_ecu_routing_down_does_not_corrupt_v1_client_endpoint_bookkeeping) {
    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe(udp_party::any(), {sd_multicast_ep_, consumer_ecu_.router_name_}, gate->get_data_pipe()));

    provider_v1_ecu_.config().service_discovery_.ttl_ = 60;

    start_apps();

    std::vector<unsigned char> const payload_v1{0x77, 0x88};
    provider_v1_->answer_request(request_v1_, [payload_v1] { return payload_v1; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_));
    auto drop_watch_v1 = watch_connection_drop(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_);

    // Block every incoming SD message to consumer_ecu_ from here on: this does not stop v2's TTL
    // from expiring (that is a purely time-based decrement, independent of incoming traffic), but
    // it does prevent provider_v1's next cyclic re-offer from silently repairing v1's bookkeeping
    // before we get to probe it below.
    gate->block_at(someip_gate::sd_predicate{[](auto const&) { return true; }});

    ASSERT_TRUE(provider_v2_ecu_.set_routing(fake_netlink_connector::state_e::DOWN));

    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(30))));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_),
                                                                std::chrono::milliseconds(250)));

    EXPECT_FALSE(drop_watch_v1.wait(std::chrono::milliseconds(0)))
            << "v1's reliable boardnet connection was dropped by v2's TTL expiry -- clear_client_endpoints() matched "
               "v1's entry purely by (service, instance), ignoring major";

    client_v1_->send_request(request_v1_);
    EXPECT_TRUE(client_v1_->wait_for_messages({payload_v1}));
}

TEST_F(test_multiversion_support_aggregate_consumption,
       local_offer_of_a_remotely_known_major_is_rejected_and_request_still_reaches_remote_provider) {
    consumer_ecu_.add_app("local_pretender_v2");
    start_apps();
    app* local_pretender_v2 = consumer_ecu_.apps_["local_pretender_v2"];

    provider_v2_->offer(interface_v2_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    // Attempt to ALSO offer the same (service, instance, major) locally on the consumer ECU,
    // where it is already known remotely (via provider_v2). Must be rejected internally.
    local_pretender_v2->offer(interface_v2_.instance_);

    client_v2_->send_request(request_v2_);

    message_checker const request_checker{.client_session_ = std::nullopt,
                                          .service_instance_ = interface_v2_.instance_,
                                          .method_ = method_,
                                          .message_type_ = vsomeip::message_type_e::MT_REQUEST};
    EXPECT_TRUE(provider_v2_->message_record_.wait_for(request_checker)) << provider_v2_->message_record_;
    EXPECT_FALSE(local_pretender_v2->message_record_.wait_for(request_checker, std::chrono::milliseconds(200)))
            << local_pretender_v2->message_record_;
}

TEST_F(test_multiversion_support_aggregate_consumption, when_neither_major_is_offered_both_are_still_found_independently) {
    // Both requesters ask before either provider offers anything -- this exercises the
    // consumer's outgoing SD FindService traffic itself, not just availability bookkeeping
    // after the fact. Both majors must be found -- but NOT necessarily in the same SD message:
    // the debounce timer is a single, continuously-running loop (not restarted per-request), so
    // whether two back-to-back requests land in the same firing is a race against the timer's
    // existing phase, not something a wider debounce window can fix. Requiring "same message"
    // would test a stronger guarantee than this implementation actually provides.
    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe(udp_party::any(), {sd_multicast_ep_, consumer_ecu_.router_name_}, gate->get_data_pipe()));
    start_apps();

    // Predicate carries state across calls (safe: someip_gate invokes it under its own mutex),
    // accumulating "seen" across however many separate messages the two Finds end up in.
    struct seen_state {
        bool has_v1_{false};
        bool has_v2_{false};
    };
    auto seen = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen->has_v1_ |= e->get_major_version() == 0x1;
                seen->has_v2_ |= e->get_major_version() == 0x2;
            }
        }
        return seen->has_v1_ && seen->has_v2_;
    }});

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    EXPECT_TRUE(gate->wait_for_blocked());
}

TEST_F(test_multiversion_support_aggregate_consumption, when_v1_and_v2_are_requested_both_are_found_before_and_after_suspend_resume) {

    consumer_ecu_.config().service_discovery_.repetitions_max_ =
            0; // this guarantess that FIND_OFFERS are not leaking across suspend/resume
    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe({consumer_ecu_.sd_endpoint(), consumer_ecu_.router_name_}, {sd_multicast_ep_}, gate->get_data_pipe()));
    start_apps();

    struct seen_state {
        bool has_v1_{false};
        bool has_v2_{false};
    };

    auto seen_before_suspend = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_before_suspend](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_before_suspend->has_v1_ |= e->get_major_version() == 0x1;
                seen_before_suspend->has_v2_ |= e->get_major_version() == 0x2;
            }
        }
        return seen_before_suspend->has_v1_ && seen_before_suspend->has_v2_;
    }});

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(30))));
    gate->block(false);

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    auto seen_after_resume = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_after_resume](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_after_resume->has_v1_ |= e->get_major_version() == 0x1;
                seen_after_resume->has_v2_ |= e->get_major_version() == 0x2;
            }
        }
        return seen_after_resume->has_v1_ && seen_after_resume->has_v2_;
    }});

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    EXPECT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(30))));
}

TEST_F(test_multiversion_support_aggregate_consumption, when_only_v1_is_offered_only_v1_becomes_available) {
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    provider_v1_->offer(interface_v1_.instance_);

    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    EXPECT_FALSE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                                std::chrono::milliseconds(10)));
}

TEST_F(test_multiversion_support_aggregate_consumption, when_only_v2_is_offered_only_v2_becomes_available) {
    start_apps();

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);

    provider_v2_->offer(interface_v2_.instance_);

    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    EXPECT_FALSE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_),
                                                                std::chrono::milliseconds(10)));
}

// Regression test for a latent major-scoping gap in reset_request_sent_counter(): it resets the
// FindService sent-counter for every major requested for a (service, instance), not just the one
// whose TTL expired, so a still-available major gets spuriously re-found too.
TEST_F(test_multiversion_support_aggregate_consumption, bringing_v2_provider_ecu_routing_down_triggers_a_find_only_for_v2_afterwards) {
    consumer_ecu_.config().service_discovery_.repetitions_max_ = 0;

    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe(udp_party::any(), {sd_multicast_ep_, consumer_ecu_.router_name_}, gate->get_data_pipe()));
    start_apps();

    struct seen_state {
        bool has_v1_{false};
        bool has_v2_{false};
    };

    auto seen_initial = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_initial](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_initial->has_v1_ |= e->get_major_version() == 0x1;
                seen_initial->has_v2_ |= e->get_major_version() == 0x2;
            }
        }
        return seen_initial->has_v1_ && seen_initial->has_v2_;
    }});
    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(30))));
    gate->block(false);

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    auto seen_after = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_after](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_after->has_v1_ |= e->get_major_version() == 0x1;
                seen_after->has_v2_ |= e->get_major_version() == 0x2;
            }
        }
        return seen_after->has_v1_ || seen_after->has_v2_;
    }});

    ASSERT_TRUE(provider_v2_ecu_.set_routing(fake_netlink_connector::state_e::DOWN));

    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(30))));
    EXPECT_TRUE(seen_after->has_v2_);
    EXPECT_FALSE(seen_after->has_v1_);
}

// Mirror of bringing_v2_provider_ecu_routing_down_triggers_a_find_only_for_v2_afterwards, roles reversed.
TEST_F(test_multiversion_support_aggregate_consumption, bringing_v1_provider_ecu_routing_down_triggers_a_find_only_for_v1_afterwards) {
    consumer_ecu_.config().service_discovery_.repetitions_max_ = 0;

    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe(udp_party::any(), {sd_multicast_ep_, consumer_ecu_.router_name_}, gate->get_data_pipe()));
    start_apps();

    struct seen_state {
        bool has_v1_{false};
        bool has_v2_{false};
    };

    auto seen_initial = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_initial](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_initial->has_v1_ |= e->get_major_version() == 0x1;
                seen_initial->has_v2_ |= e->get_major_version() == 0x2;
            }
        }
        return seen_initial->has_v1_ && seen_initial->has_v2_;
    }});
    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(30))));
    gate->block(false);

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    auto seen_after = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_after](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_after->has_v1_ |= e->get_major_version() == 0x1;
                seen_after->has_v2_ |= e->get_major_version() == 0x2;
            }
        }
        return seen_after->has_v1_ || seen_after->has_v2_;
    }});

    ASSERT_TRUE(provider_v1_ecu_.set_routing(fake_netlink_connector::state_e::DOWN));

    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(30))));
    EXPECT_TRUE(seen_after->has_v1_);
    EXPECT_FALSE(seen_after->has_v2_);
}

TEST_F(test_multiversion_support_aggregate_consumption, any_major_request_still_finds_again_after_ttl_expiry) {
    consumer_ecu_.config().service_discovery_.repetitions_max_ = 0; // ensure exactly one find_offer is send after request without offer

    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe(udp_party::any(), {sd_multicast_ep_, consumer_ecu_.router_name_}, gate->get_data_pipe()));
    start_apps();

    service_instance const any_major_v1{
            .service_ = interface_v1_.instance_.service_, .instance_ = interface_v1_.instance_.instance_, .major_ = ANY_MAJOR};
    client_v1_->request_service(any_major_v1);
    provider_v1_->offer(interface_v1_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    gate->block_at(someip_gate::sd_predicate{[](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                return true;
            }
        }
        return false;
    }});

    ASSERT_TRUE(provider_v1_ecu_.set_routing(fake_netlink_connector::state_e::DOWN));

    EXPECT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(5))))
            << "consumer never re-sent a FindService after v1's TTL expired -- the ANY_MAJOR-keyed request's "
               "sent-counter was never reset";
}

TEST_F(test_multiversion_support_aggregate_consumption, any_major_request_stops_finding_again_once_the_concrete_offer_arrives) {
    // Needs the FindService repetition phase to still be in flight when the offer arrives (see the
    // comment on the fixture below) -- vsomeip's own built-in repetition cadence is fast (10ms),
    // too close to the offer's own processing latency to reliably observe update_request() winning
    // the race, so this test configures its own, deliberately slower cadence instead of relying on
    // whatever ecu_config's default happens to be.
    auto const cycle_time = common::scaled_timeout(std::chrono::milliseconds(400));
    consumer_ecu_.config().service_discovery_.repetitions_base_delay_ = static_cast<uint32_t>(cycle_time.count());

    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe(udp_party::any(), {sd_multicast_ep_, consumer_ecu_.router_name_}, gate->get_data_pipe()));
    start_apps();

    // Arm from the very start, before requesting at all -- not after availability is confirmed.
    // Arming only after wait_for_last(available) races against the debounce timer: an already
    // in-flight repetition tick (scheduled before the offer's update_request() call takes effect)
    // can slip out during the gap between update_request() running and this test thread observing
    // availability, producing a flaky false failure unrelated to the fix. Counting occurrences and
    // only blocking on the *second* FindService sidesteps the race entirely: the very first Find is
    // the expected, legitimate one sent immediately upon request_service(), before any offer exists.
    auto seen_count = std::make_shared<int>(0);
    gate->block_at(someip_gate::sd_predicate{[seen_count](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                return ++(*seen_count) > 1;
            }
        }
        return false;
    }});

    service_instance const any_major_v1{
            .service_ = interface_v1_.instance_.service_, .instance_ = interface_v1_.instance_.instance_, .major_ = ANY_MAJOR};
    client_v1_->request_service(any_major_v1);
    provider_v1_->offer(interface_v1_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    // The first (unsuppressed) repetition tick is scheduled at exactly cycle_time after the initial
    // Find (2^0 * repetitions_base_delay); waiting for 2x that gives headroom against scheduling
    // jitter while still reliably catching a regression at its actual scheduled time.
    EXPECT_FALSE(gate->wait_for_blocked(cycle_time * 2))
            << "consumer sent a second FindService after the offer arrived -- update_request()'s ANY_MAJOR-keyed "
               "request was never marked as found, so its repetition phase never stopped";
}

TEST_F(test_multiversion_support_aggregate_consumption, releasing_v1_does_not_disturb_v2_and_only_v1_becomes_available_again) {
    start_apps();

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    client_v1_->release_service(interface_v1_.instance_);

    // Re-requesting v1 is the observable signal that the release actually completed: it should
    // become available again exactly as if freshly requested.
    client_v1_->request_service(interface_v1_.instance_);
    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    // v2 was never touched: releasing (and re-requesting) v1 must not have produced any record for
    // it at all.
    EXPECT_TRUE(client_v2_->availability_record_.equals({})) << client_v2_->availability_record_;
}

// Mirror of releasing_v1_does_not_disturb_v2_and_only_v1_becomes_available_again with the roles of
// v1 and v2 swapped, to guard against a fix that happens to only work for whichever major sorts
// first.
TEST_F(test_multiversion_support_aggregate_consumption, releasing_v2_does_not_disturb_v1_and_only_v2_becomes_available_again) {
    start_apps();

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    client_v2_->release_service(interface_v2_.instance_);

    // Re-requesting v2 is the observable signal that the release actually completed: it should
    // become available again exactly as if freshly requested.
    client_v2_->request_service(interface_v2_.instance_);
    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    // v1 was never touched: releasing (and re-requesting) v2 must not have produced any record for
    // it at all.
    EXPECT_TRUE(client_v1_->availability_record_.equals({})) << client_v1_->availability_record_;
}

TEST_F(test_multiversion_support_aggregate_consumption, releasing_v1_drops_it_from_the_post_resume_find_but_not_v2) {
    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe({consumer_ecu_.sd_endpoint(), consumer_ecu_.router_name_}, {sd_multicast_ep_}, gate->get_data_pipe()));

    start_apps();

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v1_->release_service(interface_v1_.instance_);

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    // Quiesce provider-side offers before suspend/resume so the post-resume FindService
    // observation does not race against immediate incoming OfferService.
    provider_v1_->stop_offer(interface_v1_.instance_);
    provider_v2_->stop_offer(interface_v2_.instance_);

    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::unavailable(interface_v2_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(10))));
    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    auto seen_majors = std::make_shared<attribute_recorder<vsomeip::major_version_t>>();
    gate->block_at(someip_gate::sd_predicate{[seen_majors](auto const& msg) {
        bool matched = false;
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_majors->record(e->get_major_version());
                matched = true;
            }
        }
        return matched;
    }});

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(10))));
    EXPECT_TRUE(seen_majors->wait_for_any(vsomeip::major_version_t{0x2}, std::chrono::milliseconds(0)))
            << "v2 is still requested -- it must be found again after resume";
    EXPECT_FALSE(seen_majors->wait_for_any(vsomeip::major_version_t{0x1}, std::chrono::milliseconds(0)))
            << "v1 was released -- it must not be re-requested after resume";
    gate->block(false);

    // Restore only the still-requested major: v2 should become available again, v1 stays gone.
    provider_v2_->offer(interface_v2_.instance_);
    EXPECT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(10))));
    EXPECT_TRUE(client_v1_->availability_record_.equals({})) << client_v1_->availability_record_;
}

// Mirror of releasing_v1_drops_it_from_the_post_resume_find_but_not_v2 with the roles of v1 and v2
// swapped, to guard against a fix that happens to only work for whichever major sorts first.
TEST_F(test_multiversion_support_aggregate_consumption, releasing_v2_drops_it_from_the_post_resume_find_but_not_v1) {
    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe({consumer_ecu_.sd_endpoint(), consumer_ecu_.router_name_}, {sd_multicast_ep_}, gate->get_data_pipe()));

    start_apps();

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);

    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    client_v2_->release_service(interface_v2_.instance_);

    client_v1_->availability_record_.clear();
    client_v2_->availability_record_.clear();

    // Quiesce provider-side offers before suspend/resume so the post-resume FindService
    // observation does not race against immediate incoming OfferService.
    provider_v1_->stop_offer(interface_v1_.instance_);
    provider_v2_->stop_offer(interface_v2_.instance_);

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::unavailable(interface_v1_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(10))));
    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    auto seen_majors = std::make_shared<attribute_recorder<vsomeip::major_version_t>>();
    gate->block_at(someip_gate::sd_predicate{[seen_majors](auto const& msg) {
        bool matched = false;
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_majors->record(e->get_major_version());
                matched = true;
            }
        }
        return matched;
    }});

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(10))));
    EXPECT_TRUE(seen_majors->wait_for_any(vsomeip::major_version_t{0x1}, std::chrono::milliseconds(0)))
            << "v1 is still requested -- it must be found again after resume";
    EXPECT_FALSE(seen_majors->wait_for_any(vsomeip::major_version_t{0x2}, std::chrono::milliseconds(0)))
            << "v2 was released -- it must not be re-requested after resume";
    gate->block(false);

    // Restore only the still-requested major: v1 should become available again, v2 stays gone.
    provider_v1_->offer(interface_v1_.instance_);
    EXPECT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_),
                                                               common::scaled_timeout(std::chrono::seconds(10))));
    EXPECT_TRUE(client_v2_->availability_record_.equals({})) << client_v2_->availability_record_;
}

/// The router can differentiate properly between a local and a remote offering
TEST_F(test_multiversion_support_aggregate_consumption, local_consumer_of_v2_can_dispatch_request_to_v2) {
    std::string const local_v2_client_name = "local_v2_client";
    provider_v2_ecu_.add_app(local_v2_client_name);
    start_apps();
    auto* local_client = provider_v2_ecu_.apps_[local_v2_client_name];

    std::vector<unsigned char> const payload_v2{0x33, 0x44};
    provider_v2_->answer_request(request_v2_, [payload_v2] { return payload_v2; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);
    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    // now the service should be known to the router of ecu_provider_v2
    local_client->request_service(interface_v2_.instance_);
    ASSERT_TRUE(local_client->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    local_client->send_request(request_v2_);
    EXPECT_TRUE(local_client->wait_for_messages({payload_v2}));
}
/// The router can differentiate properly between a local and a remote offering
TEST_F(test_multiversion_support_aggregate_consumption, local_consumer_of_v1_can_dispatch_request_to_v1) {
    std::string const local_v1_client_name = "local_v1_client";
    provider_v1_ecu_.add_app(local_v1_client_name);
    start_apps();
    auto* local_client = provider_v1_ecu_.apps_[local_v1_client_name];

    std::vector<unsigned char> const payload_v1{0x33, 0x44};
    provider_v1_->answer_request(request_v1_, [payload_v1] { return payload_v1; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);
    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    // now the service should be known to the router of ecu_provider_v2
    local_client->request_service(interface_v1_.instance_);
    ASSERT_TRUE(local_client->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    local_client->send_request(request_v1_);
    EXPECT_TRUE(local_client->wait_for_messages({payload_v1}));
}

/// The router can differentiate properly between a local and a remote offering
TEST_F(test_multiversion_support_aggregate_consumption, remote_consumer_of_v2_with_local_v1_offering_can_dispatch_request_to_v2) {
    static constexpr char const* remote_v2_client_name = "remote_v2_client";
    provider_v1_ecu_.config().apps_.push_back(application_config{
            .name_ = remote_v2_client_name, .id_ = provider_v1_ecu_.config().apps_.back().id_.value() + 1, .threads_ = std::nullopt});
    start_apps();
    auto* remote_client = provider_v1_ecu_.apps_[remote_v2_client_name];

    std::vector<unsigned char> const payload_v2{0x33, 0x44};
    provider_v2_->answer_request(request_v2_, [payload_v2] { return payload_v2; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);
    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    // now the service should be known to the router of ecu_provider_v2
    remote_client->request_service(interface_v2_.instance_);
    ASSERT_TRUE(remote_client->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    remote_client->send_request(request_v2_);
    EXPECT_TRUE(remote_client->wait_for_messages({payload_v2}));
}

/// The router can differentiate properly between a local and a remote offering
TEST_F(test_multiversion_support_aggregate_consumption, remote_consumer_of_v1_with_local_v2_offering_can_dispatch_request_to_v1) {
    static constexpr char const* remote_v1_client_name = "remote_v1_client";
    provider_v2_ecu_.config().apps_.push_back(application_config{
            .name_ = remote_v1_client_name, .id_ = provider_v2_ecu_.config().apps_.back().id_.value() + 1, .threads_ = std::nullopt});
    start_apps();
    auto* remote_client = provider_v2_ecu_.apps_[remote_v1_client_name];

    std::vector<unsigned char> const payload_v1{0x33, 0x44};
    provider_v1_->answer_request(request_v1_, [payload_v1] { return payload_v1; });

    provider_v1_->offer(interface_v1_.instance_);
    provider_v2_->offer(interface_v2_.instance_);
    client_v1_->request_service(interface_v1_.instance_);
    client_v2_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    // now the service should be known to the router of ecu_provider_v2
    remote_client->request_service(interface_v1_.instance_);
    ASSERT_TRUE(remote_client->availability_record_.wait_for_last(service_availability::available(interface_v1_.instance_)));

    remote_client->send_request(request_v1_);
    EXPECT_TRUE(remote_client->wait_for_messages({payload_v1}));
}

// Ensure DEFAULT_MAJOR != ANY_MAJOR
TEST_F(test_multiversion_support_aggregate_consumption, releasing_the_only_requester_of_the_default_major_still_tears_it_down) {
    start_apps();

    service_instance const major0_instance{.service_ = interface_v1_.instance_.service_,
                                           .instance_ = interface_v1_.instance_.instance_,
                                           .major_ = vsomeip::DEFAULT_MAJOR,
                                           .minor_ = vsomeip::DEFAULT_MINOR};

    provider_v1_->offer(major0_instance); // offers major 0x0, from provider_v1_ecu_
    provider_v2_->offer(interface_v2_.instance_); // offers major 0x2, from provider_v2_ecu_ (same service/instance)

    client_v1_->request_service(major0_instance); // the ONLY requester of major 0
    client_v2_->request_service(interface_v2_.instance_); // a requester of a DIFFERENT major (0x2)

    ASSERT_TRUE(client_v1_->availability_record_.wait_for_last(service_availability::available(major0_instance)));
    ASSERT_TRUE(client_v2_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_));
    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_));

    auto drop_watch_major0 = watch_connection_drop(consumer_ecu_.router_name_, provider_v1_ecu_.router_name_);
    auto drop_watch_major2 = watch_connection_drop(consumer_ecu_.router_name_, provider_v2_ecu_.router_name_);

    client_v1_->release_service(major0_instance); // release_service() takes no major -- router must
                                                  // work out this client only ever held major 0

    // Correct behavior: major 0 has no requester left, so its client endpoint/connection to
    // provider_v1_ecu_ is torn down. Currently: has_requester_unlocked()'s query-side
    // `_major == DEFAULT_MAJOR` clause treats the release-time check for major 0 as if it meant
    // "any major", spuriously matches client_v2_'s still-live major-2 requester, and the major-0
    // teardown is skipped entirely.
    EXPECT_TRUE(drop_watch_major0.wait()) << "connection to the major-0 provider was not torn down after releasing its only "
                                             "requester, because an unrelated major's requester masked it -- see review finding 5";

    // Control: major 2's own, unrelated, still-live requester must NOT be disturbed by this release.
    EXPECT_FALSE(drop_watch_major2.wait(std::chrono::milliseconds(500)))
            << "releasing the major-0 requester incorrectly also affected the major-2 connection";
}

TEST_F(test_multiversion_support_aggregate_consumption, releasing_an_undiscovered_any_major_request_stops_find_service_after_resume) {
    GTEST_SKIP() << "Requires another sensor mechanism, as it is not guaranteed that the release is processed by the router, before the "
                    "suspend,resume cycle finishes";
    // With repetitions_max_ == 0, request_service() sends exactly ONE FindService per SD start
    // cycle -- this isolates the resume-triggered (second) burst from any natural repetition-phase
    // traffic belonging to the original, immediately-released request.
    consumer_ecu_.config().service_discovery_.repetitions_max_ = 0;

    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe({consumer_ecu_.sd_endpoint(), consumer_ecu_.router_name_}, {sd_multicast_ep_}, gate->get_data_pipe()));
    start_apps(); // nobody offers anything for this (service, instance) in this test

    service_instance const any_major{
            .service_ = interface_v1_.instance_.service_, .instance_ = interface_v1_.instance_.instance_, .major_ = ANY_MAJOR};

    // Wait for the router to have actually received (and, on the same read, processed) both the
    // request and the release before touching routing state: both are asynchronous IPC commands
    // from client_any_'s own thread, while set_routing_state() below runs synchronously on THIS
    // thread, independent of the router's IPC processing -- without this, request/release could
    // still be in flight when suspend/resume runs, making the scenario non-deterministic.
    client_any_->request_service(any_major);
    ASSERT_TRUE(wait_for_command("client_any", consumer_ecu_.router_name_, protocol::id_e::REQUEST_SERVICE_ID, socket_role::server));

    client_any_->release_service(any_major);
    ASSERT_TRUE(wait_for_command("client_any", consumer_ecu_.router_name_, protocol::id_e::RELEASE_SERVICE_ID, socket_role::server));

    // Only now arm the gate for the resume-triggered FindService -- the original request's own,
    // legitimate initial FindService (sent while processing the REQUEST_SERVICE command above) is
    // irrelevant and must not be mistaken for the bug under test.
    gate->block_at(someip_gate::sd_predicate{[service = any_major.service_, instance = any_major.instance_](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == service && e->get_instance() == instance) {
                return true;
            }
        }
        return false;
    }});

    // Trigger the suspend -> resume cycle that resets every SD requested_ entry's sent_counter to
    // 0. routing_manager_impl::set_routing_state() runs each transition synchronously (including
    // flushing SD sends on suspend) before returning, so back-to-back calls are safe here.
    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);
    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    // Correct behavior: release_service() erased the ANY_MAJOR-keyed SD requested_ entry, so
    // resume's sent_counter reset loop has nothing to find for this (service, instance), and no
    // FindService is ever emitted. Currently: find_services() returned empty (nothing was ever
    // offered), the per-major teardown loop never ran, discovery_->release_service() was never
    // called, and the orphaned ANY_MAJOR entry gets its counter reset and re-fires on resume.
    EXPECT_FALSE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(3))))
            << "a FindService was observed after resume for a service that was requested then "
               "released before ever being offered -- its SD requested_ entry was never erased -- "
               "see review finding 6";
}
// Ensure that a FIND_OFFER is only dispatched for explicit requests, i.e. if a client requests
// v2 and another ANY_MAJOR both requests should be dispatched, but if v2 is dropped only the
// FIND_OFFER for ANY_MAJOR should be dispatched later.
TEST_F(test_multiversion_support_aggregate_consumption, any_major_request_does_not_keep_explicit_major_request) {

    consumer_ecu_.config().service_discovery_.repetitions_max_ =
            0; // this guarantess that FIND_OFFERS are not leaking across suspend/resume
    auto gate = someip_gate::create();
    ASSERT_TRUE(set_udp_data_pipe({consumer_ecu_.sd_endpoint(), consumer_ecu_.router_name_}, {sd_multicast_ep_}, gate->get_data_pipe()));
    start_apps();

    struct seen_state {
        std::atomic<bool> has_v1_{false};
        std::atomic<bool> has_any_{false};
    };

    auto seen_before_suspend = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_before_suspend](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_before_suspend->has_v1_ = e->get_major_version() == 0x1 || seen_before_suspend->has_v1_.load();
                seen_before_suspend->has_any_ = e->get_major_version() == ANY_MAJOR || seen_before_suspend->has_any_.load();
            }
        }
        return seen_before_suspend->has_v1_ && seen_before_suspend->has_any_;
    }});

    service_instance any_major{
            .service_ = interface_v1_.instance_.service_, .instance_ = interface_v1_.instance_.instance_, .major_ = ANY_MAJOR};

    client_v1_->request_service(interface_v1_.instance_);
    client_any_->request_service(any_major);

    ASSERT_TRUE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(30))));
    gate->block(false);
    client_v1_->release_service(interface_v1_.instance_);
    ASSERT_TRUE(wait_for_command(client_v1_name_, consumer_ecu_.router_name_, protocol::id_e::RELEASE_SERVICE_ID, socket_role::server));

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    auto seen_after_resume = std::make_shared<seen_state>();
    gate->block_at(someip_gate::sd_predicate{[seen_after_resume](auto const& msg) {
        for (auto const& e : msg->get_entries()) {
            if (e->get_type() == sd::entry_type_e::FIND_SERVICE && e->get_service() == 0x1000 && e->get_instance() == 0x1) {
                seen_after_resume->has_v1_ = e->get_major_version() == 0x1 || seen_after_resume->has_v1_.load();
                seen_after_resume->has_any_ = e->get_major_version() == ANY_MAJOR || seen_after_resume->has_any_.load();
            }
        }
        return seen_after_resume->has_v1_ && seen_after_resume->has_any_;
    }});

    consumer_ecu_.router_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    EXPECT_FALSE(gate->wait_for_blocked(common::scaled_timeout(std::chrono::milliseconds(100))));
    EXPECT_TRUE(seen_after_resume->has_any_.load());
    EXPECT_FALSE(seen_after_resume->has_v1_.load());
}

// the application-facing release_service() API
// (vsomeip::application::release_service) takes no major -- only (service, instance). The router
// works out its_client_major -- the concrete major *this client's own request* is bookkept under --
// from requested_services_, then calls ep_mgr_impl_->clear_client_endpoints(service, instance,
// its_client_major, reliable). request_service()'s own default major is ANY_MAJOR,
// so when the client requested via ANY_MAJOR,
// its_client_major recovered here is ANY_MAJOR too. But remote_services_ (which
// clear_client_endpoints() looks up) is always keyed by the concrete major from the actual offer --
// an exact {service, instance, ANY_MAJOR} lookup never matches a {service, instance, 0x2} entry, so
// the client endpoint is never stopped and its reserved local port never released. Made observable
// here through a reliable (TCP) event: releasing must tear down the underlying boardnet TCP
// connection between the consumer and provider routers, exactly as it would if the client had
// requested the concrete major instead.
struct test_multiversion_release_any_major_reliable : public base_fake_socket_fixture {
    void start_apps() {
        consumer_ecu_.add_app(client_name_);
        consumer_ecu_.add_app(other_client_name_);
        provider_ecu_.add_app(server_name_);

        consumer_ecu_.prepare();
        provider_ecu_.prepare();

        consumer_ecu_.start_apps();
        provider_ecu_.start_apps();

        client_ = consumer_ecu_.apps_[client_name_];
        other_client_ = consumer_ecu_.apps_[other_client_name_];
        server_ = provider_ecu_.apps_[server_name_];
    }

    std::string const client_name_{"client"};
    std::string const other_client_name_{"other_client"};
    std::string const server_name_{"server"};

    interface interface_v2_{service_instance{.service_ = 0x1000, .instance_ = 0x1, .major_ = 0x2, .minor_ = 0x0},
                            {event_spec{0x8001, {0x8001}, vsomeip::reliability_type_e::RT_RELIABLE}},
                            {}};
    event_ids event_{interface_v2_.instance_, interface_v2_.events_[0]};

    ecu_setup provider_ecu_{"provider", ecu_config{boardnet::ecu_one_config}.add_interface({interface_v2_}), *socket_manager_};
    ecu_setup consumer_ecu_{"consumer", boardnet::ecu_three_config, *socket_manager_};

    app* client_;
    app* other_client_;
    app* server_;
};

TEST_F(test_multiversion_release_any_major_reliable, releasing_an_any_major_request_drops_the_reliable_boardnet_connection) {
    start_apps();

    server_->offer(interface_v2_);

    service_instance const any_major{.service_ = interface_v2_.instance_.service_, .instance_ = interface_v2_.instance_.instance_};
    client_->request_service(any_major);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    client_->subscribe_event(event_);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(event_)));
    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_ecu_.router_name_));

    auto drop_watch = watch_connection_drop(consumer_ecu_.router_name_, provider_ecu_.router_name_);
    client_->release_service(any_major);

    EXPECT_TRUE(drop_watch.wait()) << "reliable boardnet connection was never dropped after releasing an ANY_MAJOR "
                                      "request -- clear_client_endpoints() exact-misses the ANY_MAJOR-keyed request "
                                      "against the concrete-major remote_services_ entry";
}
TEST_F(test_multiversion_release_any_major_reliable,
       releasing_an_any_major_request_does_not_drop_the_connection_while_another_client_still_holds_the_concrete_major) {
    start_apps();

    server_->offer(interface_v2_);

    service_instance const any_major{.service_ = interface_v2_.instance_.service_, .instance_ = interface_v2_.instance_.instance_};
    client_->request_service(any_major);
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
    client_->subscribe_event(event_);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(event_)));
    ASSERT_TRUE(await_connection(consumer_ecu_.router_name_, provider_ecu_.router_name_));

    other_client_->request_service(interface_v2_.instance_);
    ASSERT_TRUE(other_client_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));

    auto drop_watch = watch_connection_drop(consumer_ecu_.router_name_, provider_ecu_.router_name_);
    client_->release_service(any_major);

    EXPECT_FALSE(drop_watch.wait(std::chrono::milliseconds(250)))
            << "reliable boardnet connection was dropped even though other_client_ still concretely "
               "requests major 0x2 -- releasing one client's ANY_MAJOR request must not disturb another "
               "client's still-active request for the concrete major";

    // The still-requesting client must be able to keep using the service.
    ASSERT_TRUE(other_client_->availability_record_.wait_for_last(service_availability::available(interface_v2_.instance_)));
}
}

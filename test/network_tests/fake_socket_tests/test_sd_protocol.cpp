// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sample_configurations.hpp"

#include "helpers/app.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/someip_gate.hpp"

#include "common/timeout_scale.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {

std::string const router_one_name_ = "router_one";
std::string const router_two_name_ = "router_two";
std::string const ecu_two_server_name_ = "ecu_two_server";
std::string const ecu_one_client_name_ = "ecu_one_client";
std::string const ecu_one_server_name_ = "ecu_one_server";
std::string const ecu_two_client_name_ = "ecu_two_client";

struct test_field_resubscribe : public base_fake_socket_fixture {
    std::vector<event_spec> const events_{{0x8010, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE},
                                          {0x8011, {0x1, 0x5}, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    std::vector<event_spec> const fields_{{0x8012, {0x2}, vsomeip::reliability_type_e::RT_UNRELIABLE},
                                          {0x8013, {0x3}, vsomeip::reliability_type_e::RT_RELIABLE},
                                          {0x8014, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE},
                                          {0x8015, {0x1, 0x5}, vsomeip::reliability_type_e::RT_UNRELIABLE},
                                          {0x8016, {0x1, 0x5}, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    interface iface_{0x3348, events_, fields_};

    ecu_config ecu_one_cfg_{boardnet::ecu_one_config};
    ecu_config ecu_two_cfg_{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_cfg_.add_interface({iface_}), *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", ecu_two_cfg_.add_interface({iface_}), *socket_manager_};

    event_ids udp_field_{iface_.instance_, iface_.fields_[0]};
    event_ids tcp_field_{iface_.instance_, iface_.fields_[1]};
    event_ids mix_event_group_{iface_.instance_, iface_.fields_[2]};
    event_ids shared_event_group_field_{iface_.instance_, iface_.fields_[4]};
    event_ids plain_event_{iface_.instance_, iface_.events_[0]};

    std::vector<unsigned char> const payload_{0x5, 0x3};

    std::shared_ptr<someip_gate> ecu_one_sd_send_gate_ = someip_gate::create();
    std::shared_ptr<someip_gate> ecu_two_sd_send_gate_ = someip_gate::create();
    std::shared_ptr<someip_gate> notify_gate_ = someip_gate::create();

    boost::asio::ip::udp::endpoint client_endpoint() { return {boardnet::ecu_one_config.unicast_ip_, 30491}; }

    void prepare_ecus_and_apps() {
        ecu_one_.add_app(ecu_one_client_name_);
        ecu_one_.add_app(ecu_one_server_name_);
        ecu_two_.add_app(ecu_two_server_name_);

        ecu_one_.prepare();
        ecu_two_.prepare();

        ASSERT_TRUE(setup_data_pipe(ecu_one_.sd_endpoint(), router_one_name_, socket_role::server, ecu_one_sd_send_gate_->get_data_pipe()));
        ASSERT_TRUE(setup_data_pipe(ecu_two_.sd_endpoint(), router_two_name_, socket_role::server, ecu_two_sd_send_gate_->get_data_pipe()));

        ecu_one_.start_apps();
        ecu_two_.start_apps();
    }
};

TEST_F(test_field_resubscribe, field_resubscribe_recovers_after_lost_initial_notification_udp) {
    // If a field's initial notification is lost, the next cyclic offer must make
    // the client force a StopSubscribe(ttl=0)+Subscribe pair, which prompts the server to resend the initial value.
    prepare_ecus_and_apps();

    auto* router_one = ecu_one_.router_;
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    ASSERT_TRUE(setup_data_pipe(client_endpoint(), router_one_name_, socket_role::client, notify_gate_->get_data_pipe()));

    server->offer(iface_);
    server->send_event(udp_field_, payload_);

    // Arm the gate before subscribing: the subscription triggers the initial field
    // delivery, which the gate must intercept.
    notify_gate_->block_at(
            {.service_ = iface_.instance_.service_, .method_ = udp_field_.event_id_, .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    router_one->request_service(iface_.instance_);
    router_one->subscribe_field({udp_field_});
    ASSERT_TRUE(notify_gate_->wait_for_blocked());

    // Next cyclic offer must force a StopSubscribe(ttl=0) immediately followed by Subscribe.
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_sequence(
            {{sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, {sd::entry_type_e::SUBSCRIBE_EVENTGROUP, 3}},
            common::scaled_timeout(std::chrono::seconds(2))));

    // Release the withheld notification — recovery should complete.
    notify_gate_->block(false);
    message_checker checker{std::nullopt, iface_.instance_, udp_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, payload_};
    EXPECT_TRUE(router_one->message_record_.wait_for(checker));
}

TEST_F(test_field_resubscribe, field_resubscribe_recovers_after_lost_initial_notification_tcp) {
    // Same scenario as the UDP variant above, but for a TCP-transported field. The
    // initial notification travels over the TCP boardnet connection between the two
    // routers, so it must be intercepted there.
    prepare_ecus_and_apps();

    auto* router_one = ecu_one_.router_;
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    ASSERT_TRUE(setup_data_pipe(router_one_name_, router_two_name_, socket_role::client, notify_gate_->get_data_pipe()));

    server->offer(iface_);
    server->send_event(tcp_field_, payload_);

    // Arm the gate before subscribing: the subscription triggers the initial field
    // delivery, which the gate must intercept.
    notify_gate_->block_at(
            {.service_ = iface_.instance_.service_, .method_ = tcp_field_.event_id_, .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    router_one->request_service(iface_.instance_);
    router_one->subscribe_field({tcp_field_});
    ASSERT_TRUE(notify_gate_->wait_for_blocked());

    // Next cyclic offer must force a StopSubscribe(ttl=0) immediately followed by Subscribe.
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_sequence(
            {{sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, {sd::entry_type_e::SUBSCRIBE_EVENTGROUP, 3}},
            common::scaled_timeout(std::chrono::seconds(2))));

    // Release the withheld notification — recovery should complete.
    notify_gate_->block(false);
    message_checker tcp_checker{std::nullopt, iface_.instance_, tcp_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, payload_};
    EXPECT_TRUE(router_one->message_record_.wait_for(tcp_checker));
}

TEST_F(test_field_resubscribe, field_resubscribe_recovers_after_lost_initial_notification_mix_event_group) {
    // Same scenario as the variants above, though for a field in a mixed event group.
    prepare_ecus_and_apps();

    auto* router_one = ecu_one_.router_;
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    ASSERT_TRUE(setup_data_pipe(client_endpoint(), router_one_name_, socket_role::client, notify_gate_->get_data_pipe()));

    server->offer(iface_);
    server->send_event(mix_event_group_, payload_);

    // Arm the gate before subscribing: the subscription triggers the initial field
    // delivery, which the gate must intercept.
    notify_gate_->block_at({.service_ = iface_.instance_.service_,
                            .method_ = mix_event_group_.event_id_,
                            .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    router_one->request_service(iface_.instance_);
    router_one->subscribe_eventgroup_field({mix_event_group_});
    ASSERT_TRUE(notify_gate_->wait_for_blocked());

    // Next cyclic offer must force a StopSubscribe(ttl=0) immediately followed by Subscribe.
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_sequence(
            {{sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, {sd::entry_type_e::SUBSCRIBE_EVENTGROUP, 3}},
            common::scaled_timeout(std::chrono::seconds(2))));

    // Release the withheld notification — recovery should complete.
    notify_gate_->block(false);
    message_checker checker{std::nullopt, iface_.instance_, mix_event_group_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, payload_};
    EXPECT_TRUE(router_one->message_record_.wait_for(checker));
}

TEST_F(test_field_resubscribe, field_resubscribe_recovers_after_lost_initial_notification_shared_event_group) {
    // Same scenario as the variants above, though for fields in the same shared event groups.
    prepare_ecus_and_apps();

    auto* client_one = ecu_one_.apps_[ecu_one_client_name_];
    auto* client_two = ecu_one_.apps_[ecu_one_server_name_];
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    ASSERT_TRUE(setup_data_pipe(client_endpoint(), router_one_name_, socket_role::client, notify_gate_->get_data_pipe()));

    server->offer(iface_);
    server->send_event(shared_event_group_field_, payload_);

    // Arm the gate before subscribing: the subscription triggers the initial field
    // delivery, which the gate must intercept.
    notify_gate_->block_at({.service_ = iface_.instance_.service_,
                            .method_ = shared_event_group_field_.event_id_,
                            .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    client_one->request_service(iface_.instance_);
    client_two->request_service(iface_.instance_);
    client_one->subscribe_field({shared_event_group_field_});
    client_one->subscribe_field({iface_.instance_, iface_.fields_[3]});
    client_one->subscribe_field({iface_.instance_, iface_.events_[1]});
    client_two->subscribe_field({shared_event_group_field_});
    client_two->subscribe_field({iface_.instance_, iface_.fields_[3]});
    client_two->subscribe_field({iface_.instance_, iface_.events_[1]});

    ASSERT_TRUE(notify_gate_->wait_for_blocked());

    // Next cyclic offer must force a StopSubscribe(ttl=0) immediately followed by Subscribe.
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_sequence(
            {{sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, {sd::entry_type_e::SUBSCRIBE_EVENTGROUP, 3}},
            common::scaled_timeout(std::chrono::seconds(2))));

    // Release the withheld notification — recovery should complete.
    notify_gate_->block(false);
    message_checker checker{std::nullopt, iface_.instance_, shared_event_group_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                            payload_};
    EXPECT_TRUE(client_one->message_record_.wait_for(checker));
    EXPECT_TRUE(client_two->message_record_.wait_for(checker));

    // The next cyclic offer shall still trigger a StopSubscribe+Subscribe pair, because one the fields is still missing the initial
    // notification.
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_sequence(
            {{sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, {sd::entry_type_e::SUBSCRIBE_EVENTGROUP, 3}},
            common::scaled_timeout(std::chrono::seconds(2))));
}

TEST_F(test_field_resubscribe, field_resubscribe_repeats_while_value_still_missing) {
    // Retries are unbounded by design: as long as the field's value never arrives,
    // every subsequent cyclic offer must keep forcing the stop+subscribe pair.
    prepare_ecus_and_apps();

    auto* router_one = ecu_one_.router_;
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    ASSERT_TRUE(setup_data_pipe(client_endpoint(), router_one_name_, socket_role::client, notify_gate_->get_data_pipe()));

    server->offer(iface_);
    server->send_event(udp_field_, payload_);

    notify_gate_->block_at(
            {.service_ = iface_.instance_.service_, .method_ = udp_field_.event_id_, .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    router_one->request_service(iface_.instance_);
    router_one->subscribe_field({udp_field_});
    ASSERT_TRUE(notify_gate_->wait_for_blocked());

    // First forced resubscribe, on the next cyclic offer.
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_any({sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0},
                                                               common::scaled_timeout(std::chrono::seconds(2))));

    // The notification stays blocked — a second cyclic offer must trigger it again.
    ecu_one_sd_send_gate_->sd_record_.clear();
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_any({sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0},
                                                               common::scaled_timeout(std::chrono::seconds(2))));
}

TEST_F(test_field_resubscribe, no_forced_resubscribe_for_plain_event) {
    prepare_ecus_and_apps();

    auto* router_one = ecu_one_.router_;
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    ASSERT_TRUE(setup_data_pipe(client_endpoint(), router_one_name_, socket_role::client, notify_gate_->get_data_pipe()));

    server->offer(iface_);
    server->send_event(udp_field_, payload_);

    notify_gate_->block_at(
            {.service_ = iface_.instance_.service_, .method_ = udp_field_.event_id_, .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    router_one->request_service(iface_.instance_);
    router_one->subscribe_event({udp_field_});
    ASSERT_TRUE(notify_gate_->wait_for_blocked());

    ASSERT_FALSE(ecu_one_sd_send_gate_->sd_record_.wait_for_any({sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, std::chrono::seconds(2)))
            << "Plain event subscription should not trigger a forced resubscribe";
}

TEST_F(test_field_resubscribe, no_forced_resubscribe_once_initial_value_delivered) {
    // Regression guard for the healthy path: once the initial notification is
    // delivered normally, subsequent cyclic offers must not force a resubscribe.
    prepare_ecus_and_apps();

    auto* router_one = ecu_one_.router_;
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    server->offer(iface_);
    server->send_event(udp_field_, payload_);

    router_one->request_service(iface_.instance_);
    router_one->subscribe_field({udp_field_});

    message_checker checker{std::nullopt, iface_.instance_, udp_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, payload_};
    ASSERT_TRUE(router_one->message_record_.wait_for(checker));

    ecu_one_sd_send_gate_->sd_record_.clear();
    EXPECT_FALSE(ecu_one_sd_send_gate_->sd_record_.wait_for_any({sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, std::chrono::seconds(2)));
}

TEST_F(test_field_resubscribe, field_resubscribe_late_field_registration) {
    // Ensure the initial notification is properly tracked even if the field is registered after a notification has been sent for another
    // field of the eventgroup.
    prepare_ecus_and_apps();

    auto* client_one = ecu_one_.apps_[ecu_one_client_name_];
    auto* client_two = ecu_one_.apps_[ecu_one_server_name_];
    auto* server = ecu_two_.apps_[ecu_two_server_name_];

    ASSERT_TRUE(setup_data_pipe(client_endpoint(), router_one_name_, socket_role::client, notify_gate_->get_data_pipe()));

    server->offer(iface_);
    server->send_event(shared_event_group_field_, payload_);

    client_one->request_service(iface_.instance_);
    client_two->request_service(iface_.instance_);
    client_one->subscribe_field({shared_event_group_field_});
    client_two->subscribe_field({shared_event_group_field_});

    message_checker checker{std::nullopt, iface_.instance_, shared_event_group_field_.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                            payload_};
    EXPECT_TRUE(client_one->message_record_.wait_for(checker));
    EXPECT_TRUE(client_two->message_record_.wait_for(checker));

    ecu_one_sd_send_gate_->sd_record_.clear();

    // Clients now subscribe to another field in the same shared event group, which has not yet been notified.
    client_one->subscribe_field({iface_.instance_, iface_.fields_[3]});
    client_two->subscribe_field({iface_.instance_, iface_.fields_[3]});

    // The next cyclic offer must force a StopSubscribe(ttl=0) immediately followed by Subscribe, because the newly registered field has not
    // yet been notified.
    ASSERT_TRUE(ecu_one_sd_send_gate_->sd_record_.wait_for_sequence(
            {{sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP, 0}, {sd::entry_type_e::SUBSCRIBE_EVENTGROUP, 3}},
            common::scaled_timeout(std::chrono::seconds(2))));
}

const interface service_3344_instance_2{0x3344,
                                        {event_spec{0x8001, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE}},
                                        {event_spec{0x8002, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE}},
                                        0x2};

struct increased_initial_delay_with_multiple_instances : public base_fake_socket_fixture {
    ecu_setup ecu_one_{"ecu_one", boardnet::ecu_one_config, *socket_manager_};
    ecu_setup ecu_two_{
            "ecu_two",
            ecu_config{boardnet::ecu_two_config}.with_initial_delay(10000, 10000).add_interface({service_3344_instance_2}, 30502),
            *socket_manager_};
};

// This test verifies whether vSomeIP properly sents StopOffer messages during the initial_phase_wait or not.
// To ensure we never leave the initial_phase, we configure the initial_delay to 10 seconds.
TEST_F(increased_initial_delay_with_multiple_instances, sends_stop_offer_after_find_triggered_offer) {
    ecu_one_.add_guest({"guest_client", 0x1338});
    ecu_two_.add_guest({"guest_server", 0x1337});

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    auto* server = ecu_two_.apps_["guest_server"];

    // Offer and request the service
    server->offer(interfaces::boardnet::service_3344);
    client->request_service(interfaces::boardnet::service_3344.instance_);
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_),
                                                           common::scaled_timeout(std::chrono::seconds(2))));

    // Prepare Service Discovery Gate
    std::shared_ptr<someip_gate> router_one_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), ecu_one_.sd_endpoint().port()),
                                router_one_name_, socket_role::client, router_one_sd_gate->get_data_pipe()));
    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 0}, 1);

    // Stop offering the service
    server->stop_offer(interfaces::boardnet::service_3344.instance_);

    // Verify whether a StopService message was blocked or not, if it wasn't, we can safely assume it was not sent either.
    EXPECT_TRUE(router_one_sd_gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(2))));
}

// This test verifies whether vSomeIP properly sents StopOffer messages during the initial_phase_wait for each service-instance.
TEST_F(increased_initial_delay_with_multiple_instances, sends_stop_offer_for_each_service_instance) {
    ecu_one_.add_guest({"guest_client", 0x1338});
    ecu_two_.add_guest({"guest_server", 0x1337});

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    auto* server = ecu_two_.apps_["guest_server"];

    const auto first_instance = interfaces::boardnet::service_3344.instance_;
    const auto second_instance = service_3344_instance_2.instance_;

    server->offer(interfaces::boardnet::service_3344);
    server->offer(service_3344_instance_2);
    client->request_service(first_instance);
    client->request_service(second_instance);
    ASSERT_TRUE(client->availability_record_.wait_for_any(service_availability::available(first_instance)));
    ASSERT_TRUE(client->availability_record_.wait_for_any(service_availability::available(second_instance)));

    std::shared_ptr<someip_gate> router_one_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), ecu_one_.sd_endpoint().port()),
                                router_one_name_, socket_role::client, router_one_sd_gate->get_data_pipe()));

    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 0}, 1);
    server->stop_offer(first_instance);
    ASSERT_TRUE(router_one_sd_gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(2))));
    router_one_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(first_instance)));

    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 0}, 1);
    server->stop_offer(second_instance);
    ASSERT_TRUE(router_one_sd_gate->wait_for_blocked(common::scaled_timeout(std::chrono::seconds(2))));
    router_one_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(second_instance)));
}

struct sd_header_validation : public base_fake_socket_fixture {
    ecu_setup ecu_one_{"ecu_one", boardnet::ecu_one_config, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", boardnet::ecu_two_config, *socket_manager_};

    void prepare_ecus_and_apps() {
        ecu_one_.add_guest({"guest_client", 0x1338});
        ecu_two_.add_guest({"guest_server", 0x1337});

        ecu_one_.prepare();
        ecu_two_.prepare();

        ecu_one_.start_apps();
        ecu_two_.start_apps();
    }
};

TEST_F(sd_header_validation, prs_someipsd_00154_sd_offer_with_nonzero_client_id_is_rejected) {
    // SD messages shall have a Client-ID set to 0x0000.
    // Verify that an incoming SD OFFER message carrying a non-zero Client-ID is silently
    // discarded by the receiver (ECU one), while an identical message with Client-ID = 0x0000
    // is accepted and causes normal service availability signalling.

    prepare_ecus_and_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    client->request_service(interfaces::boardnet::service_3344.instance_);

    // construct_offer() builds a well-formed SD OFFER with Client-ID = 0x0000.
    // We then overwrite bytes 8–9 (VSOMEIP_CLIENT_POS_MIN) with a non-zero value to
    // simulate a non-compliant sender.
    auto malformed_offer = construct_offer({interfaces::boardnet::service_3344.instance_, interfaces::boardnet::service_3344.events_[0]},
                                           boardnet::ecu_two_config.unicast_ip_, 30501);
    // SOME/IP header: bytes 8–9 are the Client-ID (big-endian).
    malformed_offer[VSOMEIP_CLIENT_POS_MIN] = 0xDE;
    malformed_offer[VSOMEIP_CLIENT_POS_MIN + 1] = 0xAD;

    send_someip_sd_message(malformed_offer, ecu_two_.sd_endpoint(), ecu_one_.sd_endpoint());

    // ECU one must NOT process the offer; service availability must NOT be reported.
    EXPECT_FALSE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_)))
            << "SD OFFER with non-zero Client-ID (0xDEAD) was incorrectly accepted";
    ;

    // --- Valid offer: Client-ID = 0x0000 ---
    // The identical offer with the correct Client-ID must be accepted and trigger
    // service availability on ECU one.
    auto valid_offer = construct_offer({interfaces::boardnet::service_3344.instance_, interfaces::boardnet::service_3344.events_[0]},
                                       boardnet::ecu_two_config.unicast_ip_, 30501);

    send_someip_sd_message(valid_offer, ecu_two_.sd_endpoint(), ecu_one_.sd_endpoint());

    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_)))
            << "SD OFFER with Client-ID = 0x0000 was not accepted";
}
}

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sample_configurations.hpp"

#include "helpers/app.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/someip_gate.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>

namespace vsomeip_v3::testing {

std::string const router_one_name_ = "router_one";
std::string const router_two_name_ = "router_two";
std::string const ecu_two_server_name_ = "ecu_two_server";
std::string const ecu_one_client_name_ = "ecu_one_client";
std::string const ecu_one_server_name_ = "ecu_one_server";
std::string const ecu_two_client_name_ = "ecu_two_client";

struct test_sd_gate : public base_fake_socket_fixture {

    // Custom interface with 10 fields
    std::vector<event_spec> const fields_specs_{{0x800a, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE},
                                                {0x800b, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    interface field_service_{0x3344, {}, fields_specs_};
    ecu_config ecu_one_config_extended_{boardnet::ecu_one_config};
    ecu_config ecu_two_config_extended_{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_config_extended_.add_interface({field_service_}), *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", ecu_two_config_extended_.add_interface({field_service_}), *socket_manager_};

    void prepare_ecus_and_apps() {
        ecu_one_.add_guest({"guest_server", 0x1337});
        ecu_two_.add_guest({"guest_client", 0x1338});

        ecu_one_.prepare();
        ecu_two_.prepare();

        ecu_one_.start_apps();
        ecu_two_.start_apps();
    }
};

TEST_F(test_sd_gate, test_sd_unicat_gate_early_loading) {
    // Depict example where we setup a sending sd gate for unicast early loading (app not started).
    ecu_one_.add_guest({"guest_client", std::nullopt});
    ecu_two_.add_guest({"guest_server", std::nullopt});

    ecu_one_.prepare();
    ecu_two_.prepare();

    // Create the gate and prepare the pipe, will be exchanged as soon as the SD unicast endpoint from ecu two is binded.
    std::shared_ptr<someip_gate> router_two_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(ecu_two_.sd_endpoint(), router_two_name_, socket_role::server, router_two_sd_gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    auto* server = ecu_two_.apps_["guest_server"];

    // Block the pipe when ecu two tries to send an offer.
    router_two_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 3}, 1);
    client->request_service(field_service_.instance_);
    client->subscribe(field_service_);
    server->offer(field_service_);

    // Guarantee the gate has been blocked.
    ASSERT_TRUE(router_two_sd_gate->wait_for_blocked());
    // Check that no offer has been received.
    EXPECT_FALSE(client->availability_record_.wait_for_last(service_availability::available(field_service_.instance_),
                                                            std::chrono::milliseconds(250)));
    // Release the gate, message pushes through.
    router_two_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(field_service_.instance_)));

    server->stop_offer(field_service_.instance_);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(field_service_.instance_)));
}

TEST_F(test_sd_gate, test_sd_multicast_gate_late_loading) {
    // Depict example where we setup a receiving sd gate for multicast with late loading (app already started).
    ecu_one_.add_guest({"guest_client", std::nullopt});
    ecu_two_.add_guest({"guest_server", std::nullopt});

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* server = ecu_two_.apps_["guest_server"];
    auto* client = ecu_one_.apps_["guest_client"];

    // Create the gate and exchange the pipe immediatly.
    std::shared_ptr<someip_gate> router_one_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), ecu_one_.sd_endpoint().port()),
                                router_one_name_, socket_role::client, router_one_sd_gate->get_data_pipe()));

    // Block the pipe when ecu one receved an offer.
    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 3}, 1);

    client->request_service(field_service_.instance_);
    client->subscribe(field_service_);
    server->offer(field_service_);

    // Guarantee the gate has been blocked.
    ASSERT_TRUE(router_one_sd_gate->wait_for_blocked());
    // Check that no offer has been received.
    EXPECT_FALSE(client->availability_record_.wait_for_last(service_availability::available(field_service_.instance_)));
    // Release the gate, message is consumed and processed.
    router_one_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(field_service_.instance_)));

    server->stop_offer(field_service_.instance_);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(field_service_.instance_)));
}

struct test_someip_gate : public base_fake_socket_fixture {
    // TCP interface with one reliable field (0x8001) and one unreliable field (0x8002).
    // Used by blocks_notification and blocks_notification_matching_payload.
    std::vector<event_spec> const event_specs_both_{{0x8001, {0x1}, vsomeip::reliability_type_e::RT_RELIABLE},
                                                    {0x8002, {0x2}, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    interface both_interface{0x3345, {}, event_specs_both_};

    // UDP-only service (0x3347). Used by blocks_request_then_response.
    interface const udp_svc_{0x3347, {event_spec{0x8001, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE}}, {}};

    ecu_config ecu_one_cfg_{boardnet::ecu_one_config};
    ecu_config ecu_two_cfg_{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_cfg_, *socket_manager_};
    // both_interface gets unreliable=30501, reliable=30502; udp_svc_ gets unreliable=30503.
    ecu_setup ecu_two_{"ecu_two", ecu_two_cfg_.add_interface({both_interface, udp_svc_}), *socket_manager_};

    event_ids tcp_offered_field{both_interface.instance_, both_interface.fields_[0]};

    vsomeip::method_t const method_ = 0x0001;
    service_instance const si_{udp_svc_.instance_};
    // router_two's UDP unicast endpoint for udp_svc_ (port 30503).
    boost::asio::ip::udp::endpoint const svc_ep_{boardnet::ecu_two_config.unicast_ip_, 30503};

    // Shared bring-up: remote provider (0x0555) auto-answering si_, ecu_one's router and a "keeper"
    // holding a concrete request. Guests + prepare() must precede this.
    void bring_up_provider_and_keeper(request const& _req, std::vector<unsigned char> const& _rsp_payload) {
        // Provider side up first: offer the remote UDP service and auto-answer requests.
        ecu_two_.start_apps();
        auto* server = ecu_two_.apps_[ecu_two_server_name_];
        ASSERT_NE(server, nullptr);
        server->offer(si_);
        server->answer_request(_req, [_rsp_payload] { return _rsp_payload; });

        // Consumer routing manager up, then the keeper (keeps the remote service referenced).
        ecu_one_.start_router();
        auto* keeper = ecu_one_.start_one("keeper");
        ASSERT_NE(keeper, nullptr);
        ASSERT_TRUE(keeper->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        keeper->request_service(si_);
        ASSERT_TRUE(keeper->availability_record_.wait_for_last(service_availability::available(si_)));
    }
};

TEST_F(test_someip_gate, blocks_notification) {
    // Depict example where a someip_gate installed on the boardnet connection (early
    // loading) blocks the first notification of a field, then releases it.
    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);

    ecu_one_.prepare();
    ecu_two_.prepare();

    // Install the gate before the connection forms so the pipe is in place
    // once router_one connects to router_two's boardnet server.
    auto gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(router_one_name_, router_two_name_, socket_role::client, gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    std::vector<unsigned char> const payload{0x5, 0x3};

    ecu_two_server_->offer(both_interface);
    ecu_two_server_->send_event(tcp_offered_field, payload);

    // Arm the gate: block the very first matching notification.
    gate->block_at({.service_ = both_interface.instance_.service_,
                    .method_ = tcp_offered_field.event_id_,
                    .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    router_one->request_service(both_interface.instance_);
    router_one->subscribe_event({tcp_offered_field});

    // The subscription triggers the initial field delivery — the gate must intercept it.
    ASSERT_TRUE(gate->wait_for_blocked());

    message_checker checker{std::nullopt, both_interface.instance_, tcp_offered_field.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                            payload};

    // Gate is holding the notification — it must not have arrived yet.
    EXPECT_FALSE(router_one->message_record_.wait_for(checker, std::chrono::milliseconds(500)));

    // Release the gate; the buffered notification is pushed through.
    gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for(checker));
}

TEST_F(test_someip_gate, blocks_notification_matching_payload) {
    // Depict example where a someip_gate with a payload predicate is used to selectively
    // block only notifications whose first payload byte is 0xFF, while letting others pass.
    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);

    ecu_one_.prepare();
    ecu_two_.prepare();

    // Install the gate in early loading — no block_at yet, so the gate is transparent.
    auto gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(router_one_name_, router_two_name_, socket_role::client, gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    std::vector<unsigned char> const payload_safe{0x5, 0x3};
    std::vector<unsigned char> const payload_blocked{0xFF, 0x3};

    // Let the initial field event (payload_safe) pass through with no active trigger.
    ecu_two_server_->offer(both_interface);
    ecu_two_server_->send_event(tcp_offered_field, payload_safe);
    router_one->request_service(both_interface.instance_);
    router_one->subscribe_event({tcp_offered_field});

    message_checker safe_checker{std::nullopt, both_interface.instance_, tcp_offered_field.event_id_,
                                 vsomeip::message_type_e::MT_NOTIFICATION, payload_safe};
    ASSERT_TRUE(router_one->message_record_.wait_for(safe_checker));

    // Arm the gate with a payload predicate: only block notifications starting with 0xFF.
    gate->block_at({.service_ = both_interface.instance_.service_,
                    .method_ = tcp_offered_field.event_id_,
                    .type_ = vsomeip::message_type_e::MT_NOTIFICATION,
                    .payload_ = [](std::shared_ptr<vsomeip::payload> p) { return p && p->get_length() > 0 && p->get_data()[0] == 0xFF; }});

    ecu_two_server_->send_event(tcp_offered_field, payload_blocked);

    ASSERT_TRUE(gate->wait_for_blocked());

    message_checker blocked_checker{std::nullopt, both_interface.instance_, tcp_offered_field.event_id_,
                                    vsomeip::message_type_e::MT_NOTIFICATION, payload_blocked};

    // Gate is holding the 0xFF notification — it must not have arrived yet.
    EXPECT_FALSE(router_one->message_record_.wait_for(blocked_checker, std::chrono::milliseconds(500)));

    // Release the gate; the buffered notification is pushed through.
    gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for(blocked_checker));
}

TEST_F(test_someip_gate, lets_through_n_notifications) {
    // Verifies the count-based triggering: block_at(trigger, N) lets the first
    // N-1 messages through and blocks on the Nth.
    //
    // 1. Arm gate with count=3 → lets 2 through, blocks on the 3rd.
    // 2. Send 3 notifications with distinct payloads.
    // 3. Client receives the first two.
    // 4. Third is held back until gate is released.

    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);
    ecu_one_.prepare();
    ecu_two_.prepare();

    auto gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(router_one_name_, router_two_name_, socket_role::client, gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    ecu_two_server_->offer(both_interface);
    router_one->request_service(both_interface.instance_);
    router_one->subscribe_event({tcp_offered_field});
    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(both_interface.instance_)));
    ASSERT_TRUE(router_one->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(tcp_offered_field)));
    router_one->message_record_.clear();

    // 1. Arm: let first 2 through, block on the 3rd.
    someip_gate::trigger const trigger{.service_ = both_interface.instance_.service_,
                                       .method_ = tcp_offered_field.event_id_,
                                       .type_ = vsomeip::message_type_e::MT_NOTIFICATION};
    gate->block_at(trigger, 3);

    const std::vector<unsigned char> p1{0x11}, p2{0x22}, p3{0x33};
    const auto& ev = tcp_offered_field; // TCP (reliable) field
    const auto& si = both_interface.instance_;

    message_checker const c1{client_session{0, 1}, si, ev.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, std::nullopt};
    message_checker const c2{client_session{0, 2}, si, ev.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, std::nullopt};
    message_checker const c3{client_session{0, 3}, si, ev.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, std::nullopt};

    // 2. Send 3 notifications.
    ecu_two_server_->send_event(ev, p1);
    ecu_two_server_->send_event(ev, p2);
    ecu_two_server_->send_event(ev, p3);

    // Gate must have triggered on the 3rd.
    ASSERT_TRUE(gate->wait_for_blocked());

    // 3. First two must arrive.
    EXPECT_TRUE(router_one->message_record_.wait_for_any(c1));
    EXPECT_TRUE(router_one->message_record_.wait_for_last(c2));

    // 4. Third is buffered — must not arrive yet.
    EXPECT_FALSE(router_one->message_record_.wait_for_any(c3, std::chrono::milliseconds(300)));

    // Release: 3rd notification is forwarded.
    gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for_any(c3));
}

TEST_F(test_someip_gate, blocks_request_then_response) {
    // Verify that a someip_gate installed on router_two's service socket can first block
    // an incoming REQUEST, and then, once the request is released, block the outgoing
    // RESPONSE before it reaches the client.

    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    std::vector<unsigned char> const req_payload{0x01};
    std::vector<unsigned char> const rsp_payload{0x42};

    request const req{si_, method_, vsomeip::message_type_e::MT_REQUEST, false /* UDP */, req_payload};

    // Server offers the service and registers a handler that replies with rsp_payload.
    ecu_two_server_->offer(si_);
    ecu_two_server_->answer_request(req, [rsp_payload] { return rsp_payload; });

    // Client discovers the service via SD.
    router_one->request_service(si_);
    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(si_)));

    // Install both gates after apps have started so that the UDP socket at svc_ep_ is
    // already bound and replace_pipe is called directly (not deferred to pending map).
    auto request_gate = someip_gate::create();
    auto response_gate = someip_gate::create();

    // request_gate on the receiver pipe: blocks REQUESTs arriving at router_two's socket.
    ASSERT_TRUE(setup_data_pipe(svc_ep_, router_two_name_, socket_role::client, request_gate->get_data_pipe()));
    // response_gate on the sender pipe: blocks RESPONSEs leaving router_two's socket.
    ASSERT_TRUE(setup_data_pipe(svc_ep_, router_two_name_, socket_role::server, response_gate->get_data_pipe()));

    // --- Phase 1: block the REQUEST ---
    request_gate->block_at({.service_ = si_.service_, .method_ = method_, .type_ = vsomeip::message_type_e::MT_REQUEST});

    router_one->send_request(req);
    ASSERT_TRUE(request_gate->wait_for_blocked());

    message_checker rsp_checker{std::nullopt, si_, method_, vsomeip::message_type_e::MT_RESPONSE, rsp_payload};
    // Request is held at the gate — no response can have arrived at the client yet.
    EXPECT_FALSE(router_one->message_record_.wait_for(rsp_checker, std::chrono::milliseconds(500)));

    // --- Phase 2: arm the response gate, then release the request ---
    // Arm response_gate before releasing the request to avoid a race where the response
    // is sent before the gate is armed.
    response_gate->block_at({.service_ = si_.service_, .method_ = method_, .type_ = vsomeip::message_type_e::MT_RESPONSE});

    // Release the request: it reaches the server, which replies; the reply is intercepted
    // by response_gate before it leaves router_two.
    request_gate->block(false);
    ASSERT_TRUE(response_gate->wait_for_blocked());

    // Response is still held — client must not have received it.
    EXPECT_FALSE(router_one->message_record_.wait_for(rsp_checker, std::chrono::milliseconds(500)));

    // Release the response gate — the response is now forwarded to the client.
    response_gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for(rsp_checker));
}

// A response/error addressed to a departed consumer whose client id was recycled to a different
// application must NOT be delivered to the new owner of the id (which never requested the service)
// — the routing manager must drop it.
TEST_F(test_someip_gate, orphan_response_after_client_id_reuse_is_dropped) {
    ecu_one_.add_guest({"keeper", std::nullopt});
    ecu_one_.add_guest({"client_a", std::nullopt});
    ecu_one_.add_guest({"client_b", std::nullopt});
    ecu_two_.add_guest({ecu_two_server_name_, 0x0555});

    ecu_one_.prepare();
    ecu_two_.prepare();

    std::vector<unsigned char> const rsp_payload{0x42};
    request const req{si_, method_, vsomeip::message_type_e::MT_REQUEST, false /* UDP */, {0x01}};
    ASSERT_NO_FATAL_FAILURE(bring_up_provider_and_keeper(req, rsp_payload));

    // Client A (the original requester) takes the next free id.
    auto* client_a = ecu_one_.start_one("client_a");
    ASSERT_NE(client_a, nullptr);
    ASSERT_TRUE(client_a->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    vsomeip::client_t const reused_id = client_a->get_client_id();
    ASSERT_TRUE(reused_id != 0x0000 && reused_id != 0xFFFF) << "client A did not get a valid client id";

    client_a->request_service(si_);
    ASSERT_TRUE(client_a->availability_record_.wait_for_last(service_availability::available(si_)));

    // Hold the RESPONSE at the provider's egress so it cannot reach the consumer yet.
    auto response_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(svc_ep_, router_two_name_, socket_role::server, response_gate->get_data_pipe()));
    response_gate->block_at({.service_ = si_.service_, .method_ = method_, .type_ = vsomeip::message_type_e::MT_RESPONSE});

    client_a->send_request(req);
    ASSERT_TRUE(response_gate->wait_for_blocked()) << "response was not held at the provider egress";

    // Client A leaves; wait until its routing connection is fully torn down so the routing
    // manager releases the client id before B claims it. Watch armed before the stop (the trigger).
    auto drop_watch = watch_connection_drop("client_a", ecu_one_.router_name_);
    ecu_one_.stop_one("client_a");
    ASSERT_TRUE(drop_watch.wait());

    // Client B joins and takes over the very same client id — but never requests the service.
    auto* client_b = ecu_one_.start_one("client_b");
    ASSERT_NE(client_b, nullptr);
    ASSERT_TRUE(client_b->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    ASSERT_EQ(client_b->get_client_id(), reused_id) << "client B did not reuse client A's id";

    // Release the held response: it now reaches the consumer routing manager, addressed to the
    // reused id. Because B never requested this remote service, the RM must drop it.
    response_gate->block(false);

    message_checker const rsp_checker{std::nullopt, si_, method_, vsomeip::message_type_e::MT_RESPONSE, rsp_payload};
    EXPECT_FALSE(client_b->message_record_.wait_for(rsp_checker, std::chrono::milliseconds(500)))
            << "client B received an orphaned response for a service it never requested (client-id reuse cross-talk)";
}

// A consumer requests a remote service, sends a request, then releases the service before the response arrives. The routing manager must
// drop the now-orphaned response instead of delivering it to the (still-alive) consumer.
TEST_F(test_someip_gate, orphan_response_after_release_service_is_dropped) {
    ecu_one_.add_guest({"keeper", std::nullopt});
    ecu_one_.add_guest({"consumer", std::nullopt});
    ecu_two_.add_guest({ecu_two_server_name_, 0x0555});

    ecu_one_.prepare();
    ecu_two_.prepare();

    std::vector<unsigned char> const rsp_payload{0x24};
    request const req{si_, method_, vsomeip::message_type_e::MT_REQUEST, false /* UDP */, {0x01}};
    ASSERT_NO_FATAL_FAILURE(bring_up_provider_and_keeper(req, rsp_payload));

    auto* consumer = ecu_one_.start_one("consumer");
    ASSERT_NE(consumer, nullptr);
    ASSERT_TRUE(consumer->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    consumer->request_service(si_);
    ASSERT_TRUE(consumer->availability_record_.wait_for_last(service_availability::available(si_)));

    // Hold the RESPONSE at the provider's egress.
    auto response_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(svc_ep_, router_two_name_, socket_role::server, response_gate->get_data_pipe()));
    response_gate->block_at({.service_ = si_.service_, .method_ = method_, .type_ = vsomeip::message_type_e::MT_RESPONSE});

    consumer->send_request(req);
    ASSERT_TRUE(response_gate->wait_for_blocked()) << "response was not held at the provider egress";

    // The consumer releases the service while the response is still in flight. Wait until the
    // RELEASE_SERVICE command has reached the routing manager so is_requester() reflects it
    // (the held response still has to traverse the boardnet, so it arrives strictly later).
    consumer->release_service(si_);
    ASSERT_TRUE(wait_for_command("consumer", ecu_one_.router_name_, protocol::id_e::RELEASE_SERVICE_ID, socket_role::server));

    // Release the held response; the RM must drop it — the consumer is no longer a requester.
    response_gate->block(false);

    message_checker const rsp_checker{std::nullopt, si_, method_, vsomeip::message_type_e::MT_RESPONSE, rsp_payload};
    EXPECT_FALSE(consumer->message_record_.wait_for(rsp_checker, std::chrono::milliseconds(500)))
            << "consumer received a response for a service it had already released";
}

// A client that requested the service under ANY_INSTANCE must still receive responses for a concrete
// instance, even while another client (the keeper) holds a concrete (service, instance) request at
// the same time. The concrete request materializes a concrete requested_services_ node next to the
// ANY_INSTANCE node; is_requester() must union both. A fallback-only lookup would see only the
// concrete node, treat the wildcard requester as a non-requester, and wrongly drop its response.
TEST_F(test_someip_gate, wildcard_requester_still_receives_response) {
    ecu_one_.add_guest({"keeper", std::nullopt});
    // Name sorts before the "router_*" auxiliary contexts so the fake-socket io_context
    // assignment does not race with the keeper's remote connection bring-up.
    ecu_one_.add_guest({"any_consumer", std::nullopt});
    ecu_two_.add_guest({ecu_two_server_name_, 0x0555});

    ecu_one_.prepare();
    ecu_two_.prepare();

    std::vector<unsigned char> const rsp_payload{0x37};
    request const req{si_, method_, vsomeip::message_type_e::MT_REQUEST, false /* UDP */, {0x01}};
    // The keeper holds a CONCRETE (service, instance) request to si_ — the coexistence trigger.
    ASSERT_NO_FATAL_FAILURE(bring_up_provider_and_keeper(req, rsp_payload));

    // A second consumer requests the same service, but under ANY_INSTANCE.
    auto* consumer = ecu_one_.start_one("any_consumer");
    ASSERT_NE(consumer, nullptr);
    ASSERT_TRUE(consumer->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    consumer->request_service(service_instance{si_.service_, vsomeip::ANY_INSTANCE});
    // The concrete instance is reported available to the ANY_INSTANCE requester as well.
    ASSERT_TRUE(consumer->availability_record_.wait_for_last(service_availability::available(si_)));

    // The wildcard consumer issues a request to the concrete instance; its response must be
    // delivered (not dropped as an orphan), because it is a legitimate requester via ANY_INSTANCE.
    consumer->send_request(req);
    message_checker const rsp_checker{std::nullopt, si_, method_, vsomeip::message_type_e::MT_RESPONSE, rsp_payload};
    EXPECT_TRUE(consumer->message_record_.wait_for(rsp_checker)) << "wildcard (ANY_INSTANCE) requester did not receive its response";
}
}

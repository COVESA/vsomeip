// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/message_checker.hpp"

#include "internal.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <unistd.h>

#include <chrono>
#include <string>
#include <vector>

namespace vsomeip_v3::testing {
namespace {
std::string const routingmanager_name_{"routingmanagerd"};
std::string const server_name_{"server"};
std::string const client_name_{"client"};
constexpr client_t routingmanager_id_{0x0100};
constexpr client_t server_id_{0x3489};
constexpr client_t client_id_{0x3490};

constexpr service_instance service_instance_{0x3344, 0x0001};
constexpr vsomeip::method_t method_{0x1111};

/// Credentials a local peer is seen with by default: the fake UDS socket has no real SO_PEERCRED
/// to read, so unless a test configures one via socket_manager::set_peer_identity(), it reports a
/// zeroed identity (see fake_uds_socket::get_peer_credentials). These are also the credentials the
/// routing manager reports for the offering client in ROUTING_INFO.
security_credentials peer_offers_and_requests() {
    return {0, 0, {service_instance_}, {service_instance_}};
}

security_credentials peer_requests_only() {
    return {0, 0, {}, {service_instance_}};
}

/// Credentials a peer whose identity the routing manager cannot tell is seen with: ANY_UID/ANY_GID
/// in ROUTING_INFO means "unknown to the host" (see routing_info_entry_data).
security_credentials unknown_peer() {
    return {ANY_UID, ANY_GID, {service_instance_}, {service_instance_}};
}

/// An application checks its own credentials -- the real uid/gid -- when it registers. Granting no
/// offer permission here matters: as root the real uid/gid are 0/0 too, so an offer permission on
/// this entry would re-authorize the very peer the deny case must reject.
security_credentials own_credentials() {
    return {getuid(), getgid(), {}, {service_instance_}};
}

/// UDS routing with security in enforcing mode. Local TCP would not do: check_credentials() passes
/// unconditionally as soon as the sec_client carries a port.
ecu_config enforcing_config(std::vector<security_credentials> _allowed) {
    ecu_config its_config;
    its_config.apps_ = {{routingmanager_name_, routingmanager_id_}, {server_name_, server_id_}};
    its_config.routing_config_ = routingmanager_name_;
    its_config.sd_ = false;
    its_config.security_ = security_config{.check_credentials_ = true, .allowed_ = std::move(_allowed)};
    return its_config;
}
}

struct send_offer_check_fixture : public base_fake_socket_fixture {
    /// Starts routing manager and offering server from the shared configuration, and the consumer
    /// from one of its own that either authorizes the offerer's credentials to offer or does not.
    /// _extra_client_policy adds further recognized (but not necessarily offer-authorized)
    /// identities to the consumer's policy, e.g. to let it accept a connection from a peer whose
    /// real identity differs from the one reported for it.
    /// _offerer_identity, when given, is what the offerer's peers -- the routing manager included --
    /// read off its connections instead of the default zeroed identity.
    void start_ecu(security_credentials const& _client_view_of_the_offerer, std::vector<security_credentials> _extra_client_policy = {},
                   std::optional<vsomeip_sec_client_t> const& _offerer_identity = {}) {
        std::vector<security_credentials> its_client_policy{_client_view_of_the_offerer, own_credentials()};
        its_client_policy.insert(its_client_policy.end(), _extra_client_policy.begin(), _extra_client_policy.end());
        auto its_client_config = enforcing_config(std::move(its_client_policy));
        its_client_config.apps_.push_back({client_name_, client_id_});
        ecu_.add_app({client_name_, client_id_}, std::move(its_client_config));
        ecu_.prepare();
        ecu_.start_router();
        ASSERT_NE(ecu_.router_, nullptr);

        if (_offerer_identity) {
            socket_manager_->set_peer_identity(server_name_, *_offerer_identity);
        }

        server_ = ecu_.start_one(server_name_);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer(service_instance_);
        server_->answer_request(request_, [] { return std::vector<unsigned char>{0x02, 0x03}; });

        client_ = ecu_.start_one(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        client_->request_service(service_instance_);
        ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    }

    request request_{service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message_checker const request_checker_{std::nullopt, service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, std::nullopt};

    app* client_{};
    app* server_{};

    ecu_setup ecu_{"offer_check", enforcing_config({peer_offers_and_requests(), unknown_peer(), own_credentials()}), *socket_manager_};
};

// The consumer's policy does not authorize the offering client's credentials to offer the targeted
// service, so the request must be skipped before it reaches the offerer.
TEST_F(send_offer_check_fixture, request_is_skipped_when_the_offerer_is_not_allowed_to_offer) {
    start_ecu(peer_requests_only());

    client_->send_request(request_);

    EXPECT_FALSE(server_->message_record_.wait_for(request_checker_, std::chrono::milliseconds(500)))
            << "request reached the offerer despite the send-side offer check";
}

// Same setup with the offer authorization in place: the check must not get in the way.
TEST_F(send_offer_check_fixture, request_passes_when_the_offerer_is_allowed_to_offer) {
    start_ecu(peer_offers_and_requests());

    client_->send_request(request_);

    EXPECT_TRUE(server_->message_record_.wait_for(request_checker_)) << "request did not reach the offerer";
}

// The identity ROUTING_INFO reported for the offerer (and that the consumer's offer check
// authorized) can go stale relative to the offerer's real identity by the time the consumer
// actually connects to it -- e.g. the client id got reused by a different real process. The
// consumer must not trust the stale reported identity once the real one is known: the request
// must not reach a peer whose real identity no longer matches what was reported for it.
TEST_F(send_offer_check_fixture, request_is_skipped_when_the_real_offerer_identity_no_longer_matches_the_reported_one) {
    security_credentials const impersonator{5002, 5002, {}, {}};
    start_ecu(peer_offers_and_requests(), {impersonator});

    socket_manager_->set_peer_identity(
            server_name_,
            vsomeip_sec_client_t{.user = impersonator.uid_, .group = impersonator.gid_, .host = 0, .port = VSOMEIP_SEC_PORT_UNUSED});

    client_->send_request(request_);

    EXPECT_FALSE(server_->message_record_.wait_for(request_checker_, std::chrono::milliseconds(500)))
            << "request reached a peer whose real identity no longer matched the one reported for it";
}

// The routing manager does not always know the offerer's credentials (e.g. its routing connection
// is gone by the time ROUTING_INFO is built); it then reports ANY_UID/ANY_GID. The consumer can
// neither run the offer check nor verify the peer of a direct connection, so it must fall back to
// routing the request through the host -- which did check the offer when it accepted it.
TEST_F(send_offer_check_fixture, request_is_routed_via_the_host_when_the_offerer_identity_is_unknown) {
    start_ecu(peer_offers_and_requests(), {unknown_peer()},
              vsomeip_sec_client_t{.user = ANY_UID, .group = ANY_GID, .host = 0, .port = VSOMEIP_SEC_PORT_UNUSED});

    client_->send_request(request_);

    EXPECT_TRUE(server_->message_record_.wait_for(request_checker_)) << "request did not reach the offerer";
    EXPECT_EQ(0u, socket_manager_->count_established_connections(client_name_, server_name_))
            << "consumer connected directly to a peer whose identity it could not verify";
}
}

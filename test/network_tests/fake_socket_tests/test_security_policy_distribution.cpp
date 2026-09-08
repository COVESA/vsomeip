// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_message.hpp"
#include "helpers/ecu_setup.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

namespace vsomeip_v3::testing {
namespace {
std::string const routingmanager_name_{"routingmanagerd"};
std::string const client_name_{"client"};
constexpr client_t routingmanager_id_{0x0100};
constexpr client_t client_id_{0x3490};

/// Credentials the routing manager sees for an accepted local connection. The fake UDS socket does
/// not implement SO_PEERCRED, so the sec_client stays zeroed
/// (see fake_uds_socket::get_peer_credentials).
const security_credentials peer_credentials_{0, 0};

security_credentials own_credentials() {
    return {getuid(), getgid()};
}

security_credentials foreign_credentials() {
    return {static_cast<uid_t>(getuid() + 1), getgid()};
}

/// UDS routing with security in enforcing mode. Local TCP routing would not do: check_credentials()
/// passes unconditionally as soon as the sec_client carries a port. The routing manager needs a
/// fixed client id in every configuration that enables security.
ecu_config enforcing_config(std::vector<application_config> _apps, std::vector<security_credentials> _allowed) {
    ecu_config its_config;
    its_config.apps_ = std::move(_apps);
    its_config.routing_config_ = routingmanager_name_;
    its_config.sd_ = false;
    its_config.security_ = security_config{.check_credentials_ = true, .allowed_ = std::move(_allowed)};
    return its_config;
}
}

struct test_security_policy_distribution : public base_fake_socket_fixture {
    test_security_policy_distribution() {
        // registered on the connection, so the record survives the reconnects a refused
        // registration causes
        set_custom_command_handler(
                client_name_, routingmanager_name_,
                [this](command_message const& _msg) {
                    commands_to_client_.record(_msg.id_);
                    return false; // never drop a message
                },
                socket_role::server);
    }

    /// Starts the routing manager and gives the client a configuration of its own, allowing only
    /// _client_allowed. The client itself is started by the test.
    void start_router(std::vector<security_credentials> _client_allowed) {
        ecu_.add_app(
                {client_name_, client_id_},
                enforcing_config({{routingmanager_name_, routingmanager_id_}, {client_name_, client_id_}}, std::move(_client_allowed)));
        ecu_.prepare();
        ecu_.start_router();
        ASSERT_NE(ecu_.router_, nullptr);
        ASSERT_TRUE(ecu_.router_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    /// Mimics a security manager handing a policy for _credentials to the routing manager at
    /// runtime: it is applied locally and cached for every client registering afterwards.
    void distribute_policy(security_credentials const& _credentials) {
        ecu_.router_->update_security_policy_configuration(_credentials.uid_, _credentials.gid_);
    }

    // Declared before ecu_ so it is destroyed after it: ecu_'s apps (and their background IO
    // threads, which invoke the custom command handler that records into this) must be fully
    // stopped before commands_to_client_ is torn down, otherwise a still-running app thread can
    // race with the destructor of its underlying vector.
    attribute_recorder<protocol::id_e> commands_to_client_;

    /// peer_credentials_ to accept the incoming local connections, own_credentials() because the
    /// routing manager host registers itself through a routing_manager_client as well.
    ecu_setup ecu_{"security", enforcing_config({{routingmanager_name_, routingmanager_id_}}, {peer_credentials_, own_credentials()}),
                   *socket_manager_};
};

TEST_F(test_security_policy_distribution, distributed_policy_arrives_after_the_client_checked_its_own_credentials) {
    // the policy covering the client's own uid/gid is only known to the routing manager
    start_router({foreign_credentials()});
    distribute_policy(own_credentials());

    auto const its_drop = watch_connection_drop(client_name_, routingmanager_name_);
    ASSERT_NE(ecu_.start_one(client_name_), nullptr);

    // the routing manager does forward the cached policy to the registering client ...
    ASSERT_TRUE(commands_to_client_.wait_for_any(protocol::id_e::DISTRIBUTE_SECURITY_POLICIES_ID));

    // ... but routing_manager_stub::on_register_application sends it only after ASSIGN_CLIENT_ACK was
    // queued, and the client checks its own credentials while handling that ack. The check therefore
    // always fails on a fresh connection and the client starts the whole registration over.
    // Invert this expectation once the distribution happens before the client decides.
    EXPECT_FALSE(its_drop.wait());
}

TEST_F(test_security_policy_distribution, policy_in_the_static_configuration_allows_registration) {
    // same setup, but the client does not depend on the distributed policy
    start_router({own_credentials()});
    distribute_policy(own_credentials());

    auto* its_client = ecu_.start_one(client_name_);
    ASSERT_NE(its_client, nullptr);
    EXPECT_TRUE(its_client->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    EXPECT_EQ(1u, connection_count(client_name_, routingmanager_name_));
}
}

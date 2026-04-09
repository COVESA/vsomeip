// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sample_configurations.hpp"
#include "sample_interfaces.hpp"

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/service_state.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

namespace vsomeip_v3::testing {

/// Base fixture for hybrid-mode tests.
///
/// All hybrid tests operate against a single router (router_one at 127.0.0.1).
/// Apps on the same host connect to the router via UDS or TCP depending on the
/// uds_preferred setting. Apps on a remote address (127.0.0.2) are guests that
/// are forced to use TCP to reach router_one.
struct test_hybrid_mode : public base_fake_socket_fixture {
    [[nodiscard]] bool successfully_registered(app* _app) {
        return _app && _app->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED);
    }

    [[nodiscard]] bool await_service(app* _client, std::optional<service_availability> expected_availability = std::nullopt) {
        if (!expected_availability) {
            expected_availability = service_availability::available(service_instance_);
        }
        return _client->availability_record_.wait_for_last(*expected_availability);
    }

    [[nodiscard]] bool subscribe_to_event(app* _client) {
        _client->subscribe_event(offered_event_);
        return _client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_));
    }

    [[nodiscard]] bool verify_connection_uses_uds(std::string const& _from, std::string const& _to) {
        auto socket_type = get_connection_socket_type(_from, _to);
        return socket_type && socket_type.value() == vsomeip_v3::testing::socket_type::uds;
    }

    [[nodiscard]] bool verify_connection_uses_tcp(std::string const& _from, std::string const& _to) {
        auto socket_type = get_connection_socket_type(_from, _to);
        return socket_type && socket_type.value() == vsomeip_v3::testing::socket_type::tcp;
    }

    interface boardnet_interface_{0x3344};
    service_instance service_instance_{boardnet_interface_.instance_};
    event_ids offered_event_{boardnet_interface_.events_[0]};
};

TEST_F(test_hybrid_mode, test_boardnet_with_hybrid_mode_on_both_sides) {
    // 0. Router One + Client are configurated to prefer UDS
    //    Server is configured to also use UDS, however the router is in another machine,
    //    therefore it will be forced to use TCP. This test ensures that the router can handle
    //    this mixed scenario, and that the client can still subscribe to the offered service and
    //    receive the events.
    // 1. Router initiates and registers itself in the first machine
    // 2. Client (Consumer) initiates and registers to router one in the first machine
    // 3. Server (Provider) initiates and registers to router one in the second machine
    // 4. Server offers a service
    // 5. Client requests the service, receives the availability and subscribes to an event

    // Host ECU: router_one + in-process client and remote server (uds_preferred guest).
    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    // 1.
    // Router will be listening on UDS, but also on TCP, as it needs to be reachable for
    // both clients and servers, which can be configured to prefer UDS or TCP. But it will prefer UDS over TCP.
    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // 2.
    // As the client is using the same configuration, it will use UDS to connect to the router
    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one")) << "Expected UDS connection between host_client and router_one";

    // 3.
    // As the server is configured to prefer UDS, but it is in another machine,
    // it will be forced to use TCP to connect to the router.
    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one")) << "Expected TCP connection between guest_server and router_one";

    // 4.
    guest_server->offer(boardnet_interface_);

    // 5.
    // The connection between client and the server will be handled via TCP,
    // as the server is not reachable via UDS.
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "Expected TCP connection between host_client and guest_server";
}

TEST_F(test_hybrid_mode, test_internal_communication_with_hybrid_mode) {
    // 0. This test has the intent to ensure that internal Client<->Server communication works via UDS
    //    w/ UDS configured as preferred
    // 1. Router initiates and registers itself
    // 2. Client (Consumer) initiates and registers to router one in the same machine
    // 3. Server (Provider) initiates and registers to router one in the same machine
    // 4. Server offers a service
    // 5. Client requests the service, receives the availability and subscribes to an event

    // Host ECU: router_one + co-located client and server, all uds_preferred.
    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_app("host_server");
    host.prepare();
    host.start_apps();

    // 1.
    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // 2.
    // As the client is using the same configuration, it will use UDS to connect to the router
    auto* host_client = host.apps_["host_client"];
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one")) << "Expected UDS connection between host_client and router_one";

    // 3.
    // As the server is using the same configuration, it will use UDS to connect to the router
    auto* host_server = host.apps_["host_server"];
    ASSERT_TRUE(successfully_registered(host_server));
    ASSERT_TRUE(verify_connection_uses_uds("host_server", "router_one")) << "Expected UDS connection between host_server and router_one";

    // 4.
    host_server->offer(service_instance_);

    // 5.
    // Connection between client and the server will be handled via UDS, as both are in the same machine and prefer UDS.
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "host_server")) << "Expected UDS connection between host_client and host_server";
}

TEST_F(test_hybrid_mode, test_boardnet_with_hybrid_mode_on_client_side) {
    // 0. Router One + Client are configurated to prefer UDS but not the
    // server at the second machine
    // 1. Router initiates and registers itself in the first machine
    // 2. Client (Consumer) initiates and registers to router one in the first machine
    // 3. Server (Provider) initiates and registers to router one in the second machine
    // 4. Server offers a service
    // 5. Client requests the service, receives the availability and subscribes to an event

    // Host ECU: router_one + client (uds_preferred) and remote server (TCP-only guest).
    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_server", std::nullopt}); // uds_preferred=false (TCP-only)
    host.prepare();
    host.start_router();

    // 1.
    // Router will be listening on UDS, but also on TCP, as it needs to be reachable for
    // both clients and servers, which can be configured to prefer UDS or TCP. But it will prefer UDS over TCP.
    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // 2.
    // As the client is using the same configuration, it will use UDS to connect to the router
    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one")) << "Expected UDS connection between host_client and router_one";

    // 3.
    // Note, as the server is configured to prefer TCP, and it is in another machine,
    // it will be forced to use TCP to connect to the router.
    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one")) << "Expected TCP connection between guest_server and router_one";

    // 4.
    guest_server->offer(boardnet_interface_);

    // 5.
    // The connection between client and the server will be handled via TCP,
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "Expected TCP connection between host_client and guest_server";
}

TEST_F(test_hybrid_mode, test_boardnet_with_hybrid_mode_on_server_side) {
    // 0. Router One + Client are configurated to prefer TCP
    //    Server is configured to prefer UDS. The router is in another machine.
    // 1. Router initiates and registers itself in the first machine
    // 2. Client (Consumer) initiates and registers to router one in the first machine
    // 3. Server (Provider) initiates and registers to router one in the second machine
    // 4. Server offers a service
    // 5. Client requests the service, receives the availability and subscribes to an event

    // Host ECU: router_one + client (TCP-only) and remote server (uds_preferred guest — still uses TCP to reach router_one).
    ecu_setup host{"host", hybrid::host_tcp_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    // 1.
    // Router will be listening on UDS, but also on TCP, as it needs to be reachable for
    // both clients and servers, which can be configured to prefer UDS or TCP. But it will prefer UDS over TCP.
    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // 2.
    // As the client is using the same configuration, it will use UDS to connect to the router
    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "router_one")) << "Expected TCP connection between host_client and router_one";

    // 3.
    // Note, as the server is configured to prefer TCP, and it is in another machine,
    // it will be forced to use TCP to connect to the router.
    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one")) << "Expected TCP connection between guest_server and router_one";

    // 4.
    guest_server->offer(boardnet_interface_);

    // 5.
    // The connection between client and the server will be handled via TCP, as they are in different machines
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "Expected TCP connection between host_client and guest_server";
}

TEST_F(test_hybrid_mode, test_router_restart_reconnects_host_via_uds_and_guest_via_tcp) {
    // 0. Same topology as test_boardnet_with_hybrid_mode_on_both_sides.
    //    After the router is restarted:
    //    - host_client must reconnect to the new router via UDS (same machine, uds_preferred)
    //    - guest_server must reconnect to the new router via TCP (remote machine)
    //    - The service must become available again and subscription must be restored.

    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one"));

    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"));

    guest_server->offer(boardnet_interface_);
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"));

    // --- Restart the router ---
    // Clear state records before stopping so we get clean DEREGISTERED → REGISTERED sequences.
    host_client->app_state_record_.clear();
    guest_server->app_state_record_.clear();
    host_client->availability_record_.clear();
    host_client->subscription_record_.clear();

    host.stop_one("router_one");

    // Both running apps must detect the loss of the router.
    ASSERT_TRUE(host_client->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED))
            << "host_client should deregister when router stops";
    ASSERT_TRUE(guest_server->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED))
            << "guest_server should deregister when router stops";

    // Clear again so we can unambiguously wait for ST_REGISTERED on reconnect.
    host_client->app_state_record_.clear();
    guest_server->app_state_record_.clear();

    host.start_router();
    router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // Both apps must auto-reconnect to the new router instance.
    ASSERT_TRUE(successfully_registered(host_client)) << "host_client should re-register after router restart";
    ASSERT_TRUE(successfully_registered(guest_server)) << "guest_server should re-register after router restart";

    // Transport types must be preserved after the restart.
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one"))
            << "host_client should still use UDS to reach the router after restart";
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"))
            << "guest_server should still use TCP to reach the router after restart";

    // Service availability and subscription must be fully restored.
    guest_server->offer(boardnet_interface_);
    ASSERT_TRUE(await_service(host_client)) << "Service should become available again after router restart";
    ASSERT_TRUE(subscribe_to_event(host_client)) << "Subscription should be restored after router restart";
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "host_client should still use TCP to reach guest_server after router restart";
}

TEST_F(test_hybrid_mode, test_host_app_restart_reconnects_via_uds) {
    // 0. Same topology as test_boardnet_with_hybrid_mode_on_both_sides.
    //    After host_client is restarted:
    //    - It must reconnect to the router via UDS (same machine, uds_preferred)
    //    - The service must be available and subscription must work again.

    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one"));

    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"));

    guest_server->offer(boardnet_interface_);
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"));

    // --- Restart host_client ---
    // The old app object is destroyed; all its state (requests, subscriptions) is lost.
    host.stop_one("host_client");

    // A fresh host_client instance must connect to the router via UDS.
    host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client)) << "host_client should register after restart";
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one")) << "host_client should reconnect via UDS after restart";

    // The new instance must explicitly request the service and subscribe.
    host_client->request_service(service_instance_);
    ASSERT_TRUE(await_service(host_client)) << "Service should be available after host_client restart";
    ASSERT_TRUE(subscribe_to_event(host_client)) << "Subscription should work after host_client restart";
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "host_client should use TCP to reach guest_server after restart";
}

TEST_F(test_hybrid_mode, test_guest_app_restart_reconnects_via_tcp) {
    // 0. Same topology as test_boardnet_with_hybrid_mode_on_both_sides.
    //    After guest_server is restarted:
    //    - host_client must first see the service go unavailable
    //    - guest_server must reconnect to the router via TCP (remote machine, uds_preferred)
    //    - The service must become available again and subscription must be restored.

    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one"));

    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"));

    guest_server->offer(boardnet_interface_);
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"));

    // --- Restart guest_server ---
    // Clear availability and subscription records so we can track the full
    // unavailable → available cycle cleanly.
    host_client->availability_record_.clear();
    host_client->subscription_record_.clear();

    host.stop_one("guest_server");

    // host_client must observe the service going unavailable.
    ASSERT_TRUE(host_client->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)))
            << "Service should become unavailable when guest_server stops";

    // Clear records again before the new server comes up so we wait for fresh events.
    host_client->availability_record_.clear();
    host_client->subscription_record_.clear();

    // Start a fresh guest_server instance.
    guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server)) << "guest_server should register after restart";

    // Even though it prefers UDS, it is on a remote machine → must use TCP.
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one")) << "guest_server should reconnect via TCP after restart";

    // Re-offer and verify availability and subscription are fully restored.
    guest_server->offer(boardnet_interface_);
    ASSERT_TRUE(await_service(host_client)) << "Service should become available again after guest_server restart";
    ASSERT_TRUE(subscribe_to_event(host_client)) << "Subscription should be restored after guest_server restart";
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "host_client should use TCP to reach guest_server after restart";
}

TEST_F(test_hybrid_mode, test_internal_communication_on_remote_ecu_via_uds) {
    // 0. host only has the router, configured with uds_preferred.
    //    guest has both a server and a client, also configured with uds_preferred.
    //    Both guest apps must use TCP to reach router_one (different machine),
    //    but they should communicate with each other via UDS (same machine, uds_preferred).
    // 1. Router on host initiates and registers itself
    // 2. Client on guest registers to router_one via TCP
    // 3. Server on guest registers to router_one via TCP
    // 4. Server offers a service
    // 5. Client requests the service and subscribes to an event, communicating with the server via UDS

    // Host ECU: router only, uds_preferred. Both remote guests (uds_preferred) are added to host.
    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_guest({"guest_client", std::nullopt}, /*uds_preferred=*/true);
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    // 1.
    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // 2.
    auto* guest_client = host.start_one("guest_client");
    ASSERT_TRUE(successfully_registered(guest_client));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_client", "router_one")) << "Expected TCP connection between guest_client and router_one";

    // 3.
    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one")) << "Expected TCP connection between guest_server and router_one";

    // 4.
    guest_server->offer(service_instance_);

    // 5.
    // Even though both apps use TCP to reach the router, they are co-located and
    // uds_preferred, so they should communicate with each other via UDS.
    guest_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(guest_client));
    ASSERT_TRUE(verify_connection_uses_uds("guest_client", "guest_server"))
            << "Expected UDS connection between guest_client and guest_server";
}

TEST_F(test_hybrid_mode, test_full_hybrid_topology_with_intra_guest_uds) {
    // Exercises all five distinct connection paths in a single test:
    //
    //   host_client  → router_one  : UDS  (same ECU, uds_preferred)
    //   guest_client → router_one  : TCP  (remote ECU, forced TCP despite uds_preferred)
    //   guest_server → router_one  : TCP  (remote ECU, forced TCP despite uds_preferred)
    //   guest_client → guest_server: UDS  (intra-guest, co-located on the remote ECU, uds_preferred)
    //   host_client  → guest_server: TCP  (cross-machine)

    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_client", std::nullopt}, /*uds_preferred=*/true);
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // host_client is on the same ECU as the router → UDS.
    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one"))
            << "host_client should connect to router via UDS (same machine, uds_preferred)";

    // guest_client and guest_server are on the remote ECU → forced TCP to reach the router.
    auto* guest_client = host.start_one("guest_client");
    ASSERT_TRUE(successfully_registered(guest_client));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_client", "router_one"))
            << "guest_client should connect to router via TCP (remote machine, forced)";

    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"))
            << "guest_server should connect to router via TCP (remote machine, forced)";

    guest_server->offer(boardnet_interface_);

    // host_client subscribes — crosses the ECU boundary, so the data path uses TCP.
    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client)) << "host_client should successfully subscribe to guest_server's event";
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "host_client should reach guest_server via TCP (cross-machine)";

    // guest_client subscribes — both are on the same remote ECU with uds_preferred,
    // so they communicate directly via UDS (the intra-guest path).
    guest_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(guest_client)) << "guest_client should successfully subscribe to guest_server's event";
    ASSERT_TRUE(verify_connection_uses_uds("guest_client", "guest_server"))
            << "guest_client should reach guest_server via UDS (same remote ECU, uds_preferred)";
}

TEST_F(test_hybrid_mode, test_connection_break_between_guests_and_router_recovers) {
    // Same full hybrid topology as test_full_hybrid_topology_with_intra_guest_uds.
    // After establishing all connections, both TCP links from the guest ECU to router_one
    // are broken simultaneously, simulating a temporary network partition between the two ECUs.
    //
    // After vsomeip auto-reconnects, the test verifies that:
    //   - host_client's UDS link to router_one is unaffected throughout the partition
    //   - guest apps deregister then re-register, still via TCP (type preserved)
    //   - intra-guest UDS (guest_client ↔ guest_server) is restored
    //   - availability and subscriptions are fully recovered on all clients

    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_app("host_client");
    host.add_guest({"guest_client", std::nullopt}, /*uds_preferred=*/true);
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    auto* host_client = host.start_one("host_client");
    ASSERT_TRUE(successfully_registered(host_client));
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one"));

    auto* guest_client = host.start_one("guest_client");
    ASSERT_TRUE(successfully_registered(guest_client));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_client", "router_one"));

    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"));

    guest_server->offer(boardnet_interface_);

    host_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(host_client));
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"));

    guest_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(guest_client));
    ASSERT_TRUE(verify_connection_uses_uds("guest_client", "guest_server"));

    // --- Simulate network partition: sever both TCP links from the guest ECU ---
    // Clear records before the break to get clean DEREGISTERED observations.
    guest_server->app_state_record_.clear();
    guest_client->app_state_record_.clear();
    host_client->availability_record_.clear();
    host_client->subscription_record_.clear();

    ASSERT_TRUE(disconnect("guest_server", boost::asio::error::connection_reset, "router_one", boost::asio::error::connection_reset))
            << "Failed to break TCP link between guest_server and router_one";
    ASSERT_TRUE(disconnect("guest_client", boost::asio::error::connection_reset, "router_one", boost::asio::error::connection_reset))
            << "Failed to break TCP link between guest_client and router_one";

    // Both guest apps must detect the lost router connection.
    ASSERT_TRUE(guest_server->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED))
            << "guest_server should deregister after TCP link to router is severed";
    ASSERT_TRUE(guest_client->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED))
            << "guest_client should deregister after TCP link to router is severed";

    // host_client must see the service disappear, but its own UDS link is unaffected.
    ASSERT_TRUE(host_client->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)))
            << "Service should become unavailable to host_client during the partition";
    ASSERT_TRUE(verify_connection_uses_uds("host_client", "router_one"))
            << "host_client's UDS link to router must not be affected by the guest partition";

    // --- Recovery: vsomeip auto-reconnects both guest apps via TCP ---
    // Clear records to get a clean observation window for the reconnect.
    guest_server->app_state_record_.clear();
    guest_client->app_state_record_.clear();
    host_client->availability_record_.clear();
    host_client->subscription_record_.clear();
    guest_client->availability_record_.clear();
    guest_client->subscription_record_.clear();

    ASSERT_TRUE(successfully_registered(guest_server)) << "guest_server should auto-reconnect and re-register via TCP";
    ASSERT_TRUE(successfully_registered(guest_client)) << "guest_client should auto-reconnect and re-register via TCP";

    // Transport types to the router must be preserved after the reconnect.
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"))
            << "guest_server should still use TCP to reach router after reconnect";
    ASSERT_TRUE(verify_connection_uses_tcp("guest_client", "router_one"))
            << "guest_client should still use TCP to reach router after reconnect";

    // Re-offer and verify full recovery of availability, subscriptions, and transport.
    guest_server->offer(boardnet_interface_);

    // host_client never deregistered — its outstanding request_service is still active in
    // the router; it just needs to observe the service becoming available again.
    ASSERT_TRUE(await_service(host_client)) << "Service should become available again to host_client after partition heals";
    ASSERT_TRUE(subscribe_to_event(host_client)) << "host_client subscription should be restored after partition heals";
    ASSERT_TRUE(verify_connection_uses_tcp("host_client", "guest_server"))
            << "host_client should still use TCP to reach guest_server after recovery";

    // guest_client deregistered and lost its routing state — it must re-request the service.
    guest_client->request_service(service_instance_);
    ASSERT_TRUE(await_service(guest_client)) << "Service should become available again to guest_client after partition heals";
    ASSERT_TRUE(subscribe_to_event(guest_client)) << "guest_client subscription should be restored after partition heals";
    ASSERT_TRUE(verify_connection_uses_uds("guest_client", "guest_server"))
            << "guest_client should still use intra-guest UDS to reach guest_server after recovery";
}

TEST_F(test_hybrid_mode, test_short_lived_subscribers_receive_events_from_both_providers) {
    // Topology:
    //   Host machine (127.0.0.1):
    //     - router_one          (permanent routing manager, uds_preferred)
    //     - host_server         (permanent, offers service-1 / interfaces::cafe)
    //     - host_subscriber     (short-lived, subscribes to both cafe and beef)
    //
    //   Guest machine (127.0.0.2):
    //     - guest_server        (permanent, offers service-2 / interfaces::beef, uds_preferred)
    //     - guest_subscriber    (short-lived, subscribes to both cafe and beef)
    //
    // The short-lived pair is stopped and restarted `iterations` times to verify
    // that hybrid routing correctly re-establishes all connections, subscriptions,
    // and event delivery across restart cycles without resource leaks or state corruption.

    constexpr int iterations = 10;

    std::vector<uint8_t> const payload_cafe{0xCA, 0xFE};
    std::vector<uint8_t> const payload_beef{0xBE, 0xEF};

    auto const& svc_cafe = interfaces::cafe;
    auto const& svc_beef = interfaces::beef;
    event_ids const evt_cafe = svc_cafe.events_[0];
    event_ids const evt_beef = svc_beef.events_[0];
    service_instance const si_cafe = svc_cafe.instance_;
    service_instance const si_beef = svc_beef.instance_;

    // Config extends host_uds_preferred_config with svc_beef so SD assigns ports for both services.
    ecu_setup host{"host", with_interfaces(hybrid::host_uds_preferred_config, {svc_beef}), *socket_manager_};
    host.add_app("host_server");
    host.add_app("host_subscriber");
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.add_guest({"guest_subscriber", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    // Persistent providers — they live for the entire test.
    auto* host_server = host.start_one("host_server");
    ASSERT_TRUE(successfully_registered(host_server));
    host_server->offer(svc_cafe);

    auto* guest_server = host.start_one("guest_server");
    ASSERT_TRUE(successfully_registered(guest_server));
    guest_server->offer(svc_beef);

    // Wait until the guest_server's service has been discovered by the router before
    // any subscriber starts, so there is no race on the first iteration.
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one"));

    // Helper: returns true once the subscriber's message_record contains at least
    // one message carrying the requested payload.
    auto received = [](app* _sub, std::vector<uint8_t> const& _payload) {
        return _sub->message_record_.wait_for([&_payload](auto const& rec) {
            return std::any_of(rec.begin(), rec.end(), [&_payload](auto const& m) { return m.payload_ == _payload; });
        });
    };

    for (int i = 0; i < iterations; ++i) {
        SCOPED_TRACE("iteration " + std::to_string(i));

        auto* host_sub = host.start_one("host_subscriber");
        auto* guest_sub = host.start_one("guest_subscriber");
        ASSERT_TRUE(successfully_registered(host_sub));
        ASSERT_TRUE(successfully_registered(guest_sub));

        // Both subscribers request both services.
        host_sub->request_service(si_cafe);
        host_sub->request_service(si_beef);
        guest_sub->request_service(si_cafe);
        guest_sub->request_service(si_beef);

        ASSERT_TRUE(host_sub->availability_record_.wait_for_any(service_availability::available(si_cafe)))
                << "host_subscriber: cafe must become available";
        ASSERT_TRUE(host_sub->availability_record_.wait_for_any(service_availability::available(si_beef)))
                << "host_subscriber: beef must become available";
        ASSERT_TRUE(guest_sub->availability_record_.wait_for_any(service_availability::available(si_cafe)))
                << "guest_subscriber: cafe must become available";
        ASSERT_TRUE(guest_sub->availability_record_.wait_for_any(service_availability::available(si_beef)))
                << "guest_subscriber: beef must become available";

        // Both subscribers subscribe to both events.
        host_sub->subscribe_event(evt_cafe);
        host_sub->subscribe_event(evt_beef);
        ASSERT_TRUE(host_sub->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(evt_cafe)))
                << "host_subscriber: cafe subscription must be ACKed";
        ASSERT_TRUE(host_sub->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(evt_beef)))
                << "host_subscriber: beef subscription must be ACKed";

        guest_sub->subscribe_event(evt_cafe);
        guest_sub->subscribe_event(evt_beef);
        ASSERT_TRUE(guest_sub->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(evt_cafe)))
                << "guest_subscriber: cafe subscription must be ACKed";
        ASSERT_TRUE(guest_sub->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(evt_beef)))
                << "guest_subscriber: beef subscription must be ACKed";

        // Each provider fires one notification; all four delivery paths must deliver.
        host_server->send_event(evt_cafe, payload_cafe);
        guest_server->send_event(evt_beef, payload_beef);

        ASSERT_TRUE(received(host_sub, payload_cafe)) << "host_subscriber must receive cafe event  (iteration " << i << ")";
        ASSERT_TRUE(received(host_sub, payload_beef)) << "host_subscriber must receive beef event  (iteration " << i << ")";
        ASSERT_TRUE(received(guest_sub, payload_cafe)) << "guest_subscriber must receive cafe event (iteration " << i << ")";
        ASSERT_TRUE(received(guest_sub, payload_beef)) << "guest_subscriber must receive beef event (iteration " << i << ")";

        // Tear down short-lived subscribers; persistent providers keep running.
        host.stop_one("host_subscriber");
        host.stop_one("guest_subscriber");
    }
}

TEST_F(test_hybrid_mode, test_guest_uds_path_unavailable_at_startup_falls_back_then_recovers) {
    // Scenario: /var/run/someip (the directory where vsomeip writes UDS socket files) is
    // not yet set up when guest_server starts. In the fake-socket layer this is simulated by
    // setting fail_on_uds_bind BEFORE start_one, which makes the UDS local-server acceptor's
    // bind() call fail inside create_uds_local_acceptor().
    //
    // create_uds_local_acceptor retries every IPC_PORT_WAIT_TIME ms (100 ms by default)
    // until IPC_PORT_MAX_WAIT_TIME is reached or the bind succeeds — identical to the
    // retry strategy used by create_tcp_local_acceptor for transient port errors.
    //
    // Expected behaviour:
    //   1. guest_server starts; UDS local-server creation fails on the first attempt and
    //      enters the retry loop (blocking the init_receiver_side call on the vsomeip thread).
    //   2. The test immediately clears fail_on_uds_bind ("directory is now created").
    //   3. The next retry (≤ IPC_PORT_WAIT_TIME ms later) succeeds; guest_server
    //      registers with a working UDS local server.
    //   4. When guest_client subscribes, both apps are co-located and uds_preferred, so the
    //      intra-guest channel uses UDS directly — no TCP fallback.

    ecu_setup host{"host", hybrid::host_uds_preferred_config, *socket_manager_};
    host.add_guest({"guest_client", std::nullopt}, /*uds_preferred=*/true);
    host.add_guest({"guest_server", std::nullopt}, /*uds_preferred=*/true);
    host.prepare();
    host.start_router();

    auto* router_one = host.router_;
    ASSERT_TRUE(successfully_registered(router_one));

    auto* guest_client = host.start_one("guest_client");
    ASSERT_TRUE(successfully_registered(guest_client));
    ASSERT_TRUE(verify_connection_uses_tcp("guest_client", "router_one")) << "guest_client must reach the router via TCP (remote machine)";

    // Simulate /var/run/someip not being ready: the first UDS acceptor bind attempt will
    // fail, triggering the retry loop in create_uds_local_acceptor.
    fail_on_uds_bind("guest_server", true);
    auto* guest_server = host.start_one("guest_server");

    ASSERT_TRUE(await_connection("guest_server", "router_one"));
    ASSERT_TRUE(wait_for_command("guest_server", "router_one", protocol::id_e::ASSIGN_CLIENT_ACK_ID, socket_role::client));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // "Directory is now created": clear the bind block while the retry loop is running.
    // The next retry attempt (≤ IPC_PORT_WAIT_TIME ms away) will succeed.
    fail_on_uds_bind("guest_server", false);

    // guest_server must eventually register — the retry loop will succeed once the bind
    // block is cleared.
    ASSERT_TRUE(successfully_registered(guest_server)) << "guest_server should register after the UDS path becomes available";
    ASSERT_TRUE(verify_connection_uses_tcp("guest_server", "router_one")) << "guest_server must reach the router via TCP (remote machine)";

    // guest_server now has a working UDS local server. guest_client subscribes and the
    // intra-guest channel uses UDS directly (both apps are co-located and uds_preferred).
    guest_server->offer(boardnet_interface_);
    guest_client->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(guest_client)) << "Subscription should succeed once the UDS path is available";
    ASSERT_TRUE(verify_connection_uses_uds("guest_client", "guest_server"))
            << "Intra-guest communication must use UDS once the UDS path is available";
}
}

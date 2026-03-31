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

}

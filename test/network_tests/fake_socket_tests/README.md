# Fake Socket Tests

These tests run vsomeip applications in-process using fake (intercepted) sockets
instead of real OS network I/O. This makes it possible to write deterministic,
fast unit-level tests for vsomeip routing and service discovery behaviour without
requiring real network interfaces or multiple processes.

## Key abstractions

### `socket_manager`
The central coordinator for all fake sockets. It intercepts every socket creation
made by vsomeip and routes data between them in-process. It also provides fault
injection hooks (disconnect, delay, drop) and awaitable conditions (connection
established, message received) used in test assertions.

There is one shared `socket_manager` instance per test, owned by
`base_fake_socket_fixture`.

### `base_fake_socket_fixture`
The GTest fixture base class. It owns the `socket_manager` and exposes fault
injection helpers (forwarding to `socket_manager`) used in test assertions.
Application lifecycle is managed via `ecu_setup`.

### `app`
A handle to a single vsomeip application running in the test process. Wraps a
`vsomeip::application` and provides typed helpers for offering/subscribing to
services and events, sending notifications, and recording received messages and
availability changes. One `app` instance = one vsomeip client or routing manager.

### `interface`
A service contract from the *vsomeip routing client* perspective. It groups a
`service_instance` (service + instance IDs), the event/field `event_ids` for that
service, and the reliability type per event. Used at runtime with `app::offer()`
and `app::subscribe()`. No port or IP knowledge — purely about what the app wants
to communicate.

Defined in `helpers/service_state.hpp`. Sample instances are in
`sample_interfaces.hpp`.

### `ecu_config`
The daemon/routing configuration for a single ECU. Specifies which services are
offered on the boardnet (with ports), which apps run on the ECU (with optional
fixed client IDs), the local transport (UDS or TCP), SD settings, and client-port
mappings. Serialised to a vsomeip-compatible JSON string by `to_json_string()`.

The convenient constructor takes a list of `interface`s to offer on the boardnet
and an optional base port (default 30501). Ports are auto-assigned, one per
interface. Routing configuration, unicast IP, app lists, and other settings are
populated directly on the struct fields (see `sample_configurations.hpp` for
examples).

Defined in `helpers/ecu_config.hpp`. Sample instances are in
`sample_configurations.hpp`.

### `ecu_setup`
Ties `ecu_config` and `app` lifecycle together for one ECU in a test. It:

1. Writes the `ecu_config` to a temp file (`/tmp/<name>.json`) via `prepare()`.
2. Sets `VSOMEIP_CONFIGURATION_<appname>` for every app so that vsomeip picks up
   the right config file.
3. Creates and starts all apps via `start_apps()`, or individually via `start_one(name)`
   which returns a raw `app*` (or `nullptr` on failure).
4. During teardown (destructor / `stop_apps()`), stops all non-router apps first,
   then the routing manager last, to mirror a clean ECU shutdown sequence.
5. Unsets env vars and removes the temp config file.

Apps with a fixed client ID are listed in `ecu_config.apps_`. Apps that should
share the same config file but receive a dynamic client ID are added via
`add_app(name)` before calling `prepare()`.

Guest apps — apps that connect to the routing manager over TCP local transport
using their own guest IP (distinct from the host IP —
boardnet traffic goes via the host's router) — are registered via
`add_guest(application_config)` before
`prepare()`. This automatically derives a guest IP (incrementing the last octet
of the host IP), writes a separate guest config file, and sets the corresponding
`VSOMEIP_CONFIGURATION_<guestname>` env var. Requires `local_tcp_config` routing.

The routing manager app is accessible via `ecu_setup::router_`.
All running apps are accessible by name via `ecu_setup::apps_`.

Defined in `helpers/ecu_setup.hpp`.

---

## Typical test setup

```
Test (derives from base_fake_socket_fixture)
│
├── ecu_setup ecu_one{"ecu_one", ecu_one_config, *socket_manager_}   ← one ECU
│   ├── app* router_    (routing manager)
│   └── app* apps_["boardnet_client"]   (added via add_app())
│
└── ecu_setup ecu_two{"ecu_two", ecu_two_config, *socket_manager_}   ← another ECU
    ├── app* router_    (routing manager)
    └── app* apps_["ecu_two_server"]    (added via add_guest(); connects over local TCP)

socket_manager_ (owned by fixture, passed to ecu_setup at construction)
    └── routes all fake socket I/O between the apps above
```

### Minimal example

The setup below simulates two physically separate ECUs communicating over a
boardnet. All network I/O is intercepted — no actual sockets are created, no
network interfaces are required, and everything runs in a single test process.

- **ecu_one** (127.0.0.1): routing manager + one in-process client
- **ecu_two** (127.0.0.2): routing manager + guest server offering `service_3344`
- **boardnet**: fake TCP connections between ecu_one and ecu_two for SD and service data
- **local transport**: fake TCP between ecu_two's router and the guest server

```cpp
struct my_test : public base_fake_socket_fixture {
    void SetUp() override {
        // "boardnet_client" has no fixed client ID — vsomeip assigns one dynamically
        ecu_one_.add_app("boardnet_client");
        // "ecu_two_server" is a guest that connects to ecu_two's router over local TCP
        ecu_two_.add_guest({"ecu_two_server", std::nullopt});

        // writes $VSOMEIP_BASE_PATH/ecu_one.json,
        // sets VSOMEIP_CONFIGURATION_{router_one,boardnet_client}
        ecu_one_.prepare();
        // writes $VSOMEIP_BASE_PATH/ecu_two.json + $VSOMEIP_BASE_PATH/ecu_two_guest.json,
        // sets VSOMEIP_CONFIGURATION_{router_two,ecu_two_server}
        ecu_two_.prepare();
        // starts router first (awaits connectable), then all remaining apps
        ecu_one_.start_apps();
        ecu_two_.start_apps();
    }
    // No TearDown needed — ecu_setup destructor stops apps and cleans up env vars
    // and temp config files automatically.

    ecu_setup ecu_one_{"ecu_one", boardnet::ecu_one_config, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", boardnet::ecu_two_config, *socket_manager_};
};

TEST_F(my_test, client_discovers_guest_offered_service) {
    // ecu_two offers the service on the boardnet
    auto* server = ecu_two_.apps_["ecu_two_server"];
    server->offer(interfaces::boardnet::service_3344);

    // ecu_one's dynamic client subscribes to it
    auto* client = ecu_one_.apps_["boardnet_client"];
    client->subscribe(interfaces::boardnet::service_3344);

    EXPECT_TRUE(client->availability_record_.wait_for_last(
        service_availability::available(interfaces::boardnet::service_3344.instance_)));
}
```

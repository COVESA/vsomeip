// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "ecu_config.hpp"
#include "socket_manager.hpp"
#include "app.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vsomeip_v3::testing {

/// Manages the lifecycle of a vsomeip config file and per-app environment variables
/// for a single ECU in a fake-socket test.
///
/// Usage:
///   ecu_setup ecu{"ecu", my_ecu_config, *socket_manager_};
///   ecu.add_app("extra_app");   // optional: add app not in ecu_config.apps_
///   ecu.prepare();              // writes $VSOMEIP_BASE_PATH/<name>.json, sets VSOMEIP_CONFIGURATION_<name>
///   ecu.start_router();         // or ecu.start_apps() for all at once
///   auto* a = ecu.start_one("extra_app");
///   // destructor unsets env vars and removes config file
struct ecu_setup {
    explicit ecu_setup(std::string name, ecu_config cfg, socket_manager& sm);
    ~ecu_setup();

    ecu_setup(ecu_setup const&) = delete;
    ecu_setup& operator=(ecu_setup const&) = delete;

    /// Add an app that shares this ECU's config file but has no fixed client ID
    /// (not listed in ecu_config.apps_). Must be called before prepare().
    void add_app(std::string name);

    /// Add a guest app to this ECU. A guest shares the same router (host_ip_) but runs on
    /// a separate network address (host_ip_ + 1 in the last octet) with no local router.
    /// The host routing config must use local_tcp_config. Must be called before prepare().
    /// May be called multiple times to register additional guest apps.
    /// uds_preferred: whether the guest's config has UDS-preferred mode enabled (default false).
    void add_guest(application_config app, bool uds_preferred = false);

    /// Write the config to $VSOMEIP_BASE_PATH/<name>.json (falling back to the system
    /// temp directory if VSOMEIP_BASE_PATH is unset) and set VSOMEIP_CONFIGURATION_<name>
    /// for every app in ecu_config.apps_, add_app() registrations, and guest apps
    /// added via add_guest().
    void prepare();

    /// Create and start all apps listed in ecu_config.apps_ + add_app() calls,
    /// followed by guest apps added via add_guest().
    /// Populates apps_. Must call prepare() first.
    void start_apps();

    /// Start the routing manager app. Must call prepare() first.
    /// Called automatically by start_apps(); use directly when starting apps individually.
    void start_router();

    /// Create and start a single app by name. The app must have its
    /// VSOMEIP_CONFIGURATION_<name> env var set by prepare() — i.e. it must be
    /// listed in ecu_config.apps_, registered via add_app(), or added via add_guest().
    /// Must call prepare() first.
    app* start_one(std::string const& name);

    /// Stop all running apps and clear apps_.
    void stop_apps();

    /// Stop and remove a single app by name.
    void stop_one(std::string const& name);

    /// Running apps by name, populated after start_apps().
    std::map<std::string, app*> apps_;
    /// The routing manager app — always started first, stopped last.
    app* router_{nullptr};

    [[nodiscard]] ecu_config const& config() const { return config_; }

    /// Delay (or undelay) outgoing SD sends from this ECU's SD endpoint.
    /// Useful to suppress FIND messages until the remote ECU's debounce timer
    /// fires a multicast OFFER, avoiding the unicast-Offer / no-StopOffer race.
    /// Returns false if this ECU has no TCP-mode routing (UDS-only transport).
    [[nodiscard]] bool delay_sd_sending(bool _delay);

    /// Waits until the application identified by @p name is accepting connections
    /// (i.e. it has at least one bound TCP acceptor).
    /// TODO: this is a thin wrapper over socket_manager::await_connectable and should be
    ///       revisited once a cleaner ecu_setup / base_fake_socket_fixture integration exists.
    [[nodiscard]] bool await_connectable(std::string const& name, std::chrono::milliseconds timeout = std::chrono::seconds(3));

private:
    ecu_config config_;
    std::string name_;
    std::string router_name_;
    socket_manager& sm_;
    std::vector<std::string> extra_apps_;
    std::filesystem::path config_file_;
    std::vector<std::string> set_env_vars_;
    std::map<std::string, std::unique_ptr<app>> owned_apps_;

    std::optional<ecu_config> guest_config_;
    std::string guest_config_name_;
    std::filesystem::path guest_config_file_;
};

} // namespace vsomeip_v3::testing

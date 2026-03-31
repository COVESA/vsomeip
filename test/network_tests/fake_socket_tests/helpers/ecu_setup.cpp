// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ecu_setup.hpp"
#include "test_logging.hpp"

#include <gtest/gtest.h>

#include <boost/asio/ip/address_v4.hpp>

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#define LOCAL_LOG TEST_LOG << "[ecu_setup|" << name_ << "|" << __func__ << "] "
#define EARLY_LOG std::cout << "[ecu_setup|" << name_ << "|" << __func__ << "] "

namespace vsomeip_v3::testing {

namespace {
std::string derive_router_name(ecu_config const& cfg) {
    if (!cfg.routing_config_) {
        return {};
    }
    std::string name = std::visit(
            [](auto const& r) -> std::string {
                using R = std::decay_t<decltype(r)>;
                if constexpr (std::is_same_v<R, local_tcp_config>) {
                    return r.router_name_;
                } else {
                    return r;
                }
            },
            *cfg.routing_config_);
    return name;
}
} // namespace

ecu_setup::ecu_setup(std::string name, ecu_config cfg, socket_manager& sm) :
    config_{std::move(cfg)}, name_{std::move(name)}, router_name_{derive_router_name(config_)}, sm_{sm} { }

ecu_setup::~ecu_setup() {
    for (auto const& var : set_env_vars_) {
        ::unsetenv(var.c_str());
    }
    if (!config_file_.empty()) {
        std::filesystem::remove(config_file_);
    }
    if (!guest_config_file_.empty()) {
        std::filesystem::remove(guest_config_file_);
    }
}

void ecu_setup::add_guest(application_config app, bool uds_preferred) {
    LOCAL_LOG << app.name_;
    EARLY_LOG << app.name_ << std::endl;
    if (!guest_config_) {
        ASSERT_TRUE(config_.routing_config_.has_value() && std::holds_alternative<local_tcp_config>(*config_.routing_config_))
                << "add_guest requires local_tcp_config routing";

        auto const& tcp = std::get<local_tcp_config>(*config_.routing_config_);

        // derive guest IP: increment the last octet of the host IP
        auto const host_bytes = tcp.host_.to_v4().to_bytes();
        auto guest_bytes = host_bytes;
        ++guest_bytes[3];
        auto const guest_ip = boost::asio::ip::make_address_v4(guest_bytes);

        // Build the guest ecu_config:
        //   - host_ip_ = host's IP (router address, used as top-level unicast + routing.host.unicast)
        //   - routing_config_ = same router_name_, but guest_ = guest_ip (guest's own address)
        //   - no router in apps_ -> derive_router_name returns empty -> no local router started
        ecu_config gcfg;
        gcfg.unicast_ip_ = config_.unicast_ip_;
        gcfg.routing_config_ =
                local_tcp_config{.router_name_ = tcp.router_name_, .host_ = tcp.host_, .guest_ = boost::asio::ip::address{guest_ip}};
        gcfg.uds_preferred_ = uds_preferred;
        gcfg.sd_ = false;
        guest_config_name_ = name_ + "_guest";
        guest_config_ = std::move(gcfg);
    }

    guest_config_->apps_.push_back(std::move(app));
}

void ecu_setup::add_app(std::string name) {
    LOCAL_LOG << name;
    EARLY_LOG << name << std::endl;
    extra_apps_.push_back(std::move(name));
}

void ecu_setup::prepare() {
    auto const base = [] {
        if (const char* p = ::getenv("VSOMEIP_BASE_PATH"); p && p[0] != '\0') {
            return std::filesystem::path{p};
        }
        return std::filesystem::temp_directory_path();
    }();
    config_file_ = base / (name_ + ".json");

    {
        auto const json = to_json_string(config_);
        std::ofstream f{config_file_};
        if (!f) {
            throw std::runtime_error{"ecu_setup: failed to write config file: " + config_file_.string()};
        }
        f << json;
        LOCAL_LOG << "wrote config to " << config_file_.string() << ":\n" << json;
        EARLY_LOG << "wrote config to " << config_file_.string() << ":\n" << json << std::endl;
    }

    if (!router_name_.empty()) {
        auto const var = "VSOMEIP_CONFIGURATION_" + router_name_;
        ::setenv(var.c_str(), config_file_.string().c_str(), 1);
        set_env_vars_.push_back(var);
    }

    for (auto const& app : config_.apps_) {
        auto const var = "VSOMEIP_CONFIGURATION_" + app.name_;
        ::setenv(var.c_str(), config_file_.string().c_str(), 1);
        set_env_vars_.push_back(var);
    }
    for (auto const& name : extra_apps_) {
        auto const var = "VSOMEIP_CONFIGURATION_" + name;
        ::setenv(var.c_str(), config_file_.string().c_str(), 1);
        set_env_vars_.push_back(var);
    }

    if (guest_config_) {
        guest_config_file_ = base / (guest_config_name_ + ".json");
        {
            auto const json = to_json_string(*guest_config_);
            std::ofstream f{guest_config_file_};
            if (!f) {
                throw std::runtime_error{"ecu_setup: failed to write guest config file: " + guest_config_file_.string()};
            }
            f << json;
            LOCAL_LOG << "wrote guest config to " << guest_config_file_.string() << ":\n" << json;
            EARLY_LOG << "wrote guest config to " << guest_config_file_.string() << ":\n" << json << std::endl;
        }
        for (auto const& app : guest_config_->apps_) {
            auto const var = "VSOMEIP_CONFIGURATION_" + app.name_;
            ::setenv(var.c_str(), guest_config_file_.string().c_str(), 1);
            set_env_vars_.push_back(var);
        }
    }
}

app* ecu_setup::start_one(std::string const& name) {
    auto app_ptr = std::make_unique<app>(name);
    sm_.add(name);
    if (!app_ptr->start()) {
        return nullptr;
    }
    if (!sm_.await_assignment(name)) {
        LOCAL_LOG << name << " could not be assigned to an io_context";
        return nullptr;
    }
    LOCAL_LOG << name << " is started";
    apps_[name] = app_ptr.get();
    owned_apps_[name] = std::move(app_ptr);
    if (name == router_name_) {
        router_ = apps_[router_name_];
    }
    return apps_[name];
}

void ecu_setup::stop_one(std::string const& name) {
    LOCAL_LOG << name;
    if (auto it = owned_apps_.find(name); it != owned_apps_.end()) {
        it->second->stop();
        owned_apps_.erase(it);
    }
    apps_.erase(name);
    if (name == router_name_) {
        router_ = nullptr;
    }
}

void ecu_setup::start_apps() {
    start_router();
    for (auto const& app_cfg : config_.apps_) {
        if (app_cfg.name_ != router_name_) {
            start_one(app_cfg.name_);
        }
    }
    for (auto const& name : extra_apps_) {
        start_one(name);
    }
    if (guest_config_) {
        for (auto const& app_cfg : guest_config_->apps_) {
            start_one(app_cfg.name_);
        }
    }
}

void ecu_setup::stop_apps() {
    std::vector<std::string> non_routers;
    for (auto const& [name, _] : owned_apps_) {
        if (name != router_name_) {
            non_routers.push_back(name);
        }
    }
    for (auto const& name : non_routers) {
        stop_one(name);
    }
    if (owned_apps_.count(router_name_) != 0) {
        stop_one(router_name_);
    }
}

void ecu_setup::start_router() {
    if (router_name_.empty()) {
        LOCAL_LOG << "no router configured, skipping";
        return;
    }
    LOCAL_LOG << router_name_;
    start_one(router_name_);
}

bool ecu_setup::delay_sd_sending(bool _delay) {
    boost::asio::ip::udp::endpoint const sd_ep{config_.unicast_ip_, 30490};
    return sm_.delay_boardnet_sending(sd_ep, _delay);
}

bool ecu_setup::await_connectable(std::string const& name, std::chrono::milliseconds timeout) {
    LOCAL_LOG << name;
    return sm_.await_connectable(name, timeout);
}

} // namespace vsomeip_v3::testing

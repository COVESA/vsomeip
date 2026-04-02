// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ecu_config.hpp"

#include <map>
#include <sstream>

namespace vsomeip_v3::testing {
ecu_config::ecu_config(std::vector<interface> offered, vsomeip::port_t base_port) {
    add_interface(offered, base_port);
}

ecu_config& ecu_config::add_interface(std::vector<interface> offered, vsomeip::port_t base_port) {

    vsomeip::port_t next_port = base_port;
    for (auto const& iface : offered) {
        service_config svc;
        svc.si_ = iface.instance_;

        bool needs_unreliable = false;
        bool needs_reliable = false;
        for (auto const& e : iface.events_) {
            if (e.reliability_ == vsomeip::reliability_type_e::RT_UNRELIABLE) {
                needs_unreliable = true;
            }
            if (e.reliability_ == vsomeip::reliability_type_e::RT_RELIABLE) {
                needs_reliable = true;
            }
        }
        for (auto const& f : iface.fields_) {
            if (f.reliability_ == vsomeip::reliability_type_e::RT_UNRELIABLE) {
                needs_unreliable = true;
            }
            if (f.reliability_ == vsomeip::reliability_type_e::RT_RELIABLE) {
                needs_reliable = true;
            }
        }
        if (needs_unreliable) {
            svc.unreliable_port_ = next_port++;
        }
        if (needs_reliable) {
            svc.reliable_port_ = {next_port++, false};
        }

        std::map<vsomeip::eventgroup_t, std::vector<vsomeip::event_t>> groups;
        for (auto const& e : iface.events_) {
            event_config ec{e.event_id_, false, e.reliability_ == vsomeip::reliability_type_e::RT_RELIABLE};
            svc.events_.push_back(ec);
            groups[e.eventgroup_id_].push_back(e.event_id_);
        }
        for (auto const& f : iface.fields_) {
            event_config ec{f.event_id_, true, f.reliability_ == vsomeip::reliability_type_e::RT_RELIABLE};
            svc.events_.push_back(ec);
            groups[f.eventgroup_id_].push_back(f.event_id_);
        }
        for (auto const& [egid, event_ids] : groups) {
            svc.event_groups_.push_back({egid, event_ids});
        }

        services_.push_back(std::move(svc));
    }

    return *this;
}
namespace {

template<typename T>
void write_array(std::ostringstream& o, const std::vector<T>& items);

template<typename T>
std::string to_hex(T value) {
    std::ostringstream oss;
    oss << "0x" << std::hex << value;
    return oss.str();
}

void to_json(std::ostringstream& o, const application_config& app) {
    o << R"({ "name" : ")" << app.name_ << "\"";
    if (app.id_) {
        o << R"(, "id" : ")" << to_hex(*app.id_) << "\"";
    }
    o << " }";
}

void to_json(std::ostringstream& o, const event_config& event) {
    o << R"({ "event" : ")" << to_hex(event.event_id_) << "\"";
    o << R"(, "is_field" : ")" << (event.is_field_ ? "true" : "false") << "\"";
    o << R"(, "is_reliable" : ")" << (event.is_reliable_ ? "true" : "false") << "\"";
    o << " }";
}

void to_json(std::ostringstream& o, vsomeip::event_t event_id) {
    o << '"' << to_hex(event_id) << '"';
}

void to_json(std::ostringstream& o, const eventgroup_config& eg) {
    o << R"({ "eventgroup" : ")" << to_hex(eg.eventgroup_id_) << R"(", "events" : )";
    write_array(o, eg.event_ids_);
    o << " }";
}

void to_json(std::ostringstream& o, const service_config& svc) {
    o << R"({ "service" : ")" << to_hex(svc.si_.service_) << "\",";
    o << R"( "instance" : ")" << to_hex(svc.si_.instance_) << "\"";
    if (svc.unreliable_port_) {
        o << R"(, "unreliable" : ")" << *svc.unreliable_port_ << "\"";
    }
    if (svc.reliable_port_) {
        o << R"(, "reliable" : { "port" : ")" << svc.reliable_port_->port_ << "\"";
        o << R"(, "enable-magic-cookies" : ")" << (svc.reliable_port_->enable_magic_cookies_ ? "true" : "false") << "\" }";
    }
    if (!svc.events_.empty()) {
        o << R"(, "events" : )";
        write_array(o, svc.events_);
    }
    if (!svc.event_groups_.empty()) {
        o << R"(, "eventgroups" : )";
        write_array(o, svc.event_groups_);
    }
    o << " }";
}

template<typename T>
void write_array(std::ostringstream& o, const std::vector<T>& items) {
    o << "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        o << " ";
        to_json(o, items[i]);
        if (i + 1 < items.size()) {
            o << ",";
        }
    }
    o << " ]";
}

void write_routing_tcp(std::ostringstream& o, const local_tcp_config& tcp) {
    o << "\"routing\" : {";
    o << R"( "host" : { "name" : ")" << tcp.router_name_ << "\",";
    o << R"( "unicast" : ")" << tcp.host_.to_string() << "\",";
    o << R"( "port" : "8998" },)";
    o << R"( "guests" : { "unicast" : ")" << tcp.guest_.to_string() << "\",";
    o << R"( "ports" : [ { "first" : "9000", "last" : "65535" } ] })";
    o << " }";
}

void write_client_ports(std::ostringstream& o, const client_port_config& cp) {
    o << R"("clients" : [ {)";
    o << R"( "reliable_remote_ports" : { "first" : ")" << cp.remote_ports_.first_ << R"(", "last" : ")" << cp.remote_ports_.last_
      << R"(" },)";
    o << R"( "unreliable_remote_ports" : { "first" : ")" << cp.remote_ports_.first_ << R"(", "last" : ")" << cp.remote_ports_.last_
      << R"(" },)";
    o << R"( "reliable_client_ports" : { "first" : ")" << cp.client_ports_.first_ << R"(", "last" : ")" << cp.client_ports_.last_
      << R"(" },)";
    o << R"( "unreliable_client_ports" : { "first" : ")" << cp.client_ports_.first_ << R"(", "last" : ")" << cp.client_ports_.last_
      << R"(" })";
    o << R"( } ])";
}

void write_service_discovery(std::ostringstream& o, bool enabled) {
    o << R"("service-discovery" : { "enable" : ")" << (enabled ? "true" : "false") << "\"";
    if (enabled) {
        o << R"(, "multicast" : "224.244.224.245")";
        o << R"(, "port" : "30490")";
        o << R"(, "protocol" : "udp")";
        o << R"(, "initial_delay_min" : "10")";
        o << R"(, "initial_delay_max" : "100")";
        o << R"(, "repetitions_base_delay" : "200")";
        o << R"(, "repetitions_max" : "3")";
        o << R"(, "ttl" : "3")";
        o << R"(, "cyclic_offer_delay" : "2000")";
        o << R"(, "request_response_delay" : "1500")";
    }
    o << R"( })";
}

} // namespace

std::string to_json_string(const ecu_config& cfg) {
    std::ostringstream o;

    const std::string host_ip = cfg.unicast_ip_.to_string();

    o << R"({ "unicast" : ")" << host_ip << "\",";
    o << R"( "logging" : { "level" : "trace", "console" : "true", "version" : { "interval" : 0 } },)";
    o << R"( "tracing" : { "enable" : "true" },)";

    o << R"( "applications" : )";
    write_array(o, cfg.apps_);
    o << ",";

    if (!cfg.services_.empty()) {
        o << R"( "services" : )";
        write_array(o, cfg.services_);
        o << ",";
    }

    if (cfg.client_ports_) {
        o << " ";
        write_client_ports(o, *cfg.client_ports_);
        o << ",";
    }

    if (!cfg.network_.empty()) {
        o << R"( "network" : ")" << cfg.network_ << "\",";
    }

    if (cfg.routing_config_) {
        o << " ";
        std::visit(
                [&o, &cfg](const auto& r) {
                    using R = std::decay_t<decltype(r)>;
                    if constexpr (std::is_same_v<R, local_tcp_config>) {
                        write_routing_tcp(o, r);
                    } else {
                        o << R"("routing" : ")" << r << "\"";
                    }
                },
                *cfg.routing_config_);
        o << ",";
    }

    if (cfg.uds_preferred_) {
        o << R"( "uds-preferred" : "true",)";
    }

    o << " ";
    write_service_discovery(o, cfg.sd_);

    o << " }";

    return o.str();
}

} // namespace vsomeip_v3::testing

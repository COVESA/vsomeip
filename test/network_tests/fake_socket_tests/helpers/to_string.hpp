// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../../implementation/protocol/include/protocol.hpp"
#include "../../../../implementation/protocol/include/routing_info_command.hpp"
#include "../../../../implementation/protocol/include/config_command.hpp"
#include "../../../../implementation/protocol/include/command.hpp"
#include "../../../../implementation/protocol/include/routing_info_entry.hpp"
#include "../../../../implementation/service_discovery/include/enumeration_types.hpp"
#include "../../../../implementation/service_discovery/include/entry_impl.hpp"
#include "../../../../implementation/utility/include/utility.hpp"

#include <vector>
#include <sstream>
#include <iomanip>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/payload.hpp>

namespace vsomeip_v3::testing {

struct command_message;
char const* to_string(vsomeip_v3::protocol::id_e _id);
char const* to_string(vsomeip_v3::protocol::routing_info_entry_type_e e);
char const* to_string(vsomeip_v3::sd::entry_type_e, ttl_t);
std::string to_string(vsomeip_v3::protocol::service const& s);
std::string to_string(vsomeip_v3::protocol::routing_info_entry const& e);
std::string to_string(vsomeip_v3::protocol::routing_info_command const& c);
std::string to_string(vsomeip_v3::protocol::config_command const& c);
char const* to_string(vsomeip_v3::message_type_e const& m);
char const* to_string(vsomeip_v3::return_code_e const& e);
char const* to_string(vsomeip_v3::sd::entry_type_e const& e);
std::string to_string(vsomeip_v3::payload const& p);
std::string to_string(command_message const& c);
std::string to_string(std::shared_ptr<vsomeip_v3::sd::entry_impl> const& e);
// allows to be used below
std::string to_string(std::string const&);

template<typename T, typename U>
std::string to_string(std::pair<T, U> const& _p) {
    return "{" + to_string(_p.first) + " : " + to_string(_p.second) + "}";
}
template<typename T>
std::string to_string(std::vector<T> const& _container) {
    std::stringstream s;
    s << "[";
    if constexpr (std::is_same_v<unsigned char, T>) {
        s << std::hex << std::setfill('0') << std::setw(2);
    }
    bool first = true;
    for (auto const& t : _container) {
        if (first) {
            first = false;
        } else {
            s << ", ";
        }
        if constexpr (std::is_same_v<unsigned char, T>) {
            s << static_cast<int>(t);
        } else {
            s << to_string(t);
        }
    }
    s << "]";
    return s.str();
}
}

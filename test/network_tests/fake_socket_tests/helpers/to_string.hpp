// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TO_STRING_HPP_
#define VSOMEIP_V3_TO_STRING_HPP_

#include "../../../../implementation/protocol/include/protocol.hpp"
#include "../../../../implementation/protocol/include/routing_info_command.hpp"
#include "../../../../implementation/protocol/include/config_command.hpp"
#include "../../../../implementation/protocol/include/command.hpp"
#include "../../../../implementation/protocol/include/routing_info_entry.hpp"
#include <vector>
#include <sstream>
#include <iomanip>

namespace vsomeip_v3::testing {
char const* to_string(vsomeip_v3::protocol::id_e _id);
char const* to_string(vsomeip_v3::protocol::routing_info_entry_type_e e);
std::string to_string(vsomeip_v3::protocol::service const& s);
std::string to_string(vsomeip_v3::protocol::routing_info_entry const& e);
std::string to_string(vsomeip_v3::protocol::routing_info_command const& c);
std::string to_string(vsomeip_v3::protocol::config_command const& c);
std::string to_string(std::pair<std::string, std::string> const&);

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

#endif

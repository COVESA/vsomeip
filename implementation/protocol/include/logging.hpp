// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_PROTOCOL_LOGGING_HPP_
#define VSOMEIP_V3_PROTOCOL_LOGGING_HPP_

#include "protocol.hpp"

#include <string>
#include <ostream>

namespace vsomeip_v3::protocol {

struct service_data;
struct service_key;
struct event_key {
    service_key const& key_;
    event_t event_;
};

char const* to_string(id_e _id);
std::ostream& operator<<(std::ostream& _out, id_e _id);
std::ostream& operator<<(std::ostream& _out, service_key const& _data);
std::ostream& operator<<(std::ostream& _out, service_data const& _data);
std::ostream& operator<<(std::ostream& _out, event_key const& _data);
}
#endif

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_PRIMITIVE_TYPES_HPP
#define VSOMEIP_PRIMITIVE_TYPES_HPP

#include <array>
#include <cstdint>

namespace vsomeip {

typedef uint32_t message_t;
typedef uint16_t service_t;
typedef uint16_t method_t;
typedef uint16_t event_t;

typedef uint16_t instance_t;
typedef uint16_t eventgroup_t;

typedef uint8_t major_version_t;
typedef uint32_t minor_version_t;

typedef uint32_t ttl_t;

typedef uint32_t request_t;
typedef uint16_t client_t;
typedef uint16_t session_t;

typedef uint32_t length_t;

typedef uint8_t protocol_version_t;
typedef uint8_t interface_version_t;

typedef uint8_t byte_t;

// Addresses
typedef std::array<byte_t, 4> ipv4_address_t;
typedef std::array<byte_t, 16> ipv6_address_t;

typedef std::string trace_channel_t;

typedef std::string trace_filter_type_t;

typedef std::uint16_t pending_subscription_id_t;
} // namespace vsomeip

#endif // VSOMEIP_PRIMITIVE_TYPES_HPP

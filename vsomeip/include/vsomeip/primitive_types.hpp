//
// primitive_types.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_PRIMITIVE_TYPES_HPP
#define VSOMEIP_PRIMITIVE_TYPES_HPP

#include <cstdint>
#include <string>

namespace vsomeip {

typedef uint32_t message_id;
typedef uint16_t service_id;
typedef uint16_t method_id;
typedef uint16_t event_id;

typedef uint32_t length;

typedef uint32_t request_id;
typedef uint16_t client_id;
typedef uint16_t session_id;

typedef uint8_t protocol_version;
typedef uint8_t interface_version;

typedef uint8_t flags;
typedef uint8_t event_type;
typedef uint8_t option_index;
typedef uint16_t instance_id;
typedef uint8_t major_version;
typedef uint32_t time_to_live;
typedef uint32_t minor_version;
typedef uint16_t eventgroup_id;

typedef std::string ip_address;
typedef uint32_t ipv4_address;
typedef uint8_t ipv6_address[128];
typedef uint16_t ip_port;

typedef uint16_t priority;
typedef uint16_t weight;

typedef uint32_t alive_counter;
typedef uint32_t crc;

} // namespace vsomeip

#endif // VSOMEIP_PRIMITIVE_TYPES_HPP

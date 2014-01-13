//
// primitive_types.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_PRIMITIVETYPES_HPP
#define VSOMEIP_SERVICE_DISCOVERY_PRIMITIVETYPES_HPP

#include <cstdint>

namespace vsomeip {
namespace service_discovery {

// Service Discovery Message
typedef uint8_t flags;
typedef uint8_t event_type;
typedef uint8_t option_index;
typedef uint16_t instance_id;
typedef uint8_t major_version;
typedef uint32_t time_to_live;
typedef uint32_t minor_version;
typedef uint16_t eventgroup_id;

// IP Endpoint Option
typedef uint32_t ipv4_address;
typedef uint8_t ipv6_address[128];
typedef uint16_t ip_port;

// Load Balancing Option
typedef uint16_t priority;
typedef uint16_t weight;

// Protection Option
typedef uint32_t alive_counter;
typedef uint32_t crc;

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_PRIMITIVETYPES_HPP

//
// enumeration_types.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_ENUMERATION_TYPES_HPP
#define VSOMEIP_SERVICE_DISCOVERY_ENUMERATION_TYPES_HPP

#include <cstdint>

namespace vsomeip {
namespace service_discovery {

enum class ip_protocol : uint8_t {
	TCP = 0x06,
	UDP = 0x11,
	UNKNOWN = 0xFF
};

enum class option_type : uint8_t {
	CONFIGURATION = 0x1,
	LOAD_BALANCING = 0x2,
	PROTECTION = 0x3,
	IP4_ENDPOINT = 0x4,
	IP6_ENDPOINT = 0x6,
	UNKNOWN = 0xFF
};

enum class entry_type : uint8_t {
	FIND_SERVICE = 0x00,
	OFFER_SERVICE = 0x01,
	STOP_OFFER_SERVICE = 0x01,
	REQUEST_SERVICE = 0x2,
	FIND_EVENT_GROUP = 0x4,
	PUBLISH_EVENTGROUP = 0x5,
	STOP_PUBLISH_EVENTGROUP = 0x5,
	SUBSCRIBE_EVENTGROUP = 0x06,
	STOP_SUBSCRIBE_EVENTGROUP = 0x06,
	SUBSCRIBE_EVENTGROUP_ACK = 0x07,
	STOP_SUBSCRIBE_EVENTGROUP_ACK = 0x07,
	UNKNOWN = 0xFF
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_ENUMERATION_TYPES_HPP

//
// enumeration_types.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SD_ENUMERATION_TYPES_HPP
#define VSOMEIP_SD_ENUMERATION_TYPES_HPP

#include <cstdint>

namespace vsomeip {
namespace sd {

enum class option_type : uint8_t {
	CONFIGURATION = 0x1,
	LOAD_BALANCING = 0x2,
	PROTECTION = 0x3,
	IP4_ENDPOINT = 0x4,
	IP6_ENDPOINT = 0x6,
	IP4_MULTICAST = 0x14,
	IP6_MULTICAST = 0x16,
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

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_ENUMERATION_TYPES_HPP

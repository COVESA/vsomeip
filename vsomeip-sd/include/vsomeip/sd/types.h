//
// types.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_TYPES_H__
#define __VSOMEIP_SD_TYPES_H__

#include <cstdint>

namespace vsomeip {

namespace sd {

// Service Discovery Message
typedef uint8_t Flags;
typedef uint8_t EventType;
typedef uint8_t OptionIndex;
typedef uint16_t InstanceId;
typedef uint8_t MajorVersion;
typedef uint32_t TimeToLive;
typedef uint32_t MinorVersion;
typedef uint16_t EventGroupId;

// IP Endpoint Option
typedef uint32_t IPv4Address;
typedef uint8_t IPv6Address[128];
typedef uint16_t IPPort;
enum class IPProtocol : uint8_t {
	TCP = 0x06,
	UDP = 0x11,
	UNKNOWN = 0xFF
};

// Load Balancing Option
typedef uint16_t Priority;
typedef uint16_t Weight;

// Protection Option
typedef uint32_t AliveCounter;
typedef uint32_t Crc;

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_TYPES_H__

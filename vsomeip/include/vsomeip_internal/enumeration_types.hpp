//
// enumeration_types.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_ENUMERATION_TYPES_HPP
#define VSOMEIP_INTERNAL_ENUMERATION_TYPES_HPP

#include <cstdint>

namespace vsomeip {

enum class command_enum : uint8_t {
	REGISTER_APPLICATION = 0x10,
	DEREGISTER_APPLICATION = 0x11,
	REGISTER_SERVICE = 0x12,
	DEREGISTER_SERVICE = 0x13,
	REQUEST_SERVICE = 0x14,
	RELEASE_SERVICE = 0x15,
	START_SERVICE = 0x16,
	STOP_SERVICE = 0x17,

	REGISTER_METHOD = 0x18,
	DEREGISTER_METHOD = 0x19,

	REGISTER_SERVICE_ACK = 0x22,
	DEREGISTER_SERVICE_ACK = 0x23,
	REQUEST_SERVICE_ACK = 0x24,
	RELEASE_SERVICE_ACK = 0x25,

	APPLICATION_INFO = 0x31,
	APPLICATION_LOST = 0x32,

	SOMEIP_MESSAGE = 0x40,

	PING = 0xE0,
	PONG = 0xE1
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_ENUMERATION_TYPES_HPP

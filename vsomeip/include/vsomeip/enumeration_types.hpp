//
// enumeration_types.hpp
//
// Date: 	Nov 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef ENUMERATION_TYPES_HPP
#define ENUMERATION_TYPES_HPP

#include <cstdint>

namespace vsomeip {

enum class message_type : uint8_t {
	REQUEST = 0x0,
	REQUEST_NO_RETURN = 0x1,
	NOTIFICATION = 0x2,
	REQUEST_ACK = 0x40,
	REQUEST_NO_RETURN_ACK = 0x41,
	NOTIFICATION_ACK = 0x42,
	RESPONSE = 0x80,
	ERROR = 0x81,
	RESPONSE_ACK = 0xC0,
	ERROR_ACK = 0xC1,
	UNKNOWN = 0xFF
};

enum class return_code : uint8_t {
	OK = 0x0,
	NOT_OK = 0x1,
	UNKNOWN_SERVICE = 0x2,
	UNKNOWN_METHOD = 0x3,
	NOT_READY = 0x4,
	NOT_REACHABLE = 0x5,
	TIMEOUT = 0x6,
	WRONG_PROTOCOL_VERSION = 0x7,
	WRONG_INTERFACE_VERSION = 0x8,
	MALFORMED_MESSAGE = 0x9,
	UNKNOWN = 0xFF
};

enum class ip_protocol : uint8_t {
	TCP = 0x06,
	UDP = 0x11,
	UNKNOWN = 0xFF
};

} // namespace vsomeip

#endif // ENUMERATION_TYPES_HPP

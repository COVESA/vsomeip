//
// enumeration_types.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_ENUMERATION_TYPES_HPP
#define VSOMEIP_ENUMERATION_TYPES_HPP

#include <cstdint>

namespace vsomeip {

/// \enum message_type
/// Message type constants according to Some/IP specificiation.
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

/// \enum return_code
/// Return code constants according to Some/IP specificiation.
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

/// \enum transport_protocol
/// Constants representing the supported transport protocols.
enum class transport_protocol : uint8_t {
	UDP = 0x06,
	TCP = 0x11,
	UNKNOWN = 0xFF
};

/// \enum transport_protocol_version
/// Constants representing the supported transport protocol versions.
enum class transport_protocol_version : uint8_t {
	V4 = 0x04,
	V6 = 0x06,
	UNKNOWN = 0xFF
};

enum class command_enum : uint8_t {
	REGISTER_APPLICATION = 0x10,
	DEREGISTER_APPLICATION = 0x11,

	REGISTER_APPLICATION_ACK = 0x20,
	DEREGISTER_APPLICATION_ACK = 0x21,

	PING = 0xe0,
	PONG = 0xe1
};

} // namespace vsomeip

#endif // VSOMEIP_ENUMERATION_TYPES_HPP

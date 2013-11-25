//
// types.h
//
// Date: 	Oct 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_TYPES_H__
#define __VSOMEIP_TYPES_H__

#include <cstdint>

#include <vsomeip/config.h>

namespace vsomeip {

// Content of Some/IP messages
typedef uint32_t MessageId;
typedef uint16_t ServiceId;
typedef uint16_t MethodId;

typedef uint32_t RequestId;
typedef uint16_t ClientId;
typedef uint16_t SessionId;

typedef uint8_t ProtocolVersion;
typedef uint8_t InterfaceVersion;

enum class MessageType : uint8_t {
	REQUEST = 0x00,
	REQUEST_NO_RETURN = 0x1,
	NOTIFICATION = 0x02,
	UNKNOWN = 0xFF
};

enum class ReturnCode : uint8_t {
	REQUEST = 0x00,
	REQUEST_NO_RETURN = 0x00,
	NOTIFICATION = 0x00,
	OK = 0x00,
	NOT_OK = 0x01,
	UNKNOWN_SERVICE = 0x02,
	UNKNOWN_METHOD = 0x03,
	NOT_READY = 0x04,
	NOT_REACHABLE = 0x05,
	TIMEOUT = 0x06,
	WRONG_PROTOCOL_VERSION = 0x07,
	WRONG_INTERFACE_VERSION = 0x08,
	MALFORMED_MESSAGE = 0x09,
	UNKNOWN = 0xFF
};

} // vsomeip

#endif // __VSOMEIP_TYPES_H__

// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENUMERATION_TYPES_HPP
#define VSOMEIP_ENUMERATION_TYPES_HPP

#include <cstdint>

namespace vsomeip {

enum class event_type_e : uint8_t {
	REGISTERED = 0x0,
	DEREGISTERED = 0x1
};


// SIP_RPC_684
enum class message_type_e : uint8_t {
	REQUEST = 0x00,
	REQUEST_NO_RETURN = 0x01,
	NOTIFICATION = 0x02,
	REQUEST_ACK = 0x40,
	REQUEST_NO_RETURN_ACK = 0x41,
	NOTIFICATION_ACK = 0x42,
	RESPONSE = 0x80,
	ERROR = 0x81,
	RESPONSE_ACK = 0xC0,
	ERROR_ACK = 0xC1,
	UNKNOWN = 0xFF
};

// SIP_RPC_371
enum class return_code_e : uint8_t {
	E_OK = 0x00,
	E_NOT_OK = 0x01,
	E_UNKNOWN_SERVICE = 0x02,
	E_UNKNOWN_METHOD = 0x03,
	E_NOT_READY = 0x04,
	E_NOT_REACHABLE = 0x05,
	E_TIMEOUT = 0x06,
	E_WRONG_PROTOCOL_VERSION = 0x07,
	E_WRONG_INTERFACE_VERSION = 0x08,
	E_MALFORMED_MESSAGE = 0x09,
	E_WRONG_MESSAGE_TYPE = 0xA,
	E_UNKNOWN = 0xFF
};

} // namespace vsomeip

#endif // VSOMEIP_ENUMERATION_TYPES_HPP

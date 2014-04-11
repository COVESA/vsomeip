//
// constants.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_CONSTANTS_HPP
#define VSOMEIP_INTERNAL_CONSTANTS_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>

namespace vsomeip {

const uint8_t vsomeip_protocol_reserved_byte
	= 0x00;

const uint16_t vsomeip_protocol_reserved_word
	= 0x0000;

const uint32_t vsomeip_protocol_reserved_long
	= 0x00000000;

const uint32_t VSOMEIP_STATIC_HEADER_SIZE = 8;

const uint8_t VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_ID = 0x00;
const uint8_t VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_ID = 0x80;
const length VSOMEIP_MAGIC_COOKIE_SIZE
	= 0x00000008;
const request_id VSOMEIP_MAGIC_COOKIE_REQUEST_ID
	= 0xDEADBEEF;
const protocol_version VSOMEIP_MAGIC_COOKIE_PROTOCOL_VERSION
	= 0x01;
const interface_version VSOMEIP_MAGIC_COOKIE_INTERFACE_VERSION
	= 0x01;
const message_type_enum VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_TYPE
	= message_type_enum::REQUEST_NO_RETURN;
const message_type_enum VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_TYPE
	= message_type_enum::NOTIFICATION;
const return_code_enum VSOMEIP_MAGIC_COOKIE_RETURN_CODE
	= return_code_enum::OK;

const uint8_t VSOMEIP_LENGTH_POSITION = 4;
const uint8_t VSOMEIP_STATIC_SERVICE_DISCOVERY_DATA_LENGTH = 12;
const uint8_t VSOMEIP_ENTRY_LENGTH = 16;
const uint8_t VSOMEIP_OPTION_HEADER_LENGTH = 3;

#define VSOMEIP_POS_MESSAGE_TYPE	14

#define VSOMEIP_UNKNOWN_ID		0xFFFF
#define VSOMEIP_METHOD_ID_ZERO	0x0000

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_CONSTANTS_HPP

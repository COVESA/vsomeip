//
// constants_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_CONSTANTS_IMPL_HPP
#define VSOMEIP_IMPL_CONSTANTS_IMPL_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>

namespace vsomeip {

const uint8_t vsomeip_protocol_reserved_byte
	= 0x00;

const uint16_t vsomeip_protocol_reserved_word
	= 0x0000;

const uint32_t vsomeip_protocol_reserved_long
	= 0x00000000;

const uint32_t VSOMEIP_STATIC_HEADER_LENGTH = 8;

// Magic Cookie commons
const service_id VSOMEIP_MAGIC_COOKIE_SERVICE_ID = 0xFFFF;
const client_id VSOMEIP_MAGIC_COOKIE_CLIENT_ID = 0xDEAD;
const session_id VSOMEIP_MAGIC_COOKIE_SESSION_ID	 = 0xBEEF;
const interface_version VSOMEIP_MAGIC_COOKIE_INTERFACE_VERSION = 0x01;

// Magic Cookie sent from Client to Service
const method_id VSOMEIP_CLIENT_MAGIC_COOKIE_METHOD_ID = 0x0000;
const message_type VSOMEIP_CLIENT_MAGIC_COOKIE_MESSAGE_TYPE
	= message_type::REQUEST_NO_RETURN;

// Magic Cookie sent from Service to Client
const method_id VSOMEIP_SERVICE_MAGIC_COOKIE_METHOD_ID = 0x8000;
const message_type VSOMEIP_SERVICE_MAGIC_COOKIE_MESSAGE_TYPE
	= message_type::NOTIFICATION;

const uint8_t VSOMEIP_LENGTH_POSITION = 4;
const uint8_t VSOME_IP_STATIC_SERVICE_DISCOVERY_DATA_LENGTH = 12;
const uint8_t VSOMEIP_ENTRY_LENGTH = 16;
const uint8_t VSOMEIP_OPTION_HEADER_LENGTH = 3;

} // namespace vsomeip

#endif // VSOMEIP_IMPL_CONSTANTS_IMPL_HPP

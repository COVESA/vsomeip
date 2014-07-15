// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CONSTANTS_HPP
#define VSOMEIP_CONSTANTS_HPP

#include <string>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>

namespace vsomeip {

const std::string VSOMEIP_BASE_PATH = "/tmp/vsomeip-";

const major_version_t VSOMEIP_DEFAULT_MAJOR = 0x01;
const minor_version_t VSOMEIP_DEFAULT_MINOR = 0x000000;
const ttl_t VSOMEIP_DEFAULT_TTL = 0xFFFFFF; // basically means "forever"

const std::string VSOMEIP_DEFAULT_MULTICAST = "224.0.0.0";
const uint16_t VSOMEIP_DEFAULT_PORT = 30500;
const uint16_t VSOMEIP_ILLEGAL_PORT = 0xFFFF;

const service_t VSOMEIP_ANY_SERVICE = 0xFFFF;
const instance_t VSOMEIP_ANY_INSTANCE = 0xFFFF;
const method_t VSOMEIP_ANY_METHOD = 0xFFFF;
const major_version_t VSOMEIP_ANY_MAJOR = 0xFF;
const minor_version_t VSOMEIP_ANY_MINOR = 0xFFFFFF;
const ttl_t VSOMEIP_ANY_TTL = 1;

const byte_t VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_ID = 0x00;
const byte_t VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_ID = 0x80;
const length_t VSOMEIP_MAGIC_COOKIE_SIZE = 0x00000008;
const request_t VSOMEIP_MAGIC_COOKIE_REQUEST_ID	= 0xDEADBEEF;
const protocol_version_t VSOMEIP_MAGIC_COOKIE_PROTOCOL_VERSION = 0x01;
const interface_version_t VSOMEIP_MAGIC_COOKIE_INTERFACE_VERSION = 0x01;
const message_type_e VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_TYPE = message_type_e::REQUEST_NO_RETURN;
const message_type_e VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_TYPE = message_type_e::NOTIFICATION;
const return_code_e VSOMEIP_MAGIC_COOKIE_RETURN_CODE = return_code_e::E_OK;

const byte_t client_cookie[] = {
	0xFF, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08,
	0xDE, 0xAD, 0xBE, 0xEF,
	0x01, 0x01, 0x01, 0x00
};

const byte_t server_cookie[] = {
	0xFF, 0xFF, 0x80, 0x00,
	0x00, 0x00, 0x00, 0x08,
	0xDE, 0xAD, 0xBE, 0xEF,
	0x01, 0x01, 0x02, 0x00
};

} // namespace vsomeip

#endif // VSOMEIP_CONSTANTS_HPP

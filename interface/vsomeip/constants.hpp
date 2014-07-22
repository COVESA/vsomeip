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

const std::string base_path = "/tmp/vsomeip-";

const major_version_t default_major = 0x01;
const minor_version_t default_minor = 0x000000;
const ttl_t default_ttl = 0xFFFFFF; // basically means "forever"

const std::string default_multicast = "224.0.0.0";
const uint16_t default_port = 30500;
const uint16_t illegal_port = 0xFFFF;

const service_t any_service = 0xFFFF;
const instance_t any_instance = 0xFFFF;
const method_t any_method = 0xFFFF;
const major_version_t any_major = 0xFF;
const minor_version_t any_minor = 0xFFFFFF;
const ttl_t any_ttl = 1;

const byte_t magic_cookie_client_message = 0x00;
const byte_t magic_cookie_service_message = 0x80;
const length_t magic_cookie_size = 0x00000008;
const request_t magic_cookie_request = 0xDEADBEEF;
const protocol_version_t magic_cookie_protocol_version = 0x01;
const interface_version_t magic_cookie_interface_version = 0x01;
const message_type_e magic_cookie_client_message_type = message_type_e::REQUEST_NO_RETURN;
const message_type_e magic_cookie_service_message_type = message_type_e::NOTIFICATION;
const return_code_e magic_cookie_return_code = return_code_e::E_OK;

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

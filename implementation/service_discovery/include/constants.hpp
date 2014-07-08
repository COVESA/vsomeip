// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_CONSTANTS_HPP
#define VSOMEIP_SD_CONSTANTS_HPP

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace sd {

const service_t VSOMEIP_SD_SERVICE = 0xFFFF;
const instance_t VSOMEIP_SD_INSTANCE = 0x0000;
const method_t  VSOMEIP_SD_METHOD  = 0x8100;
const client_t VSOMEIP_SD_CLIENT = 0x0000;
const protocol_version_t VSOMEIP_SD_PROTOCOL_VERSION = 0x01;
const interface_version_t VSOMEIP_SD_INTERFACE_VERSION = 0x01;
const message_type_e VSOMEIP_SD_MESSAGE_TYPE = message_type_e::NOTIFICATION;
const return_code_e VSOMEIP_SD_RETURN_CODE = return_code_e::E_OK;

namespace protocol {

const uint8_t reserved_byte = 0x0;
const uint16_t reserved_word = 0x0;
const uint32_t reserved_long = 0x0;

const uint8_t udp = 0x06;
const uint8_t tcp = 0x11;

} // namespace protocol
} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_CONSTANTS_HPP

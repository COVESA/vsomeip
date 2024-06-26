// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __PRIMITIVES_HPP__
#define __PRIMITIVES_HPP__

#include <array>
#include <cstdint>

namespace vsomeip_utilities {
namespace someip {
namespace types {

/**
 * @brief SomeIp header types
 *
 */
using serviceId_t = std::uint16_t;
using methodId_t =  std::uint16_t;

using length_t = std::uint32_t;

using clientId_t = std::uint16_t;
using sessionId_t = std::uint16_t;

using protocolVersion_t = std::uint8_t;
using interfaceVersion_t = std::uint8_t;

enum class messageType_e : std::uint8_t {
    E_MT_REQUEST = 0x00,
    E_MT_REQUEST_NO_RETURN = 0x01,
    E_MT_NOTIFICATION = 0x02,
    E_MT_TRANSPORT = 0x20,
    E_MT_REQUEST_ACK = 0x40,
    E_MT_REQUEST_NO_RETURN_ACK = 0x41,
    E_MT_NOTIFICATION_ACK = 0x42,
    E_MT_RESPONSE = 0x80,
    E_MT_ERROR = 0x81,
    E_MT_RESPONSE_ACK = 0xC0,
    E_MT_ERROR_ACK = 0xC1,
    E_MT_UNKNOWN = 0xFF
};

enum class returnCode_e : std::uint8_t {
    E_RC_OK = 0x00,
    E_RC_NOT_OK = 0x01,
    E_RC_UNKNOWN_SERVICE = 0x02,
    E_RC_UNKNOWN_METHOD = 0x03,
    E_RC_NOT_READY = 0x04,
    E_RC_NOT_REACHABLE = 0x05,
    E_RC_TIMEOUT = 0x06,
    E_RC_WRONG_PROTOCOL_VERSION = 0x07,
    E_RC_WRONG_INTERFACE_VERSION = 0x08,
    E_RC_MALFORMED_MESSAGE = 0x09,
    E_RC_WRONG_MESSAGE_TYPE = 0x0A,
    E_RC_UNKNOWN = 0xFF
};

/**
 * @brief SomeIp SD Entry/Option types
 */

// Entries
using sdFlags_t = std::uint8_t;
using sdEntryType = std::uint8_t;
using sdIndex1st = std::uint8_t;
using sdIndex2nd = std::uint8_t;
using sdOptsCount = std::uint8_t;
using sdServiceId = std::uint16_t;
using sdInstanceId = std::uint16_t;
using sdMajorVersion = std::uint8_t;
using sdMinorVersion = std::uint32_t;
using sdEventGroupId = std::uint16_t;
using sdCounter = std::uint8_t;
using sdTtl = std::uint32_t;
using sdEntryLength = std::uint16_t;

// Options
using sdOptionLength = std::uint16_t;
using sdOptionType = std::uint8_t;
using sdOptionProtocol = std::uint8_t;
using sdOptionPort = std::uint16_t;
using sdOptionReserved = std::uint8_t;

} // namespace types
} // namespace someip
} // namespace vsomeip_utilities

#endif // __PRIMITIVES_HPP__

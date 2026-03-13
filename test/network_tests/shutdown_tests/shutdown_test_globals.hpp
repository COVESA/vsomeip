// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef SHUTDOWN_TEST_GLOBALS_HPP_
#define SHUTDOWN_TEST_GLOBALS_HPP_

#include <cstdint>

namespace shutdown_test {
constexpr std::uint32_t SHUTDOWN_SIZE_UDS = 1024 * 128;
constexpr std::uint32_t SHUTDOWN_SIZE_UDP = 1024;
constexpr std::uint32_t SHUTDOWN_SIZE_TCP = 1024 * 128;
constexpr std::uint32_t SHUTDOWN_NUMBER_MESSAGES = 5;
constexpr vsomeip::byte_t DATA_SERVICE_TO_CLIENT = 0xAA;
constexpr vsomeip::byte_t DATA_CLIENT_TO_SERVICE = 0xFF;

constexpr vsomeip::service_t TEST_SERVICE_SERVICE_ID = 0x1236;

constexpr vsomeip::instance_t TEST_SERVICE_INSTANCE_ID = 0x1;
constexpr vsomeip::method_t TEST_SERVICE_METHOD_ID = 0x8421;

constexpr vsomeip::method_t STOP_METHOD = 0x0999;
}

#endif /* SHUTDOWN_TEST_GLOBALS_HPP_ */

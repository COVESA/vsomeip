// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include <vsomeip/vsomeip.hpp>

namespace memory_test_someip_tp {

constexpr vsomeip::service_t SERVICE = 0xb51c;
constexpr vsomeip::instance_t INSTANCE = 0x0001;
constexpr vsomeip::method_t START_METHOD = 0x0422;
constexpr vsomeip::method_t STOP_METHOD = 0x0423;
constexpr vsomeip::method_t WARMUP_METHOD = 0x0424;
constexpr vsomeip::method_t RETENTION_CHECK_METHOD = 0x0426;
constexpr vsomeip::event_t RETENTION_EVENT = 0x8001;
constexpr vsomeip::eventgroup_t EVENTGROUP = 0x0001;
constexpr vsomeip::major_version_t MAJOR = 0x01;
constexpr vsomeip::minor_version_t MINOR = 0x00;

constexpr std::size_t RETENTION_NOTIFICATIONS = 500;
constexpr std::size_t RETENTION_PAYLOAD_SIZE = 1500;

constexpr auto AVAILABILITY_TIMEOUT = std::chrono::seconds(10);
constexpr auto RESPONSE_TIMEOUT = std::chrono::seconds(10);
constexpr auto EVENT_TIMEOUT = std::chrono::seconds(10);
constexpr auto RETENTION_RSS_SETTLING_TIMEOUT = std::chrono::seconds(1);
constexpr auto TEST_TIMEOUT = std::chrono::seconds(30);

constexpr std::uint64_t RETENTION_RSS_GROWTH_LIMIT_KIB = 256;

} // namespace memory_test_someip_tp

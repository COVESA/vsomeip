// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include <vsomeip/vsomeip.hpp>

namespace memory_test_pending_trains {

constexpr vsomeip::service_t SERVICE = 0xb51b;
constexpr vsomeip::instance_t INSTANCE = 0x0001;
constexpr vsomeip::method_t TRAFFIC_METHOD = 0x0421;
constexpr vsomeip::method_t START_METHOD = 0x0422;
constexpr vsomeip::method_t STOP_METHOD = 0x0423;
constexpr vsomeip::major_version_t MAJOR = 0x01;
constexpr vsomeip::minor_version_t MINOR = 0x00;

constexpr std::size_t WARMUP_REQUESTS = 16;
constexpr std::size_t BURST_REQUESTS = 16;
constexpr std::size_t PAYLOAD_SIZE = 256 * 1024;

constexpr auto AVAILABILITY_TIMEOUT = std::chrono::seconds(10);
constexpr auto RESPONSE_TIMEOUT = std::chrono::seconds(10);
constexpr auto TRAIN_DRAIN_DURATION = std::chrono::milliseconds(1200);
constexpr auto HELD_RSS_SETTLING_TIMEOUT = std::chrono::milliseconds(500);
constexpr auto RETAINED_RSS_SETTLING_TIMEOUT = std::chrono::seconds(2);
constexpr auto TEST_TIMEOUT = std::chrono::seconds(30);

// Twice the single request that the 300,000-byte queue limit lets through, and an
// eighth of the BURST_REQUESTS * PAYLOAD_SIZE bytes the burst offers.
constexpr std::uint64_t HELD_RSS_GROWTH_LIMIT_KIB = 512;
constexpr std::uint64_t RETAINED_RSS_GROWTH_LIMIT_KIB = 128;

} // namespace memory_test_pending_trains

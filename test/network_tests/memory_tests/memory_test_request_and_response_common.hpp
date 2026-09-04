// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include <vsomeip/vsomeip.hpp>

namespace memory_test_request_and_response {

constexpr vsomeip::service_t SERVICE = 0xb51a;
constexpr vsomeip::instance_t INSTANCE = 0x0001;
constexpr vsomeip::method_t REQUEST_METHOD = 0x0421;
constexpr vsomeip::method_t START_MEASUREMENT_METHOD = 0x0422;
constexpr vsomeip::method_t STOP_MEASUREMENT_METHOD = 0x0423;
constexpr vsomeip::method_t BARRIER_METHOD = 0x0424;
constexpr vsomeip::major_version_t MAJOR = 0x01;
constexpr vsomeip::minor_version_t MINOR = 0x00;

constexpr std::size_t WARMUP_REQUESTS = 20;
constexpr std::size_t REQUESTS_PER_BATCH = 100;
constexpr std::size_t MEASURED_BATCHES = 100;
constexpr std::size_t MEASURED_REQUESTS = REQUESTS_PER_BATCH * MEASURED_BATCHES;
constexpr std::size_t PAYLOAD_SIZE = 256;

constexpr auto AVAILABILITY_TIMEOUT = std::chrono::seconds(10);
constexpr auto RESPONSE_TIMEOUT = std::chrono::seconds(10);
constexpr auto FINAL_RESPONSE_GRACE_PERIOD = std::chrono::milliseconds(100);
constexpr auto RSS_SETTLING_TIMEOUT = std::chrono::seconds(2);
constexpr auto TEST_TIMEOUT = std::chrono::seconds(60);

constexpr std::uint64_t MEMORY_GROWTH_LIMIT_KIB = 512;

} // namespace memory_test_request_and_response

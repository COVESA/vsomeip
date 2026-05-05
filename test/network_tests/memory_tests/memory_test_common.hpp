// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t MEMORY_SERVICE = 0xb519;
constexpr vsomeip::instance_t MEMORY_INSTANCE = 0x0001;
constexpr vsomeip::method_t MEMORY_START_METHOD = 0x0998;
constexpr vsomeip::method_t MEMORY_STOP_METHOD = 0x0999;
constexpr vsomeip::event_t MEMORY_EVENT = 0x8008;
constexpr vsomeip::eventgroup_t MEMORY_EVENTGROUP = 0x0005;
constexpr vsomeip::major_version_t MEMORY_MAJOR = 0x01;
constexpr vsomeip::minor_version_t MEMORY_MINOR = 0x01;

constexpr auto MEMORY_CHECKER_INTERVAL = std::chrono::seconds(5);
// Target throughput: ~12 MB/s
// Each iteration sends: 40 messages × 4000 bytes = 160,000 bytes
// Required rate: 12,000,000 / 160,000 = 75 iterations/sec
// → 1 iteration ≈ 13.33 ms
//
// We use 7 ms + 7 ms = 14 ms per iteration (~71.4 iterations/sec)
// → ~11.4 MB/s
//
// Slightly below target to account for timing inaccuracies of sleep_for()
// and avoid overshooting the bandwidth.
constexpr auto MESSAGE_SENDER_INTERVAL = std::chrono::milliseconds(7);
constexpr auto WATCHDOG_INTERVAL = std::chrono::seconds(2);
constexpr auto WAIT_AVAILABILITY = std::chrono::milliseconds(15000);
constexpr auto WAIT_START_MESSAGE = std::chrono::milliseconds(10000);
constexpr auto WAIT_STOP_MESSAGE = std::chrono::seconds(30);

constexpr uint16_t TEST_EVENT_NUMBER = 20;
constexpr uint16_t TEST_MESSAGE_NUMBER = 9000;
constexpr int NOTIFY_PAYLOAD_SIZE = 4000;
constexpr double MEMORY_LOAD_LIMIT = 1.15; // meaning 15% limit above the average value

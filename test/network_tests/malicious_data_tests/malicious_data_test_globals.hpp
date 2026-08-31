// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>

namespace malicious_data_test {

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
    vsomeip::event_t event_id;
    vsomeip::eventgroup_t eventgroup_id;
    vsomeip::method_t shutdown_method_id;
    vsomeip::method_t notify_method_id;
};

struct service_info service = {0x3344, 0x1, 0x1111, 0x8002, 0x1, 0x1404, 0x4242};
constexpr uint32_t EXPECTED_MESSAGE_COUNT = 9;

// The peer closes a desynchronised stream through wait_until_sent(), armed for 10 s.
constexpr std::chrono::seconds CONNECTION_CLOSE_TIMEOUT = std::chrono::seconds(20);

// Bound for every barrier that waits for the peer to react: sd message, connection, error response.
constexpr std::chrono::seconds BARRIER_TIMEOUT = std::chrono::seconds(10);

// Bound for the messages still in flight when the shutdown request arrives.
constexpr std::chrono::seconds DELIVERY_TIMEOUT = std::chrono::seconds(10);

enum test_mode_e { MALICIOUS_EVENTS, PROTOCOL_VERSION, MESSAGE_TYPE, RETURN_CODE, WRONG_HEADER_FIELDS_UDP };
}

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <csignal>
#include <signal.h>
#include <vsomeip/primitive_types.hpp>

namespace invalid_event_test_globals {

// Identifier of the test service.
static constexpr vsomeip_v3::service_t TEST_SERVICE_ID = 0x1234;

// Identifier of the test service instance.
static constexpr vsomeip_v3::instance_t TEST_INSTANCE_ID = 0x0001;

// Identifier of the valid event offered by the test service.
static constexpr vsomeip_v3::event_t TEST_VALID_EVENT_ID = 0x0001;

// Identifier of the invalid event requested by the test client.
static constexpr vsomeip_v3::event_t TEST_INVALID_EVENT_ID = 0x0002;

// Identifier of the test service event group.
static constexpr vsomeip_v3::eventgroup_t TEST_EVENTGROUP_ID = 0x0001;

// Identifier of the special service.
static constexpr vsomeip_v3::service_t ON_REGISTERED_SERVICE_ID = 0x5678;

// Identifier of the special service instance.
static constexpr vsomeip_v3::instance_t ON_REGISTERED_INSTANCE_ID = 0x0001;

// Prepares the thread to react to SIGUSR1.
//
// Should be called as early as possible.
static inline void block_signal() {
    // Create a mask to block SIGUSR1.
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

    // Block the signal.
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

// Block until SIGUSR1 is received.
static inline void wait_for_signal() {
    // Create a mask to block SIGUSR1.
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

    // Wait for the signal.
    int sig = 0;
    sigwait(&set, &sig);
}

}

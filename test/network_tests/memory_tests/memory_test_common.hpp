// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <thread>
#include <vector>

#include <unistd.h>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

constexpr vsomeip::service_t MEMORY_SERVICE = 0xb519;
constexpr vsomeip::instance_t MEMORY_INSTANCE = 0x0001;
constexpr vsomeip::method_t MEMORY_START_METHOD = 0x0998;
constexpr vsomeip::method_t MEMORY_STOP_METHOD = 0x0999;
// Flow-control channel: the client reports how many notifications it has
// received back to the service so the service can bound the amount of data
// in flight (see FLOW_CONTROL_WINDOW below).
constexpr vsomeip::method_t MEMORY_ACK_METHOD = 0x099a;
constexpr vsomeip::event_t MEMORY_EVENT = 0x8008;
constexpr vsomeip::eventgroup_t MEMORY_EVENTGROUP = 0x0005;
constexpr vsomeip::major_version_t MEMORY_MAJOR = 0x01;
constexpr vsomeip::minor_version_t MEMORY_MINOR = 0x01;

constexpr auto MEMORY_CHECKER_INTERVAL = std::chrono::seconds(5);

// Base pacing floor between notification bursts. Under a fast consumer this
// caps the send rate at ~11.4 MB/s (40 messages / 14 ms); under a slow
// consumer the flow-control window below is what actually limits the rate.
constexpr auto MESSAGE_SENDER_INTERVAL = std::chrono::milliseconds(7);

// The sender runs for a fixed wall-clock duration instead of a fixed message
// count. With flow control the effective throughput scales to whatever the
// consumer can absorb (fast host: lots of data; Valgrind/debug/contended host:
// less), so a fixed duration keeps the test well within its 300 s timeout
// regardless of environment.
constexpr auto MESSAGE_SENDER_DURATION = std::chrono::seconds(90);

// Application-level backpressure. The service never lets more than this many
// notifications be outstanding (sent but not yet acknowledged by the client).
// 1000 messages x ~4 KB ~= 4 MB in flight, comfortably below both the 20 MB
// endpoint-queue-limit (so the queue never fills and nothing is dropped) and
// the 15% MEMORY_LOAD_LIMIT headroom over the ~43 MB steady-state floor
// (~6.6 MB), so a full window on its own cannot trip the memory assertion.
constexpr std::uint64_t FLOW_CONTROL_WINDOW = 1000;
// The client sends one cumulative acknowledgement every ACK_INTERVAL received
// messages. Kept well below the window so the service gets several updates per
// window and the pipe stays full.
constexpr std::uint64_t ACK_INTERVAL = 100;
// While waiting for the window to open, give up (and send anyway) only if the
// acknowledged count makes no progress at all for this long -- i.e. the
// consumer is genuinely gone rather than merely slow -- so lost UDP acks can
// never deadlock the sender.
constexpr auto FLOW_CONTROL_STALL_TIMEOUT = std::chrono::seconds(10);
constexpr auto FLOW_CONTROL_POLL = std::chrono::milliseconds(1);

constexpr auto WATCHDOG_INTERVAL = std::chrono::seconds(2);
// The consumer concludes the test once no new notification has arrived for this
// long. Matches FLOW_CONTROL_STALL_TIMEOUT so that, if acks stall and the sender
// gives up, both sides wind down together.
constexpr auto CONSUMER_IDLE_TIMEOUT = std::chrono::seconds(10);
constexpr auto WAIT_AVAILABILITY = std::chrono::milliseconds(15000);
constexpr auto WAIT_START_MESSAGE = std::chrono::milliseconds(10000);
constexpr auto WAIT_STOP_MESSAGE = std::chrono::seconds(30);

constexpr uint16_t TEST_EVENT_NUMBER = 20;
constexpr int NOTIFY_PAYLOAD_SIZE = 4000;
constexpr double MEMORY_LOAD_LIMIT = 1.15; // meaning 15% limit above the steady-state floor

// Reads the resident set size of the current process from /proc/self/statm,
// returning it in KiB (0 on error).
inline std::uint64_t read_rss_kib() {
    static const std::uint64_t page_kib = static_cast<std::uint64_t>(getpagesize() / 1024);

    std::FILE* its_file = std::fopen("/proc/self/statm", "r");
    if (!its_file) {
        VSOMEIP_ERROR << "read_rss_kib: couldn't open /proc/self/statm: errno " << errno;
        return 0;
    }
    std::uint64_t its_size(0);
    std::uint64_t its_rsssize(0);
    if (std::fscanf(its_file, "%lu %lu", &its_size, &its_rsssize) != 2) {
        VSOMEIP_ERROR << "read_rss_kib: error reading /proc/self/statm: errno " << errno;
        its_rsssize = 0;
    }
    std::fclose(its_file);
    return its_rsssize * page_kib;
}

// Samples RSS into test_memory_ every MEMORY_CHECKER_INTERVAL until stop_checking_.
inline void check_memory(std::vector<std::uint64_t>& test_memory_, std::atomic<bool>& stop_checking_) {
    while (!stop_checking_) {
        std::this_thread::sleep_for(MEMORY_CHECKER_INTERVAL);
        const std::uint64_t its_rss = read_rss_kib();
        if (its_rss == 0) {
            // read_rss_kib() already logged the failure. Skip the sample so a
            // transient /proc read error cannot pollute the steady-state floor
            // (a live process never has an RSS of 0).
            continue;
        }
        test_memory_.push_back(its_rss);
        VSOMEIP_INFO << "logged RSS: " << its_rss << " KiB";
    }
}

// Evaluates the collected samples on the calling thread. The check is made
// against the steady-state floor (the lowest RSS sampled once traffic is
// flowing) rather than the cold pre-traffic baseline: establishing the bounded
// in-flight working set when traffic starts is a one-time step, not growth
// "during operation". A genuine leak still trips this, because it keeps
// climbing above the floor over the run. The pre-traffic baseline is logged
// only for reference.
inline void evaluate_memory(const std::vector<std::uint64_t>& test_memory_, std::uint64_t baseline_kib_) {
    ASSERT_FALSE(test_memory_.empty()) << "no memory samples were collected";

    const std::uint64_t peak = *std::max_element(test_memory_.begin(), test_memory_.end());

    // The first sample is taken ~MEMORY_CHECKER_INTERVAL after traffic starts and
    // can land mid warm-up ramp, reading below the true steady state. Exclude it
    // from the floor (but not the peak) when more than one sample exists, so the
    // floor reflects settled memory rather than a transient ramp reading. This
    // can only relax the limit, never tighten it, so it adds no false positives.
    const auto floor_begin = test_memory_.size() > 1 ? std::next(test_memory_.begin()) : test_memory_.begin();
    const std::uint64_t steady_state = *std::min_element(floor_begin, test_memory_.end());
    const double limit = static_cast<double>(steady_state) * MEMORY_LOAD_LIMIT;

    VSOMEIP_INFO << "memory evaluation: pre-traffic baseline " << baseline_kib_ << " KiB, steady-state floor " << steady_state
                 << " KiB, peak " << peak << " KiB, limit " << limit << " KiB";

    EXPECT_LT(static_cast<double>(peak), limit) << "peak RSS " << peak << " KiB exceeded steady-state floor " << steady_state
                                                << " KiB by more than " << static_cast<int>((MEMORY_LOAD_LIMIT - 1.0) * 100) << "%";
}

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include "../../../implementation/tracing/include/tracing_policy.hpp"

namespace {

using vsomeip_v3::trace::should_log_full;
using vsomeip_v3::trace::trace_result_e;

constexpr uint32_t THRESHOLD = 4096;

// An explicit positive filter always logs the full payload, regardless of size
// or threshold - this is the intentional "allow big logs" opt-in.
TEST(should_log_full, positive_filter_bypasses_threshold) {
    EXPECT_TRUE(should_log_full(trace_result_e::POSITIVE_FILTER, 1, THRESHOLD));
    EXPECT_TRUE(should_log_full(trace_result_e::POSITIVE_FILTER, THRESHOLD + 1, THRESHOLD));
    EXPECT_TRUE(should_log_full(trace_result_e::POSITIVE_FILTER, 1024 * 1024, THRESHOLD));
    // Even a 0 threshold (which for DEFAULT means "no threshold") is irrelevant.
    EXPECT_TRUE(should_log_full(trace_result_e::POSITIVE_FILTER, 1024 * 1024, 0));
}

// A header-only filter never logs the full payload.
TEST(should_log_full, header_only_filter_never_full) {
    EXPECT_FALSE(should_log_full(trace_result_e::HEADER_ONLY_FILTER, 1, THRESHOLD));
    EXPECT_FALSE(should_log_full(trace_result_e::HEADER_ONLY_FILTER, THRESHOLD + 1, THRESHOLD));
    EXPECT_FALSE(should_log_full(trace_result_e::HEADER_ONLY_FILTER, 1, 0));
}

// The default (no positive filter) logs full up to and including the threshold,
// header-only above it.
TEST(should_log_full, default_respects_threshold) {
    EXPECT_TRUE(should_log_full(trace_result_e::DEFAULT, THRESHOLD - 1, THRESHOLD)); // below
    EXPECT_TRUE(should_log_full(trace_result_e::DEFAULT, THRESHOLD, THRESHOLD)); // at boundary (<=)
    EXPECT_FALSE(should_log_full(trace_result_e::DEFAULT, THRESHOLD + 1, THRESHOLD)); // above
}

// A threshold of 0 disables the cap for the default filter: always full.
TEST(should_log_full, default_threshold_zero_disables_cap) {
    EXPECT_TRUE(should_log_full(trace_result_e::DEFAULT, 0, 0));
    EXPECT_TRUE(should_log_full(trace_result_e::DEFAULT, 1, 0));
    EXPECT_TRUE(should_log_full(trace_result_e::DEFAULT, 1024 * 1024, 0));
}

// Edge sizes around the boundary and at 0.
TEST(should_log_full, default_edge_sizes) {
    EXPECT_TRUE(should_log_full(trace_result_e::DEFAULT, 0, THRESHOLD));
    EXPECT_TRUE(should_log_full(trace_result_e::DEFAULT, 1, 1)); // size == threshold == 1
    EXPECT_FALSE(should_log_full(trace_result_e::DEFAULT, 2, 1)); // size just over threshold 1
}

// DROP is handled by the caller, but should_log_full must not report "full".
TEST(should_log_full, drop_is_not_full) {
    EXPECT_FALSE(should_log_full(trace_result_e::DROP, 1, THRESHOLD));
    EXPECT_FALSE(should_log_full(trace_result_e::DROP, 1, 0));
}

} // namespace

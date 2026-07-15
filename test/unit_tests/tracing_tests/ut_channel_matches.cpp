// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <vsomeip/constants.hpp>

#include "../../../implementation/tracing/include/channel_impl.hpp"
#include "../../../implementation/tracing/include/enumeration_types.hpp"

namespace {

using vsomeip_v3::trace::channel_impl;
using vsomeip_v3::trace::filter_type_e;
using vsomeip_v3::trace::match_t;
using vsomeip_v3::trace::trace_result_e;

constexpr vsomeip_v3::service_t SERVICE = 0x1234;
constexpr vsomeip_v3::instance_t INSTANCE = 0x5678;
constexpr vsomeip_v3::method_t METHOD = 0x0abc;

match_t any_match() {
    return match_t{vsomeip_v3::ANY_SERVICE, vsomeip_v3::ANY_INSTANCE, vsomeip_v3::ANY_METHOD};
}

// A channel without any filter forwards everything as DEFAULT, i.e. full
// payload but subject to the size threshold applied by the connector.
TEST(channel_matches, no_filter_is_default) {
    channel_impl its_channel("TC", "Test Channel");
    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::DEFAULT);
}

// An explicit positive filter yields POSITIVE_FILTER for matching messages, and
// drops non-matching ones (a positive filter exists).
TEST(channel_matches, positive_filter) {
    channel_impl its_channel("TC", "Test Channel");
    its_channel.add_filter(match_t{SERVICE, INSTANCE, METHOD}, filter_type_e::POSITIVE);

    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::POSITIVE_FILTER);
    EXPECT_EQ(its_channel.matches(0x9999, INSTANCE, METHOD), trace_result_e::DROP);
}

// A header-only filter yields HEADER_ONLY_FILTER for matching messages. It is
// not a "positive" filter, so non-matching messages still fall through to the
// forward-everything default.
TEST(channel_matches, header_only_filter) {
    channel_impl its_channel("TC", "Test Channel");
    its_channel.add_filter(match_t{SERVICE, INSTANCE, METHOD}, filter_type_e::HEADER_ONLY);

    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::HEADER_ONLY_FILTER);
    EXPECT_EQ(its_channel.matches(0x9999, INSTANCE, METHOD), trace_result_e::DEFAULT);
}

// A negative filter drops matching messages; non-matching ones fall through to
// the forward-everything default when no positive filter is present.
TEST(channel_matches, negative_filter) {
    channel_impl its_channel("TC", "Test Channel");
    its_channel.add_filter(match_t{SERVICE, INSTANCE, METHOD}, filter_type_e::NEGATIVE);

    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::DROP);
    EXPECT_EQ(its_channel.matches(0x9999, INSTANCE, METHOD), trace_result_e::DEFAULT);
}

// A negative filter overrules a positive one: a message matching both is
// dropped.
TEST(channel_matches, negative_overrules_positive) {
    channel_impl its_channel("TC", "Test Channel");
    its_channel.add_filter(any_match(), filter_type_e::POSITIVE);
    its_channel.add_filter(match_t{SERVICE, INSTANCE, METHOD}, filter_type_e::NEGATIVE);

    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::DROP);
    // Other messages still match the wildcard positive filter.
    EXPECT_EQ(its_channel.matches(0x9999, INSTANCE, METHOD), trace_result_e::POSITIVE_FILTER);
}

// A full-payload filter forces full logging (POSITIVE_FILTER) for matching
// messages but, unlike a positive filter, does NOT restrict the channel to an
// allow-list: non-matching messages still fall through to the forward-everything
// default. This is the mirror image of a header-only filter.
TEST(channel_matches, full_payload_filter) {
    channel_impl its_channel("TC", "Test Channel");
    its_channel.add_filter(match_t{SERVICE, INSTANCE, METHOD}, filter_type_e::FULL_PAYLOAD);

    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::POSITIVE_FILTER);
    EXPECT_EQ(its_channel.matches(0x9999, INSTANCE, METHOD), trace_result_e::DEFAULT);
}

// A wildcard full-payload filter leaves unrelated messages on the default path:
// it never turns the channel into an allow-list, so nothing is dropped.
TEST(channel_matches, full_payload_coexists_with_default) {
    channel_impl its_channel("TC", "Test Channel");
    its_channel.add_filter(match_t{SERVICE, INSTANCE, METHOD}, filter_type_e::FULL_PAYLOAD);
    its_channel.add_filter(match_t{0x1111, 0x2222, 0x3333}, filter_type_e::FULL_PAYLOAD);

    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::POSITIVE_FILTER);
    EXPECT_EQ(its_channel.matches(0x1111, 0x2222, 0x3333), trace_result_e::POSITIVE_FILTER);
    // A message matching no filter is not dropped.
    EXPECT_EQ(its_channel.matches(0x9999, INSTANCE, METHOD), trace_result_e::DEFAULT);
}

// A negative filter overrules a full-payload one: a message matching both is
// dropped, while other messages still get forced to full.
TEST(channel_matches, negative_overrules_full_payload) {
    channel_impl its_channel("TC", "Test Channel");
    its_channel.add_filter(any_match(), filter_type_e::FULL_PAYLOAD);
    its_channel.add_filter(match_t{SERVICE, INSTANCE, METHOD}, filter_type_e::NEGATIVE);

    EXPECT_EQ(its_channel.matches(SERVICE, INSTANCE, METHOD), trace_result_e::DROP);
    EXPECT_EQ(its_channel.matches(0x9999, INSTANCE, METHOD), trace_result_e::POSITIVE_FILTER);
}

} // namespace

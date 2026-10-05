// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ============================================================================
// SOME/IP-TP reassembly (tp_message) is a pure parsing/data-structure problem:
// no threading, no I/O. These tests therefore drive tp_message directly with
// hand crafted datagrams. The datagrams live in exactly sized heap buffers, so
// any read past the received bytes is a real heap over-read that ASan/valgrind
// report - and, where the over-read length is attacker scalable, a plain build
// crashes as well.
// ============================================================================

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include <vsomeip/defines.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>

#include "../../../implementation/endpoints/include/tp.hpp"
#include "../../../implementation/endpoints/include/tp_message.hpp"

namespace {

using vsomeip_v3::byte_t;
using vsomeip_v3::tp::tp_message;

constexpr uint32_t max_message_size = 16 * 1024 * 1024;

void write_uint32_be(std::vector<byte_t>& _buffer, size_t _pos, uint32_t _value) {
    _buffer[_pos] = static_cast<byte_t>(_value >> 24);
    _buffer[_pos + 1] = static_cast<byte_t>(_value >> 16);
    _buffer[_pos + 2] = static_cast<byte_t>(_value >> 8);
    _buffer[_pos + 3] = static_cast<byte_t>(_value);
}

// Builds a well formed SOME/IP-TP datagram. The returned buffer is exactly as long as the
// datagram, so reading past it is detectable.
std::vector<byte_t> make_segment(uint32_t _offset, bool _more_segments, uint32_t _payload_size, byte_t _fill) {
    std::vector<byte_t> its_datagram(VSOMEIP_TP_PAYLOAD_POS + _payload_size, _fill);

    write_uint32_be(its_datagram, 0, 0x11112222); // service / method
    write_uint32_be(its_datagram, VSOMEIP_LENGTH_POS_MIN, static_cast<uint32_t>(its_datagram.size()) - VSOMEIP_SOMEIP_HEADER_SIZE);
    write_uint32_be(its_datagram, VSOMEIP_CLIENT_POS_MIN, 0x33330001); // client / session
    its_datagram[VSOMEIP_PROTOCOL_VERSION_POS] = 0x01;
    its_datagram[VSOMEIP_INTERFACE_VERSION_POS] = 0x01;
    its_datagram[VSOMEIP_MESSAGE_TYPE_POS] = vsomeip_v3::tp::tp::tp_flag_set(vsomeip_v3::message_type_e::MT_REQUEST);
    its_datagram[VSOMEIP_RETURN_CODE_POS] = 0x00;
    // 28 bit offset + 3 bit reserved + 1 bit "more segments"
    write_uint32_be(its_datagram, VSOMEIP_TP_HEADER_POS_MIN, _offset | (_more_segments ? 0x1u : 0x0u));

    return its_datagram;
}

// -----------------------------------------------------------------------------------------------
// An out-of-order segment which partially overlaps its predecessor but has a gap
// before its successor must only copy the bytes it actually carries. Bounding the copy by the
// *next* segment's start reads far past the received datagram.
// -----------------------------------------------------------------------------------------------
TEST(tp_message_test, overlapping_segment_with_gap_does_not_over_read) {

    // segment A -> [0, 63]
    const auto its_segment_a = make_segment(0, true, 64, 0xA1);
    tp_message its_message(its_segment_a.data(), static_cast<uint32_t>(its_segment_a.size()), max_message_size);

    // segment C -> [8388608, 8388623], far behind A so that a gap remains in between
    constexpr uint32_t far_offset = 8 * 1024 * 1024;
    const auto its_segment_c = make_segment(far_offset, true, 16, 0xC3);
    EXPECT_FALSE(its_message.add_segment(its_segment_c.data(), static_cast<uint32_t>(its_segment_c.size())));

    // segment B -> [48, 79]: overlaps A on [48, 63] and ends long before C starts.
    // Without the fix the copy length is C.start - 64 == 8388544 bytes, read from a 52 byte
    // datagram: an 8 MiB heap over-read that reliably faults even without sanitizers.
    const auto its_segment_b = make_segment(48, true, 32, 0xB2);
    EXPECT_FALSE(its_message.add_segment(its_segment_b.data(), static_cast<uint32_t>(its_segment_b.size())));
}

// The reassembled payload must follow the "first received bytes win" rule for overlaps.
TEST(tp_message_test, overlapping_segment_with_gap_reassembles_correctly) {

    // A -> [0, 63], C -> [96, 111] (last), B -> [48, 79] (kept as [64, 79]), D -> [80, 95]
    const auto its_segment_a = make_segment(0, true, 64, 0xA1);
    const auto its_segment_c = make_segment(96, false, 16, 0xC3);
    const auto its_segment_b = make_segment(48, true, 32, 0xB2);
    const auto its_segment_d = make_segment(80, true, 16, 0xD4);

    tp_message its_message(its_segment_a.data(), static_cast<uint32_t>(its_segment_a.size()), max_message_size);
    EXPECT_FALSE(its_message.add_segment(its_segment_c.data(), static_cast<uint32_t>(its_segment_c.size())));
    EXPECT_FALSE(its_message.add_segment(its_segment_b.data(), static_cast<uint32_t>(its_segment_b.size())));
    ASSERT_TRUE(its_message.add_segment(its_segment_d.data(), static_cast<uint32_t>(its_segment_d.size())));

    std::vector<byte_t> its_expected_payload;
    its_expected_payload.insert(its_expected_payload.end(), 64, 0xA1); // [0, 63]    from A
    its_expected_payload.insert(its_expected_payload.end(), 16, 0xB2); // [64, 79]   from B
    its_expected_payload.insert(its_expected_payload.end(), 16, 0xD4); // [80, 95]   from D
    its_expected_payload.insert(its_expected_payload.end(), 16, 0xC3); // [96, 111]  from C

    const auto its_reassembled = its_message.get_message();
    ASSERT_EQ(its_reassembled.size(), VSOMEIP_FULL_HEADER_SIZE + its_expected_payload.size());
    EXPECT_TRUE(std::equal(its_expected_payload.begin(), its_expected_payload.end(), its_reassembled.begin() + VSOMEIP_FULL_HEADER_SIZE));
}

// -----------------------------------------------------------------------------------------------
// A zero length segment creates the inverted range (offset, offset - 1), which breaks the
// "segments never overlap and start_ <= end_" invariant that every bounds computation in
// add_segment() relies on. It must be rejected by check_lengths().
// -----------------------------------------------------------------------------------------------
TEST(tp_message_test, zero_length_segment_is_rejected) {

    // first segment -> [1024, 1039]
    const auto its_first = make_segment(1024, true, 16, 0xA1);
    tp_message its_message(its_first.data(), static_cast<uint32_t>(its_first.size()), max_message_size);

    // 20 byte datagram: segment size 0 with offset 0 -> segment_t(0, 0xFFFFFFFF).
    // Without the fix this sorts before [1024, 1039] and memcpy's 1024 bytes out of a 20 byte
    // datagram straight into the message that is later handed to the application.
    const auto its_zero_length = make_segment(0, true, 0, 0x00);
    ASSERT_EQ(its_zero_length.size(), static_cast<size_t>(VSOMEIP_TP_PAYLOAD_POS));
    EXPECT_FALSE(its_message.add_segment(its_zero_length.data(), static_cast<uint32_t>(its_zero_length.size())));
}

TEST(tp_message_test, zero_length_last_segment_is_rejected) {

    // first segment -> [0, 63]
    const auto its_first = make_segment(0, true, 64, 0xA1);
    tp_message its_message(its_first.data(), static_cast<uint32_t>(its_first.size()), max_message_size);

    // Without the fix segment_t(0, 0xFFFFFFFF) sorts *after* [0, 63] and produces a
    // std::vector::insert with first > last.
    const auto its_zero_length = make_segment(0, false, 0, 0x00);
    EXPECT_FALSE(its_message.add_segment(its_zero_length.data(), static_cast<uint32_t>(its_zero_length.size())));
}

// -----------------------------------------------------------------------------------------------
// If the very first datagram is too short to hold a SOME/IP-TP header, the tp_message holds no
// header at all. Completing such a message wrote the length and return code fields out of bounds.
// -----------------------------------------------------------------------------------------------
TEST(tp_message_test, segment_after_headerless_first_datagram_is_rejected) {

    // 16 bytes: enough for the SOME/IP header, too short for the TP header
    const std::vector<byte_t> its_truncated(VSOMEIP_FULL_HEADER_SIZE, 0x00);
    tp_message its_message(its_truncated.data(), static_cast<uint32_t>(its_truncated.size()), max_message_size);

    // A single, self contained last segment would otherwise "complete" the header-less message.
    const auto its_last = make_segment(0, false, 16, 0xB2);
    EXPECT_FALSE(its_message.add_segment(its_last.data(), static_cast<uint32_t>(its_last.size())));
}

// -----------------------------------------------------------------------------------------------
// A segment that lies strictly inside its predecessor carries no new data. Accounting it as
// "current_end - seg_prev->end_" underflows current_message_size_ to ~4 GiB, after which
// check_lengths() rejects every further segment and the message can never be completed.
// -----------------------------------------------------------------------------------------------
TEST(tp_message_test, strictly_overlapped_segment_does_not_wedge_reassembly) {

    // A -> [0, 63], C -> [96, 111] (last), B -> [16, 47] (strictly inside A), D -> [64, 95]
    const auto its_segment_a = make_segment(0, true, 64, 0xA1);
    const auto its_segment_c = make_segment(96, false, 16, 0xC3);
    const auto its_segment_b = make_segment(16, true, 32, 0xB2);
    const auto its_segment_d = make_segment(64, true, 32, 0xD4);

    tp_message its_message(its_segment_a.data(), static_cast<uint32_t>(its_segment_a.size()), max_message_size);
    EXPECT_FALSE(its_message.add_segment(its_segment_c.data(), static_cast<uint32_t>(its_segment_c.size())));
    EXPECT_FALSE(its_message.add_segment(its_segment_b.data(), static_cast<uint32_t>(its_segment_b.size())));
    // Without the fix this returns false forever: the message is wedged until the 5 s cleanup.
    ASSERT_TRUE(its_message.add_segment(its_segment_d.data(), static_cast<uint32_t>(its_segment_d.size())));

    std::vector<byte_t> its_expected_payload;
    its_expected_payload.insert(its_expected_payload.end(), 64, 0xA1); // [0, 63]    from A
    its_expected_payload.insert(its_expected_payload.end(), 32, 0xD4); // [64, 95]   from D
    its_expected_payload.insert(its_expected_payload.end(), 16, 0xC3); // [96, 111]  from C

    const auto its_reassembled = its_message.get_message();
    ASSERT_EQ(its_reassembled.size(), VSOMEIP_FULL_HEADER_SIZE + its_expected_payload.size());
    EXPECT_TRUE(std::equal(its_expected_payload.begin(), its_expected_payload.end(), its_reassembled.begin() + VSOMEIP_FULL_HEADER_SIZE));
}

} // namespace

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include <vsomeip/primitive_types.hpp>

#include "../../../implementation/utility/include/utility.hpp"

namespace {

// Streams a single utility::Hex value (as produced by hex2/hex4/hex8) and returns the result.
std::string to_hex_string(vsomeip_v3::utility::Hex _value) {
    std::ostringstream os;
    os << _value;
    return os.str();
}

} // namespace

TEST(hex_test, hex2_pads_and_lowercases) {
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex2(0x00)), "00");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex2(0x0a)), "0a");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex2(0xab)), "ab");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex2(0xff)), "ff");
}

TEST(hex_test, hex4_pads_all_four_digits) {
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex4(0x0000)), "0000");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex4(0x00ab)), "00ab");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex4(0x1234)), "1234");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex4(0xffff)), "ffff");
}

TEST(hex_test, hex8_pads_all_eight_digits) {
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex8(0x00000000u)), "00000000");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex8(0x000000abu)), "000000ab");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex8(0xdeadbeefu)), "deadbeef");
    EXPECT_EQ(to_hex_string(vsomeip_v3::hex8(0xffffffffu)), "ffffffff");
}

TEST(hex_test, hex_chars_returns_two_digit_pair) {
    static_assert(vsomeip_v3::hex_chars(0x00).size() == 2, "hex_chars must yield exactly two chars");
    EXPECT_EQ(std::string(vsomeip_v3::hex_chars(0x00).data(), 2), "00");
    EXPECT_EQ(std::string(vsomeip_v3::hex_chars(0x0a).data(), 2), "0a");
    EXPECT_EQ(std::string(vsomeip_v3::hex_chars(0xff).data(), 2), "ff");
}

TEST(hex_test, append_hex_byte_appends_two_digit_pair) {
    std::string out;
    vsomeip_v3::append_hex_byte(out, 0x00);
    vsomeip_v3::append_hex_byte(out, 0x0a);
    vsomeip_v3::append_hex_byte(out, 0xff);
    EXPECT_EQ(out, "000aff");
}

TEST(hex_test, append_hex_appends_contiguous_hex) {
    const std::vector<vsomeip_v3::byte_t> data{0xde, 0xad, 0xbe, 0xef};
    std::string out;
    vsomeip_v3::append_hex(out, data);
    EXPECT_EQ(out, "deadbeef");
}

TEST(hex_test, append_hex_appends_to_existing_content) {
    const std::vector<vsomeip_v3::byte_t> data{0x01, 0x02};
    std::string out = "prefix:";
    vsomeip_v3::append_hex(out, data);
    EXPECT_EQ(out, "prefix:0102");
}

TEST(hex_test, append_hex_handles_empty_input) {
    std::string out = "unchanged";
    vsomeip_v3::append_hex(out, {});
    EXPECT_EQ(out, "unchanged");
}

TEST(dump_test, dump_empty_data_returns_empty_string) {
    EXPECT_EQ(vsomeip_v3::utility::dump(nullptr, 0), "");
}

TEST(dump_test, dump_formats_all_bytes_when_under_the_limit) {
    const std::vector<vsomeip_v3::byte_t> data{0xde, 0xad, 0xbe, 0xef};
    EXPECT_EQ(vsomeip_v3::utility::dump(data.data(), data.size()), "deadbeef");
}

// Exercises the exact max_dump_length (512 bytes) boundary in utility::dump(): at the
// limit, no truncation should be applied yet.
TEST(dump_test, dump_does_not_truncate_at_exactly_the_limit) {
    const std::vector<vsomeip_v3::byte_t> data(512, 0xaa);
    const std::string result = vsomeip_v3::utility::dump(data.data(), data.size());
    EXPECT_EQ(result, std::string(1024, 'a'));
}

TEST(dump_test, dump_truncates_and_reports_total_length_beyond_the_limit) {
    const std::vector<vsomeip_v3::byte_t> data(513, 0xaa);
    const std::string result = vsomeip_v3::utility::dump(data.data(), data.size());
    EXPECT_EQ(result, std::string(1024, 'a') + " (truncated, total length: 513)");
}

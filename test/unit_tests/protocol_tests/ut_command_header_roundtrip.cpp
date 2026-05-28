// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ============================================================================
// Round-trip tests for the struct-based command_header serialize/deserialize.
// ============================================================================

#include <gtest/gtest.h>

#include "../../../implementation/protocol/include/command_types.hpp"
#include "../../../implementation/protocol/include/deserialize.hpp"
#include "../../../implementation/protocol/include/serialize.hpp"

namespace vsomeip_v3::protocol {

TEST(ut_command_header_roundtrip, ping_roundtrip) {
    auto cmd = create_ping_cmd(0x1234);

    std::vector<uint8_t> buf(wire_size(cmd));
    serialize(cmd, buf.data());

    command_header out{};
    ASSERT_TRUE(deserialize(buf.data(), static_cast<uint32_t>(buf.size()), out));
    EXPECT_EQ(out, cmd);
}

TEST(ut_command_header_roundtrip, pong_roundtrip) {
    auto cmd = create_pong_cmd(0xABCD);

    std::vector<uint8_t> buf(wire_size(cmd));
    serialize(cmd, buf.data());

    command_header out{};
    ASSERT_TRUE(deserialize(buf.data(), static_cast<uint32_t>(buf.size()), out));
    EXPECT_EQ(out, cmd);
}

TEST(ut_command_header_roundtrip, suspend_roundtrip) {
    auto cmd = create_suspend_cmd(0x00FF);

    std::vector<uint8_t> buf(wire_size(cmd));
    serialize(cmd, buf.data());

    command_header out{};
    ASSERT_TRUE(deserialize(buf.data(), static_cast<uint32_t>(buf.size()), out));
    EXPECT_EQ(out, cmd);
}

TEST(ut_command_header_roundtrip, deserialize_rejects_truncated_input) {
    auto cmd = create_ping_cmd(0x04CF);

    std::vector<uint8_t> buf(wire_size(cmd));
    serialize(cmd, buf.data());

    command_header out{};
    EXPECT_FALSE(deserialize(buf.data(), static_cast<uint32_t>(buf.size()) - 1, out));
}

} // namespace vsomeip_v3::protocol

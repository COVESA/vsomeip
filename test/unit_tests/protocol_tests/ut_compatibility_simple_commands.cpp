// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ============================================================================
// Simple Command Compatibility Tests (header-only commands)
// ============================================================================
//
// Proves that the new struct-based serialization/deserialization produces
// byte-for-byte identical wire output to the old class-based approach and
// that each side can correctly consume what the other produces.
//
// For each command we verify:
//   1. old serialize → new deserialize (new code reads old format correctly)
//   2. new serialize → old deserialize (old code reads new format correctly)
// ============================================================================

#include <gtest/gtest.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <vector>

#include "../../../implementation/protocol/include/command_types.hpp"
#include "../../../implementation/protocol/include/deserialize.hpp"
#include "../../../implementation/protocol/include/protocol.hpp"
#include "../../../implementation/protocol/include/serialize.hpp"

#include "../../../implementation/protocol/include/ping_command.hpp"
#include "../../../implementation/protocol/include/pong_command.hpp"
#include "../../../implementation/protocol/include/suspend_command.hpp"

namespace vsomeip_v3::protocol {

template<typename T>
static std::vector<uint8_t> to_wire(T const& _cmd) {
    std::vector<uint8_t> buf(wire_size(_cmd));
    serialize(_cmd, buf.data());
    return buf;
}

template<typename OldCmd>
static std::vector<uint8_t> old_to_wire(OldCmd const& _cmd) {
    std::vector<uint8_t> buf;
    _cmd.serialize(buf);
    return buf;
}

// ============================================================================
// Ping
// ============================================================================

TEST(ut_compatibility_simple_commands, ping_old_serialize_new_deserialize) {
    ping_command old_cmd;
    old_cmd.set_client(0x04CF);
    auto bytes = old_to_wire(old_cmd);

    command_header result{};
    ASSERT_TRUE(deserialize(bytes.data(), static_cast<uint32_t>(bytes.size()), result));
    EXPECT_EQ(result.id_, old_cmd.get_id());
    EXPECT_EQ(result.client_, old_cmd.get_client());
    EXPECT_EQ(result.length_, old_cmd.get_size());
}

TEST(ut_compatibility_simple_commands, ping_new_serialize_old_deserialize) {
    auto bytes = to_wire(create_ping_cmd(0x04CF));

    ping_command old_cmd;
    error_e err;
    std::vector<uint8_t> v(bytes.begin(), bytes.end());
    old_cmd.deserialize(v, err);

    ASSERT_EQ(err, error_e::ERROR_OK);
    EXPECT_EQ(old_cmd.get_id(), id_e::PING_ID);
    EXPECT_EQ(old_cmd.get_client(), 0x04CF);
}

// ============================================================================
// Pong
// ============================================================================

TEST(ut_compatibility_simple_commands, pong_old_serialize_new_deserialize) {
    pong_command old_cmd;
    old_cmd.set_client(0x00FF);
    auto bytes = old_to_wire(old_cmd);

    command_header result{};
    ASSERT_TRUE(deserialize(bytes.data(), static_cast<uint32_t>(bytes.size()), result));
    EXPECT_EQ(result.id_, old_cmd.get_id());
    EXPECT_EQ(result.client_, old_cmd.get_client());
    EXPECT_EQ(result.length_, old_cmd.get_size());
}

TEST(ut_compatibility_simple_commands, pong_new_serialize_old_deserialize) {
    auto bytes = to_wire(create_pong_cmd(0x00FF));

    pong_command old_cmd;
    error_e err;
    std::vector<uint8_t> v(bytes.begin(), bytes.end());
    old_cmd.deserialize(v, err);

    ASSERT_EQ(err, error_e::ERROR_OK);
    EXPECT_EQ(old_cmd.get_id(), id_e::PONG_ID);
    EXPECT_EQ(old_cmd.get_client(), 0x00FF);
}

// ============================================================================
// Suspend
// ============================================================================

TEST(ut_compatibility_simple_commands, suspend_old_serialize_new_deserialize) {
    suspend_command old_cmd;
    old_cmd.set_client(0x04CF);
    auto bytes = old_to_wire(old_cmd);

    command_header result{};
    ASSERT_TRUE(deserialize(bytes.data(), static_cast<uint32_t>(bytes.size()), result));
    EXPECT_EQ(result.id_, old_cmd.get_id());
    EXPECT_EQ(result.client_, old_cmd.get_client());
    EXPECT_EQ(result.length_, old_cmd.get_size());
}

TEST(ut_compatibility_simple_commands, suspend_new_serialize_old_deserialize) {
    auto bytes = to_wire(create_suspend_cmd(0x04CF));

    suspend_command old_cmd;
    error_e err;
    std::vector<uint8_t> v(bytes.begin(), bytes.end());
    old_cmd.deserialize(v, err);

    ASSERT_EQ(err, error_e::ERROR_OK);
    EXPECT_EQ(old_cmd.get_id(), id_e::SUSPEND_ID);
    EXPECT_EQ(old_cmd.get_client(), 0x04CF);
}

} // namespace vsomeip_v3::protocol

#pragma GCC diagnostic pop

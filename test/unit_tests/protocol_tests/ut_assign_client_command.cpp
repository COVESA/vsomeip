// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include "../../../implementation/protocol/include/assign_client_command.hpp"
#include "../../../implementation/protocol/include/protocol.hpp"

#include <boost/asio/ip/address_v4.hpp>

namespace assign_client_command_tests {

// Tester Note: Expect Little-Endian representation for serialized data.
// Command: client=0x0042, name="my_app", address=127.0.0.2, port=31492
const std::vector<std::uint8_t> serialized_assign_client_command = {
        0x00, // ASSIGN_CLIENT_ID
        static_cast<std::uint8_t>(vsomeip_v3::protocol::IPC_VERSION & 0xFF), // Version (low byte)
        static_cast<std::uint8_t>((vsomeip_v3::protocol::IPC_VERSION >> 8) & 0xFF), // Version (high byte)
        0x42, 0x00, // Client.
        0x11, 0x00, 0x00, 0x00, // Size (payload = 17 bytes).
        // Payload.
        0x06, 0x00, 0x00, 0x00, // name_length = 6.
        0x6d, 0x79, 0x5f, 0x61, 0x70, 0x70, // "my_app"
        0x01, // has_address = true.
        0x7f, 0x00, 0x00, 0x02, // 127.0.0.2
        0x04, 0x7b, // port = 31492 (0x7B04 little-endian)
};

TEST(assign_client_command_test, accessors) {
    vsomeip_v3::protocol::assign_client_command command;
    ASSERT_TRUE(command.get_name().empty());
    command.set_name("my_app");
    ASSERT_EQ(command.get_name(), "my_app");
    command.set_address(boost::asio::ip::make_address_v4("127.0.0.2"));
    ASSERT_EQ(command.get_address(), boost::asio::ip::make_address_v4("127.0.0.2"));
    command.set_port(31492);
    ASSERT_EQ(command.get_port(), 31492);
}

TEST(assign_client_command_test, serialize) {
    vsomeip_v3::protocol::assign_client_command command;
    command.set_client(0x0042);
    command.set_name("my_app");
    command.set_address(boost::asio::ip::make_address_v4("127.0.0.2"));
    command.set_port(31492);

    std::vector<std::uint8_t> buffer;
    command.serialize(buffer);
    ASSERT_EQ(buffer, serialized_assign_client_command);
}

TEST(assign_client_command_test, deserialize) {
    vsomeip_v3::protocol::assign_client_command command;
    vsomeip_v3::protocol::error_e error;
    command.deserialize(serialized_assign_client_command, error);
    ASSERT_EQ(error, vsomeip_v3::protocol::error_e::ERROR_OK);
    EXPECT_EQ(command.get_client(), 0x0042);
    EXPECT_EQ(command.get_name(), "my_app");
    EXPECT_EQ(command.get_address(), boost::asio::ip::make_address_v4("127.0.0.2"));
    EXPECT_EQ(command.get_port(), 31492);
}

} // namespace assign_client_command_tests

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include "../../../implementation/utility/include/utility.hpp"

namespace {

using vsomeip_v3::client_t;
using vsomeip_v3::diagnosis_t;
using vsomeip_v3::message_type_e;
using vsomeip_v3::utility;

constexpr diagnosis_t HOST_A_DIAGNOSIS = 0x98;
constexpr diagnosis_t HOST_A_MASK = 0xEB00; // clears bits 2 and 4 of the prefix -> 1024 ids
constexpr diagnosis_t HOST_B_DIAGNOSIS = 0xA7;
constexpr diagnosis_t HOST_B_MASK = 0xF300;
constexpr diagnosis_t DEFAULT_MASK = 0xFF00; // 255 ids, a single prefix

bool valid(client_t _client, diagnosis_t _diagnosis, diagnosis_t _mask, message_type_e _type) {
    return utility::is_valid_client_id(_client, _diagnosis, _mask, _type);
}

// A mask that clears bits of the diagnosis prefix is valid: it enlarges the block, which then
// starts at the masked diagnosis address and spans several prefixes.
TEST(client_id_range_test, base_and_range) {
    EXPECT_EQ(utility::get_client_id_base(HOST_B_DIAGNOSIS, DEFAULT_MASK), 0xA700);
    EXPECT_EQ(utility::get_client_id_base(HOST_A_DIAGNOSIS, HOST_A_MASK), 0x8800);
    EXPECT_EQ(utility::get_client_id_base(HOST_B_DIAGNOSIS, HOST_B_MASK), 0xA300);

    // Host A owns 0x88xx, 0x8Cxx, 0x98xx and 0x9Cxx.
    for (const client_t its_client : {client_t{0x8800}, client_t{0x8819}, client_t{0x8CFF}, client_t{0x9800}, client_t{0x9C42}}) {
        EXPECT_TRUE(utility::is_client_id_in_range(its_client, HOST_A_DIAGNOSIS, HOST_A_MASK)) << "client=" << std::hex << its_client;
    }
    for (const client_t its_client : {client_t{0x6512}, client_t{0x8912}, client_t{0x9912}, client_t{0xA800}}) {
        EXPECT_FALSE(utility::is_client_id_in_range(its_client, HOST_A_DIAGNOSIS, HOST_A_MASK)) << "client=" << std::hex << its_client;
    }
}

// Every id of the block is one of this host's own ids, whichever prefix it carries.
TEST(is_valid_client_id_test, host_a_own_ids) {
    for (const client_t its_client : {client_t{0x8819}, client_t{0x8C19}, client_t{0x9819}, client_t{0x9C19}}) {
        EXPECT_TRUE(valid(its_client, HOST_A_DIAGNOSIS, HOST_A_MASK, message_type_e::MT_RESPONSE)) << "client=" << std::hex << its_client;
        EXPECT_FALSE(valid(its_client, HOST_A_DIAGNOSIS, HOST_A_MASK, message_type_e::MT_REQUEST)) << "client=" << std::hex << its_client;
    }
}

// Another ECU's ids are the expected source of a request and never of a response.
TEST(is_valid_client_id_test, host_a_foreign_ids) {
    for (const client_t its_client : {client_t{0x6512}, client_t{0x8912}, client_t{0xA312}}) {
        EXPECT_TRUE(valid(its_client, HOST_A_DIAGNOSIS, HOST_A_MASK, message_type_e::MT_REQUEST)) << "client=" << std::hex << its_client;
        EXPECT_FALSE(valid(its_client, HOST_A_DIAGNOSIS, HOST_A_MASK, message_type_e::MT_RESPONSE)) << "client=" << std::hex << its_client;
    }
}

TEST(is_valid_client_id_test, host_b_own_ids) {
    for (const client_t its_client : {client_t{0xA312}, client_t{0xA712}, client_t{0xAB12}, client_t{0xAF12}}) {
        EXPECT_TRUE(valid(its_client, HOST_B_DIAGNOSIS, HOST_B_MASK, message_type_e::MT_RESPONSE)) << "client=" << std::hex << its_client;
        EXPECT_FALSE(valid(its_client, HOST_B_DIAGNOSIS, HOST_B_MASK, message_type_e::MT_REQUEST)) << "client=" << std::hex << its_client;
    }
}

TEST(is_valid_client_id_test, single_prefix_mask) {
    EXPECT_TRUE(valid(0xA700, HOST_B_DIAGNOSIS, DEFAULT_MASK, message_type_e::MT_RESPONSE));
    EXPECT_TRUE(valid(0xA7FF, HOST_B_DIAGNOSIS, DEFAULT_MASK, message_type_e::MT_RESPONSE));
    EXPECT_FALSE(valid(0xA312, HOST_B_DIAGNOSIS, DEFAULT_MASK, message_type_e::MT_RESPONSE));
    EXPECT_FALSE(valid(0xA712, HOST_B_DIAGNOSIS, DEFAULT_MASK, message_type_e::MT_REQUEST));
    EXPECT_TRUE(valid(0xA312, HOST_B_DIAGNOSIS, DEFAULT_MASK, message_type_e::MT_REQUEST));
}

// Only requests and responses are checked; MT_ERROR escapes although it is a response.
TEST(is_valid_client_id_test, other_message_types) {
    for (const auto its_type : {message_type_e::MT_NOTIFICATION, message_type_e::MT_ERROR, message_type_e::MT_RESPONSE_ACK,
                                message_type_e::MT_REQUEST_NO_RETURN}) {
        EXPECT_TRUE(valid(0x6512, HOST_A_DIAGNOSIS, HOST_A_MASK, its_type));
        EXPECT_TRUE(valid(0x8819, HOST_A_DIAGNOSIS, HOST_A_MASK, its_type));
    }
}

} // namespace

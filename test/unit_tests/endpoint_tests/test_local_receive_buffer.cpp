// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include "../../../implementation/endpoints/include/local_receive_buffer.hpp"
#include "../../../implementation/protocol/include/protocol.hpp"

namespace vsomeip_v3::testing {

struct test_receive_buffer : ::testing::Test {

    [[nodiscard]] bool next_message() { return buf_.next_message(result_); }

    std::vector<uint8_t> craft_message_of_size(uint32_t _size) {
        std::vector<uint8_t> mes;
        if (_size < protocol::COMMAND_POSITION_SIZE + sizeof(uint32_t)) {
            throw std::logic_error("unreasonable small message requested");
        }
        mes.reserve(_size);
        mes.resize(_size, 0);
        uint32_t length = static_cast<uint32_t>(_size - protocol::COMMAND_HEADER_SIZE);
        memcpy(&mes[protocol::COMMAND_POSITION_SIZE], &length, sizeof(length));
        return mes;
    }
    std::vector<uint8_t> craft_message_that_leaves_room_for_another_header() {
        auto asio_buf = buf_.buffer();
        auto const header = protocol::COMMAND_POSITION_SIZE + sizeof(uint32_t);
        if (asio_buf.size() < header) {
            throw std::logic_error("unreasonable buffer size returned");
        }
        return craft_message_of_size(static_cast<uint32_t>(asio_buf.size() - header));
    }

    std::vector<uint8_t> craft_message_that_is_bigger_then_buffer(uint32_t overflow) {
        auto asio_buf = buf_.buffer();
        auto const big_size = asio_buf.size() + overflow;
        return craft_message_of_size(static_cast<uint32_t>(big_size));
    }

    [[nodiscard]] bool add_message(local_receive_buffer& _buf, uint8_t const* _ptr, size_t _length, size_t& _missing_bytes) {
        auto asio_buf = _buf.buffer();
        auto l = std::min(_length, asio_buf.size());
        _missing_bytes = _length - l;
        memcpy(asio_buf.data(), _ptr, l);
        return _buf.bump_end(l);
    }

    [[nodiscard]] bool add_message(uint8_t const* _ptr, size_t _length, size_t& _missing_bytes) {
        return add_message(buf_, _ptr, _length, _missing_bytes);
    }

    next_message_result result_{};
    uint32_t const shrink_threshold_{512};
    local_receive_buffer buf_{std::numeric_limits<uint32_t>::max(), shrink_threshold_};
};

TEST_F(test_receive_buffer, given_no_bump_when_calling_next_message_then_false_and_no_error_and_no_buffer_size_change) {

    auto const before = buf_.buffer().size();
    EXPECT_FALSE(next_message());
    EXPECT_EQ(false, result_.error_);
    EXPECT_EQ(0, result_.message_size_);
    EXPECT_EQ(nullptr, result_.message_data_);
    EXPECT_EQ(before, buf_.buffer().size());
}

TEST_F(test_receive_buffer, given_bump_beyond_capacity_when_calling_bump_end_then_false_returned) {
    auto buf_size = buf_.buffer().size();

    // when/then
    EXPECT_FALSE(buf_.bump_end(buf_size + 1));
    EXPECT_TRUE(buf_.bump_end(buf_size)); // Exactly at capacity should work
}
TEST_F(test_receive_buffer, given_buffer_exactly_full_when_getting_buffer_then_zero_size_returned) {
    // given - fill buffer completely
    auto msg = craft_message_of_size(static_cast<uint32_t>(buf_.buffer().size()));
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(&msg[0], msg.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, 0);

    // when
    auto empty_buf = buf_.buffer();

    // then
    EXPECT_EQ(0, empty_buf.size());
}

TEST_F(test_receive_buffer, given_a_bump_smaller_then_the_header_when_calling_next_message_then_false_and_no_error_is_returned) {

    auto const before = buf_.buffer().size();
    auto small_bump = protocol::COMMAND_HEADER_SIZE - 1;
    ASSERT_TRUE(buf_.bump_end(small_bump));

    EXPECT_FALSE(next_message());
    EXPECT_EQ(false, result_.error_);
    EXPECT_EQ(0, result_.message_size_);
    EXPECT_EQ(nullptr, result_.message_data_);
    EXPECT_EQ(before, buf_.buffer().size() + small_bump);
}

TEST_F(test_receive_buffer,
       given_a_partial_message_that_would_fit_into_buffer_when_calling_next_message_then_false_and_no_error_is_returned_and_the_buffer_size_remains) {

    auto const before = buf_.buffer().size();
    // given
    auto small_message = craft_message_that_leaves_room_for_another_header();

    // when
    size_t missing_bytes{0};
    auto const missing_message_bytes = 2;
    ASSERT_TRUE(add_message(&small_message[0], small_message.size() - missing_message_bytes, missing_bytes));
    ASSERT_EQ(missing_bytes, 0);

    // then
    EXPECT_FALSE(next_message());
    EXPECT_EQ(false, result_.error_);
    EXPECT_EQ(0, result_.message_size_);
    EXPECT_EQ(nullptr, result_.message_data_);
    EXPECT_EQ(buf_.buffer().size() + small_message.size() - missing_message_bytes, before);
}
TEST_F(test_receive_buffer, given_a_message_that_fits_into_buffer_when_calling_next_message_then_true_and_no_error_is_returned) {

    auto const before = buf_.buffer().size();
    // given
    auto small_message = craft_message_that_leaves_room_for_another_header();

    // when
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(&small_message[0], small_message.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, 0);

    // then
    EXPECT_TRUE(next_message());
    EXPECT_EQ(false, result_.error_);
    EXPECT_EQ(small_message.size(), result_.message_size_);
    EXPECT_NE(nullptr, result_.message_data_);
    buf_.shift_front();
    EXPECT_EQ(before, buf_.buffer().size());
}
TEST_F(test_receive_buffer, given_multiple_complete_messages_when_calling_next_message_then_all_are_parsed) {
    // given
    auto msg1 = craft_message_of_size(32);
    auto msg2 = craft_message_of_size(40);
    auto msg3 = craft_message_of_size(48);

    // when - add all messages at once
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(&msg1[0], msg1.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, 0);
    ASSERT_TRUE(add_message(&msg2[0], msg2.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, 0);
    ASSERT_TRUE(add_message(&msg3[0], msg3.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, 0);

    // then - parse all three in succession
    EXPECT_TRUE(next_message());
    EXPECT_EQ(msg1.size(), result_.message_size_);

    EXPECT_TRUE(next_message());
    EXPECT_EQ(msg2.size(), result_.message_size_);

    EXPECT_TRUE(next_message());
    EXPECT_EQ(msg3.size(), result_.message_size_);

    EXPECT_FALSE(next_message());
}
TEST_F(test_receive_buffer,
       given_two_message_and_the_second_fits_only_partially_when_calling_next_message_three_times_then_both_messages_are_parsed) {

    // given
    uint32_t additional_bytes = 50; // arbitrary amount
    auto small_message = craft_message_that_leaves_room_for_another_header();
    auto big_message = craft_message_that_is_bigger_then_buffer(additional_bytes);
    ASSERT_LT(buf_.buffer().size(), big_message.size()); // second message would not fit into buffer in isolation

    // when
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(&small_message[0], small_message.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, 0);
    ASSERT_TRUE(add_message(&big_message[0], big_message.size(), missing_bytes));
    // second message does not fit
    ASSERT_NE(missing_bytes, 0);

    // then
    // first message can be parsed on first trial
    ASSERT_TRUE(next_message());
    ASSERT_EQ(false, result_.error_);
    ASSERT_EQ(small_message.size(), result_.message_size_);
    ASSERT_NE(nullptr, result_.message_data_);

    // second message header only is contained, but the buffer resizes already to be big enough for the entire second message
    ASSERT_FALSE(next_message());
    ASSERT_EQ(false, result_.error_);
    // simulate being done with the iterations
    buf_.shift_front();

    // second message can be placed into the buffer now:
    size_t second_missing{0};
    ASSERT_TRUE(add_message(&big_message[big_message.size() - missing_bytes - 1], missing_bytes, second_missing));
    ASSERT_EQ(second_missing, 0);

    // and the message can be consumed
    EXPECT_TRUE(next_message());
    EXPECT_EQ(false, result_.error_);
}

TEST_F(test_receive_buffer, given_the_buffer_did_not_increase_when_subsequent_messages_are_small_then_the_buffer_shrinks) {
    // given...
    auto const before = static_cast<uint32_t>(buf_.buffer().size());
    // when...
    auto small_message = craft_message_of_size(before / 4);
    size_t missing_bytes{0};
    for (uint32_t i = 0; i < shrink_threshold_; ++i) {
        ASSERT_TRUE(add_message(&small_message[0], small_message.size(), missing_bytes));
        ASSERT_EQ(missing_bytes, 0);
        ASSERT_TRUE(next_message());
        ASSERT_EQ(false, result_.error_);
        ASSERT_FALSE(next_message());
        ASSERT_EQ(false, result_.error_);
        buf_.shift_front();
    }

    // then
    auto const after = buf_.buffer().size();
    EXPECT_EQ(after, before);
}

TEST_F(test_receive_buffer, given_the_buffer_increased_when_subsequent_messages_are_small_then_the_buffer_shrinks) {
    // given...
    auto const original_size = static_cast<uint32_t>(buf_.buffer().size());
    auto const additional_bytes = original_size * 3;
    auto big_message = craft_message_that_is_bigger_then_buffer(additional_bytes);
    // initially it does not fit into the buffer
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(&big_message[0], big_message.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, additional_bytes); // test consistency check
    // after once trying to parse the message it is understood that the buffer is too small
    ASSERT_FALSE(next_message());
    ASSERT_EQ(false, result_.error_);
    // and now the remaining message can be put into the increased buffer
    ASSERT_TRUE(add_message(&big_message[big_message.size() - missing_bytes - 1], missing_bytes, missing_bytes));
    // and the buffer can be parsed successfully
    ASSERT_TRUE(next_message());
    buf_.shift_front();

    auto const before = buf_.buffer().size();
    // when...
    auto small_message = craft_message_of_size(original_size);
    for (uint32_t i = 0; i < shrink_threshold_; ++i) {
        ASSERT_TRUE(add_message(&small_message[0], small_message.size(), missing_bytes));
        ASSERT_EQ(missing_bytes, 0);
        ASSERT_TRUE(next_message());
        ASSERT_EQ(false, result_.error_);
        ASSERT_FALSE(next_message());
        ASSERT_EQ(false, result_.error_);
        buf_.shift_front();
    }

    // then
    auto const after = buf_.buffer().size();
    EXPECT_LT(after, before);
    EXPECT_EQ(after, original_size);
}

TEST_F(test_receive_buffer, given_the_buffer_increased_when_subsequent_messages_are_small_and_big_then_the_buffer_does_not_shrink) {
    // given...
    auto const original_size = static_cast<uint32_t>(buf_.buffer().size());
    auto const additional_bytes = original_size * 3;
    auto big_message = craft_message_that_is_bigger_then_buffer(additional_bytes);
    // initially it does not fit into the buffer
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(&big_message[0], big_message.size(), missing_bytes));
    ASSERT_EQ(missing_bytes, additional_bytes); // test consistency check
    // after once trying to parse the message it is understood that the buffer is too small
    ASSERT_FALSE(next_message());
    ASSERT_EQ(false, result_.error_);
    // and now the remaining message can be put into the increased buffer
    ASSERT_TRUE(add_message(&big_message[big_message.size() - missing_bytes - 1], missing_bytes, missing_bytes));
    // and the buffer can be parsed successfully
    ASSERT_TRUE(next_message());
    buf_.shift_front();

    auto const before = buf_.buffer().size();
    // when...
    auto small_message = craft_message_of_size(original_size);
    for (uint32_t i = 0; i < shrink_threshold_; ++i) {
        // first small message
        ASSERT_TRUE(add_message(&small_message[0], small_message.size(), missing_bytes));
        ASSERT_EQ(missing_bytes, 0);
        ASSERT_TRUE(next_message());
        ASSERT_EQ(false, result_.error_);
        ASSERT_FALSE(next_message());
        ASSERT_EQ(false, result_.error_);
        buf_.shift_front();

        // now big message
        ASSERT_TRUE(add_message(&big_message[0], big_message.size(), missing_bytes));
        ASSERT_EQ(missing_bytes, 0); // test consistency check
        ASSERT_TRUE(next_message());
        ASSERT_EQ(false, result_.error_);
        ASSERT_FALSE(next_message());
        ASSERT_EQ(false, result_.error_);
        buf_.shift_front();
    }

    // then
    auto const after = buf_.buffer().size();
    EXPECT_EQ(after, before);
}

TEST_F(test_receive_buffer, given_message_exceeds_max_length_when_parsing_then_error_returned) {
    // Create a buffer with limited max message size
    local_receive_buffer limited_buf{256, 0}; // max 256 bytes

    auto msg = craft_message_of_size(512); //
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(limited_buf, &msg[0], msg.size(), missing_bytes));
    ASSERT_NE(0, missing_bytes);

    // when/then
    next_message_result result;
    EXPECT_FALSE(limited_buf.next_message(result));
    EXPECT_TRUE(result.error_);
}

TEST_F(test_receive_buffer, given_data_at_beginning_when_shift_called_then_noop) {
    auto msg = craft_message_of_size(32);
    size_t missing_bytes{0};
    ASSERT_TRUE(add_message(&msg[0], msg.size(), missing_bytes));

    auto before_addr = buf_.buffer().data();

    // when - shift when already at beginning
    buf_.shift_front();

    // then - buffer address unchanged
    auto after_addr = buf_.buffer().data();
    EXPECT_EQ(before_addr, after_addr);
}

}

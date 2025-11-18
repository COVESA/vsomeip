// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_RECEIVE_BUFFER_
#define VSOMEIP_V3_LOCAL_RECEIVE_BUFFER_

#include <cstdint>
#include <cstddef>
#include <vector>
#include <ostream>

#include <boost/asio.hpp>

namespace vsomeip_v3 {

/**
 * @struct next_message_result
 * @brief Result of attempting to parse the next message from the buffer.
 *
 * After calling `next_message()`, check the fields to determine what action to take:
 *
 * **Success** (next_message() returns true):
 * - `error_ == false`
 * - `message_data_` points to the message payload
 * - `message_size_` contains the message length
 *
 * **Insufficient Data** (next_message() returns false, no error):
 * - `error_ == false`
 * - `message_data_ == nullptr`
 * - More data needed; call receive again
 * - **Note**: Buffer may have grown automatically if a large message header was detected
 *
 * **Error** (next_message() returns false with error):
 * - `error_ == true`
 * - Message length exceeds `max_message_length_`
 * - Connection should be closed
 *
 * @warning `message_data_` pointer is invalidated by:
 *          - Calling `shift_front()` or any buffer modification
 *          - Receiving more data that causes reallocation
 */
struct next_message_result {
    void set_message(uint8_t const* _data, uint32_t _size);
    void set_error();
    void clear();

    bool error_{false}; ///< True if message parsing failed (invalid length)
    uint8_t const* message_data_{nullptr}; ///< Pointer to message data (valid until next buffer operation)
    uint32_t message_size_{0}; ///< Size of the message in bytes
};

/**
 * @class local_receive_buffer
 * @brief Manages buffering and parsing of incoming messages on local connections.
 *
 * This class handles:
 * - Dynamic buffer growth when messages exceed current capacity
 * - **Automatic buffer shrinking** after processing many small messages
 * - Message boundary detection using vsomeip protocol headers
 * - **Offset-based buffer management** for safe memory operations
 *
 * **Buffer Lifecycle**:
 * 1. **Initial state**: Buffer starts at 128 bytes
 * 2. **Growth**: Automatically grows when messages don't fit
 * 3. **Shrinking**: Automatically shrinks to 128 bytes after processing `shrink_threshold` small messages
 *
 * **Shrink Logic**:
 * - Small messages (≤ capacity/2) increment shrink counter
 * - Large messages (> capacity/2) reset shrink counter to 0
 * - When counter exceeds threshold AND buffer is empty, shrinks to initial size
 *
 * **Thread-safety**: This class is NOT thread-safe. External synchronization required.
 *
 * **Memory Management**:
 * - Uses **offset-based approach** (not iterators) to avoid ptrdiff_t overflow on 32-bit systems
 * - Offsets (`start_` and `end_`) remain valid after vector reallocation
 * - Pointers returned by `buffer()` are invalidated by: shift_front(), add_capacity(), shrink()
 *
 * **Usage Pattern**:
 * ```cpp
 * local_receive_buffer buf{1024 * 1024, 512};
 *
 * void receive() {
 *     auto asio_buf = buf.buffer();
 *     socket.async_receive(asio_buf, [this](error_code ec, size_t n) {
 *         if (ec) return;
 *
 *         buf.bump_end(n);
 *
 *         next_message_result result;
 *         while (buf.next_message(result)) {
 *             handle_message(result.message_data_, result.message_size_);
 *         }
 *         if (result.error_) {
 *             close_connection();
 *             return;
 *         }
 *         buf.shift_front();
 *
 *         receive();
 *     });
 * }
 * ```
 *
 * @note Capacity management (growth and shrinking) is **automatic** inside next_message().
 *       The caller only needs to call shift_front() to reclaim consumed space.
 */
class local_receive_buffer {
public:
    /**
     * @brief Constructs a receive buffer with the specified limits.
     *
     * @param _max_message_length Maximum allowed message size in bytes.
     *        Use MESSAGE_SIZE_UNLIMITED for no limit. Messages exceeding
     *        this size cause `next_message()` to return an error.
     * @param _buffer_shrink_threshold Number of consecutive small messages
     *        after which the buffer should shrink. Set to 0 to disable shrinking.
     */
    local_receive_buffer(uint32_t _max_message_length, uint32_t _buffer_shrink_threshold);

    uint8_t const& operator[](size_t _idx) const { return mem_[_idx]; }

    /**
     * @brief Attempts to parse the next message from the buffer.
     *
     * @param _result Output parameter containing parse result.
     * @return true if a complete message was parsed, false otherwise.
     *
     * **Behavior**:
     * 1. Checks if enough data for header (8 bytes)
     * 2. Reads message length from header
     * 3. Validates length against `max_message_length_`
     * 4. **Automatically grows buffer** if message doesn't fit (calls add_capacity())
     * 5. If complete message available, returns message pointer and advances `start_`
     * 6. Updates shrink counter based on message size
     * 7. **Automatically shrinks buffer** if conditions met (after threshold small messages)
     *
     * **Shrink Counter Logic**:
     * - Messages > capacity/2: Reset counter to 0 (considered "large")
     * - Messages ≤ capacity/2: Increment counter (considered "small")
     * - When counter ≥ threshold AND buffer empty: Shrink to 128 bytes
     *
     * **Automatic Operations**:
     * - **Growth**: Transparently grows buffer when large message header is detected
     * - **Shrinking**: Transparently shrinks buffer after many small messages
     *
     * **Error Conditions**:
     * - Message length > `max_message_length_`: Sets `_result.error_ = true`
     * - Insufficient memory for growth: Sets `_result.error_ = true`
     * - Message size exceeds numerical limits: Sets `_result.error_ = true`
     *
     * **After Error**:
     * The buffer state is undefined after an error. The connection should be closed
     * and the buffer should not be reused. This typically indicates:
     * - Corrupted protocol data
     * - Malicious/malformed input
     * - System resource exhaustion
     *
     * **Side Effects**:
     * - May modify buffer size (grow or shrink)
     * - Advances `start_` offset on successful parse
     * - Invalidates previous `message_data_` pointers
     *
     * @note Call this repeatedly until it returns false to process all buffered messages.
     * @note After processing all messages, call shift_front() to reclaim space.
     */
    bool next_message(next_message_result& _result);

    /**
     * @brief Shifts unconsumed data to the beginning of the buffer.
     *
     * This reclaims space occupied by consumed messages (data before `start_`).
     * Call this before `add_capacity()` when more space is needed, as it may
     * provide sufficient space without growing the buffer.
     *
     * @note This is a no-op if `start_ == 0` (no wasted space).
     * @note Invalidates any pointers to buffer data.
     */
    void shift_front();

    /**
     * @brief Returns a mutable buffer for receiving data.
     *
     * @return Boost.Asio mutable buffer starting at `end_` with available space.
     *
     * This buffer should be passed to async I/O operations. After receiving data,
     * call `bump_end()` with the number of bytes received to update the buffer state.
     *
     * @note The returned buffer might become invalid after any operation that modifies
     *       the buffer (shift_front, next_message).
     */
    auto buffer() { return boost::asio::buffer(&mem_[end_], mem_.size() - end_); }

    /**
     * @brief Updates the end offset after receiving data.
     *
     * @param _new_bytes Number of bytes to add to the `end_` pointer.
     * @return true if successful, false if the new end would exceed buffer size.
     *
     * Call this after receiving data into the buffer returned by `buffer()`.
     *
     * Example:
     * ```cpp
     * auto buf = rcv_buf.buffer();
     * size_t n = socket.receive(buf);
     * rcv_buf.bump_end(n);
     * ```
     */
    [[nodiscard]] bool bump_end(size_t _new_bytes);

    friend std::ostream& operator<<(std::ostream& _out, local_receive_buffer const& _buffer);

private:
    /**
     * @brief Shrinks the buffer to its initial size and resets state.
     *
     * **Called automatically** by next_message() when:
     * - Buffer is empty (start_ == end_)
     * - Shrink counter ≥ threshold
     * - Current size > initial size (128 bytes)
     *
     * **Effects**:
     * - Resizes buffer to 128 bytes
     * - Calls shrink_to_fit() to release excess memory
     * - Resets shrink counter to 0
     * - Resets start_ and end_ to beginning
     *
     * @warning Should only be called when buffer is empty (start_ == end_).
     */
    void shrink();

    /**
     * @brief Increases buffer capacity by the specified amount.
     *
     * **Called automatically** by next_message() when:
     * - Message header indicates message larger than current buffer
     * - Not enough space for minimum header size
     *
     * **Effects**:
     * - Grows buffer by `_capacity` bytes
     *
     * @param _capacity Number of bytes to add to current buffer size.
     * @return true if successful, false if size would overflow.
     */
    [[nodiscard]] bool add_capacity(size_t _capacity);

    static constexpr uint32_t initial_buffer_size_{128}; ///< Initial and minimum buffer size
    uint32_t const max_message_length_{0}; ///< Maximum allowed message size (0 = unlimited)
    uint32_t const buffer_shrink_threshold_{0}; ///< Number of small messages before shrink (0 = no shrink)
    uint32_t shrink_ct_{0}; ///< Counter for small messages (reset on large message)

    std::vector<uint8_t> mem_; ///< Backing storage for buffer

    /// Offset to first unprocessed byte (advanced by next_message()).
    /// Index of the start of the next message to parse.
    size_t start_{0};

    /// Offset to one past last received byte (advanced by bump_end()).
    /// Index where next received data will be written.
    /// Invariant: start_ <= end_ <= mem_.size()
    size_t end_{0};
};
}

#endif

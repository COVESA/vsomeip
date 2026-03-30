// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command_message.hpp"

#include <utility>
#include <boost/asio.hpp>

#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>
#include <functional>

namespace vsomeip_v3::testing {

enum class data_pipe_state {
    OPEN, ///< Data flows through to the consumer.
    CLOSED, ///< Incoming data is buffered and not forwarded.
};

/**
 * A thread-safe byte buffer that sits between a producer (fake socket's consume)
 * and a consumer (async_receive). An optional checker function inspects each
 * complete vsomeip command message and decides whether to forward or hold it;
 * without a checker all data passes through unconditionally.
 *
 * Typical layering:
 *   fake_tcp_socket_handle  owns a data_pipe  (transport)
 *   command_gate            wraps a data_pipe with a command-level policy
 */
class data_pipe {
public:
    /// Called once per incoming command message; return CLOSED to stop forwarding.
    using local_message_checker_t = std::function<data_pipe_state(command_message const&)>;
    /// Called (outside the internal lock) when the pipe transitions to OPEN and
    /// buffered data has been promoted to the forward queue.
    using open_reaction_t = std::function<void()>;
    explicit data_pipe(local_message_checker_t _checker = nullptr);

    /**
     * Resets the pipe for reuse on a new socket:
     * - installs the callback that wakes the socket's async_receive;
     * - clears both internal queues.
     * Must be called before the first add_data on a freshly installed pipe.
     */
    void init(open_reaction_t _react);

    /**
     * Swaps the queues (data_to_forward_ and input_data_) with the
     * passed in _pipe
     * Both pipes are locked simultaneously to avoid deadlock.
     */
    void exchange_queues(data_pipe& _pipe);

    /// Returns the number of bytes ready to be consumed (data_to_forward_ only).
    /// input_data_ held behind a closed gate is excluded: it cannot be consumed
    /// and must not suppress connection-reset injection in async_receive.
    size_t size() const;

    /// Appends _data and, if OPEN, immediately promotes any complete messages
    /// through the checker into the forward queue.
    void add_data(std::vector<unsigned char> _data);

    /// Copies up to _out.size() bytes from the forward queue into _out.
    /// Returns the number of bytes written (0 if nothing ready).
    [[nodiscard]] size_t fetch_data(boost::asio::mutable_buffer _out);

    /// Transitions the pipe to _input. Switching to OPEN promotes any buffered
    /// input_data_ and fires the open_reaction_ callback.
    void set_state(data_pipe_state _input);

private:
    void push_data(std::scoped_lock<std::mutex> const&);
    void push_local_data(std::scoped_lock<std::mutex> const&);
    void push_through(std::scoped_lock<std::mutex> const&);

    data_pipe_state state_{data_pipe_state::OPEN};
    local_message_checker_t checker_;
    open_reaction_t open_reaction_;
    std::vector<unsigned char> input_data_;
    std::vector<unsigned char> data_to_forward_;

    mutable std::mutex mtx_;
};

}

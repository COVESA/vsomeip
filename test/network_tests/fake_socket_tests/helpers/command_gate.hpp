// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "data_pipe.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <condition_variable>

namespace vsomeip_v3::testing {

enum class command_gate_state { BLOCKED, UNBLOCKED };

/**
 * A test-control object that wraps a data_pipe with command-message-level
 * blocking logic. The pipe is inspected message-by-message; once the
 * configured trigger fires the gate blocks further forwarding until explicitly
 * re-opened via block(false).
 *
 * Usage pattern:
 *   auto gate = command_gate::create();
 *   // install before start_apps so the pipe is in place when the socket forms:
 *   ASSERT_TRUE(setup_data_pipe(client, server, socket_role::client, gate->get_data_pipe()));
 *   ...
 *   gate->block_at(protocol::id_e::ROUTING_INFO_ID);
 *   // trigger something that sends a ROUTING_INFO to the client
 *   ASSERT_TRUE(gate->wait_for_blocked());
 *   // now exactly one ROUTING_INFO has been delivered; more are held
 *   gate->block(false); // release
 */
class command_gate : public std::enable_shared_from_this<command_gate> {
    struct hidden { };

public:
    command_gate(hidden);
    /// Factory — must be used instead of direct construction so that the
    /// internal weak_self reference is valid from the start.
    static std::shared_ptr<command_gate> create();

    /// Returns the underlying data_pipe to pass to setup_data_pipe.
    std::shared_ptr<data_pipe> get_data_pipe() const;

    /// Checker callback invoked by data_pipe for every forwarded command.
    data_pipe_state operator()(command_message const& _msg);

    /// Lets the first _count messages with id _id through, then blocks.
    /// Resets an in-progress search. Reopens the pipe to flush any buffered data.
    void block_at(vsomeip_v3::protocol::id_e _id, uint32_t _count = 1);

    /// Unconditionally blocks (_block=true) or unblocks (_block=false)
    /// regardless of message content, cancelling any active block_at search.
    void block(bool _block = false);

    /// Waits until the gate is in the BLOCKED state.
    [[nodiscard]] bool wait_for_blocked(std::chrono::milliseconds _timeout = std::chrono::seconds(2)) const;

private:
    std::shared_ptr<data_pipe> pipe_;
    struct search {
        vsomeip_v3::protocol::id_e id_;
        uint32_t barrier_;
        uint32_t count_;
    };
    std::optional<search> search_;
    command_gate_state state_{command_gate_state::UNBLOCKED};
    mutable std::mutex mtx_;
    mutable std::condition_variable cv_;
};

}

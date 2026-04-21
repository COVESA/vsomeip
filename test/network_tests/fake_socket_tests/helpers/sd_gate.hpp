// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "data_pipe.hpp"
#include "someip_message.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <condition_variable>

namespace vsomeip_v3::testing {

enum class gate_state { BLOCKED, UNBLOCKED };

/**
 * @see command_gate.
 *
 * Exactly the same underlying behavior as the command_gate developed for internal communication,
 * although for service discovery. A someip gate shall be developed as future enhancement.
 * The pipe is inspected message-by-message, iterating through all sd entries of each message and
 * once the configured trigger fires the gate blocks further forwarding until explicitly
 * re-opened via block(false).
 */
class sd_gate : public std::enable_shared_from_this<sd_gate> {
    struct hidden { };

public:
    sd_gate(hidden);

    /**
     * @see command_gate::create
     */
    static std::shared_ptr<sd_gate> create();

    /*
     * Returns the underlying data_pipe to pass to setup_data_pipe.
     **/
    std::shared_ptr<data_pipe> get_data_pipe() const;

    /*
     *Checker callback invoked by data_pipe for every forwarded command.
     **/
    data_pipe_state operator()(someip_message const& _msg);

    /**
     * @see command_gate::block_at
     */
    void block_at(someip_sd_record_message _sd_msg, uint32_t _count = 1);

    /**
     * @see command_gate::block
     */
    void block(bool _block = false);

    /**
     * @see command_gate::wait_for_blocked
     */
    [[nodiscard]] bool wait_for_blocked(std::chrono::milliseconds _timeout = std::chrono::seconds(2)) const;

private:
    std::shared_ptr<data_pipe> pipe_;
    struct search {
        someip_sd_record_message sd_;
        uint32_t barrier_;
        uint32_t count_;
    };
    std::optional<search> search_;
    gate_state state_{gate_state::UNBLOCKED};
    mutable std::mutex mtx_;
    mutable std::condition_variable cv_;
};

}

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
#include <variant>

#include <vsomeip/vsomeip.hpp>

namespace vsomeip_v3::testing {

enum class gate_state { BLOCKED, UNBLOCKED };

/**
 * A unified gate for intercepting SOME/IP traffic on a data_pipe — covering
 * both regular service messages (requests, responses, notifications) and
 * Service Discovery messages.
 *
 * The pipe is inspected message-by-message. Once the configured trigger fires,
 * the gate blocks further forwarding until explicitly re-opened via block(false).
 *
 * Two trigger kinds are supported:
 *  - trigger     : matches regular SOME/IP messages by service, method, optional
 *                  message type, and optional payload predicate.
 *  - sd_trigger  : matches SD entries by entry type and TTL.
 *
 * @note For SD multicast endpoints that may be restarted internally, it is
 *       important to call block(false) after the gated test part is complete.
 *       The SD multicast endpoint can be restarted if it doesn't receive any
 *       messages for a certain amount of time (110% of cyclic_offer_delay_);
 *       the gate handles being re-applied after each restart internally.
 */
class someip_gate : public std::enable_shared_from_this<someip_gate> {
    struct hidden { };

public:
    someip_gate(hidden);

    /// Factory — must be used instead of direct construction.
    static std::shared_ptr<someip_gate> create();

    /// Returns the underlying data_pipe to pass to setup_data_pipe.
    std::shared_ptr<data_pipe> get_data_pipe() const;

    /// Checker callback invoked by data_pipe for every forwarded someip message.
    data_pipe_state operator()(someip_message const& _msg);

    struct trigger {
        vsomeip::service_t service_;
        vsomeip::method_t method_;
        std::optional<vsomeip::message_type_e> type_; ///< nullopt matches any message type

        /// Optional predicate over the message payload. nullopt matches any payload.
        using payload_pred_t = std::function<bool(std::shared_ptr<vsomeip::payload>)>;
        std::optional<payload_pred_t> payload_ = std::nullopt;

        bool operator==(trigger const& _other) const {
            return service_ == _other.service_ && method_ == _other.method_ && type_ == _other.type_;
        }
    };

    /// Trigger for Service Discovery traffic.
    struct sd_trigger {
        sd::entry_type_e id_;
        ttl_t ttl_;

        bool operator==(sd_trigger const& _other) const { return id_ == _other.id_ && ttl_ == _other.ttl_; }
    };

    /// Lets the first _count SOME/IP messages matching _trigger through, then blocks.
    /// Resets any in-progress search and reopens the pipe to flush buffered data.
    void block_at(trigger _trigger, uint32_t _count = 1);

    /// Lets the first _count SD entries matching _trigger through, then blocks.
    /// Resets any in-progress search and reopens the pipe to flush buffered data.
    /// @note See class-level note for SD multicast endpoint lifecycle details.
    void block_at(sd_trigger _trigger, uint32_t _count = 1);

    /// Unconditionally blocks (_block=true) or unblocks (_block=false),
    /// cancelling any active block_at search.
    void block(bool _block = false);

    /// Waits until the gate transitions to the BLOCKED state.
    [[nodiscard]] bool wait_for_blocked(std::chrono::milliseconds _timeout = std::chrono::seconds(2)) const;

private:
    std::shared_ptr<data_pipe> pipe_;

    struct search {
        std::variant<trigger, sd_trigger> trigger_;
        uint32_t barrier_;
        uint32_t count_;
    };

    std::optional<search> search_;
    gate_state state_{gate_state::UNBLOCKED};
    mutable std::mutex mtx_;
    mutable std::condition_variable cv_;
};

} // namespace vsomeip_v3::testing

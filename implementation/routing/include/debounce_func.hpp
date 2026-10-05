// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../configuration/include/debounce_filter_impl.hpp"
#include "../../endpoints/include/abstract_socket_factory.hpp"
#include "../../endpoints/include/abstract_clock.hpp"

#include <vsomeip/function_types.hpp>
#include <vsomeip/payload.hpp>

#include <chrono>
#include <memory>

namespace vsomeip_v3 {

/**
 * @brief Compiles a per-subscriber debounce filter into an epsilon-change predicate.
 *
 * Shared by `event` and `provider_event`. The returned predicate forwards a payload
 * update when the (optionally byte-masked) value changed and/or the configured
 * interval has elapsed. Interval state (`last_forwarded_`) lives on the filter object,
 * so the closure is portable and holds no owner-specific state.
 *
 * The current time is read from `abstract_socket_factory::get()->get_clock()`.
 */
inline epsilon_change_func_t make_debounce_func(std::shared_ptr<debounce_filter_impl_t> const& _filter) {
    auto clock = abstract_socket_factory::get()->get_clock();
    return [_filter, clock](std::shared_ptr<payload> const& _old, std::shared_ptr<payload> const& _new) {
        bool is_changed = false, is_elapsed = false;

        if (_filter->on_change_) {
            length_t min_length, max_length;
            if (_old->get_length() < _new->get_length()) {
                min_length = _old->get_length();
                max_length = _new->get_length();
            } else {
                min_length = _new->get_length();
                max_length = _old->get_length();
            }

            // Any additional byte that is not fully excluded (0xFF mask) is a change.
            for (length_t i = min_length; i < max_length; i++) {
                auto j = _filter->ignore_.find(i);
                if (j == _filter->ignore_.end() || j->second != 0xFF) {
                    is_changed = true;
                    break;
                }
            }

            if (!is_changed) {
                const byte_t* old_data = _old->get_data();
                const byte_t* new_data = _new->get_data();
                for (length_t i = 0; i < min_length; i++) {
                    auto j = _filter->ignore_.find(i);
                    if (j == _filter->ignore_.end()) {
                        if (old_data[i] != new_data[i]) {
                            is_changed = true;
                            break;
                        }
                    } else if (j->second != 0xFF) {
                        if ((old_data[i] & ~(j->second)) != (new_data[i] & ~(j->second))) {
                            is_changed = true;
                            break;
                        }
                    }
                }
            }
        }

        if (_filter->interval_ > -1) {
            auto const now = clock->now();
            auto const last = _filter->last_forwarded_.load();
            int64_t const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
            is_elapsed = (last == std::chrono::steady_clock::time_point::max() || elapsed >= _filter->interval_);
            if (is_elapsed || (is_changed && _filter->on_change_resets_interval_)) {
                _filter->last_forwarded_.store(now);
            }
        }

        return (is_changed || is_elapsed);
    };
}

} // namespace vsomeip_v3

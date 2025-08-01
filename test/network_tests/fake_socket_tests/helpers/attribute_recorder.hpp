// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TESTING_ATTRIBUTE_RECORDER_HPP_
#define VSOMEIP_V3_TESTING_ATTRIBUTE_RECORDER_HPP_

#include <algorithm>
#include <chrono>
#include <optional>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <vector>

namespace vsomeip_v3::testing {

/**
 * Helper to block one thread while waiting for some events to happen on a background thread.
 */
template<typename Value>
class attribute_recorder {
public:
    void record(Value _value) {
        auto const lock = std::scoped_lock(mtx_);
        record_.push_back(_value);
        cv_.notify_all();
    }

    void clear() {
        auto const lock = std::scoped_lock(mtx_);
        record_.clear();
        // not necessary to notify anybody, as we are only providing helpers for awaiting
        // a value, not nothing
    }

    template<typename Predicate>
    [[nodiscard]] bool wait_for(Predicate p, std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        auto lock = std::unique_lock(mtx_);
        if (p(record_)) {
            return true;
        }
        return cv_.wait_for(lock, timeout, [&] { return p(record_); });
    }

    [[nodiscard]] bool wait_for(Value const& _value, std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        return wait_for(
                [&](auto const& record) {
                    return std::any_of(record.begin(), record.end(), [&](auto const& rec) { return rec == _value; });
                },
                timeout);
    }

    [[nodiscard]] std::optional<Value> last() {
        auto const lock = std::scoped_lock(mtx_);
        return record_.empty() ? std::nullopt : std::optional(record_.back());
    }

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<Value> record_;
};
}
#endif

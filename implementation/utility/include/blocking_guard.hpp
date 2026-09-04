// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <ostream>
#include <utility>

#include "../../logger/include/logger_ext.hpp"

namespace vsomeip_v3 {

// Routing manager callbacks run in the io threads: while one blocks, that thread routes nothing. Warn as soon as the delay is noticeable,
// raise an error once it is certainly harmful.
constexpr std::chrono::milliseconds VSOMEIP_CALLBACK_WARN_TIME{10};
constexpr std::chrono::milliseconds VSOMEIP_CALLBACK_ERROR_TIME{250};

template<typename Writer>
struct lazy_description {
    Writer write_;
};

template<typename Writer>
lazy_description(Writer) -> lazy_description<Writer>;

template<typename Writer>
std::ostream& operator<<(std::ostream& _os, const lazy_description<Writer>& _description) {
    _description.write_(_os);
    return _os;
}

// Reports how long the guarded scope blocked the io thread. _describe names the callback the way it is named in the API,
// arguments included.
template<typename Describe>
class blocking_guard {
public:
    explicit blocking_guard(Describe _describe) : describe_{std::move(_describe)} { }

    blocking_guard(const blocking_guard&) = delete;
    blocking_guard& operator=(const blocking_guard&) = delete;

    ~blocking_guard() {
        const auto its_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_);
        const auto its_log = [this, its_elapsed](auto&& _message) {
            _message << "BLOCKING CALL " << describe_ << " returned after " << its_elapsed.count() << " ms";
        };

        if (its_elapsed >= VSOMEIP_CALLBACK_ERROR_TIME) {
            its_log(VSOMEIP_ERROR);
        } else if (its_elapsed >= VSOMEIP_CALLBACK_WARN_TIME) {
            its_log(VSOMEIP_WARNING);
        }
    }

private:
    Describe describe_;
    const std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
};

template<typename Describe>
blocking_guard(Describe) -> blocking_guard<Describe>;

// Invokes a callback under a blocking_guard.
template<typename Callback, typename Describe, typename... Args>
decltype(auto) invoke_watched(const Callback& _callback, Describe _describe, Args&&... _args) {
    blocking_guard its_guard{std::move(_describe)};
    return _callback(std::forward<Args>(_args)...);
}

}

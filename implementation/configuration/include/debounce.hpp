// Copyright (C) 2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_DEBOUNCE_HPP
#define VSOMEIP_CFG_DEBOUNCE_HPP

#include <map>

namespace vsomeip {
namespace cfg {

// Messages are forwarded either because their value differs from the
// last received message (on_change=true) or because the specified time
// (interval_) between two messages has elapsed. A message that is forwarded
// because of a changed value may reset the time until the next unchanged
// message is forwarded or not (on_change_resets_interval). By specifiying
// indexes and bit masks, the comparison that is carried out to decide whether
// or not two message values differ is configurable (ignore_).
struct debounce {
    debounce() : on_change_(false),
            on_change_resets_interval_(false),
            interval_(0),
            last_forwarded_((std::chrono::steady_clock::time_point::max)()) {
    }

    bool on_change_;
    bool on_change_resets_interval_;
    std::map<std::size_t, byte_t> ignore_;

    long interval_;
    std::chrono::steady_clock::time_point last_forwarded_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_DEBOUNCE_HPP

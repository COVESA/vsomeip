// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/observer.hpp"

namespace vsomeip_v3 {

bool observer::observe(const boost::asio::ip::address& _host, const bool _is_multicast) {
    std::scoped_lock lock{mutex_};

    // Update the entry.
    auto& record = hosts_[_host];
    if (_is_multicast) {
        if (!record.multicast) {
            record.multicast = true;
            return false; // Not previously observed.
        }
    } else {
        if (!record.unicast) {
            record.unicast = true;
            return false; // Not previously observed.
        }
    }

    // Previously observed.
    return true;
}

void observer::clear() {
    std::scoped_lock lock{mutex_};
    hosts_.clear();
}

} // namespace vsomeip_v3

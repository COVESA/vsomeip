// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_CFG_LOCAL_CLIENTS_KEEPALIVE_HPP_
#define VSOMEIP_V3_CFG_LOCAL_CLIENTS_KEEPALIVE_HPP_

namespace vsomeip_v3 {
namespace cfg {

struct local_clients_keepalive {
    local_clients_keepalive() : is_enabled_(false), time_in_ms_(VSOMEIP_DEFAULT_LOCAL_CLIENTS_KEEPALIVE_TIME) { }

    bool is_enabled_;
    std::chrono::milliseconds time_in_ms_;
};

} // namespace cfg
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_CFG_LOCAL_CLIENTS_KEEPALIVE_HPP_

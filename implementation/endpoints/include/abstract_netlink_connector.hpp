// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ABSTRACT_NETLINK_CONNECTOR_HPP_
#define VSOMEIP_V3_ABSTRACT_NETLINK_CONNECTOR_HPP_

#if defined(__linux__)

#include <string>
#include <functional>

namespace vsomeip_v3 {

using net_if_changed_handler_t = std::function<void(bool, // true = is interface, false = is route
                                                    std::string, // interface name
                                                    bool) // available?
                                               >;

class abstract_netlink_connector {
public:
    virtual ~abstract_netlink_connector() = default;

    virtual void register_net_if_changes_handler(const net_if_changed_handler_t& _handler) = 0;
    virtual void unregister_net_if_changes_handler() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

} // namespace vsomeip_v3

#endif // __linux__

#endif // VSOMEIP_V3_NETLINK_CONNECTOR_HPP_

// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TESTING_FAKE_NETLINK_CONNECTOR_HPP_
#define VSOMEIP_V3_TESTING_FAKE_NETLINK_CONNECTOR_HPP_

#if defined(__linux__) || defined(ANDROID)
#include "../../../implementation/endpoints/include/abstract_netlink_connector.hpp"

#include "fake_tcp_socket.hpp"
#include "socket_manager.hpp"

namespace vsomeip_v3::testing {

/**
 * Fake impl that will report that the interface is available immediately.
 **/
class fake_netlink_connector : public abstract_netlink_connector, public std::enable_shared_from_this<fake_netlink_connector> {
public:
    fake_netlink_connector(boost::asio::io_context& _io) : io_(_io) { }
    ~fake_netlink_connector() { stop(); }

private:
    virtual void register_net_if_changes_handler(const net_if_changed_handler_t& _handler) { handler_ = _handler; }
    virtual void unregister_net_if_changes_handler() { handler_ = nullptr; }
    virtual void start() {
        boost::asio::post(io_, [self = weak_from_this()] {
            if (auto lock = self.lock(); lock) {
                if (lock->handler_) {
                    lock->handler_(true /*interface*/, "fake-interface", true /*is avail*/);
                }
            }
        });
    }
    virtual void stop() { handler_ = nullptr; }

    boost::asio::io_context& io_;
    net_if_changed_handler_t handler_;
};
}

#endif
#endif

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#if defined(__linux__)
#include "../../../implementation/endpoints/include/abstract_netlink_connector.hpp"

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
    virtual void register_net_if_changes_handler(const net_if_changed_handler_t& _handler) {
        std::scoped_lock lock(mutex_);
        handler_ = _handler;
    }
    virtual void unregister_net_if_changes_handler() {
        std::scoped_lock lock(mutex_);
        handler_ = nullptr;
    }
    virtual void start() {
        std::scoped_lock lock(mutex_);
        boost::asio::post(io_, [handler = handler_] {
            if (handler) {
                handler(true /*interface*/, "fake-interface", true /*is avail*/);
                handler(false /*route*/, "fake-interface", true /*is avail*/);
            }
        });
    }
    virtual void stop() {
        std::scoped_lock lock(mutex_);
        handler_ = nullptr;
    }

    std::mutex mutex_;
    boost::asio::io_context& io_;
    net_if_changed_handler_t handler_;
};
}

#endif

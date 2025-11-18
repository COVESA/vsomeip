// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_DELEGATING_SOCKET_FACTORY_
#define VSOMEIP_V3_DELEGATING_SOCKET_FACTORY_

#include "../../../implementation/endpoints/include/abstract_socket_factory.hpp"

namespace vsomeip_v3::testing {

/**
 * Allows to inject this factory into a test binary having the need for different socket factories
 **/
class delegating_socket_factory : public abstract_socket_factory {
public:
    std::shared_ptr<abstract_netlink_connector> create_netlink_connector(boost::asio::io_context& _io, const boost::asio::ip::address& _a,
                                                                         const boost::asio::ip::address& _b, bool _c) override {
        return impl_->create_netlink_connector(_io, _a, _b, _c);
    }

    virtual std::unique_ptr<tcp_socket> create_tcp_socket(boost::asio::io_context& _io) override { return impl_->create_tcp_socket(_io); }
    virtual std::unique_ptr<tcp_acceptor> create_tcp_acceptor(boost::asio::io_context& _io) override {
        return impl_->create_tcp_acceptor(_io);
    }
    virtual std::unique_ptr<abstract_timer> create_timer(boost::asio::io_context& _io) override { return impl_->create_timer(_io); }

    // assumed to be filled by the fixtures.
    std::shared_ptr<abstract_socket_factory> impl_;
};

}

#endif

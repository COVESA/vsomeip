// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TESTING_FAKE_SOCKET_FACTORY_HPP_
#define VSOMEIP_V3_TESTING_FAKE_SOCKET_FACTORY_HPP_

#include "../../../implementation/endpoints/include/abstract_socket_factory.hpp"

#include "fake_netlink_connector.hpp"
#include "fake_tcp_socket.hpp"
#include "socket_manager.hpp"

namespace vsomeip_v3::testing {
/**
 * Fake impl of the abstract_socket_factory, that will register each
 * created fake_socket_handle (that is owned by the fake_socket) with the
 * injected socket_manager.
 *
 * For the chosen injection mechanism there will be exactly one fake_socket_factory
 * per test binary. Each test should renew the socket_manager to ensure no interference.
 * It is adviced to use this class not directly but by using the
 * base_fake_socket_fixture.
 **/
class fake_socket_factory : public abstract_socket_factory {
public:
    void set_manager(std::shared_ptr<socket_manager> const& _sm) { socket_manager_ = _sm; }

private:
    std::shared_ptr<abstract_netlink_connector> create_netlink_connector(boost::asio::io_context& _io, const boost::asio::ip::address&,
                                                                         const boost::asio::ip::address&, bool) override {
        return std::make_shared<fake_netlink_connector>(_io);
    }

    virtual std::unique_ptr<tcp_socket> create_tcp_socket(boost::asio::io_context& _io) override {
        if (auto sm = socket_manager_.lock()) {
            auto state = std::make_shared<fake_tcp_socket_handle>(_io);
            sm->add_socket(state, &_io);
            return std::make_unique<fake_tcp_socket>(state);
        }
        return nullptr;
    }
    virtual std::unique_ptr<tcp_acceptor> create_tcp_acceptor(boost::asio::io_context& _io) override {
        if (auto sm = socket_manager_.lock()) {
            auto state = std::make_shared<fake_tcp_acceptor_handle>(_io);
            sm->add_acceptor(state, &_io);
            return std::make_unique<fake_tcp_acceptor>(state);
        }
        return nullptr;
    }

    std::weak_ptr<socket_manager> socket_manager_;
};
}

#endif

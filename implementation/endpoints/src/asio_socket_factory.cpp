// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/asio_socket_factory.hpp"
#include "../include/asio_tcp_socket.hpp"
#include "../include/asio_timer.hpp"
#include "../include/netlink_connector.hpp"

namespace vsomeip_v3 {

#if defined(__linux__)
std::shared_ptr<abstract_netlink_connector>
asio_socket_factory::create_netlink_connector(boost::asio::io_context& _io, const boost::asio::ip::address& _address,
                                              const boost::asio::ip::address& _multicast_address, bool _is_requiring_link) {
    return std::make_shared<netlink_connector>(_io, _address, _multicast_address, _is_requiring_link);
}
#endif

std::unique_ptr<tcp_socket> asio_socket_factory::create_tcp_socket(boost::asio::io_context& _io) {
    return std::make_unique<asio_tcp_socket>(_io);
}

std::unique_ptr<tcp_acceptor> asio_socket_factory::create_tcp_acceptor(boost::asio::io_context& _io) {
    return std::make_unique<asio_tcp_acceptor>(_io);
}

std::unique_ptr<abstract_timer> asio_socket_factory::create_timer(boost::asio::io_context& _io) {
    return std::make_unique<asio_timer>(_io);
}
}

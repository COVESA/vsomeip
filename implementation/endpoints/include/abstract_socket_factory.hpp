// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ABSTRACT_SOCKET_FACTORY_HPP_
#define VSOMEIP_V3_ABSTRACT_SOCKET_FACTORY_HPP_

#include <boost/asio/ip/udp.hpp>
#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
#include <boost/asio/local/stream_protocol.hpp>
#endif
#include <functional>

#include "abstract_netlink_connector.hpp"
#include "tcp_socket.hpp"

namespace vsomeip_v3 {

class abstract_socket_factory {
public:
    virtual ~abstract_socket_factory() = default;

    static abstract_socket_factory* get();

#if defined(__linux__) || defined(ANDROID)
    // netlink needs to be faked in order to test local-tcp connection handling
    virtual std::shared_ptr<abstract_netlink_connector> create_netlink_connector(boost::asio::io_context& _io,
                                                                                 const boost::asio::ip::address& _address,
                                                                                 const boost::asio::ip::address& _multicast_address,
                                                                                 bool _is_requiring_link = true) = 0;
#endif

    // only tcp sockets are fakable atm.
    virtual std::unique_ptr<tcp_socket> create_tcp_socket(boost::asio::io_context& _io) = 0;
    virtual std::unique_ptr<tcp_acceptor> create_tcp_acceptor(boost::asio::io_context& _io) = 0;

    std::unique_ptr<boost::asio::ip::udp::socket> create_udp_socket(boost::asio::io_context& _io) {
        return std::make_unique<boost::asio::ip::udp::socket>(_io);
    }

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
    std::unique_ptr<boost::asio::local::stream_protocol::socket> create_uds_socket(boost::asio::io_context& _io) {
        return std::make_unique<boost::asio::local::stream_protocol::socket>(_io);
    }
#endif
};

// In order for this function to change the globally used abstract_socket_factory,
// it needs to be called before the first call of abstract_socket_factory::get().
// If this function is not called the asio_socket_factory is used as the global
// factory.
void set_abstract_factory(std::shared_ptr<abstract_socket_factory> ptr);

}

#endif

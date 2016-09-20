// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP
#define VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP

#include <boost/asio/ip/tcp.hpp>

#include <vsomeip/defines.hpp>
#include "client_endpoint_impl.hpp"

namespace vsomeip {

typedef client_endpoint_impl<
            boost::asio::ip::tcp
        > tcp_client_endpoint_base_impl;

class tcp_client_endpoint_impl: public tcp_client_endpoint_base_impl {
public:
    tcp_client_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
							 endpoint_type _remote,
                             boost::asio::io_service &_io,
                             std::uint32_t _max_message_size);
    virtual ~tcp_client_endpoint_impl();

    void start();

    bool get_remote_address(boost::asio::ip::address &_address) const;
    unsigned short get_local_port() const;
    unsigned short get_remote_port() const;
    bool is_reliable() const;
    bool is_local() const;

private:
    void send_queued();
    bool is_magic_cookie(size_t _offset) const;
    void send_magic_cookie(message_buffer_ptr_t &_buffer);

    void receive_cbk(boost::system::error_code const &_error,
                     std::size_t _bytes);

    void connect();
    void receive();

    receive_buffer_t recv_buffer_;
    size_t recv_buffer_size_;
};

} // namespace vsomeip

#endif // VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP

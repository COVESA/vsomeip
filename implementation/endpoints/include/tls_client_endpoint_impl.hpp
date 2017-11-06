// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TLS_CLIENT_ENDPOINT_HPP
#define VSOMEIP_TLS_CLIENT_ENDPOINT_HPP

#include <boost/asio/ip/tcp.hpp>

#include <vsomeip/defines.hpp>
#include "client_endpoint_impl.hpp"
#include "secure_endpoint_base.hpp"

namespace vsomeip {

typedef client_endpoint_impl<
            boost::asio::ip::tcp
        > tcp_client_endpoint_base_impl;

class tls_client_endpoint_impl: public tcp_client_endpoint_base_impl,
                                public secure_endpoint_base {
public:
    tls_client_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
                             endpoint_type _remote,
                             boost::asio::io_service &_io,
                             std::uint32_t _max_message_size,
                             std::uint32_t buffer_shrink_threshold,
                             bool _confidential,
                             const std::vector<std::uint8_t> &_psk,
                             const std::string &_pskid);
    virtual ~tls_client_endpoint_impl();

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
    void calculate_shrink_count();

    const std::uint32_t recv_buffer_size_initial_;
    message_buffer_t recv_buffer_;
    size_t recv_buffer_size_;
    std::uint32_t missing_capacity_;
    std::uint32_t shrink_count_;
    const std::uint32_t buffer_shrink_threshold_;

    const boost::asio::ip::address remote_address_;
    const std::uint16_t remote_port_;
};

} // namespace vsomeip

#endif // VSOMEIP_TLS_CLIENT_ENDPOINT_HPP

// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_INTERNAL_DTLS_CLIENT_IMPL_HPP
#define VSOMEIP_INTERNAL_DTLS_CLIENT_IMPL_HPP

#include <memory>
#include <deque>
#include <string>
#include <vector>
#include <mutex>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>

#include <vsomeip/defines.hpp>

#include "client_endpoint_impl.hpp"
#include "secure_endpoint_base.hpp"

#include "../../helper/tls_socket_wrapper.hpp"

namespace vsomeip {

class endpoint_adapter;

typedef client_endpoint_impl<boost::asio::ip::udp>
    udp_client_endpoint_base_impl;

class dtls_client_endpoint_impl : public secure_endpoint_base,
                                  public udp_client_endpoint_base_impl {
public:
    dtls_client_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                            endpoint_type _local, endpoint_type _remote,
                            boost::asio::io_service &_io, bool _confidential,
                            const std::vector<std::uint8_t> &_psk, const std::string &_pskid);
    virtual ~dtls_client_endpoint_impl();

    void start();

    void connect_cbk_intern(boost::system::error_code const &_error);
    void handshake_cbk(boost::system::error_code const &_error);
    void receive_cbk(boost::system::error_code const &_error, std::size_t _bytes);

    bool get_remote_address(boost::asio::ip::address &_address) const;
    unsigned short get_local_port() const;
    unsigned short get_remote_port() const;
    bool is_local() const;

private:
    void connect();
    void send_queued();
    void receive();

    message_buffer_ptr_t recv_buffer_;

    const boost::asio::ip::address remote_address_;
    const std::uint16_t remote_port_;

    tls::tls_socket_wrapper<boost::asio::ip::udp, true> tls_socket_;
};

}  // namespace vsomeip

#endif  // VSOMEIP_INTERNAL_DTLS_CLIENT_IMPL_HPP

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_LOCAL_CLIENT_ENDPOINT_IMPL_HPP
#define VSOMEIP_LOCAL_CLIENT_ENDPOINT_IMPL_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#ifdef _WIN32
#include <boost/asio/ip/tcp.hpp>
#endif

#include <vsomeip/defines.hpp>

#include "client_endpoint_impl.hpp"

namespace vsomeip {

#ifdef _WIN32
typedef client_endpoint_impl<
            boost::asio::ip::tcp
        > local_client_endpoint_base_impl;
#else
typedef client_endpoint_impl<
            boost::asio::local::stream_protocol
        > local_client_endpoint_base_impl;
#endif

class local_client_endpoint_impl: public local_client_endpoint_base_impl {
public:
    local_client_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                               endpoint_type _remote,
                               boost::asio::io_service &_io,
                               std::uint32_t _max_message_size,
                               configuration::endpoint_queue_limit_t _queue_limit);

    virtual ~local_client_endpoint_impl();

    void start();
    void stop();

    bool is_local() const;

    bool get_remote_address(boost::asio::ip::address &_address) const;
    std::uint16_t get_remote_port() const;

    void restart(bool _force);
    void print_status();

    bool send(const std::vector<byte_t>& _cmd_header, const byte_t *_data,
              uint32_t _size, bool _flush = true);
private:
    void send_queued();

    void send_magic_cookie();

    void connect();
    void receive();
    void receive_cbk(boost::system::error_code const &_error,
                     std::size_t _bytes);
    void set_local_port();
    std::string get_remote_information() const;

    message_buffer_t recv_buffer_;
};

} // namespace vsomeip

#endif // VSOMEIP_LOCAL_CLIENT_ENDPOINT_IMPL_HPP

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP
#define VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP

#include <boost/asio/ip/tcp.hpp>
#include <chrono>

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
                             std::uint32_t _max_message_size,
                             std::uint32_t buffer_shrink_threshold,
                             std::chrono::milliseconds _send_timeout,
                             configuration::endpoint_queue_limit_t _queue_limit);
    virtual ~tcp_client_endpoint_impl();

    void start();
    void restart(bool _force);

    bool get_remote_address(boost::asio::ip::address &_address) const;
    std::uint16_t get_remote_port() const;
    bool is_reliable() const;
    bool is_local() const;
    void print_status();

    void send_cbk(boost::system::error_code const &_error, std::size_t _bytes,
                  message_buffer_ptr_t _sent_msg);
private:
    void send_queued();
    bool is_magic_cookie(const message_buffer_ptr_t& _recv_buffer,
                         size_t _offset) const;
    void send_magic_cookie(message_buffer_ptr_t &_buffer);

    void receive_cbk(boost::system::error_code const &_error,
                     std::size_t _bytes,
                     message_buffer_ptr_t  _recv_buffer,
                     std::size_t _recv_buffer_size);

    void connect();
    void receive();
    void receive(message_buffer_ptr_t  _recv_buffer,
                 std::size_t _recv_buffer_size,
                 std::size_t _missing_capacity);
    void calculate_shrink_count(const message_buffer_ptr_t& _recv_buffer,
                                std::size_t _recv_buffer_size);
    const std::string get_address_port_remote() const;
    const std::string get_address_port_local() const;
    void handle_recv_buffer_exception(const std::exception &_e,
                                      const message_buffer_ptr_t& _recv_buffer,
                                      std::size_t _recv_buffer_size);
    void set_local_port();
    std::size_t write_completion_condition(
            const boost::system::error_code& _error,
            std::size_t _bytes_transferred, std::size_t _bytes_to_send,
            service_t _service, method_t _method, client_t _client, session_t _session,
            std::chrono::steady_clock::time_point _start);
    std::string get_remote_information() const;

    const std::uint32_t recv_buffer_size_initial_;
    message_buffer_ptr_t recv_buffer_;
    std::uint32_t shrink_count_;
    const std::uint32_t buffer_shrink_threshold_;

    const boost::asio::ip::address remote_address_;
    const std::uint16_t remote_port_;
    std::chrono::steady_clock::time_point last_cookie_sent_;
    const std::chrono::milliseconds send_timeout_;
    const std::chrono::milliseconds send_timeout_warning_;
};

} // namespace vsomeip

#endif // VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP

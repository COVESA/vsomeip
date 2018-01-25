// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TCP_SERVER_ENDPOINT_IMPL_HPP
#define VSOMEIP_TCP_SERVER_ENDPOINT_IMPL_HPP

#include <map>
#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/export.hpp>
#include "server_endpoint_impl.hpp"

#include <chrono>

namespace vsomeip {

typedef server_endpoint_impl<
            boost::asio::ip::tcp
        > tcp_server_endpoint_base_impl;

class tcp_server_endpoint_impl: public tcp_server_endpoint_base_impl {

public:
    tcp_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
                             boost::asio::io_service &_io,
                             std::uint32_t _max_message_size,
                             std::uint32_t _buffer_shrink_threshold,
                             std::chrono::milliseconds _send_timeout,
                             configuration::endpoint_queue_limit_t _queue_limit);
    virtual ~tcp_server_endpoint_impl();

    void start();
    void stop();

    bool send_to(const std::shared_ptr<endpoint_definition> _target,
                 const byte_t *_data, uint32_t _size, bool _flush);
    void send_queued(const queue_iterator_type _queue_iterator);

    VSOMEIP_EXPORT bool is_established(std::shared_ptr<endpoint_definition> _endpoint);

    bool get_default_target(service_t, endpoint_type &) const;

    std::uint16_t get_local_port() const;
    bool is_reliable() const;
    bool is_local() const;

    client_t get_client(std::shared_ptr<endpoint_definition> _endpoint);

    // dummies to implement endpoint_impl interface
    // TODO: think about a better design!
    void receive();
    void print_status();
private:
    class connection: public std::enable_shared_from_this<connection> {

    public:
        typedef std::shared_ptr<connection> ptr;

        static ptr create(std::weak_ptr<tcp_server_endpoint_impl> _server,
                          std::uint32_t _max_message_size,
                          std::uint32_t _buffer_shrink_threshold,
                          bool _magic_cookies_enabled,
                          boost::asio::io_service & _io_service,
                          std::chrono::milliseconds _send_timeout);
        socket_type & get_socket();
        std::unique_lock<std::mutex> get_socket_lock();

        void start();
        void stop();
        void receive();

        void send_queued(const queue_iterator_type _queue_iterator);

        void set_remote_info(const endpoint_type &_remote);
        const std::string get_address_port_remote() const;
        std::size_t get_recv_buffer_capacity() const;

    private:
        connection(std::weak_ptr<tcp_server_endpoint_impl> _server,
                   std::uint32_t _max_message_size,
                   std::uint32_t _recv_buffer_size_initial,
                   std::uint32_t _buffer_shrink_threshold,
                   bool _magic_cookies_enabled,
                   boost::asio::io_service & _io_service,
                   std::chrono::milliseconds _send_timeout);
        bool send_magic_cookie(message_buffer_ptr_t &_buffer);
        bool is_magic_cookie(size_t _offset) const;
        void receive_cbk(boost::system::error_code const &_error,
                         std::size_t _bytes);
        void calculate_shrink_count();
        const std::string get_address_port_local() const;
        void handle_recv_buffer_exception(const std::exception &_e);
        std::size_t write_completion_condition(
                const boost::system::error_code& _error,
                std::size_t _bytes_transferred, std::size_t _bytes_to_send,
                service_t _service, method_t _method, client_t _client, session_t _session,
                std::chrono::steady_clock::time_point _start);
        void stop_and_remove_connection();

        std::mutex socket_mutex_;
        tcp_server_endpoint_impl::socket_type socket_;
        std::weak_ptr<tcp_server_endpoint_impl> server_;

        const uint32_t max_message_size_;
        const uint32_t recv_buffer_size_initial_;

        message_buffer_t recv_buffer_;
        size_t recv_buffer_size_;
        std::uint32_t missing_capacity_;
        std::uint32_t shrink_count_;
        const std::uint32_t buffer_shrink_threshold_;

        endpoint_type remote_;
        boost::asio::ip::address remote_address_;
        std::uint16_t remote_port_;
        std::atomic<bool> magic_cookies_enabled_;
        std::chrono::steady_clock::time_point last_cookie_sent_;
        const std::chrono::milliseconds send_timeout_;
        const std::chrono::milliseconds send_timeout_warning_;
    };

    std::mutex acceptor_mutex_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::mutex connections_mutex_;
    typedef std::map<endpoint_type, connection::ptr> connections_t;
    connections_t connections_;
    const std::uint32_t buffer_shrink_threshold_;
    const std::uint16_t local_port_;
    const std::chrono::milliseconds send_timeout_;

private:
    void remove_connection(connection *_connection);
    void accept_cbk(connection::ptr _connection,
                    boost::system::error_code const &_error);
    std::string get_remote_information(
            const queue_iterator_type _queue_iterator) const;
};

} // namespace vsomeip

#endif // VSOMEIP_TCP_SERVER_ENDPOINT_IMPL_HPP

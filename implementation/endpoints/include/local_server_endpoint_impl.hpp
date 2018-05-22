// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_LOCAL_SERVER_ENDPOINT_IMPL_HPP
#define VSOMEIP_LOCAL_SERVER_ENDPOINT_IMPL_HPP

#include <map>
#include <thread>
#include <condition_variable>
#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#ifdef _WIN32
#include <boost/asio/ip/tcp.hpp>
#endif

#include <vsomeip/defines.hpp>

#include "buffer.hpp"
#include "server_endpoint_impl.hpp"

namespace vsomeip {

#ifdef _WIN32
typedef server_endpoint_impl<
            boost::asio::ip::tcp
        > local_server_endpoint_base_impl;
#else
typedef server_endpoint_impl<
            boost::asio::local::stream_protocol
        > local_server_endpoint_base_impl;
#endif

class local_server_endpoint_impl: public local_server_endpoint_base_impl {

public:
    local_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                               endpoint_type _local,
                               boost::asio::io_service &_io,
                               std::uint32_t _max_message_size,
                               std::uint32_t _buffer_shrink_threshold,
                               configuration::endpoint_queue_limit_t _queue_limit);

    local_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                               endpoint_type _local,
                               boost::asio::io_service &_io,
                               std::uint32_t _max_message_size,
                               int native_socket,
                               std::uint32_t _buffer_shrink_threshold,
                               configuration::endpoint_queue_limit_t _queue_limit);

    virtual ~local_server_endpoint_impl();

    void start();
    void stop();

    void receive();

    bool send_to(const std::shared_ptr<endpoint_definition>,
                 const byte_t *_data, uint32_t _size, bool _flush);
    void send_queued(const queue_iterator_type _queue_iterator);

    bool get_default_target(service_t, endpoint_type &) const;

    bool is_local() const;

    void accept_client_func();
    void print_status();

private:
    class connection: public std::enable_shared_from_this<connection> {

    public:
        typedef std::shared_ptr<connection> ptr;

        static ptr create(std::weak_ptr<local_server_endpoint_impl> _server,
                          std::uint32_t _max_message_size,
                          std::uint32_t _buffer_shrink_threshold,
                          boost::asio::io_service &_io_service);
        socket_type & get_socket();
        std::unique_lock<std::mutex> get_socket_lock();

        void start();
        void stop();

        void send_queued(const queue_iterator_type _queue_iterator);

        void set_bound_client(client_t _client);
        std::size_t get_recv_buffer_capacity() const;

    private:
        connection(std::weak_ptr<local_server_endpoint_impl> _server,
                   std::uint32_t _recv_buffer_size_initial,
                   std::uint32_t _max_message_size,
                   std::uint32_t _buffer_shrink_threshold,
                   boost::asio::io_service &_io_service);

        void receive_cbk(boost::system::error_code const &_error,
                         std::size_t _bytes);
        void calculate_shrink_count();
        const std::string get_path_local() const;
        const std::string get_path_remote() const;
        void handle_recv_buffer_exception(const std::exception &_e);

        std::mutex socket_mutex_;
        local_server_endpoint_impl::socket_type socket_;
        std::weak_ptr<local_server_endpoint_impl> server_;

        const std::uint32_t recv_buffer_size_initial_;
        const std::uint32_t max_message_size_;

        message_buffer_t recv_buffer_;
        size_t recv_buffer_size_;
        std::uint32_t missing_capacity_;
        std::uint32_t shrink_count_;
        const std::uint32_t buffer_shrink_threshold_;

        client_t bound_client_;

    };

    std::mutex acceptor_mutex_;
#ifdef _WIN32
    boost::asio::ip::tcp::acceptor acceptor_;
#else
    boost::asio::local::stream_protocol::acceptor acceptor_;
#endif

    typedef std::map<endpoint_type, connection::ptr> connections_t;
    std::mutex connections_mutex_;
    connections_t connections_;
    const std::uint32_t buffer_shrink_threshold_;

private:
    void remove_connection(connection *_connection);
    void accept_cbk(connection::ptr _connection,
                    boost::system::error_code const &_error);
    std::string get_remote_information(
            const queue_iterator_type _queue_iterator) const;
};

} // namespace vsomeip

#endif // VSOMEIP_LOCAL_SERVER_ENDPOINT_IMPL_HPP

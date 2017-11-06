// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TLS_SERVER_ENDPOINT_IMPL_HPP
#define VSOMEIP_TLS_SERVER_ENDPOINT_IMPL_HPP

#include <map>

#include <boost/asio/ip/tcp.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/export.hpp>
#include "server_endpoint_impl.hpp"
#include "secure_endpoint_base.hpp"

namespace vsomeip {

typedef server_endpoint_impl<
            boost::asio::ip::tcp
        > tcp_server_endpoint_base_impl;

class tls_server_endpoint_impl: public tcp_server_endpoint_base_impl,
                                public secure_endpoint_base {
public:
    tls_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
                             boost::asio::io_service &_io,
                             std::uint32_t _max_message_size,
                             std::uint32_t _buffer_shrink_threshold,
                             bool _confidential,
                             const std::vector<std::uint8_t> &_psk,
                             const std::string &_pskid);
    virtual ~tls_server_endpoint_impl();

    void start();
    void stop();

    void stop_all_connections(const boost::asio::ip::address &_address);

    bool send_to(const std::shared_ptr<endpoint_definition> _target,
                 const byte_t *_data, uint32_t _size, bool _flush);
    void send_queued(queue_iterator_type _queue_iterator);

    VSOMEIP_EXPORT bool is_established(std::shared_ptr<endpoint_definition> _endpoint);

    bool get_default_target(service_t, endpoint_type &) const;

    unsigned short get_local_port() const;
    bool is_reliable() const;
    bool is_local() const;

    client_t get_client(std::shared_ptr<endpoint_definition> _endpoint);

    // dummies to implement endpoint_impl interface
    // TODO: think about a better design!
    void receive();
    void restart();

private:
    class connection: public boost::enable_shared_from_this<connection> {

    public:
        typedef boost::shared_ptr<connection> ptr;

        static ptr create(std::weak_ptr<tls_server_endpoint_impl> _server,
                          std::uint32_t _max_message_size,
                          std::uint32_t _buffer_shrink_threshold);
        socket_type & get_socket();
        std::unique_lock<std::mutex> get_socket_lock();

        void start();
        void stop();
        void receive();

        client_t get_client(endpoint_type _endpoint_type);

        void send_queued(queue_iterator_type _queue_iterator);

        void set_remote_info(const endpoint_type &_remote);

    private:
        connection(std::weak_ptr<tls_server_endpoint_impl> _server,
                   std::uint32_t _max_message_size,
                   std::uint32_t _recv_buffer_size_initial,
                   std::uint32_t _buffer_shrink_threshold);
        void send_magic_cookie(message_buffer_ptr_t &_buffer);
        bool is_magic_cookie(size_t _offset) const;
        void receive_cbk(boost::system::error_code const &_error,
                         std::size_t _bytes);
        void calculate_shrink_count();

        std::mutex socket_mutex_;
        tls_server_endpoint_impl::socket_type socket_;
        std::weak_ptr<tls_server_endpoint_impl> server_;

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
    };

    std::mutex acceptor_mutex_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::mutex connections_mutex_;
    std::map<endpoint_type, connection::ptr> connections_;
    const std::uint32_t buffer_shrink_threshold_;
    const std::uint16_t local_port_;

private:
    void remove_connection(connection *_connection);
    void accept_cbk(connection::ptr _connection,
                    boost::system::error_code const &_error);
};

} // namespace vsomeip

#endif // VSOMEIP_TLS_SERVER_ENDPOINT_IMPL_HPP

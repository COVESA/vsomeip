// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TCP_SERVER_ENDPOINT_IMPL_HPP
#define VSOMEIP_TCP_SERVER_ENDPOINT_IMPL_HPP

#include <map>

#include <boost/asio/ip/tcp.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <vsomeip/defines.hpp>
#include "server_endpoint_impl.hpp"

namespace vsomeip {

typedef server_endpoint_impl<boost::asio::ip::tcp,
        VSOMEIP_MAX_TCP_MESSAGE_SIZE> tcp_server_endpoint_base_impl;

class tcp_server_endpoint_impl: public tcp_server_endpoint_base_impl {

public:
    tcp_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
                             boost::asio::io_service &_io);
    virtual ~tcp_server_endpoint_impl();

    void start();
    void stop();

    bool send_to(const std::shared_ptr<endpoint_definition> _target,
                 const byte_t *_data, uint32_t _size, bool _flush);
    void send_queued(endpoint_type _target, message_buffer_ptr_t _buffer);

    endpoint_type get_remote() const;
    bool get_multicast(service_t, event_t, endpoint_type &) const;

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

        static ptr create(tcp_server_endpoint_impl *_server);
        socket_type & get_socket();

        void start();
        void stop();

        client_t get_client(endpoint_type _endpoint_type);

        void send_queued(message_buffer_ptr_t _buffer);

    private:
        connection(tcp_server_endpoint_impl *_owner);
        void send_magic_cookie(message_buffer_ptr_t &_buffer);

        tcp_server_endpoint_impl::socket_type socket_;
        tcp_server_endpoint_impl *server_;

        message_buffer_t message_;

    private:
        bool is_magic_cookie() const;
        void receive_cbk(packet_buffer_ptr_t _buffer,
                         boost::system::error_code const &_error,
                         std::size_t _bytes);
    };

    boost::asio::ip::tcp::acceptor acceptor_;
    std::map<endpoint_type, connection::ptr> connections_;
    connection *current_;

private:
    void accept_cbk(connection::ptr _connection,
                    boost::system::error_code const &_error);
};

} // namespace vsomeip

#endif // VSOMEIP_TCP_SERVER_ENDPOINT_IMPL_HPP

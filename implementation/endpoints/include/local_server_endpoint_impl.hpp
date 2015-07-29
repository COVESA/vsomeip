// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_LOCAL_SERVER_ENDPOINT_IMPL_HPP
#define VSOMEIP_LOCAL_SERVER_ENDPOINT_IMPL_HPP

#include <map>

#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/enable_shared_from_this.hpp>

#ifdef WIN32
#include <boost/asio/ip/tcp.hpp>
#endif

#include <vsomeip/defines.hpp>

#include "buffer.hpp"
#include "server_endpoint_impl.hpp"

namespace vsomeip {

#ifdef WIN32
typedef server_endpoint_impl<boost::asio::ip::tcp,
        VSOMEIP_MAX_TCP_MESSAGE_SIZE > local_server_endpoint_base_impl;
#else
typedef server_endpoint_impl<boost::asio::local::stream_protocol,
        VSOMEIP_MAX_LOCAL_MESSAGE_SIZE> local_server_endpoint_base_impl;
#endif

class local_server_endpoint_impl: public local_server_endpoint_base_impl {

public:
    local_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                               endpoint_type _local,
                               boost::asio::io_service &_io);
    virtual ~local_server_endpoint_impl();

    void start();
    void stop();

    void restart();
    void receive();

    bool send_to(const std::shared_ptr<endpoint_definition>,
                 const byte_t *_data, uint32_t _size, bool _flush);
    void send_queued(endpoint_type _target, message_buffer_ptr_t _data);

    endpoint_type get_remote() const;
    bool get_multicast(service_t, event_t, endpoint_type &) const;

    bool is_local() const;

private:
    class connection: public boost::enable_shared_from_this<connection> {

    public:
        typedef boost::shared_ptr<connection> ptr;

        static ptr create(local_server_endpoint_impl *_server);
        socket_type & get_socket();

        void start();

        void send_queued(message_buffer_ptr_t _data);

    private:
        connection(local_server_endpoint_impl *_owner);

        void send_magic_cookie();

        local_server_endpoint_impl::socket_type socket_;
        local_server_endpoint_impl *server_;

        // the current message
        message_buffer_t message_;

    private:
        void receive_cbk(packet_buffer_ptr_t _buffer,
                         boost::system::error_code const &_error,
                         std::size_t _bytes);
    };

#ifdef WIN32
    boost::asio::ip::tcp::acceptor acceptor_;
#else
    boost::asio::local::stream_protocol::acceptor acceptor_;
#endif

    std::map<endpoint_type, connection::ptr> connections_;
    connection *current_;

private:
    void remove_connection(connection *_connection);
    void accept_cbk(connection::ptr _connection,
                    boost::system::error_code const &_error);
};

} // namespace vsomeip

#endif // VSOMEIP_LOCAL_SERVER_ENDPOINT_IMPL_HPP

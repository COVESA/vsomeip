// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_SERVER_ENDPOINT_IMPL_HPP_
#define VSOMEIP_V3_LOCAL_SERVER_ENDPOINT_IMPL_HPP_

#include <map>
#include <thread>
#include <condition_variable>
#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol_ext.hpp>


#ifdef _WIN32
#include <boost/asio/ip/tcp.hpp>
#endif

#include <vsomeip/defines.hpp>

#include "buffer.hpp"
#include "server_endpoint_impl.hpp"

namespace vsomeip_v3 {

#ifdef _WIN32
typedef server_endpoint_impl<
            boost::asio::ip::tcp
        > local_server_endpoint_base_impl;
#else
typedef server_endpoint_impl<
            boost::asio::local::stream_protocol_ext
        > local_server_endpoint_base_impl;
#endif

class local_server_endpoint_impl: public local_server_endpoint_base_impl {

public:
    local_server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                               const std::shared_ptr<routing_host>& _routing_host,
                               const endpoint_type& _local,
                               boost::asio::io_service &_io,
                               const std::shared_ptr<configuration>& _configuration,
                               bool _is_routing_endpoint);

    local_server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                               const std::shared_ptr<routing_host>& _routing_host,
                               const endpoint_type& _local,
                               boost::asio::io_service &_io,
                               int native_socket,
                               const std::shared_ptr<configuration>& _configuration,
                               bool _is_routing_endpoint);

    virtual ~local_server_endpoint_impl();

    void start();
    void stop();

    void receive();

    // this overrides server_endpoint_impl::send to disable the nPDU feature
    // for local communication
    bool send(const uint8_t *_data, uint32_t _size);
    bool send_to(const std::shared_ptr<endpoint_definition>,
                 const byte_t *_data, uint32_t _size);
    bool send_error(const std::shared_ptr<endpoint_definition> _target,
                const byte_t *_data, uint32_t _size);
    void send_queued(const queue_iterator_type _queue_iterator);
    void get_configured_times_from_endpoint(
            service_t _service, method_t _method,
            std::chrono::nanoseconds *_debouncing,
            std::chrono::nanoseconds *_maximum_retention) const;

    bool get_default_target(service_t, endpoint_type &) const;

    bool is_local() const;

    void accept_client_func();
    void print_status();

    bool is_reliable() const;
    std::uint16_t get_local_port() const;

    client_t assign_client(const byte_t *_data, uint32_t _size);

private:
    class connection: public std::enable_shared_from_this<connection> {

    public:
        typedef std::shared_ptr<connection> ptr;

        static ptr create(const std::shared_ptr<local_server_endpoint_impl>& _server,
                          std::uint32_t _max_message_size,
                          std::uint32_t _buffer_shrink_threshold,
                          boost::asio::io_service &_io_service);
        socket_type & get_socket();
        std::unique_lock<std::mutex> get_socket_lock();

        void start();
        void stop();


        void send_queued(const message_buffer_ptr_t& _buffer);

        void set_bound_client(client_t _client);
        client_t get_bound_client() const;
#ifndef _WIN32
        void set_bound_uid_gid(uid_t _uid, gid_t _gid);
#endif

        std::size_t get_recv_buffer_capacity() const;

    private:
        connection(const std::shared_ptr<local_server_endpoint_impl>& _server,
                   std::uint32_t _max_message_size,
                   std::uint32_t _initial_recv_buffer_size,
                   std::uint32_t _buffer_shrink_threshold,
                   boost::asio::io_service &_io_service);

        void send_cbk(const message_buffer_ptr_t _buffer,
                boost::system::error_code const &_error, std::size_t _bytes);
        void receive_cbk(boost::system::error_code const &_error,
                         std::size_t _bytes
#ifndef _WIN32
                         , std::uint32_t const &_uid, std::uint32_t const &_gid
#endif
        );
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
#ifndef _WIN32
        uid_t bound_uid_;
        gid_t bound_gid_;
#endif
        bool assigned_client_;
    };

    std::mutex acceptor_mutex_;
#ifdef _WIN32
    boost::asio::ip::tcp::acceptor acceptor_;
#else
    boost::asio::local::stream_protocol_ext::acceptor acceptor_;
#endif

    typedef std::map<client_t, connection::ptr> connections_t;
    std::mutex connections_mutex_;
    connections_t connections_;

    const std::uint32_t buffer_shrink_threshold_;

    const bool is_routing_endpoint_;

private:
    bool add_connection(const client_t &_client,
            const std::shared_ptr<connection> &_connection);
    void remove_connection(const client_t &_client);
    void accept_cbk(const connection::ptr& _connection,
                    boost::system::error_code const &_error);
    std::string get_remote_information(
            const queue_iterator_type _queue_iterator) const;
    std::string get_remote_information(
            const endpoint_type& _remote) const;

    bool check_packetizer_space(queue_iterator_type _queue_iterator,
                                message_buffer_ptr_t* _packetizer,
                                std::uint32_t _size);
    bool tp_segmentation_enabled(service_t _service, method_t _method) const;
    void send_client_identifier(const client_t &_client);
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_LOCAL_SERVER_ENDPOINT_IMPL_HPP_

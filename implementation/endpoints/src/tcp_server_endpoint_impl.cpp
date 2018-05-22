// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <boost/asio/write.hpp>

#include <vsomeip/constants.hpp>

#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/tcp_server_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/utility.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../configuration/include/internal.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_server_endpoint_impl::tcp_server_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _local,
        boost::asio::io_service &_io, std::uint32_t _max_message_size,
        std::uint32_t _buffer_shrink_threshold,
        std::chrono::milliseconds _send_timeout,
        configuration::endpoint_queue_limit_t _queue_limit)
    : tcp_server_endpoint_base_impl(_host, _local, _io,
                                    _max_message_size, _queue_limit),
        acceptor_(_io),
        buffer_shrink_threshold_(_buffer_shrink_threshold),
        local_port_(_local.port()),
        send_timeout_(_send_timeout) {
    is_supporting_magic_cookies_ = true;

    boost::system::error_code ec;
    acceptor_.open(_local.protocol(), ec);
    boost::asio::detail::throw_error(ec, "acceptor open");
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    boost::asio::detail::throw_error(ec, "acceptor set_option");
    acceptor_.bind(_local, ec);
    boost::asio::detail::throw_error(ec, "acceptor bind");
    acceptor_.listen(boost::asio::socket_base::max_connections, ec);
    boost::asio::detail::throw_error(ec, "acceptor listen");
}

tcp_server_endpoint_impl::~tcp_server_endpoint_impl() {
}

bool tcp_server_endpoint_impl::is_local() const {
    return false;
}

void tcp_server_endpoint_impl::start() {
    std::lock_guard<std::mutex> its_lock(acceptor_mutex_);
    if (acceptor_.is_open()) {
        connection::ptr new_connection = connection::create(
                std::dynamic_pointer_cast<tcp_server_endpoint_impl>(
                        shared_from_this()), max_message_size_,
                        buffer_shrink_threshold_, has_enabled_magic_cookies_,
                        service_, send_timeout_);

        {
            std::unique_lock<std::mutex> its_socket_lock(new_connection->get_socket_lock());
            acceptor_.async_accept(new_connection->get_socket(),
                    std::bind(&tcp_server_endpoint_impl::accept_cbk,
                            std::dynamic_pointer_cast<tcp_server_endpoint_impl>(
                                    shared_from_this()), new_connection,
                            std::placeholders::_1));
        }
    }
}

void tcp_server_endpoint_impl::stop() {
    server_endpoint_impl::stop();
    {
        std::lock_guard<std::mutex> its_lock(acceptor_mutex_);
        if(acceptor_.is_open()) {
            boost::system::error_code its_error;
            acceptor_.close(its_error);
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        for (const auto &c : connections_) {
            c.second->stop();
        }
        connections_.clear();
    }
}

bool tcp_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data,
        uint32_t _size, bool _flush) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());
    return send_intern(its_target, _data, _size, _flush);
}

void tcp_server_endpoint_impl::send_queued(const queue_iterator_type _queue_iterator) {
    connection::ptr its_connection;
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        auto connection_iterator = connections_.find(_queue_iterator->first);
        if (connection_iterator != connections_.end()) {
            its_connection = connection_iterator->second;
        } else {
            VSOMEIP_INFO << "Didn't find connection: "
                    << _queue_iterator->first.address().to_string() << ":" << std::dec
                    << static_cast<std::uint16_t>(_queue_iterator->first.port())
                    << " dropping outstanding messages (" << std::dec
                    << _queue_iterator->second.second.size() << ").";
            queues_.erase(_queue_iterator->first);
        }
    }
    if (its_connection) {
        its_connection->send_queued(_queue_iterator);
    }
}

bool tcp_server_endpoint_impl::is_established(std::shared_ptr<endpoint_definition> _endpoint) {
    bool is_connected = false;
    endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        auto connection_iterator = connections_.find(endpoint);
        if (connection_iterator != connections_.end()) {
            is_connected = true;
        } else {
            VSOMEIP_INFO << "Didn't find TCP connection: Subscription "
                    << "rejected for: " << endpoint.address().to_string() << ":"
                    << std::dec << static_cast<std::uint16_t>(endpoint.port());
        }
    }
    return is_connected;
}

bool tcp_server_endpoint_impl::get_default_target(service_t,
        tcp_server_endpoint_impl::endpoint_type &) const {
    return false;
}

void tcp_server_endpoint_impl::remove_connection(
        tcp_server_endpoint_impl::connection *_connection) {
    std::lock_guard<std::mutex> its_lock(connections_mutex_);
    for (auto it = connections_.begin(); it != connections_.end();) {
        if (it->second.get() == _connection) {
            it = connections_.erase(it);
            break;
        } else {
            ++it;
        }
    }
}

void tcp_server_endpoint_impl::accept_cbk(connection::ptr _connection,
        boost::system::error_code const &_error) {

    if (!_error) {
        boost::system::error_code its_error;
        endpoint_type remote;
        {
            std::unique_lock<std::mutex> its_socket_lock(_connection->get_socket_lock());
            socket_type &new_connection_socket = _connection->get_socket();
            remote = new_connection_socket.remote_endpoint(its_error);
            _connection->set_remote_info(remote);
            // Nagle algorithm off
            new_connection_socket.set_option(ip::tcp::no_delay(true), its_error);

            new_connection_socket.set_option(boost::asio::socket_base::keep_alive(true), its_error);
            if (its_error) {
                VSOMEIP_WARNING << "tcp_server_endpoint::connect: couldn't enable "
                        << "keep_alive: " << its_error.message();
            }
        }
        if (!its_error) {
            {
                std::lock_guard<std::mutex> its_lock(connections_mutex_);
                connections_[remote] = _connection;
            }
            _connection->start();
        }
    }
    if (_error != boost::asio::error::bad_descriptor
            && _error != boost::asio::error::operation_aborted
            && _error != boost::asio::error::no_descriptors) {
        start();
    }
}

std::uint16_t tcp_server_endpoint_impl::get_local_port() const {
    return local_port_;
}

bool tcp_server_endpoint_impl::is_reliable() const {
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// class tcp_service_impl::connection
///////////////////////////////////////////////////////////////////////////////
tcp_server_endpoint_impl::connection::connection(
        std::weak_ptr<tcp_server_endpoint_impl> _server,
        std::uint32_t _max_message_size,
        std::uint32_t _initial_recv_buffer_size,
        std::uint32_t _buffer_shrink_threshold,
        bool _magic_cookies_enabled,
        boost::asio::io_service &_io_service,
        std::chrono::milliseconds _send_timeout) :
        socket_(_io_service),
        server_(_server),
        max_message_size_(_max_message_size),
        recv_buffer_size_initial_(_initial_recv_buffer_size),
        recv_buffer_(_initial_recv_buffer_size, 0),
        recv_buffer_size_(0),
        missing_capacity_(0),
        shrink_count_(0),
        buffer_shrink_threshold_(_buffer_shrink_threshold),
        remote_port_(0),
        magic_cookies_enabled_(_magic_cookies_enabled),
        last_cookie_sent_(std::chrono::steady_clock::now() - std::chrono::seconds(11)),
        send_timeout_(_send_timeout),
        send_timeout_warning_(_send_timeout / 2) {
}

tcp_server_endpoint_impl::connection::ptr
tcp_server_endpoint_impl::connection::create(
        std::weak_ptr<tcp_server_endpoint_impl> _server,
        std::uint32_t _max_message_size,
        std::uint32_t _buffer_shrink_threshold,
        bool _magic_cookies_enabled,
        boost::asio::io_service & _io_service,
        std::chrono::milliseconds _send_timeout) {
    const std::uint32_t its_initial_receveive_buffer_size =
            VSOMEIP_SOMEIP_HEADER_SIZE + 8 + MAGIC_COOKIE_SIZE + 8
                    + VSOMEIP_MAX_TCP_MESSAGE_SIZE;
    return ptr(new connection(_server, _max_message_size,
                    its_initial_receveive_buffer_size,
                    _buffer_shrink_threshold, _magic_cookies_enabled,
                    _io_service, _send_timeout));
}

tcp_server_endpoint_impl::socket_type &
tcp_server_endpoint_impl::connection::get_socket() {
    return socket_;
}

std::unique_lock<std::mutex>
tcp_server_endpoint_impl::connection::get_socket_lock() {
    return std::unique_lock<std::mutex>(socket_mutex_);
}

void tcp_server_endpoint_impl::connection::start() {
    receive();
}

void tcp_server_endpoint_impl::connection::receive() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if(socket_.is_open()) {
        const std::size_t its_capacity(recv_buffer_.capacity());
        size_t buffer_size = its_capacity - recv_buffer_size_;
        try {
            if (missing_capacity_) {
                if (missing_capacity_ > MESSAGE_SIZE_UNLIMITED) {
                    VSOMEIP_ERROR << "Missing receive buffer capacity exceeds allowed maximum!";
                    return;
                }
                const std::size_t its_required_capacity(recv_buffer_size_ + missing_capacity_);
                if (its_capacity < its_required_capacity) {
                    recv_buffer_.reserve(its_required_capacity);
                    recv_buffer_.resize(its_required_capacity, 0x0);
                }
                buffer_size = missing_capacity_;
                missing_capacity_ = 0;
            } else if (buffer_shrink_threshold_
                    && shrink_count_ > buffer_shrink_threshold_
                    && recv_buffer_size_ == 0) {
                recv_buffer_.resize(recv_buffer_size_initial_, 0x0);
                recv_buffer_.shrink_to_fit();
                buffer_size = recv_buffer_size_initial_;
                shrink_count_ = 0;
            }
        } catch (const std::exception &e) {
            handle_recv_buffer_exception(e);
            // don't start receiving again
            return;
        }
        socket_.async_receive(boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
                std::bind(&tcp_server_endpoint_impl::connection::receive_cbk,
                        shared_from_this(), std::placeholders::_1,
                        std::placeholders::_2));
    }
}

void tcp_server_endpoint_impl::connection::stop() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (socket_.is_open()) {
        boost::system::error_code its_error;
        socket_.shutdown(socket_.shutdown_both, its_error);
        socket_.close(its_error);
    }
}

void tcp_server_endpoint_impl::connection::send_queued(
        const queue_iterator_type _queue_iterator) {
    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "tcp_server_endpoint_impl::connection::send_queued "
                " couldn't lock server_";
        return;
    }
    message_buffer_ptr_t its_buffer = _queue_iterator->second.second.front();
    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
            (*its_buffer)[VSOMEIP_SERVICE_POS_MIN],
            (*its_buffer)[VSOMEIP_SERVICE_POS_MAX]);
    const method_t its_method = VSOMEIP_BYTES_TO_WORD(
            (*its_buffer)[VSOMEIP_METHOD_POS_MIN],
            (*its_buffer)[VSOMEIP_METHOD_POS_MAX]);
    const client_t its_client = VSOMEIP_BYTES_TO_WORD(
            (*its_buffer)[VSOMEIP_CLIENT_POS_MIN],
            (*its_buffer)[VSOMEIP_CLIENT_POS_MAX]);
    const session_t its_session = VSOMEIP_BYTES_TO_WORD(
            (*its_buffer)[VSOMEIP_SESSION_POS_MIN],
            (*its_buffer)[VSOMEIP_SESSION_POS_MAX]);
    if (magic_cookies_enabled_) {
        const std::chrono::steady_clock::time_point now =
                std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_cookie_sent_) > std::chrono::milliseconds(10000)) {
            if (send_magic_cookie(its_buffer)) {
                last_cookie_sent_ = now;
                _queue_iterator->second.first += sizeof(SERVICE_COOKIE);
            }
        }
    }

    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        boost::asio::async_write(socket_, boost::asio::buffer(*its_buffer),
                 std::bind(&tcp_server_endpoint_impl::connection::write_completion_condition,
                           shared_from_this(),
                           std::placeholders::_1,
                           std::placeholders::_2,
                           its_buffer->size(),
                           its_service, its_method, its_client, its_session,
                           std::chrono::steady_clock::now()),
                std::bind(&tcp_server_endpoint_base_impl::send_cbk,
                          its_server,
                          _queue_iterator,
                          std::placeholders::_1,
                          std::placeholders::_2));
    }
}

bool tcp_server_endpoint_impl::connection::send_magic_cookie(
        message_buffer_ptr_t &_buffer) {
    if (max_message_size_ == MESSAGE_SIZE_UNLIMITED
            || max_message_size_ - _buffer->size() >=
    VSOMEIP_SOMEIP_HEADER_SIZE + VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE) {
        _buffer->insert(_buffer->begin(), SERVICE_COOKIE,
                SERVICE_COOKIE + sizeof(SERVICE_COOKIE));
        return true;
    }
    return false;
}

bool tcp_server_endpoint_impl::connection::is_magic_cookie(size_t _offset) const {
    return (0 == std::memcmp(CLIENT_COOKIE, &recv_buffer_[_offset],
                             sizeof(CLIENT_COOKIE)));
}

void tcp_server_endpoint_impl::connection::receive_cbk(
        boost::system::error_code const &_error,
        std::size_t _bytes) {
    if (_error == boost::asio::error::operation_aborted) {
        // endpoint was stopped
        return;
    }
    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_ERROR << "tcp_server_endpoint_impl::connection::receive_cbk "
                " couldn't lock server_";
        return;
    }
#if 0
    std::stringstream msg;
    for (std::size_t i = 0; i < _bytes + recv_buffer_size_; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0')
                << (int) recv_buffer_[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    std::shared_ptr<endpoint_host> its_host = its_server->host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            if (recv_buffer_size_ + _bytes < recv_buffer_size_) {
                VSOMEIP_ERROR << "receive buffer overflow in tcp client endpoint ~> abort!";
                return;
            }
            recv_buffer_size_ += _bytes;

            size_t its_iteration_gap = 0;
            bool has_full_message;
            do {
                uint64_t read_message_size
                    = utility::get_message_size(&recv_buffer_[its_iteration_gap],
                            recv_buffer_size_);
                if (read_message_size > MESSAGE_SIZE_UNLIMITED) {
                    VSOMEIP_ERROR << "Message size exceeds allowed maximum!";
                    return;
                }
                uint32_t current_message_size = static_cast<uint32_t>(read_message_size);
                has_full_message = (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE
                                   && current_message_size <= recv_buffer_size_);
                if (has_full_message) {
                    bool needs_forwarding(true);
                    if (is_magic_cookie(its_iteration_gap)) {
                        magic_cookies_enabled_ = true;
                    } else {
                        if (magic_cookies_enabled_) {
                            uint32_t its_offset
                                = its_server->find_magic_cookie(&recv_buffer_[its_iteration_gap],
                                        recv_buffer_size_);
                            if (its_offset < current_message_size) {
                                VSOMEIP_ERROR << "Detected Magic Cookie within message data. Resyncing.";
                                if (!is_magic_cookie(its_iteration_gap)) {
                                    its_host->on_error(&recv_buffer_[its_iteration_gap],
                                            static_cast<length_t>(recv_buffer_size_),its_server.get(),
                                            remote_address_, remote_port_);
                                }
                                current_message_size = its_offset;
                                needs_forwarding = false;
                            }
                        }
                    }
                    if (needs_forwarding) {
                        if (utility::is_request(
                                recv_buffer_[its_iteration_gap
                                        + VSOMEIP_MESSAGE_TYPE_POS])) {
                            client_t its_client;
                            std::memcpy(&its_client,
                                &recv_buffer_[its_iteration_gap + VSOMEIP_CLIENT_POS_MIN],
                                sizeof(client_t));
                            session_t its_session;
                            std::memcpy(&its_session,
                                &recv_buffer_[its_iteration_gap + VSOMEIP_SESSION_POS_MIN],
                                sizeof(session_t));
                            its_server->clients_mutex_.lock();
                            its_server->clients_[its_client][its_session] = remote_;
                            its_server->endpoint_to_client_[remote_] = its_client;
                            its_server->clients_mutex_.unlock();
                        }
                        if (!magic_cookies_enabled_) {
                            its_host->on_message(&recv_buffer_[its_iteration_gap],
                                    current_message_size, its_server.get(),
                                    boost::asio::ip::address(),
                                    VSOMEIP_ROUTING_CLIENT, remote_address_,
                                    remote_port_);
                        } else {
                            // Only call on_message without a magic cookie in front of the buffer!
                            if (!is_magic_cookie(its_iteration_gap)) {
                                its_host->on_message(&recv_buffer_[its_iteration_gap],
                                        current_message_size, its_server.get(),
                                        boost::asio::ip::address(),
                                        VSOMEIP_ROUTING_CLIENT,
                                        remote_address_, remote_port_);
                            }
                        }
                    }
                    calculate_shrink_count();
                    missing_capacity_ = 0;
                    recv_buffer_size_ -= current_message_size;
                    its_iteration_gap += current_message_size;
                } else if (magic_cookies_enabled_ && recv_buffer_size_ > 0) {
                    uint32_t its_offset =
                            its_server->find_magic_cookie(&recv_buffer_[its_iteration_gap],
                                    recv_buffer_size_);
                    if (its_offset < recv_buffer_size_) {
                        VSOMEIP_ERROR << "Detected Magic Cookie within message data. Resyncing.";
                        if (!is_magic_cookie(its_iteration_gap)) {
                            its_host->on_error(&recv_buffer_[its_iteration_gap],
                                    static_cast<length_t>(recv_buffer_size_), its_server.get(),
                                    remote_address_, remote_port_);
                        }
                        recv_buffer_size_ -= its_offset;
                        its_iteration_gap += its_offset;
                        has_full_message = true; // trigger next loop
                        if (!is_magic_cookie(its_iteration_gap)) {
                            its_host->on_error(&recv_buffer_[its_iteration_gap],
                                    static_cast<length_t>(recv_buffer_size_), its_server.get(),
                                    remote_address_, remote_port_);
                        }
                    }
                }

                if (!has_full_message) {
                    if (max_message_size_ != MESSAGE_SIZE_UNLIMITED
                            && current_message_size > max_message_size_) {
                        std::lock_guard<std::mutex> its_lock(socket_mutex_);
                        recv_buffer_size_ = 0;
                        recv_buffer_.resize(recv_buffer_size_initial_, 0x0);
                        recv_buffer_.shrink_to_fit();
                        if (magic_cookies_enabled_) {
                            VSOMEIP_ERROR << "Received a TCP message which exceeds "
                                          << "maximum message size ("
                                          << std::dec << current_message_size
                                          << " > " << std::dec << max_message_size_
                                          << "). Magic Cookies are enabled: "
                                          << "Resetting receiver. local: "
                                          << get_address_port_local() << " remote: "
                                          << get_address_port_remote();
                        } else {
                            VSOMEIP_ERROR << "Received a TCP message which exceeds "
                                          << "maximum message size ("
                                          << std::dec << current_message_size
                                          << " > " << std::dec << max_message_size_
                                          << ") Magic cookies are disabled: "
                                          << "Connection will be disabled! local: "
                                          << get_address_port_local() << " remote: "
                                          << get_address_port_remote();
                            return;
                        }
                    } else if (current_message_size > recv_buffer_size_) {
                        missing_capacity_ = current_message_size
                                - static_cast<std::uint32_t>(recv_buffer_size_);
                    } else if (VSOMEIP_SOMEIP_HEADER_SIZE > recv_buffer_size_) {
                        missing_capacity_ = VSOMEIP_SOMEIP_HEADER_SIZE
                                - static_cast<std::uint32_t>(recv_buffer_size_);
                    } else {
                        {
                            std::lock_guard<std::mutex> its_lock(socket_mutex_);
                            VSOMEIP_ERROR << "tse::c<" << this
                                    << ">rcb: recv_buffer_size is: " << std::dec
                                    << recv_buffer_size_ << " but couldn't read "
                                    "out message_size. recv_buffer_capacity: "
                                    << recv_buffer_.capacity()
                                    << " its_iteration_gap: " << its_iteration_gap
                                    << "local: " << get_address_port_local()
                                    << " remote: " << get_address_port_remote()
                                    << ". Closing connection due to missing/broken data TCP stream.";
                        }
                        {
                            std::lock_guard<std::mutex> its_lock(its_server->connections_mutex_);
                            stop();
                        }
                        its_server->remove_connection(this);
                        return;
                    }
                }
            } while (has_full_message && recv_buffer_size_);
            if (its_iteration_gap) {
                // Copy incomplete message to front for next receive_cbk iteration
                for (size_t i = 0; i < recv_buffer_size_; ++i) {
                    recv_buffer_[i] = recv_buffer_[i + its_iteration_gap];
                }
                // Still more capacity needed after shifting everything to front?
                if (missing_capacity_ &&
                        missing_capacity_ <= recv_buffer_.capacity() - recv_buffer_size_) {
                    missing_capacity_ = 0;
                }
            }
            receive();
        }
    }
    if (_error == boost::asio::error::eof
            || _error == boost::asio::error::connection_reset
            || _error == boost::asio::error::timed_out) {
        if(_error == boost::asio::error::timed_out) {
            std::lock_guard<std::mutex> its_lock(socket_mutex_);
            VSOMEIP_WARNING << "tcp_server_endpoint receive_cbk: " << _error.message()
                    << " local: " << get_address_port_local()
                    << " remote: " << get_address_port_remote();
        }
        {
            std::lock_guard<std::mutex> its_lock(its_server->connections_mutex_);
            stop();
        }
        its_server->remove_connection(this);
    }
}

void tcp_server_endpoint_impl::connection::calculate_shrink_count() {
    if (buffer_shrink_threshold_) {
        if (recv_buffer_.capacity() != recv_buffer_size_initial_) {
            if (recv_buffer_size_ < (recv_buffer_.capacity() >> 1)) {
                shrink_count_++;
            } else {
                shrink_count_ = 0;
            }
        }
    }
}

client_t tcp_server_endpoint_impl::get_client(std::shared_ptr<endpoint_definition> _endpoint) {
    const endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    std::lock_guard<std::mutex> its_lock(clients_mutex_);
    auto found_endpoint = endpoint_to_client_.find(endpoint);
    if (found_endpoint != endpoint_to_client_.end()) {
        // TODO: Check system byte order before convert!
        const client_t client = client_t(found_endpoint->second << 8 | found_endpoint->second >> 8);
        return client;
    }
    return 0;
}

void tcp_server_endpoint_impl::connection::set_remote_info(
        const endpoint_type &_remote) {
    remote_ = _remote;
    remote_address_ = _remote.address();
    remote_port_ = _remote.port();
}

const std::string tcp_server_endpoint_impl::connection::get_address_port_remote() const {
    std::string its_address_port;
    its_address_port.reserve(21);
    boost::system::error_code ec;
    its_address_port += remote_address_.to_string(ec);
    its_address_port += ":";
    its_address_port += std::to_string(remote_port_);
    return its_address_port;
}

const std::string tcp_server_endpoint_impl::connection::get_address_port_local() const {
    std::string its_address_port;
    its_address_port.reserve(21);
    boost::system::error_code ec;
    if (socket_.is_open()) {
        endpoint_type its_local_endpoint = socket_.local_endpoint(ec);
        if (!ec) {
            its_address_port += its_local_endpoint.address().to_string(ec);
            its_address_port += ":";
            its_address_port += std::to_string(its_local_endpoint.port());
        }
    }
    return its_address_port;
}

void tcp_server_endpoint_impl::connection::handle_recv_buffer_exception(
        const std::exception &_e) {
    std::stringstream its_message;
    its_message <<"tcp_server_endpoint_impl::connection catched exception"
            << _e.what() << " local: " << get_address_port_local()
            << " remote: " << get_address_port_remote()
            << " shutting down connection. Start of buffer: ";

    for (std::size_t i = 0; i < recv_buffer_size_ && i < 16; i++) {
        its_message << std::setw(2) << std::setfill('0') << std::hex
            << (int) (recv_buffer_[i]) << " ";
    }

    its_message << " Last 16 Bytes captured: ";
    for (int i = 15; recv_buffer_size_ > 15 && i >= 0; i--) {
        its_message << std::setw(2) << std::setfill('0') << std::hex
            << (int) (recv_buffer_[i]) << " ";
    }
    VSOMEIP_ERROR << its_message.str();
    recv_buffer_.clear();
    if (socket_.is_open()) {
        boost::system::error_code its_error;
        socket_.shutdown(socket_.shutdown_both, its_error);
        socket_.close(its_error);
    }
    std::shared_ptr<tcp_server_endpoint_impl> its_server = server_.lock();
    if (its_server) {
        its_server->remove_connection(this);
    }
}

std::size_t
tcp_server_endpoint_impl::connection::get_recv_buffer_capacity() const {
    return recv_buffer_.capacity();
}

std::size_t
tcp_server_endpoint_impl::connection::write_completion_condition(
        const boost::system::error_code& _error,
        std::size_t _bytes_transferred, std::size_t _bytes_to_send,
        service_t _service, method_t _method, client_t _client, session_t _session,
        std::chrono::steady_clock::time_point _start) {
    if (_error) {
        VSOMEIP_ERROR << "tse::write_completion_condition: "
                << _error.message() << "(" << std::dec << _error.value()
                << ") bytes transferred: " << std::dec << _bytes_transferred
                << " bytes to sent: " << std::dec << _bytes_to_send << " "
                << "remote:" << get_address_port_remote() << " ("
                << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _method << "."
                << std::hex << std::setw(4) << std::setfill('0') << _session << "]";
        stop_and_remove_connection();
        return 0;
    }

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::milliseconds passed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start);
    if (passed > send_timeout_warning_) {
        if (passed > send_timeout_) {
            VSOMEIP_ERROR << "tse::write_completion_condition: "
                    << _error.message() << "(" << std::dec << _error.value()
                    << ") took longer than " << std::dec << send_timeout_.count()
                    << "ms bytes transferred: " << std::dec << _bytes_transferred
                    << " bytes to sent: " << std::dec << _bytes_to_send
                    << " remote:" << get_address_port_remote() << " ("
                    << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _method << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _session << "]";
        } else {
            VSOMEIP_WARNING << "tse::write_completion_condition: "
                    << _error.message() << "(" << std::dec << _error.value()
                    << ") took longer than " << std::dec << send_timeout_warning_.count()
                    << "ms bytes transferred: " << std::dec << _bytes_transferred
                    << " bytes to sent: " << std::dec << _bytes_to_send
                    << " remote:" << get_address_port_remote() << " ("
                    << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _method << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _session << "]";
        }
    }
    return _bytes_to_send - _bytes_transferred;
}

void tcp_server_endpoint_impl::connection::stop_and_remove_connection() {
    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_ERROR << "tse::connection::stop_and_remove_connection "
                " couldn't lock server_";
        return;
    }
    {
        std::lock_guard<std::mutex> its_lock(its_server->connections_mutex_);
        stop();
    }
    its_server->remove_connection(this);
}

// Dummies
void tcp_server_endpoint_impl::receive() {
    // intentionally left empty
}

void tcp_server_endpoint_impl::print_status() {
    std::lock_guard<std::mutex> its_lock(mutex_);
    connections_t its_connections;
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        its_connections = connections_;
    }

    VSOMEIP_INFO << "status tse: " << std::dec << local_port_
            << " connections: " << std::dec << its_connections.size()
            << " queues: " << std::dec << queues_.size();
    for (const auto &c : its_connections) {
        std::size_t its_data_size(0);
        std::size_t its_queue_size(0);
        std::size_t its_recv_size(0);
        {
            std::unique_lock<std::mutex> c_s_lock(c.second->get_socket_lock());
            its_recv_size = c.second->get_recv_buffer_capacity();
        }
        auto found_queue = queues_.find(c.first);
        if (found_queue != queues_.end()) {
            its_queue_size = found_queue->second.second.size();
            its_data_size = found_queue->second.first;
        }
        VSOMEIP_INFO << "status tse: client: "
                << c.second->get_address_port_remote()
                << " queue: " << std::dec << its_queue_size
                << " data: " << std::dec << its_data_size
                << " recv_buffer: " << std::dec << its_recv_size;
    }
}

std::string tcp_server_endpoint_impl::get_remote_information(
        const queue_iterator_type _queue_iterator) const {
    boost::system::error_code ec;
    return _queue_iterator->first.address().to_string(ec) + ":"
            + std::to_string(_queue_iterator->first.port());
}

}  // namespace vsomeip

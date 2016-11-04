// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_server_endpoint_impl::tcp_server_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _local,
        boost::asio::io_service &_io, std::uint32_t _max_message_size)
    : tcp_server_endpoint_base_impl(_host, _local, _io, _max_message_size),
        acceptor_(_io),
        current_(0) {
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
    connection::ptr new_connection = connection::create(
            std::dynamic_pointer_cast<tcp_server_endpoint_impl>(
                    shared_from_this()), max_message_size_);

    acceptor_.async_accept(new_connection->get_socket(),
            std::bind(&tcp_server_endpoint_impl::accept_cbk,
                    std::dynamic_pointer_cast<tcp_server_endpoint_impl>(
                            shared_from_this()), new_connection,
                    std::placeholders::_1));
}

void tcp_server_endpoint_impl::stop() {
    if(acceptor_.is_open()) {
        boost::system::error_code its_error;
        acceptor_.close(its_error);
    }
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        for (const auto& c : connections_) {
            if(c.second->get_socket().is_open()) {
                boost::system::error_code its_error;
                c.second->get_socket().cancel(its_error);
            }
        }
        connections_.clear();
    }
    server_endpoint_impl::stop();
}

bool tcp_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data,
        uint32_t _size, bool _flush) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());
    return send_intern(its_target, _data, _size, _flush);
}

void tcp_server_endpoint_impl::send_queued(queue_iterator_type _queue_iterator) {
    connection::ptr its_connection;
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        auto connection_iterator = connections_.find(_queue_iterator->first);
        if (connection_iterator != connections_.end()) {
            its_connection = connection_iterator->second;
        } else {
            VSOMEIP_DEBUG << "Didn't find connection: "
                    << _queue_iterator->first.address().to_string() << ":" << std::dec
                    << static_cast<std::uint16_t>(_queue_iterator->first.port())
                    << " dropping message.";
            _queue_iterator->second.pop_front();
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
            VSOMEIP_DEBUG << "Didn't find TCP connection: Subscription "
                    << "rejected for: " << endpoint.address().to_string() << ":"
                    << std::dec << static_cast<std::uint16_t>(endpoint.port());
        }
    }
    return is_connected;
}

bool tcp_server_endpoint_impl::get_remote_address(
        boost::asio::ip::address &_address) const {

    if (current_) {
        boost::system::error_code its_error;
        tcp_server_endpoint_impl::endpoint_type its_endpoint =
                current_->get_socket().remote_endpoint(its_error);
        if (!its_error) {
            boost::asio::ip::address its_address = its_endpoint.address();
            if (!its_address.is_unspecified()) {
                _address = its_address;
                return true;
            }
        }
    }
    return false;
}

unsigned short tcp_server_endpoint_impl::get_remote_port() const {
    if (current_) {
        boost::system::error_code its_error;
        tcp_server_endpoint_impl::endpoint_type its_endpoint =
                current_->get_socket().remote_endpoint(its_error);
        if (!its_error) {
            return its_endpoint.port();
        }
    }
    return 0;
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
        socket_type &new_connection_socket = _connection->get_socket();
        boost::system::error_code its_error;
        endpoint_type remote = new_connection_socket.remote_endpoint(its_error);
        if(!its_error) {
            std::lock_guard<std::mutex> its_lock(connections_mutex_);
            connections_[remote] = _connection;
            _connection->start();
        }
    }
    if (_error != boost::asio::error::bad_descriptor
            && _error != boost::asio::error::operation_aborted) {
        start();
    } else {
        VSOMEIP_DEBUG << "Endpoint was stopped, don't starting again";
    }
}

unsigned short tcp_server_endpoint_impl::get_local_port() const {
    boost::system::error_code its_error;
    return acceptor_.local_endpoint(its_error).port();
}

bool tcp_server_endpoint_impl::is_reliable() const {
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// class tcp_service_impl::connection
///////////////////////////////////////////////////////////////////////////////
tcp_server_endpoint_impl::connection::connection(
        std::weak_ptr<tcp_server_endpoint_impl> _server,
        std::uint32_t _max_message_size) :
        socket_(_server.lock()->service_), server_(_server),
        max_message_size_(_max_message_size),
        recv_buffer_(_max_message_size, 0),
        recv_buffer_size_(0) {
}

tcp_server_endpoint_impl::connection::ptr
tcp_server_endpoint_impl::connection::create(
        std::weak_ptr<tcp_server_endpoint_impl> _server,
        std::uint32_t _max_message_size) {
    return ptr(new connection(_server, _max_message_size));
}

tcp_server_endpoint_impl::socket_type &
tcp_server_endpoint_impl::connection::get_socket() {
    return socket_;
}

void tcp_server_endpoint_impl::connection::start() {
    receive();
    // Nagle algorithm off
    boost::system::error_code ec;
    socket_.set_option(ip::tcp::no_delay(true), ec);
}

void tcp_server_endpoint_impl::connection::receive() {
    if (recv_buffer_size_ == max_message_size_) {
        // Overrun -> Reset buffer
        recv_buffer_size_ = 0;
    }
    std::lock_guard<std::mutex> its_lock(stop_mutex_);
    if(socket_.is_open()) {
        size_t buffer_size = max_message_size_ - recv_buffer_size_;
        socket_.async_receive(boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
                std::bind(&tcp_server_endpoint_impl::connection::receive_cbk,
                        shared_from_this(), std::placeholders::_1,
                        std::placeholders::_2));
    }
}

void tcp_server_endpoint_impl::connection::stop() {
    std::lock_guard<std::mutex> its_lock(stop_mutex_);
    if (socket_.is_open()) {
        boost::system::error_code its_error;
        socket_.shutdown(socket_.shutdown_both, its_error);
        socket_.close(its_error);
    }
}

void tcp_server_endpoint_impl::connection::send_queued(
        queue_iterator_type _queue_iterator) {
    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "tcp_server_endpoint_impl::connection::send_queued "
                " couldn't lock server_";
        return;
    }
    message_buffer_ptr_t its_buffer = _queue_iterator->second.front();
    if (its_server->has_enabled_magic_cookies_) {
        send_magic_cookie(its_buffer);
    }

    boost::asio::async_write(socket_, boost::asio::buffer(*its_buffer),
            std::bind(&tcp_server_endpoint_base_impl::send_cbk,
                      its_server,
                      _queue_iterator, std::placeholders::_1,
                      std::placeholders::_2));
}

void tcp_server_endpoint_impl::connection::send_magic_cookie(
        message_buffer_ptr_t &_buffer) {
    if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - _buffer->size() >=
    VSOMEIP_SOMEIP_HEADER_SIZE + VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE) {
        _buffer->insert(_buffer->begin(), SERVICE_COOKIE,
                SERVICE_COOKIE + sizeof(SERVICE_COOKIE));
    }
}

bool tcp_server_endpoint_impl::connection::is_magic_cookie(size_t _offset) const {
    return (0 == std::memcmp(CLIENT_COOKIE, &recv_buffer_[_offset],
                             sizeof(CLIENT_COOKIE)));
}

void tcp_server_endpoint_impl::connection::receive_cbk(
        boost::system::error_code const &_error,
        std::size_t _bytes) {
    if (_error == boost::asio::error::operation_aborted) {
        stop();
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
    VSOMEIP_DEBUG<< msg.str();
#endif
    std::shared_ptr<endpoint_host> its_host = its_server->host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            recv_buffer_size_ += _bytes;

            size_t its_iteration_gap = 0;
            bool has_full_message;
            do {
                uint32_t current_message_size
                    = utility::get_message_size(&recv_buffer_[its_iteration_gap],
                            (uint32_t) recv_buffer_size_);
                has_full_message = (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE
                                   && current_message_size <= recv_buffer_size_);
                if (has_full_message) {
                    bool needs_forwarding(true);
                    if (is_magic_cookie(its_iteration_gap)) {
                        its_server->has_enabled_magic_cookies_ = true;
                    } else {
                        if (its_server->has_enabled_magic_cookies_) {
                            uint32_t its_offset
                                = its_server->find_magic_cookie(&recv_buffer_[its_iteration_gap],
                                        recv_buffer_size_);
                            if (its_offset < current_message_size) {
                                VSOMEIP_ERROR << "Detected Magic Cookie within message data. Resyncing.";
                                if (!is_magic_cookie(its_iteration_gap)) {
                                    its_host->on_error(&recv_buffer_[its_iteration_gap],
                                            static_cast<length_t>(recv_buffer_size_), its_server.get());
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
                            {
                                std::lock_guard<std::mutex> its_lock(stop_mutex_);
                                if (socket_.is_open()) {
                                    its_server->clients_mutex_.lock();
                                    boost::system::error_code its_error;
                                    endpoint_type its_endpoint(socket_.remote_endpoint(its_error));
                                    if (!its_error) {
                                        its_server->clients_[its_client][its_session] = its_endpoint;
                                    }
                                    its_server->clients_mutex_.unlock();
                                    its_server->current_ = this;
                                }
                            }
                        }
                        if (!its_server->has_enabled_magic_cookies_) {
                            its_host->on_message(&recv_buffer_[its_iteration_gap],
                                    current_message_size, its_server.get());
                        } else {
                            // Only call on_message without a magic cookie in front of the buffer!
                            if (!is_magic_cookie(its_iteration_gap)) {
                                its_host->on_message(&recv_buffer_[its_iteration_gap],
                                        current_message_size, its_server.get());
                            }
                        }
                    }
                    recv_buffer_size_ -= current_message_size;
                    its_iteration_gap += current_message_size;
                } else if (its_server->has_enabled_magic_cookies_ && recv_buffer_size_ > 0){
                    uint32_t its_offset =
                            its_server->find_magic_cookie(&recv_buffer_[its_iteration_gap],
                                    recv_buffer_size_);
                    if (its_offset < recv_buffer_size_) {
                        VSOMEIP_ERROR << "Detected Magic Cookie within message data. Resyncing.";
                        if (!is_magic_cookie(its_iteration_gap)) {
                            its_host->on_error(&recv_buffer_[its_iteration_gap],
                                    static_cast<length_t>(recv_buffer_size_), its_server.get());
                        }
                        recv_buffer_size_ -= its_offset;
                        its_iteration_gap += its_offset;
                        has_full_message = true; // trigger next loop
                        if (!is_magic_cookie(its_iteration_gap)) {
                            its_host->on_error(&recv_buffer_[its_iteration_gap],
                                    static_cast<length_t>(recv_buffer_size_), its_server.get());
                        }
                    }
                } else if (current_message_size > max_message_size_) {
                    if (its_server->has_enabled_magic_cookies_) {
                        VSOMEIP_ERROR << "Received a TCP message which exceeds "
                                      << "maximum message size ("
                                      << std::dec << current_message_size
                                      << "). Magic Cookies are enabled: "
                                      << "Resetting receiver.";
                        recv_buffer_size_ = 0;
                    } else {
                        VSOMEIP_ERROR << "Received a TCP message which exceeds "
                                      << "maximum message size ("
                                      << std::dec << current_message_size
                                      << ") Magic cookies are disabled: "
                                      << "Connection will be disabled!";
                        recv_buffer_size_ = 0;
                        return;
                    }
                }
            } while (has_full_message && recv_buffer_size_);
            if (its_iteration_gap) {
                // Copy incomplete message to front for next receive_cbk iteration
                for (size_t i = 0; i < recv_buffer_size_; ++i) {
                    recv_buffer_[i] = recv_buffer_[i + its_iteration_gap];
                }
            }
            receive();
        }
    }
    if (_error == boost::asio::error::eof
            || _error == boost::asio::error::connection_reset) {
        {
            std::lock_guard<std::mutex> its_lock(its_server->connections_mutex_);
            stop();
        }
        its_server->remove_connection(this);
    }
}

client_t tcp_server_endpoint_impl::get_client(std::shared_ptr<endpoint_definition> _endpoint) {
    endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        auto its_remote = connections_.find(endpoint);
        if (its_remote != connections_.end()) {
            return its_remote->second->get_client(endpoint);
        }
    }
    return 0;
}

client_t tcp_server_endpoint_impl::connection::get_client(endpoint_type _endpoint_type) {
    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "tcp_server_endpoint_impl::connection::get_client "
                " couldn't lock server_";
        return 0;
    }
    std::lock_guard<std::mutex> its_lock(its_server->clients_mutex_);
    for (const auto its_client : its_server->clients_) {
        for (const auto its_session : its_server->clients_[its_client.first]) {
            auto endpoint = its_session.second;
            if (endpoint == _endpoint_type) {
                // TODO: Check system byte order before convert!
                client_t client = client_t(its_client.first << 8 | its_client.first >> 8);
                return client;
            }
        }
    }
    return 0;
}

// Dummies
void tcp_server_endpoint_impl::receive() {
    // intentionally left empty
}

void tcp_server_endpoint_impl::restart() {
    // intentionally left empty
}

}  // namespace vsomeip

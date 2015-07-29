// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <boost/asio/write.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/logger.hpp>

#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/tcp_server_endpoint_impl.hpp"
#include "../../utility/include/utility.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_server_endpoint_impl::tcp_server_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _local,
        boost::asio::io_service &_io)
    : tcp_server_endpoint_base_impl(_host, _local, _io),
      acceptor_(_io, _local),
      current_(0) {
    is_supporting_magic_cookies_ = true;
}

tcp_server_endpoint_impl::~tcp_server_endpoint_impl() {
}

bool tcp_server_endpoint_impl::is_local() const {
    return false;
}

void tcp_server_endpoint_impl::start() {
    connection::ptr new_connection = connection::create(this);

    acceptor_.async_accept(new_connection->get_socket(),
            std::bind(&tcp_server_endpoint_impl::accept_cbk,
                    std::dynamic_pointer_cast<tcp_server_endpoint_impl>(
                            shared_from_this()), new_connection,
                    std::placeholders::_1));
}

void tcp_server_endpoint_impl::stop() {
    for (auto& i : connections_)
        i.second->stop();
    acceptor_.close();
}

bool tcp_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data,
        uint32_t _size, bool _flush) {
    endpoint_type its_target(_target->get_address(), _target->get_port());
    return send_intern(its_target, _data, _size, _flush);
}

void tcp_server_endpoint_impl::send_queued(endpoint_type _target,
        message_buffer_ptr_t _buffer) {
    auto connection_iterator = connections_.find(_target);
    if (connection_iterator != connections_.end())
        connection_iterator->second->send_queued(_buffer);
}

tcp_server_endpoint_impl::endpoint_type
tcp_server_endpoint_impl::get_remote() const {
    return current_->get_socket().remote_endpoint();
}

bool tcp_server_endpoint_impl::get_multicast(service_t, event_t,
        tcp_server_endpoint_impl::endpoint_type &) const {
    return false;
}

void tcp_server_endpoint_impl::accept_cbk(connection::ptr _connection,
        boost::system::error_code const &_error) {

    if (!_error) {
        socket_type &new_connection_socket = _connection->get_socket();
        endpoint_type remote = new_connection_socket.remote_endpoint();

        connections_[remote] = _connection;
        _connection->start();
    }

    start();
}

unsigned short tcp_server_endpoint_impl::get_local_port() const {
    return acceptor_.local_endpoint().port();
}

bool tcp_server_endpoint_impl::is_reliable() const {
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// class tcp_service_impl::connection
///////////////////////////////////////////////////////////////////////////////
tcp_server_endpoint_impl::connection::connection(
        tcp_server_endpoint_impl *_server) :
        socket_(_server->service_), server_(_server) {
}

tcp_server_endpoint_impl::connection::ptr
tcp_server_endpoint_impl::connection::create(
        tcp_server_endpoint_impl *_server) {
    return ptr(new connection(_server));
}

tcp_server_endpoint_impl::socket_type &
tcp_server_endpoint_impl::connection::get_socket() {
    return socket_;
}

void tcp_server_endpoint_impl::connection::start() {
    packet_buffer_ptr_t its_buffer = std::make_shared<packet_buffer_t>();
    socket_.async_receive(boost::asio::buffer(*its_buffer),
            std::bind(&tcp_server_endpoint_impl::connection::receive_cbk,
                    shared_from_this(), its_buffer, std::placeholders::_1,
                    std::placeholders::_2));
}

void tcp_server_endpoint_impl::connection::stop() {
    socket_.close();
}

void tcp_server_endpoint_impl::connection::send_queued(
        message_buffer_ptr_t _buffer) {
    if (server_->has_enabled_magic_cookies_)
        send_magic_cookie(_buffer);

    boost::asio::async_write(socket_, boost::asio::buffer(*_buffer),
            std::bind(&tcp_server_endpoint_base_impl::send_cbk,
                      server_->shared_from_this(),
                      _buffer, std::placeholders::_1,
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

bool tcp_server_endpoint_impl::connection::is_magic_cookie() const {
    return (0 == std::memcmp(CLIENT_COOKIE, &message_[0],
                             sizeof(CLIENT_COOKIE)));
}

void tcp_server_endpoint_impl::connection::receive_cbk(
        packet_buffer_ptr_t _buffer, boost::system::error_code const &_error,
        std::size_t _bytes) {
#if 0
    std::stringstream msg;
    for (std::size_t i = 0; i < _bytes; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0')
                << (int) (*_buffer)[i] << " ";
    VSOMEIP_DEBUG<< msg.str();
#endif
    std::shared_ptr<endpoint_host> its_host = server_->host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            message_.insert(message_.end(), _buffer->begin(),
                    _buffer->begin() + _bytes);

            static int i = 1;

            bool has_full_message;
            do {
                uint32_t current_message_size
                    = utility::get_message_size(message_);
                has_full_message = (current_message_size > 0
                                   && current_message_size <= message_.size());
                if (has_full_message) {
                    bool needs_forwarding(true);
                    if (is_magic_cookie()) {
                        server_->has_enabled_magic_cookies_ = true;
                    } else {
                        if (server_->has_enabled_magic_cookies_) {
                            uint32_t its_offset
                                = server_->find_magic_cookie(message_);
                            if (its_offset < current_message_size) {
                                VSOMEIP_ERROR << "Detected Magic Cookie within message data. Resyncing.";
                                current_message_size = its_offset;
                                needs_forwarding = false;
                            }
                        }
                    }
                    if (needs_forwarding) {
                        if (utility::is_request(message_[VSOMEIP_MESSAGE_TYPE_POS])) {
                            client_t its_client;
                            std::memcpy(&its_client,
                                &message_[VSOMEIP_CLIENT_POS_MIN],
                                sizeof(client_t));
                            session_t its_session;
                            std::memcpy(&its_session,
                                &message_[VSOMEIP_SESSION_POS_MIN],
                                sizeof(session_t));
                            server_->clients_[its_client][its_session] =
                                    socket_.remote_endpoint();
                        }
                        its_host->on_message(&message_[0], current_message_size, server_);
                    }
                    message_.erase(message_.begin(), message_.begin() + current_message_size);
                } else if (server_->has_enabled_magic_cookies_ && message_.size() > 0){
                    uint32_t its_offset = server_->find_magic_cookie(message_);
                    if (its_offset < message_.size()) {
                        VSOMEIP_ERROR << "Detected Magic Cookie within message data. Resyncing.";
                        message_.erase(message_.begin(), message_.begin() + its_offset);
                        has_full_message = true; // trigger next loop
                    }
                } else if (message_.size() > VSOMEIP_MAX_TCP_MESSAGE_SIZE) {
                    VSOMEIP_ERROR << "Message exceeds maximum message size. Resetting receiver.";
                    message_.clear();
                }
            } while (has_full_message);

            start();
        }
    }
}

client_t tcp_server_endpoint_impl::get_client(std::shared_ptr<endpoint_definition> _endpoint) {
    endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    auto its_remote = connections_.find(endpoint);
    if (its_remote != connections_.end()) {
        return its_remote->second->get_client(endpoint);
    }
    return 0;
}

client_t tcp_server_endpoint_impl::connection::get_client(endpoint_type _endpoint_type) {
    for (auto its_client : server_->clients_) {
        for (auto its_session : server_->clients_[its_client.first]) {
            auto endpoint = its_session.second;
            if (endpoint == _endpoint_type) {
                // TODO: Check system byte order before convert!
                client_t client = its_client.first << 8 | its_client.first >> 8;
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

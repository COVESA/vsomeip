// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <deque>
#include <iomanip>
#include <sstream>

#include <sys/types.h>
#include <boost/asio/write.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/local_server_endpoint_impl.hpp"

#include "../../logging/include/logger.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../configuration/include/configuration.hpp"

// Credentials
#ifndef _WIN32
#include "../include/credentials.hpp"
#endif

namespace vsomeip {

local_server_endpoint_impl::local_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local, boost::asio::io_service &_io,
        std::uint32_t _max_message_size,
        std::uint32_t _buffer_shrink_threshold,
        configuration::endpoint_queue_limit_t _queue_limit)
    : local_server_endpoint_base_impl(_host, _local, _io,
                                      _max_message_size, _queue_limit),
      acceptor_(_io),
      buffer_shrink_threshold_(_buffer_shrink_threshold) {
    is_supporting_magic_cookies_ = false;

    boost::system::error_code ec;
    acceptor_.open(_local.protocol(), ec);
    boost::asio::detail::throw_error(ec, "acceptor open");
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    boost::asio::detail::throw_error(ec, "acceptor set_option");
    acceptor_.bind(_local, ec);
    boost::asio::detail::throw_error(ec, "acceptor bind");
    acceptor_.listen(boost::asio::socket_base::max_connections, ec);
    boost::asio::detail::throw_error(ec, "acceptor listen");

#ifndef _WIN32
    if (_host->get_configuration()->is_security_enabled()) {
        credentials::activate_credentials(acceptor_.native());
    }
#endif
}

local_server_endpoint_impl::local_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local, boost::asio::io_service &_io,
        std::uint32_t _max_message_size,
        int native_socket,
        std::uint32_t _buffer_shrink_threshold,
        configuration::endpoint_queue_limit_t _queue_limit)
    : local_server_endpoint_base_impl(_host, _local, _io,
                                      _max_message_size, _queue_limit),
      acceptor_(_io),
      buffer_shrink_threshold_(_buffer_shrink_threshold) {
    is_supporting_magic_cookies_ = false;

   boost::system::error_code ec;
   acceptor_.assign(_local.protocol(), native_socket, ec);
   boost::asio::detail::throw_error(ec, "acceptor assign native socket");

#ifndef _WIN32
    if (_host->get_configuration()->is_security_enabled()) {
        credentials::activate_credentials(acceptor_.native());
    }
#endif
}

local_server_endpoint_impl::~local_server_endpoint_impl() {
}

bool local_server_endpoint_impl::is_local() const {
    return true;
}

void local_server_endpoint_impl::start() {
    std::lock_guard<std::mutex> its_lock(acceptor_mutex_);
    if (acceptor_.is_open()) {
        connection::ptr new_connection = connection::create(
                std::dynamic_pointer_cast<local_server_endpoint_impl>(
                        shared_from_this()), max_message_size_,
                        buffer_shrink_threshold_,
                        service_);

        {
            std::unique_lock<std::mutex> its_lock(new_connection->get_socket_lock());
            acceptor_.async_accept(
                new_connection->get_socket(),
                std::bind(
                    &local_server_endpoint_impl::accept_cbk,
                    std::dynamic_pointer_cast<
                        local_server_endpoint_impl
                    >(shared_from_this()),
                    new_connection,
                    std::placeholders::_1
                )
            );
        }
    }
}

void local_server_endpoint_impl::stop() {
    server_endpoint_impl::stop();
    {
        std::lock_guard<std::mutex> its_lock(acceptor_mutex_);
        if (acceptor_.is_open()) {
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

bool local_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data, uint32_t _size, bool _flush) {
    (void)_target;
    (void)_data;
    (void)_size;
    (void)_flush;
    return false;
}

void local_server_endpoint_impl::send_queued(
        const queue_iterator_type _queue_iterator) {
    connection::ptr its_connection;
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        auto connection_iterator = connections_.find(_queue_iterator->first);
        if (connection_iterator != connections_.end()) {
            connection_iterator->second->send_queued(_queue_iterator);
            its_connection = connection_iterator->second;
        } else {
            VSOMEIP_INFO << "Didn't find connection: "
#ifdef _WIN32
                    << _queue_iterator->first.address().to_string() << ":" << std::dec
                    << static_cast<std::uint16_t>(_queue_iterator->first.port())
#else
                    << _queue_iterator->first.path()
#endif
                    << " dropping outstanding messages (" << std::dec
                    << _queue_iterator->second.second.size() << ").";
            queues_.erase(_queue_iterator->first);
        }
    }
    if (its_connection) {
        its_connection->send_queued(_queue_iterator);
    }
}

void local_server_endpoint_impl::receive() {
    // intentionally left empty
}

bool local_server_endpoint_impl::get_default_target(
        service_t,
        local_server_endpoint_impl::endpoint_type &) const {
    return false;
}

void local_server_endpoint_impl::remove_connection(
        local_server_endpoint_impl::connection *_connection) {
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

void local_server_endpoint_impl::accept_cbk(
        connection::ptr _connection, boost::system::error_code const &_error) {

    if (_error != boost::asio::error::bad_descriptor
            && _error != boost::asio::error::operation_aborted
            && _error != boost::asio::error::no_descriptors) {
        start();
    }

    if (!_error) {
#ifndef _WIN32
        auto its_host = host_.lock();
        if (its_host) {
            if (its_host->get_configuration()->is_security_enabled()) {
                std::unique_lock<std::mutex> its_socket_lock(_connection->get_socket_lock());
                socket_type &new_connection_socket = _connection->get_socket();
                uid_t uid(0);
                gid_t gid(0);
                client_t client = credentials::receive_credentials(
                     new_connection_socket.native(), uid, gid);
                if (!its_host->check_credentials(client, uid, gid)) {
                     VSOMEIP_WARNING << std::hex << "Client 0x" << its_host->get_client()
                             << " received client credentials from client 0x" << client
                             << " which violates the security policy : uid/gid="
                             << std::dec << uid << "/" << gid;
                     boost::system::error_code er;
                     new_connection_socket.shutdown(new_connection_socket.shutdown_both, er);
                     new_connection_socket.close(er);
                     return;
                }
                _connection->set_bound_client(client);
                credentials::deactivate_credentials(new_connection_socket.native());
            }
        }
#endif

        boost::system::error_code its_error;
        endpoint_type remote;
        {
            std::unique_lock<std::mutex> its_socket_lock(_connection->get_socket_lock());
            socket_type &new_connection_socket = _connection->get_socket();
            remote = new_connection_socket.remote_endpoint(its_error);
        }
        if (!its_error) {
            {
                std::lock_guard<std::mutex> its_lock(connections_mutex_);
                connections_[remote] = _connection;
            }
            _connection->start();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// class local_service_impl::connection
///////////////////////////////////////////////////////////////////////////////

local_server_endpoint_impl::connection::connection(
        std::weak_ptr<local_server_endpoint_impl> _server,
        std::uint32_t _max_message_size,
        std::uint32_t _initial_recv_buffer_size,
        std::uint32_t _buffer_shrink_threshold,
        boost::asio::io_service &_io_service)
    : socket_(_io_service),
      server_(_server),
      recv_buffer_size_initial_(_initial_recv_buffer_size + 8),
      max_message_size_(_max_message_size),
      recv_buffer_(recv_buffer_size_initial_, 0),
      recv_buffer_size_(0),
      missing_capacity_(0),
      shrink_count_(0),
      buffer_shrink_threshold_(_buffer_shrink_threshold),
      bound_client_(VSOMEIP_ROUTING_CLIENT) {
}

local_server_endpoint_impl::connection::ptr
local_server_endpoint_impl::connection::create(
        std::weak_ptr<local_server_endpoint_impl> _server,
        std::uint32_t _max_message_size,
        std::uint32_t _buffer_shrink_threshold,
        boost::asio::io_service &_io_service) {
    const std::uint32_t its_initial_buffer_size = VSOMEIP_COMMAND_HEADER_SIZE
            + VSOMEIP_MAX_LOCAL_MESSAGE_SIZE
            + static_cast<std::uint32_t>(sizeof(instance_t) + sizeof(bool)
                    + sizeof(bool));
    return ptr(new connection(_server, _max_message_size, its_initial_buffer_size,
            _buffer_shrink_threshold, _io_service));
}

local_server_endpoint_impl::socket_type &
local_server_endpoint_impl::connection::get_socket() {
    return socket_;
}

std::unique_lock<std::mutex>
local_server_endpoint_impl::connection::get_socket_lock() {
    return std::unique_lock<std::mutex>(socket_mutex_);
}

void local_server_endpoint_impl::connection::start() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (socket_.is_open()) {
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
        socket_.async_receive(
            boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
            std::bind(
                &local_server_endpoint_impl::connection::receive_cbk,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    }
}

void local_server_endpoint_impl::connection::stop() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (socket_.is_open()) {
        boost::system::error_code its_error;
        socket_.shutdown(socket_.shutdown_both, its_error);
        socket_.close(its_error);
    }
}

void local_server_endpoint_impl::connection::send_queued(
        const queue_iterator_type _queue_iterator) {

    // TODO: We currently do _not_ use the send method of the local server
    // endpoints. If we ever need it, we need to add the "start tag", "data",
    // "end tag" sequence here.
    std::shared_ptr<local_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "local_server_endpoint_impl::connection::send_queued "
                " couldn't lock server_";
        return;
    }

    message_buffer_ptr_t its_buffer = _queue_iterator->second.second.front();
#if 0
        std::stringstream msg;
        msg << "lse::sq: ";
        for (std::size_t i = 0; i < its_buffer->size(); i++)
            msg << std::setw(2) << std::setfill('0') << std::hex
                << (int)(*its_buffer)[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(*its_buffer),
            std::bind(
                &local_server_endpoint_base_impl::send_cbk,
                its_server,
                _queue_iterator,
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    }
}

void local_server_endpoint_impl::connection::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {

    if (_error == boost::asio::error::operation_aborted) {
        // connection was stopped
        return;
    }
    std::shared_ptr<local_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "local_server_endpoint_impl::connection::receive_cbk "
                " couldn't lock server_";
        return;
    }
    std::shared_ptr<endpoint_host> its_host = its_server->host_.lock();
    if (its_host) {
        std::size_t its_start = 0;
        std::size_t its_end = 0;
        std::size_t its_iteration_gap = 0;
        std::uint32_t its_command_size = 0;

        if (!_error && 0 < _bytes) {
    #if 0
            std::stringstream msg;
            msg << "lse::c<" << this << ">rcb: ";
            for (std::size_t i = 0; i < _bytes + recv_buffer_size_; i++)
                msg << std::setw(2) << std::setfill('0') << std::hex
                    << (int) (recv_buffer_[i]) << " ";
            VSOMEIP_INFO << msg.str();
    #endif

            if (recv_buffer_size_ + _bytes < recv_buffer_size_) {
                VSOMEIP_ERROR << "receive buffer overflow in local server endpoint ~> abort!";
                return;
            }
            recv_buffer_size_ += _bytes;

            bool message_is_empty(false);
            bool found_message(false);

            do {
                found_message = false;
                message_is_empty = false;

                its_start = 0 + its_iteration_gap;
                if (its_start + 3 < its_start) {
                    VSOMEIP_ERROR << "buffer overflow in local server endpoint ~> abort!";
                    return;
                }
                while (its_start + 3 < recv_buffer_size_ + its_iteration_gap &&
                    (recv_buffer_[its_start] != 0x67 ||
                    recv_buffer_[its_start+1] != 0x37 ||
                    recv_buffer_[its_start+2] != 0x6d ||
                    recv_buffer_[its_start+3] != 0x07)) {
                    its_start++;
                }

                if (its_start + 3 == recv_buffer_size_ + its_iteration_gap) {
                    message_is_empty = true;
                } else {
                    its_start += 4;
                }

                if (!message_is_empty) {
                    if (its_start + 6 < recv_buffer_size_ + its_iteration_gap) {
                        its_command_size = VSOMEIP_BYTES_TO_LONG(
                                        recv_buffer_[its_start + 6],
                                        recv_buffer_[its_start + 5],
                                        recv_buffer_[its_start + 4],
                                        recv_buffer_[its_start + 3]);

                        its_end = its_start + 6 + its_command_size;
                    } else {
                        its_end = its_start;
                    }
                    if (its_command_size && max_message_size_ != MESSAGE_SIZE_UNLIMITED
                            && its_command_size > max_message_size_) {
                        std::lock_guard<std::mutex> its_lock(socket_mutex_);
                        VSOMEIP_ERROR << "Received a local message which exceeds "
                              << "maximum message size (" << std::dec << its_command_size
                              << ") aborting! local: " << get_path_local() << " remote: "
                              << get_path_remote();
                        recv_buffer_.resize(recv_buffer_size_initial_, 0x0);
                        recv_buffer_.shrink_to_fit();
                        return;
                    }
                    if (its_end + 3 < its_end) {
                        VSOMEIP_ERROR << "buffer overflow in local server endpoint ~> abort!";
                        return;
                    }
                    while (its_end + 3 < recv_buffer_size_ + its_iteration_gap &&
                        (recv_buffer_[its_end] != 0x07 ||
                        recv_buffer_[its_end+1] != 0x6d ||
                        recv_buffer_[its_end+2] != 0x37 ||
                        recv_buffer_[its_end+3] != 0x67)) {
                        its_end ++;
                    }
                    if (its_end + 4 < its_end) {
                        VSOMEIP_ERROR << "buffer overflow in local server endpoint ~> abort!";
                        return;
                    }
                    // check if we received a full message
                    if (recv_buffer_size_ + its_iteration_gap < its_end + 4
                            || recv_buffer_[its_end] != 0x07
                            || recv_buffer_[its_end+1] != 0x6d
                            || recv_buffer_[its_end+2] != 0x37
                            || recv_buffer_[its_end+3] != 0x67) {
                        // start tag (4 Byte) + command (1 Byte) + client id (2 Byte)
                        // + command size (4 Byte) + data itself + stop tag (4 byte)
                        // = 15 Bytes not covered in command size.
                        if (its_command_size && its_command_size + 15 > recv_buffer_size_) {
                            missing_capacity_ = its_command_size + 15 - std::uint32_t(recv_buffer_size_);
                        } else if (recv_buffer_size_ < 11) {
                            // to little data to read out the command size
                            // minimal amount of data needed to read out command size = 11
                            missing_capacity_ = 11 - static_cast<std::uint32_t>(recv_buffer_size_);
                        } else {
                            VSOMEIP_ERROR << "lse::c<" << this
                                    << ">rcb: recv_buffer_size is: " << std::dec
                                    << recv_buffer_size_ << " but couldn't read "
                                    "out command size. recv_buffer_capacity: "
                                    << recv_buffer_.capacity()
                                    << " its_iteration_gap: " << its_iteration_gap;
                        }
                    }
                }

                if (!message_is_empty &&
                    its_end + 3 < recv_buffer_size_ + its_iteration_gap) {
                    its_host->on_message(&recv_buffer_[its_start],
                                         uint32_t(its_end - its_start), its_server.get(),
                                         boost::asio::ip::address(), bound_client_);

                    #if 0
                            std::stringstream local_msg;
                            local_msg << "lse::c<" << this << ">rcb::thunk: ";
                            for (std::size_t i = its_start; i < its_end; i++)
                                local_msg << std::setw(2) << std::setfill('0') << std::hex
                                    << (int) recv_buffer_[i] << " ";
                            VSOMEIP_INFO << local_msg.str();
                    #endif
                    calculate_shrink_count();
                    recv_buffer_size_ -= (its_end + 4 - its_iteration_gap);
                    missing_capacity_ = 0;
                    its_command_size = 0;
                    found_message = true;
                    its_iteration_gap = its_end + 4;
                } else {
                    if (!message_is_empty && its_iteration_gap) {
                        // Message not complete and not in front of the buffer!
                        // Copy last part to front for consume in future receive_cbk call!
                        for (size_t i = 0; i < recv_buffer_size_; ++i) {
                            recv_buffer_[i] = recv_buffer_[i + its_iteration_gap];
                        }
                        // Still more capacity needed after shifting everything to front?
                        if (missing_capacity_ &&
                                missing_capacity_ <= recv_buffer_.capacity() - recv_buffer_size_) {
                            missing_capacity_ = 0;
                        }
                    }
                }
            } while (recv_buffer_size_ > 0 && found_message);
        }

        if (_error == boost::asio::error::eof
                || _error == boost::asio::error::connection_reset) {
            stop();
            its_server->remove_connection(this);
        } else if (_error != boost::asio::error::bad_descriptor) {
            start();
        }
    }
}

void local_server_endpoint_impl::connection::set_bound_client(client_t _client) {
    bound_client_ = _client;
}

void local_server_endpoint_impl::connection::calculate_shrink_count() {
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

const std::string local_server_endpoint_impl::connection::get_path_local() const {
    boost::system::error_code ec;
    std::string its_local_path;
    if (socket_.is_open()) {
        endpoint_type its_local_endpoint = socket_.local_endpoint(ec);
        if (!ec) {
#ifdef _WIN32
            its_local_path += its_local_endpoint.address().to_string(ec);
            its_local_path += ":";
            its_local_path += std::to_string(its_local_endpoint.port());
#else
            its_local_path += its_local_endpoint.path();
#endif

        }
    }
    return its_local_path;
}

const std::string local_server_endpoint_impl::connection::get_path_remote() const {
    boost::system::error_code ec;
    std::string its_remote_path;
    if (socket_.is_open()) {
        endpoint_type its_remote_endpoint = socket_.remote_endpoint(ec);
        if (!ec) {
#ifdef _WIN32
            its_remote_path += its_remote_endpoint.address().to_string(ec);
            its_remote_path += ":";
            its_remote_path += std::to_string(its_remote_endpoint.port());
#else
            its_remote_path += its_remote_endpoint.path();
#endif
        }
    }
    return its_remote_path;
}

void local_server_endpoint_impl::connection::handle_recv_buffer_exception(
        const std::exception &_e) {
    std::stringstream its_message;
    its_message <<"local_server_endpoint_impl::connection catched exception"
            << _e.what() << " local: " << get_path_local() << " remote: "
            << get_path_remote() << " shutting down connection. Start of buffer: ";

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
    std::shared_ptr<local_server_endpoint_impl> its_server = server_.lock();
    if (its_server) {
        its_server->remove_connection(this);
    }
}

std::size_t
local_server_endpoint_impl::connection::get_recv_buffer_capacity() const {
    return recv_buffer_.capacity();
}

void local_server_endpoint_impl::print_status() {
    std::lock_guard<std::mutex> its_lock(mutex_);
    connections_t its_connections;
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        its_connections = connections_;
    }
#ifndef _WIN32
    std::string its_local_path(local_.path());
#else
    std::string its_local_path("");
#endif
    VSOMEIP_INFO << "status lse: " << its_local_path << " connections: "
            << std::dec << its_connections.size() << " queues: "
            << std::dec << queues_.size();
    for (const auto &c : its_connections) {
#ifndef _WIN32
        std::string its_remote_path(c.first.path());
#else
        std::string its_remote_path("");
#endif
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
        VSOMEIP_INFO << "status lse: client: " << its_remote_path
                << " queue: " << std::dec << its_queue_size
                << " data: " << std::dec << its_data_size
                << " recv_buffer: " << std::dec << its_recv_size;
    }
}
std::string local_server_endpoint_impl::get_remote_information(
        const queue_iterator_type _queue_iterator) const {
#ifdef _WIN32
    boost::system::error_code ec;
    return _queue_iterator->first.address().to_string(ec) + ":"
            + std::to_string(_queue_iterator->first.port());
#else
    return _queue_iterator->first.path();
#endif
}

} // namespace vsomeip

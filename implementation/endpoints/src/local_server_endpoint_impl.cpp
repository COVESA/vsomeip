// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include "../../configuration/include/configuration.hpp"

// Credentials
#ifndef WIN32
#include "../include/credentials.hpp"
#endif

namespace vsomeip {

local_server_endpoint_impl::local_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local, boost::asio::io_service &_io,
        std::uint32_t _max_message_size)
    : local_server_endpoint_base_impl(_host, _local, _io, _max_message_size),
      acceptor_(_io), current_(nullptr) {
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

#ifndef WIN32
    if (_host->get_configuration()->is_security_enabled()) {
        credentials::activate_credentials(acceptor_.native());
    }
#endif
}

local_server_endpoint_impl::local_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local, boost::asio::io_service &_io,
        std::uint32_t _max_message_size,
		int native_socket)
    : local_server_endpoint_base_impl(_host, _local, _io, _max_message_size),
      acceptor_(_io), current_(nullptr) {
    is_supporting_magic_cookies_ = false;

   boost::system::error_code ec;
   acceptor_.assign(_local.protocol(), native_socket, ec);
   boost::asio::detail::throw_error(ec, "acceptor assign native socket");

#ifndef WIN32
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
    if (acceptor_.is_open()) {
        current_ = connection::create(
                std::dynamic_pointer_cast<local_server_endpoint_impl>(
                        shared_from_this()), max_message_size_);

        acceptor_.async_accept(
            current_->get_socket(),
            std::bind(
                &local_server_endpoint_impl::accept_cbk,
                std::dynamic_pointer_cast<
                    local_server_endpoint_impl
                >(shared_from_this()),
                current_,
                std::placeholders::_1
            )
        );
    }
}

void local_server_endpoint_impl::stop() {
    if (acceptor_.is_open()) {
        boost::system::error_code its_error;
        acceptor_.close(its_error);
    }
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        for (const auto &c : connections_) {
            if (c.second->get_socket().is_open()) {
                boost::system::error_code its_error;
                c.second->get_socket().cancel(its_error);
            }
        }
        connections_.clear();
    }
    server_endpoint_impl::stop();
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
        queue_iterator_type _queue_iterator) {
    std::lock_guard<std::mutex> its_lock(connections_mutex_);
    auto connection_iterator = connections_.find(_queue_iterator->first);
    if (connection_iterator != connections_.end())
        connection_iterator->second->send_queued(_queue_iterator);
}

void local_server_endpoint_impl::receive() {
    // intentionally left empty
}

void local_server_endpoint_impl::restart() {
    current_->start();
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

    if (_error != boost::asio::error::bad_descriptor &&
            _error != boost::asio::error::operation_aborted) {
        start();
    }

    if (!_error) {
        socket_type &new_connection_socket = _connection->get_socket();
#ifndef WIN32
        auto its_host = host_.lock();
        if (its_host) {
            if (its_host->get_configuration()->is_security_enabled()) {
                uid_t uid;
                gid_t gid;
                client_t client = credentials::receive_credentials(
                     new_connection_socket.native(), uid, gid);
                if (!its_host->check_credentials(client, uid, gid)) {
                     VSOMEIP_WARNING << std::hex << "Client 0x" << its_host->get_client()
                             << " received client credentials from client 0x" << client
                             << " which violates the security policy : uid/gid="
                             << std::dec << uid << "/" << gid;
                     boost::system::error_code er;
                     new_connection_socket.close(er);
                     return;
                }
                _connection->set_bound_client(client);
                credentials::deactivate_credentials(new_connection_socket.native());
            }
        }
#endif
        boost::system::error_code its_error;
        endpoint_type remote = new_connection_socket.remote_endpoint(its_error);
        if (!its_error) {
            std::lock_guard<std::mutex> its_lock(connections_mutex_);
            connections_[remote] = _connection;
            _connection->start();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// class local_service_impl::connection
///////////////////////////////////////////////////////////////////////////////

local_server_endpoint_impl::connection::connection(
        std::weak_ptr<local_server_endpoint_impl> _server,
        std::uint32_t _max_message_size)
    : socket_(_server.lock()->service_),
      server_(_server),
      max_message_size_(_max_message_size + 8),
      recv_buffer_(max_message_size_, 0),
      recv_buffer_size_(0), bound_client_(VSOMEIP_ROUTING_CLIENT) {
}

local_server_endpoint_impl::connection::ptr
local_server_endpoint_impl::connection::create(
        std::weak_ptr<local_server_endpoint_impl> _server,
        std::uint32_t _max_message_size) {
    return ptr(new connection(_server, _max_message_size));
}

local_server_endpoint_impl::socket_type &
local_server_endpoint_impl::connection::get_socket() {
    return socket_;
}

void local_server_endpoint_impl::connection::start() {
    if (socket_.is_open()) {
        if (recv_buffer_size_ == max_message_size_) {
            // Overrun -> Reset buffer
            recv_buffer_size_ = 0;
        }
        size_t buffer_size = max_message_size_ - recv_buffer_size_;
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
    if (socket_.is_open()) {
        boost::system::error_code its_error;
        socket_.shutdown(socket_.shutdown_both, its_error);
        socket_.close(its_error);
    }
}

void local_server_endpoint_impl::connection::send_queued(
        queue_iterator_type _queue_iterator) {

    // TODO: We currently do _not_ use the send method of the local server
    // endpoints. If we ever need it, we need to add the "start tag", "data",
    // "end tag" sequence here.
    std::shared_ptr<local_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "local_server_endpoint_impl::connection::send_queued "
                " couldn't lock server_";
        return;
    }

    message_buffer_ptr_t its_buffer = _queue_iterator->second.front();
#if 0
        std::stringstream msg;
        msg << "lse::sq: ";
        for (std::size_t i = 0; i < its_buffer->size(); i++)
            msg << std::setw(2) << std::setfill('0') << std::hex
                << (int)(*its_buffer)[i] << " ";
        VSOMEIP_DEBUG << msg.str();
#endif
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

void local_server_endpoint_impl::connection::send_magic_cookie() {
}

void local_server_endpoint_impl::connection::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {

    if (_error == boost::asio::error::operation_aborted) {
        stop();
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
        std::size_t its_start;
        std::size_t its_end;
        std::size_t its_iteration_gap = 0;

        if (!_error && 0 < _bytes) {
    #if 0
            std::stringstream msg;
            msg << "lse::c<" << this << ">rcb: ";
            for (std::size_t i = 0; i < _bytes + recv_buffer_size_; i++)
                msg << std::setw(2) << std::setfill('0') << std::hex
                    << (int) (recv_buffer_[i]) << " ";
            VSOMEIP_DEBUG << msg.str();
    #endif

            recv_buffer_size_ += _bytes;

    #define MESSAGE_IS_EMPTY     std::size_t(-1)
    #define FOUND_MESSAGE        std::size_t(-2)

            do {
                its_start = 0 + its_iteration_gap;
                while (its_start + 3 < recv_buffer_size_ + its_iteration_gap &&
                    (recv_buffer_[its_start] != 0x67 ||
                    recv_buffer_[its_start+1] != 0x37 ||
                    recv_buffer_[its_start+2] != 0x6d ||
                    recv_buffer_[its_start+3] != 0x07)) {
                    its_start ++;
                }

                its_start = (its_start + 3 == recv_buffer_size_ + its_iteration_gap ?
                                 MESSAGE_IS_EMPTY : its_start+4);

                if (its_start != MESSAGE_IS_EMPTY) {
                    its_end = its_start;
                    while (its_end + 3 < recv_buffer_size_ + its_iteration_gap &&
                        (recv_buffer_[its_end] != 0x07 ||
                        recv_buffer_[its_end+1] != 0x6d ||
                        recv_buffer_[its_end+2] != 0x37 ||
                        recv_buffer_[its_end+3] != 0x67)) {
                        its_end ++;
                    }
                }

                if (its_start != MESSAGE_IS_EMPTY &&
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
                            VSOMEIP_DEBUG << local_msg.str();
                    #endif

                    recv_buffer_size_ -= (its_end + 4 - its_iteration_gap);
                    its_start = FOUND_MESSAGE;
                    its_iteration_gap = its_end + 4;
                } else {
                    if (its_start != MESSAGE_IS_EMPTY && its_iteration_gap) {
                        // Message not complete and not in front of the buffer!
                        // Copy last part to front for consume in future receive_cbk call!
                        for (size_t i = 0; i < recv_buffer_size_; ++i) {
                            recv_buffer_[i] = recv_buffer_[i + its_iteration_gap];
                        }
                    }
                }
            } while (recv_buffer_size_ > 0 && its_start == FOUND_MESSAGE);
        }

        if (_error == boost::asio::error::eof
                || _error == boost::asio::error::connection_reset) {
            {
                std::lock_guard<std::mutex> its_lock(its_server->connections_mutex_);
                stop();
            }
            its_server->remove_connection(this);
        } else if (_error != boost::asio::error::bad_descriptor) {
            start();
        }
    }
}

void local_server_endpoint_impl::connection::set_bound_client(client_t _client) {
    bound_client_ = _client;
}

} // namespace vsomeip

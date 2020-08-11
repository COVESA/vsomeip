// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <deque>
#include <iomanip>
#include <sstream>

#include <sys/types.h>
#include <boost/asio/write.hpp>

#include <vsomeip/internal/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../include/local_server_endpoint_impl.hpp"
#include "../../security/include/security.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../utility/include/utility.hpp"

// Credentials
#ifndef _WIN32
#include "../include/credentials.hpp"
#endif

namespace vsomeip_v3 {

local_server_endpoint_impl::local_server_endpoint_impl(
        const std::shared_ptr<endpoint_host>& _endpoint_host,
        const std::shared_ptr<routing_host>& _routing_host,
        const endpoint_type& _local, boost::asio::io_service &_io,
        const std::shared_ptr<configuration>& _configuration,
        bool _is_routing_endpoint)
    : local_server_endpoint_base_impl(_endpoint_host, _routing_host, _local,
                                      _io,
                                      _configuration->get_max_message_size_local(),
                                      _configuration->get_endpoint_queue_limit_local(),
                                      _configuration),
      acceptor_(_io),
      buffer_shrink_threshold_(_configuration->get_buffer_shrink_threshold()),
      is_routing_endpoint_(_is_routing_endpoint) {
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
    if (chmod(_local.path().c_str(),
            static_cast<mode_t>(_configuration->get_permissions_uds())) == -1) {
        VSOMEIP_ERROR << __func__ << ": chmod: " << strerror(errno);
    }
    credentials::activate_credentials(acceptor_.native_handle());
#endif
}

local_server_endpoint_impl::local_server_endpoint_impl(
        const std::shared_ptr<endpoint_host>& _endpoint_host,
        const std::shared_ptr<routing_host>& _routing_host,
        const endpoint_type& _local, boost::asio::io_service &_io,
        int native_socket,
        const std::shared_ptr<configuration>& _configuration,
        bool _is_routing_endpoint)
    : local_server_endpoint_base_impl(_endpoint_host, _routing_host, _local, _io,
                                      _configuration->get_max_message_size_local(),
                                      _configuration->get_endpoint_queue_limit_local(),
                                      _configuration),
      acceptor_(_io),
      buffer_shrink_threshold_(configuration_->get_buffer_shrink_threshold()),
      is_routing_endpoint_(_is_routing_endpoint) {
    is_supporting_magic_cookies_ = false;

   boost::system::error_code ec;
   acceptor_.assign(_local.protocol(), native_socket, ec);
   boost::asio::detail::throw_error(ec, "acceptor assign native socket");

#ifndef _WIN32
    if (chmod(_local.path().c_str(),
            static_cast<mode_t>(_configuration->get_permissions_uds())) == -1) {
       VSOMEIP_ERROR << __func__ << ": chmod: " << strerror(errno);
    }
    credentials::activate_credentials(acceptor_.native_handle());
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

bool local_server_endpoint_impl::send(const uint8_t *_data, uint32_t _size) {
#if 0
    std::stringstream msg;
    msg << "lse::send ";
    for (uint32_t i = 0; i < _size; i++)
        msg << std::setw(2) << std::setfill('0') << std::hex << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (endpoint_impl::sending_blocked_) {
        return false;
    }

    client_t its_client;
    std::memcpy(&its_client, &_data[7], sizeof(its_client));

    connection::ptr its_connection;
    {
        std::lock_guard<std::mutex> its_lock(connections_mutex_);
        const auto its_iterator = connections_.find(its_client);
        if (its_iterator == connections_.end()) {
            return false;
        } else {
            its_connection = its_iterator->second;
        }
    }

    auto its_buffer = std::make_shared<message_buffer_t>();
    its_buffer->insert(its_buffer->end(), _data, _data + _size);
    its_connection->send_queued(its_buffer);

    return true;
}

bool local_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data, uint32_t _size) {
    (void)_target;
    (void)_data;
    (void)_size;
    return false;
}

bool local_server_endpoint_impl::send_error(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data, uint32_t _size) {
    (void)_target;
    (void)_data;
    (void)_size;
    return false;
}

void local_server_endpoint_impl::send_queued(
        const queue_iterator_type _queue_iterator) {
    (void)_queue_iterator;
}

void local_server_endpoint_impl::receive() {
    // intentionally left empty
}

bool local_server_endpoint_impl::get_default_target(
        service_t,
        local_server_endpoint_impl::endpoint_type &) const {
    return false;
}

bool local_server_endpoint_impl::add_connection(const client_t &_client,
        const std::shared_ptr<connection> &_connection) {
    bool ret = false;
    std::lock_guard<std::mutex> its_lock(connections_mutex_);
    auto find_connection = connections_.find(_client);
    if (find_connection == connections_.end()) {
        connections_[_client] = _connection;
        ret = true;
    } else {
        VSOMEIP_WARNING << "Attempt to add already existing "
            "connection to client " << std::hex << _client;
    }
    return ret;
}

void local_server_endpoint_impl::remove_connection(
        const client_t &_client) {
    std::lock_guard<std::mutex> its_lock(connections_mutex_);
    connections_.erase(_client);
}

void local_server_endpoint_impl::accept_cbk(
        const connection::ptr& _connection, boost::system::error_code const &_error) {
    if (_error != boost::asio::error::bad_descriptor
            && _error != boost::asio::error::operation_aborted
            && _error != boost::asio::error::no_descriptors) {
        start();
    } else if (_error == boost::asio::error::no_descriptors) {
        VSOMEIP_ERROR << "local_server_endpoint_impl::accept_cbk: "
                << _error.message() << " (" << std::dec << _error.value()
                << ") Will try to accept again in 1000ms";
        std::shared_ptr<boost::asio::steady_timer> its_timer =
                std::make_shared<boost::asio::steady_timer>(service_,
                        std::chrono::milliseconds(1000));
        auto its_ep = std::dynamic_pointer_cast<local_server_endpoint_impl>(
                shared_from_this());
        its_timer->async_wait([its_timer, its_ep]
                               (const boost::system::error_code& _error) {
            if (!_error) {
                its_ep->start();
            }
        });
    }

    if (!_error) {
#ifndef _WIN32
        auto its_host = endpoint_host_.lock();
        client_t client = 0;

        socket_type &new_connection_socket = _connection->get_socket();
        uid_t uid(ANY_UID);
        gid_t gid(ANY_GID);
        client = credentials::receive_credentials(
             new_connection_socket.native_handle(), uid, gid);

        if (its_host && security::get()->is_enabled()) {
            if (!configuration_->check_routing_credentials(client, uid, gid)) {
                VSOMEIP_WARNING << "vSomeIP Security: Rejecting new connection with routing manager client ID 0x" << std::hex << client
                        << " uid/gid= " << std::dec << uid << "/" << gid
                        << " because passed credentials do not match with routing manager credentials!";
                boost::system::error_code er;
                new_connection_socket.shutdown(new_connection_socket.shutdown_both, er);
                new_connection_socket.close(er);
                return;
            }

            if (is_routing_endpoint_) {
                // rm_impl receives VSOMEIP_CLIENT_UNSET initially -> check later
                _connection->set_bound_uid_gid(uid, gid);
            } else {
                {
                    std::lock_guard<std::mutex> its_connection_lock(connections_mutex_);
                    // rm_impl receives VSOMEIP_CLIENT_UNSET initially -> check later
                    const auto found_client = connections_.find(client);
                    if (found_client != connections_.end()) {
                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex
                                << its_host->get_client() << " is rejecting new connection with client ID 0x"
                                << client << " uid/gid= " << std::dec << uid << "/" << gid
                                << " because of already existing connection using same client ID";
                        boost::system::error_code er;
                        new_connection_socket.shutdown(new_connection_socket.shutdown_both, er);
                        new_connection_socket.close(er);
                        return;
                    }
                }
                if (!security::get()->check_credentials(client, uid, gid)) {
                     VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex
                             << its_host->get_client() << " received client credentials from client 0x"
                             << client << " which violates the security policy : uid/gid="
                             << std::dec << uid << "/" << gid;
                     boost::system::error_code er;
                     new_connection_socket.shutdown(new_connection_socket.shutdown_both, er);
                     new_connection_socket.close(er);
                     return;
                }
                // rm_impl receives VSOMEIP_CLIENT_UNSET initially -> set later
                _connection->set_bound_client(client);
                add_connection(client, _connection);
            }
        } else {
            security::get()->store_client_to_uid_gid_mapping(client, uid, gid);
            security::get()->store_uid_gid_to_client_mapping(uid, gid, client);
        }
#endif
        _connection->start();
    }
}

///////////////////////////////////////////////////////////////////////////////
// class local_service_impl::connection
///////////////////////////////////////////////////////////////////////////////

local_server_endpoint_impl::connection::connection(
        const std::shared_ptr<local_server_endpoint_impl>& _server,
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
      bound_client_(VSOMEIP_CLIENT_UNSET),
#ifndef _WIN32
      bound_uid_(ANY_UID),
      bound_gid_(ANY_GID),
#endif
      assigned_client_(false) {
    if (_server->is_routing_endpoint_ &&
            !security::get()->is_enabled()) {
        assigned_client_ = true;
    }
}

local_server_endpoint_impl::connection::ptr
local_server_endpoint_impl::connection::create(
        const std::shared_ptr<local_server_endpoint_impl>& _server,
        std::uint32_t _max_message_size,
        std::uint32_t _buffer_shrink_threshold,
        boost::asio::io_service &_io_service) {
    const std::uint32_t its_initial_buffer_size = VSOMEIP_COMMAND_HEADER_SIZE
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
#ifndef _WIN32
        socket_.async_receive(
            boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
            std::bind(
                &local_server_endpoint_impl::connection::receive_cbk,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4
            )
        );
#else
        socket_.async_receive(
            boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
            std::bind(
                &local_server_endpoint_impl::connection::receive_cbk,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
#endif
    }
}

void local_server_endpoint_impl::connection::stop() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (socket_.is_open()) {
#ifndef _WIN32
        if (-1 == fcntl(socket_.native_handle(), F_GETFD)) {
            VSOMEIP_ERROR << "lse: socket/handle closed already '" << std::string(std::strerror(errno))
                          << "' (" << errno << ") " << get_path_local();
        }
#endif
        boost::system::error_code its_error;
        socket_.shutdown(socket_.shutdown_both, its_error);
        socket_.close(its_error);
    }
}

void local_server_endpoint_impl::connection::send_queued(
        const message_buffer_ptr_t& _buffer) {
    std::shared_ptr<local_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "local_server_endpoint_impl::connection::send_queued "
                " couldn't lock server_";
        return;
    }

    static const byte_t its_start_tag[] = { 0x67, 0x37, 0x6D, 0x07 };
    static const byte_t its_end_tag[] = { 0x07, 0x6D, 0x37, 0x67 };
    std::vector<boost::asio::const_buffer> bufs;

#if 0
        std::stringstream msg;
        msg << "lse::sq: ";
        for (std::size_t i = 0; i < _buffer->size(); i++)
            msg << std::setw(2) << std::setfill('0') << std::hex
                << (int)(*_buffer)[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif

    bufs.push_back(boost::asio::buffer(its_start_tag));
    bufs.push_back(boost::asio::buffer(*_buffer));
    bufs.push_back(boost::asio::buffer(its_end_tag));

    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        boost::asio::async_write(
            socket_,
            bufs,
            std::bind(
                &local_server_endpoint_impl::connection::send_cbk,
                shared_from_this(),
                _buffer,
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    }
}

client_t local_server_endpoint_impl::assign_client(
        const byte_t *_data, uint32_t _size) {
    client_t its_client(VSOMEIP_CLIENT_UNSET);
    std::string its_name;
    uint32_t its_name_length;

    if (_size >= VSOMEIP_COMMAND_PAYLOAD_POS) {
        std::memcpy(&its_client, &_data[VSOMEIP_COMMAND_CLIENT_POS], sizeof(its_client));
        std::memcpy(&its_name_length, &_data[VSOMEIP_COMMAND_SIZE_POS_MIN],
            sizeof(its_name_length));

        if (its_name_length > 0)
            its_name.assign(reinterpret_cast<const char *>(
                    &_data[VSOMEIP_COMMAND_PAYLOAD_POS]), its_name_length);
    }

    its_client = utility::request_client_id(configuration_, its_name, its_client);

    return its_client;
}

void local_server_endpoint_impl::get_configured_times_from_endpoint(
        service_t _service,
        method_t _method, std::chrono::nanoseconds *_debouncing,
        std::chrono::nanoseconds *_maximum_retention) const {
    (void)_service;
    (void)_method;
    (void)_debouncing;
    (void)_maximum_retention;
    VSOMEIP_ERROR << "local_server_endpoint_impl::get_configured_times_from_endpoint.";
}

void local_server_endpoint_impl::connection::send_cbk(const message_buffer_ptr_t _buffer,
        boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_buffer;
    (void)_bytes;
    if (_error)
        VSOMEIP_WARNING << "sei::send_cbk received error: " << _error.message();
}

void local_server_endpoint_impl::connection::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes
#ifndef _WIN32
        , std::uint32_t const &_uid, std::uint32_t const &_gid
#endif
        )
{
    std::shared_ptr<local_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_TRACE << "local_server_endpoint_impl::connection::receive_cbk "
                " couldn't lock server_";
        return;
    }

    std::shared_ptr<routing_host> its_host = its_server->routing_host_.lock();
    if (!its_host)
        return;

    if (_error == boost::asio::error::operation_aborted) {
        if (its_server->is_routing_endpoint_ &&
                bound_client_ != VSOMEIP_CLIENT_UNSET) {
            utility::release_client_id(bound_client_);
            set_bound_client(VSOMEIP_CLIENT_UNSET);
        }

        // connection was stopped
        return;
    }

    bool is_error(false);
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
                    // command (1 Byte) + client id (2 Byte)
                    // + command size (4 Byte) + data itself + stop tag (4 byte)
                    // = 11 Bytes not covered in command size.
                    if (its_start - its_iteration_gap + its_command_size + 11 > recv_buffer_size_) {
                        missing_capacity_ =
                                std::uint32_t(its_start) - std::uint32_t(its_iteration_gap)
                                + its_command_size + 11 - std::uint32_t(recv_buffer_size_);
                    } else if (recv_buffer_size_ < 11) {
                        // to little data to read out the command size
                        // minimal amount of data needed to read out command size = 11
                        missing_capacity_ = 11 - static_cast<std::uint32_t>(recv_buffer_size_);
                    } else {
                        std::stringstream local_msg;
                        for (std::size_t i = its_iteration_gap;
                                i < recv_buffer_size_ + its_iteration_gap &&
                                i - its_iteration_gap < 32; i++) {
                            local_msg << std::setw(2) << std::setfill('0')
                                << std::hex << (int) recv_buffer_[i] << " ";
                        }
                        VSOMEIP_ERROR << "lse::c<" << this
                                << ">rcb: recv_buffer_size is: " << std::dec
                                << recv_buffer_size_ << " but couldn't read "
                                "out command size. recv_buffer_capacity: "
                                << std::dec << recv_buffer_.capacity()
                                << " its_iteration_gap: " << std::dec
                                << its_iteration_gap << " bound client: 0x"
                                << std::hex << bound_client_ << " buffer: "
                                << local_msg.str();
                        recv_buffer_size_ = 0;
                        missing_capacity_ = 0;
                        its_iteration_gap = 0;
                        message_is_empty = true;
                    }
                }
            }

            if (!message_is_empty &&
                its_end + 3 < recv_buffer_size_ + its_iteration_gap) {

                if (its_server->is_routing_endpoint_
                        && recv_buffer_[its_start] == VSOMEIP_ASSIGN_CLIENT) {
                    client_t its_client = its_server->assign_client(
                            &recv_buffer_[its_start], uint32_t(its_end - its_start));
#ifndef _WIN32
                    if (security::get()->is_enabled()) {
                        if (!its_server->add_connection(its_client, shared_from_this())) {
                            VSOMEIP_WARNING << std::hex << "Client 0x" << its_host->get_client()
                                    << " is rejecting new connection with client ID 0x" << its_client
                                    << " uid/gid= " << std::dec << bound_uid_ << "/" << bound_gid_
                                    << " because of already existing connection using same client ID";
                            stop();
                            return;
                        } else if (!security::get()->check_credentials(
                                its_client, bound_uid_, bound_gid_)) {
                            VSOMEIP_WARNING << std::hex << "Client 0x" << its_host->get_client()
                                    << " received client credentials from client 0x" << its_client
                                    << " which violates the security policy : uid/gid="
                                    << std::dec << bound_uid_ << "/" << bound_gid_;
                            its_server->remove_connection(its_client);
                            utility::release_client_id(its_client);
                            stop();
                            return;
                        } else {
                            set_bound_client(its_client);
                        }
                    } else
#endif
                    {
                        set_bound_client(its_client);
                        its_server->add_connection(its_client, shared_from_this());
                    }
                    its_server->send_client_identifier(its_client);
                    assigned_client_ = true;
                } else if (!its_server->is_routing_endpoint_ || assigned_client_) {
#ifndef _WIN32
                    credentials_t its_credentials = std::make_pair(_uid, _gid);
#else
                    credentials_t its_credentials = std::make_pair(ANY_UID, ANY_GID);
#endif
                    its_host->on_message(&recv_buffer_[its_start],
                                         uint32_t(its_end - its_start), its_server.get(),
                                         boost::asio::ip::address(), bound_client_, its_credentials);
                } else {
                    VSOMEIP_WARNING << std::hex << "Client 0x" << its_host->get_client()
                            << " didn't receive VSOMEIP_ASSIGN_CLIENT as first message";
                }
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
                if (its_iteration_gap) {
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
                } else if (message_is_empty) {
                    VSOMEIP_ERROR << "Received garbage data.";
                    is_error = true;
                }
            }
        } while (recv_buffer_size_ > 0 && found_message);
    }

    if (_error == boost::asio::error::eof
            || _error == boost::asio::error::connection_reset
            || is_error) {
        stop();
        its_server->remove_connection(bound_client_);
        security::get()->remove_client_to_uid_gid_mapping(bound_client_);
    } else if (_error != boost::asio::error::bad_descriptor) {
        start();
    }
}

void local_server_endpoint_impl::connection::set_bound_client(client_t _client) {
    bound_client_ = _client;
}

client_t local_server_endpoint_impl::connection::get_bound_client() const {
    return bound_client_;
}

#ifndef _WIN32
void local_server_endpoint_impl::connection::set_bound_uid_gid(uid_t _uid, gid_t _gid) {
    bound_uid_ = _uid;
    bound_gid_ = _gid;
}
#endif

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
    for (int i = 15; recv_buffer_size_ > 15u && i >= 0; i--) {
        its_message << std::setw(2) << std::setfill('0') << std::hex
            << (int) (recv_buffer_[static_cast<size_t>(i)]) << " ";
    }
    VSOMEIP_ERROR << its_message.str();
    recv_buffer_.clear();
    if (socket_.is_open()) {
#ifndef _WIN32
        if (-1 == fcntl(socket_.native_handle(), F_GETFD)) {
            VSOMEIP_ERROR << "lse: socket/handle closed already '" << std::string(std::strerror(errno))
                          << "' (" << errno << ") " << get_path_local();
        }
#endif
        boost::system::error_code its_error;
        socket_.shutdown(socket_.shutdown_both, its_error);
        socket_.close(its_error);
    }
    std::shared_ptr<local_server_endpoint_impl> its_server = server_.lock();
    if (its_server) {
        its_server->remove_connection(bound_client_);
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
        std::string its_remote_path; // TODO: construct the path
#else
        std::string its_remote_path("");
#endif

        std::size_t its_recv_size(0);
        {
            std::unique_lock<std::mutex> c_s_lock(c.second->get_socket_lock());
            its_recv_size = c.second->get_recv_buffer_capacity();
        }

        VSOMEIP_INFO << "status lse: client: " << its_remote_path
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
    (void)_queue_iterator;
    return "local";
#endif
}

std::string local_server_endpoint_impl::get_remote_information(
        const endpoint_type& _remote) const {
#ifdef _WIN32
    boost::system::error_code ec;
    return _remote.address().to_string(ec) + ":"
            + std::to_string(_remote.port());
#else
    (void)_remote;
    return "local";
#endif
}

bool local_server_endpoint_impl::is_reliable() const {
    return false;
}

std::uint16_t local_server_endpoint_impl::get_local_port() const {
    return 0;
}

bool local_server_endpoint_impl::check_packetizer_space(
        queue_iterator_type _queue_iterator, message_buffer_ptr_t* _packetizer,
        std::uint32_t _size) {
    if ((*_packetizer)->size() + _size < (*_packetizer)->size()) {
        VSOMEIP_ERROR << "Overflow in packetizer addition ~> abort sending!";
        return false;
    }
    if ((*_packetizer)->size() + _size > max_message_size_
            && !(*_packetizer)->empty()) {
        _queue_iterator->second.second.push_back(*_packetizer);
        _queue_iterator->second.first += (*_packetizer)->size();
        *_packetizer = std::make_shared<message_buffer_t>();
    }
    return true;
}

void
local_server_endpoint_impl::send_client_identifier(
        const client_t &_client) {
    byte_t its_command[] = {
            VSOMEIP_ASSIGN_CLIENT_ACK,
            0x0, 0x0, // client
            0x2, 0x0, 0x0, 0x0, // size
            0x0, 0x0 // assigned client
    };

    std::memcpy(its_command+7, &_client, sizeof(_client));

    send(its_command, sizeof(its_command));
}


bool local_server_endpoint_impl::tp_segmentation_enabled(
        service_t _service, method_t _method) const {
    (void)_service;
    (void)_method;
    return false;
}


} // namespace vsomeip_v3

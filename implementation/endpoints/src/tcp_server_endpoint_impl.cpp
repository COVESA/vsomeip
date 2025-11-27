
// Copyright (C) 2014-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <boost/asio/write.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../include/tcp_server_endpoint_impl.hpp"
#include "../../utility/include/utility.hpp"
#include "../../utility/include/bithelper.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip_v3 {

tcp_server_endpoint_impl::tcp_server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                                                   const std::shared_ptr<routing_host>& _routing_host, boost::asio::io_context& _io,
                                                   const std::shared_ptr<configuration>& _configuration, bool _use_magic_cookies) :
    tcp_server_endpoint_base_impl(_endpoint_host, _routing_host, _io, _configuration), use_magic_cookies_(_use_magic_cookies),
    acceptor_(_io), buffer_shrink_threshold_(configuration_->get_buffer_shrink_threshold()),
    // send timeout after 2/3 of configured ttl, warning after 1/3
    send_timeout_(configuration_->get_sd_ttl() * 666) {

    static std::atomic<unsigned> instance_count = 0;
    instance_name_ = "tsei#" + std::to_string(++instance_count) + "::";

    VSOMEIP_INFO << instance_name_ << __func__;
}

tcp_server_endpoint_impl::~tcp_server_endpoint_impl() {
    VSOMEIP_INFO << instance_name_ << __func__;
}

bool tcp_server_endpoint_impl::is_local() const {
    return false;
}

void tcp_server_endpoint_impl::init(const endpoint_type& _local, boost::system::error_code& _error) {
    VSOMEIP_INFO << instance_name_ << __func__ << ": " << _local.address() << ":" << _local.port();

    acceptor_.open(_local.protocol(), _error);
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to open socket, " << _error.message();
        return;
    }

#if defined(__linux__)
    int enable = 1;
    if (setsockopt(acceptor_.native_handle(), SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0) {
        VSOMEIP_WARNING << instance_name_ << __func__ << ": failed to set option SO_REUSEPORT, errno=" << errno;
    }
#endif

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), _error);
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to set option SO_REUSEADDR, " << _error.message();
        return;
    }

#if defined(__linux__) || defined(__QNX__)
    // If specified, bind to device
    std::string its_device(configuration_->get_device());
    if (its_device != "") {
        if (setsockopt(acceptor_.native_handle(), SOL_SOCKET, SO_BINDTODEVICE, its_device.c_str(),
                       static_cast<socklen_t>(its_device.size()))
            == -1) {
            VSOMEIP_WARNING << instance_name_ << __func__ << ": could not bind to device \"" << its_device << "\"";
        }
    }
#endif

    acceptor_.bind(_local, _error);
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to bind, " << _error.message();
        return;
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, _error);
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to listen, " << _error.message();
        return;
    }

    if (local_ != _local) {
        instance_name_ += _local.address().to_string();
        instance_name_ += ":";
        instance_name_ += std::to_string(_local.port());
        instance_name_ += "::";

        local_ = _local;
    }

    this->max_message_size_ = configuration_->get_max_message_size_reliable(_local.address().to_string(), _local.port());
    this->queue_limit_ = configuration_->get_endpoint_queue_limit(_local.address().to_string(), _local.port());

    VSOMEIP_INFO << instance_name_ << __func__ << ": done";
}

void tcp_server_endpoint_impl::start() {
    VSOMEIP_INFO << instance_name_ << __func__;

    std::scoped_lock its_lock(acceptor_mutex_);
    if (acceptor_.is_open()) {
        connection::ptr new_connection =
                connection::create(std::dynamic_pointer_cast<tcp_server_endpoint_impl>(shared_from_this()), max_message_size_,
                                   buffer_shrink_threshold_, use_magic_cookies_, io_, send_timeout_);

        acceptor_.async_accept(new_connection->get_socket(),
                               std::bind(&tcp_server_endpoint_impl::accept_cbk,
                                         std::dynamic_pointer_cast<tcp_server_endpoint_impl>(shared_from_this()), new_connection,
                                         std::placeholders::_1));
    } else {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": start failed, acceptor is closed";
    }

    VSOMEIP_INFO << instance_name_ << __func__ << ": done";
}

void tcp_server_endpoint_impl::stop(bool /*_due_to_error*/) {
    VSOMEIP_INFO << instance_name_ << __func__;

    {
        std::scoped_lock first_lock(acceptor_mutex_);

        if (acceptor_.is_open()) {
            boost::system::error_code its_error;
            acceptor_.close(its_error);
            VSOMEIP_INFO << instance_name_ << __func__ << ": acceptor closed, " << its_error.message();
        }

        std::scoped_lock second_lock(connections_mutex_);

        for (const auto& [_, c] : connections_) {
            c->stop();
        }
        connections_.clear();
    }

    VSOMEIP_INFO << instance_name_ << __func__ << ": done";
}

bool tcp_server_endpoint_impl::send_to(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) {
    std::scoped_lock its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());
    return send_intern(its_target, _data, _size);
}

bool tcp_server_endpoint_impl::send_error(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) {
    std::scoped_lock its_lock(mutex_);
    const endpoint_type its_target(_target->get_address(), _target->get_port());
    const auto its_target_iterator(find_or_create_target_unlocked(its_target));
    auto& its_data = its_target_iterator->second;

    if (check_queue_limit(_data, _size, its_data) && check_message_size(_size)) {
        its_data.queue_.emplace_back(std::make_pair(std::make_shared<message_buffer_t>(_data, _data + _size), 0));
        its_data.queue_size_ += _size;

        if (!its_data.is_sending_) { // no writing in progress
            (void)send_queued(its_target_iterator);
        }
        return true;
    }

    return false;
}

bool tcp_server_endpoint_impl::send_queued(const target_data_iterator_type _it) {

    bool must_erase(false);
    connection::ptr its_connection;
    {
        std::scoped_lock its_lock(connections_mutex_);
        auto connection_iterator = connections_.find(_it->first);
        if (connection_iterator != connections_.end()) {
            its_connection = connection_iterator->second;
            if (its_connection) {
                its_connection->send_queued(_it);
            }
        } else {
            VSOMEIP_INFO << instance_name_ << __func__ << ": didn't find connection: " << _it->first.address().to_string() << ":"
                         << std::dec << static_cast<std::uint16_t>(_it->first.port()) << " dropping outstanding messages (" << std::dec
                         << _it->second.queue_.size() << ").";

            if (_it->second.queue_.size()) {
                std::set<service_t> its_services;

                // check all outstanding messages of this connection
                // whether stop handlers need to be called
                for (const auto& its_q : _it->second.queue_) {
                    auto its_buffer(its_q.first);
                    if (its_buffer && its_buffer->size() > VSOMEIP_SESSION_POS_MAX) {
                        service_t its_service = bithelper::read_uint16_be(&(*its_buffer)[VSOMEIP_SERVICE_POS_MIN]);
                        its_services.insert(its_service);
                    }
                }

                for (auto its_service : its_services) {
                    auto found_cbk = prepare_stop_handlers_.find(its_service);
                    if (found_cbk != prepare_stop_handlers_.end()) {
                        VSOMEIP_INFO << instance_name_ << __func__ << ": calling prepare stop handler "
                                     << "for service: 0x" << std::hex << std::setfill('0') << std::setw(4) << its_service;
                        auto handler = found_cbk->second;
                        auto ptr = this->shared_from_this();
                        boost::asio::post(io_, [ptr, handler]() { handler(ptr); });
                        prepare_stop_handlers_.erase(found_cbk);
                    }
                }
            }

            // Drop outstanding messages.
            _it->second.queue_.clear();
            must_erase = true;
        }
    }

    return (must_erase);
}

void tcp_server_endpoint_impl::get_configured_times_from_endpoint(service_t _service, method_t _method,
                                                                  std::chrono::nanoseconds* _debouncing,
                                                                  std::chrono::nanoseconds* _maximum_retention) const {
    configuration_->get_configured_timing_responses(_service, tcp_server_endpoint_base_impl::local_.address().to_string(),
                                                    tcp_server_endpoint_base_impl::local_.port(), _method, _debouncing, _maximum_retention);
}

bool tcp_server_endpoint_impl::is_established_to(const std::shared_ptr<endpoint_definition>& _endpoint) {
    bool is_connected = false;
    endpoint_type its_endpoint(_endpoint->get_address(), _endpoint->get_port());
    {
        std::scoped_lock its_lock(connections_mutex_);
        auto connection_iterator = connections_.find(its_endpoint);
        if (connection_iterator != connections_.end()) {
            is_connected = true;
        } else {
            VSOMEIP_INFO << instance_name_ << __func__ << ": didn't find TCP connection: Subscription "
                         << "rejected for: " << its_endpoint.address().to_string() << ":" << std::dec << its_endpoint.port();
        }
    }
    return is_connected;
}

bool tcp_server_endpoint_impl::get_default_target(service_t, tcp_server_endpoint_impl::endpoint_type&) const {
    return false;
}

void tcp_server_endpoint_impl::remove_connection(endpoint_type _endpoint, connection* _connection) {
    connection::ptr its_old_connection;
    {
        std::scoped_lock its_lock(connections_mutex_);
        if (auto its_itr = connections_.find(_endpoint); its_itr != connections_.end() && its_itr->second.get() == _connection) {
            its_old_connection = its_itr->second;
            if (!connections_.erase(_endpoint)) {
                VSOMEIP_WARNING << "ltsei::" << __func__ << ": remote endpoint: " << _endpoint << " has no registered connection to "
                                << " remove, endpoint > " << this;
            }
            // if client still has a connection but a different one
        } else if (its_itr != connections_.end() && its_itr->second.get() != _connection) {
            VSOMEIP_WARNING << "ltsei::" << __func__ << ": tried to remove old connection " << _connection << " for endpoint " << _endpoint
                            << " new connection: " << connections_[_endpoint];
            its_old_connection = _connection->shared_from_this();
        }
    }

    if (its_old_connection) {
        its_old_connection->stop();
        its_old_connection.reset();
    }
}

void tcp_server_endpoint_impl::accept_cbk(connection::ptr _connection, boost::system::error_code const& _error) {

    if (!_error) {
        boost::system::error_code its_error;
        endpoint_type remote;
        {
            std::unique_lock<std::mutex> its_socket_lock(_connection->get_socket_lock());
            socket_type& new_connection_socket = _connection->get_socket();
            remote = new_connection_socket.remote_endpoint(its_error);
            if (its_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": remote endpoint info failed, " << its_error.message();
            }

            _connection->set_remote_info(remote);
            // Nagle algorithm off
            new_connection_socket.set_option(ip::tcp::no_delay(true), its_error);
            if (its_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": set option TCP_NODELAY failed, " << its_error.message();
            }

            new_connection_socket.set_option(boost::asio::socket_base::keep_alive(true), its_error);
            if (its_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": set option SO_KEEPALIVE failed, " << its_error.message();
            }

            // force always TCP RST on close/shutdown, in order to:
            // 1) avoid issues with TIME_WAIT, which otherwise lasts for 120 secs with a
            // non-responding endpoint (see also 4396812d2)
            // 2) handle by default what needs to happen at suspend/shutdown
            new_connection_socket.set_option(boost::asio::socket_base::linger(true, 0), its_error);
            if (its_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": set option SO_LINGER failed, " << its_error.message();
            }

#if defined(__linux__)
            // set a user timeout
            // along the keep alives, this ensures connection closes if endpoint is unreachable
            unsigned int opt = configuration_->get_external_tcp_user_timeout();
            if (setsockopt(new_connection_socket.native_handle(), IPPROTO_TCP, TCP_USER_TIMEOUT, &opt, sizeof(opt)) == -1) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": set option TCP_USER_TIMEOUT failed, errno=" << errno;
            }

            // override kernel settings
            // unfortunate, but there are plenty of custom keep-alive settings, and need to
            // enforce some sanity here
            auto opt2 = static_cast<int>(configuration_->get_external_tcp_keepidle());
            if (setsockopt(new_connection_socket.native_handle(), IPPROTO_TCP, TCP_KEEPIDLE, &opt2, sizeof(opt2)) == -1) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": set option TCP_KEEPIDLE failed, errno=" << errno;
            }
            opt2 = static_cast<int>(configuration_->get_external_tcp_keepintvl());
            if (setsockopt(new_connection_socket.native_handle(), IPPROTO_TCP, TCP_KEEPINTVL, &opt2, sizeof(opt2)) == -1) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": set option TCP_KEEPINTVL failed, errno=" << errno;
            }
            opt2 = static_cast<int>(configuration_->get_external_tcp_keepcnt());
            if (setsockopt(new_connection_socket.native_handle(), IPPROTO_TCP, TCP_KEEPCNT, &opt2, sizeof(opt2)) == -1) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": set option TCP_KEEPCNT failed, errno=" << errno;
            }
#endif
        }
        if (!its_error) {
            {
                std::scoped_lock its_lock(connections_mutex_);
                connections_[remote] = _connection;
            }
            _connection->start();
        } else {
            VSOMEIP_ERROR << instance_name_ << __func__ << ": socket couldn't be started, " << its_error.message();
        }
    } else {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": error, " << _error.message();
    }

    if (_error != boost::asio::error::bad_descriptor && _error != boost::asio::error::operation_aborted
        && _error != boost::asio::error::no_descriptors) {
        start();
    } else if (_error == boost::asio::error::no_descriptors) {
        auto its_timer = std::make_shared<boost::asio::steady_timer>(io_, std::chrono::milliseconds(1000));
        auto its_ep = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(shared_from_this());
        its_timer->async_wait([its_timer, its_ep](const boost::system::error_code& _error_inner) {
            if (!_error_inner) {
                its_ep->start();
            } else {
                VSOMEIP_ERROR << its_ep->instance_name_ << __func__ << ": recovery error, " << _error_inner.message();
            }
        });
    }
}

std::uint16_t tcp_server_endpoint_impl::get_local_port() const {

    return local_.port();
}

bool tcp_server_endpoint_impl::is_reliable() const {
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// class tcp_service_impl::connection
///////////////////////////////////////////////////////////////////////////////
tcp_server_endpoint_impl::connection::connection(const std::weak_ptr<tcp_server_endpoint_impl>& _server, std::uint32_t _max_message_size,
                                                 std::uint32_t _recv_buffer_size_initial, std::uint32_t _buffer_shrink_threshold,
                                                 bool _use_magic_cookies, boost::asio::io_context& _io,
                                                 std::chrono::milliseconds _send_timeout) :
    socket_(_io), server_(_server), max_message_size_(_max_message_size), recv_buffer_size_initial_(_recv_buffer_size_initial),
    recv_buffer_(_recv_buffer_size_initial, 0), recv_buffer_size_(0), missing_capacity_(0), shrink_count_(0),
    buffer_shrink_threshold_(_buffer_shrink_threshold), remote_port_(0), use_magic_cookies_(_use_magic_cookies),
    last_cookie_sent_(std::chrono::steady_clock::now() - std::chrono::seconds(11)), send_timeout_(_send_timeout),
    send_timeout_warning_(_send_timeout / 2) {
    auto its_server(_server.lock());
    instance_name_ = its_server->instance_name_;
}

tcp_server_endpoint_impl::connection::~connection() {

    auto its_server(server_.lock());
    if (its_server) {
        auto its_routing_host(its_server->routing_host_.lock());
        if (its_routing_host) {
            its_routing_host->remove_subscriptions(its_server->local_.port(), remote_address_, remote_port_);
        }
    }

    // ensure socket close() before boost destructor
    // otherwise boost asio removes linger, which may leave connection in TIME_WAIT
    VSOMEIP_INFO << instance_name_ << __func__ << ": endpoint > " << this;
    stop();
}

tcp_server_endpoint_impl::connection::ptr
tcp_server_endpoint_impl::connection::create(const std::weak_ptr<tcp_server_endpoint_impl>& _server, std::uint32_t _max_message_size,
                                             std::uint32_t _buffer_shrink_threshold, bool _magic_cookies_enabled,
                                             boost::asio::io_context& _io, std::chrono::milliseconds _send_timeout) {
    const std::uint32_t its_initial_receveive_buffer_size = VSOMEIP_SOMEIP_HEADER_SIZE + 8 + MAGIC_COOKIE_SIZE + 8;
    return ptr(new connection(_server, _max_message_size, its_initial_receveive_buffer_size, _buffer_shrink_threshold,
                              _magic_cookies_enabled, _io, _send_timeout));
}

tcp_server_endpoint_impl::socket_type& tcp_server_endpoint_impl::connection::get_socket() {
    return socket_;
}

std::unique_lock<std::mutex> tcp_server_endpoint_impl::connection::get_socket_lock() {
    return std::unique_lock<std::mutex>(socket_mutex_);
}

void tcp_server_endpoint_impl::connection::start() {
    receive();
}

void tcp_server_endpoint_impl::connection::receive() {
    std::unique_lock its_lock(socket_mutex_);
    if (socket_.is_open()) {
        const std::size_t its_capacity(recv_buffer_.capacity());
        if (recv_buffer_size_ > its_capacity) {
            VSOMEIP_ERROR << __func__ << "Received buffer size is greater than the buffer capacity!"
                          << " recv_buffer_size_: " << recv_buffer_size_ << " its_capacity: " << its_capacity;
            return;
        }
        size_t left_buffer_size = its_capacity - recv_buffer_size_;
        try {
            if (missing_capacity_) {
                if (missing_capacity_ > MESSAGE_SIZE_UNLIMITED) {
                    VSOMEIP_ERROR << instance_name_ << __func__ << ": missing receive buffer capacity exceeds allowed maximum!";
                    return;
                }
                const std::size_t its_required_capacity(recv_buffer_size_ + missing_capacity_);
                if (its_capacity < its_required_capacity) {
                    // Make the resize to its_required_capacity
                    recv_buffer_.reserve(its_required_capacity);
                    recv_buffer_.resize(its_required_capacity, 0x0);
                    if (recv_buffer_.size() > 1048576) {
                        VSOMEIP_INFO << instance_name_ << __func__ << ": recv_buffer size is: " << recv_buffer_.size()
                                     << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();
                    }
                }
                left_buffer_size = missing_capacity_;
                missing_capacity_ = 0;
            } else if (buffer_shrink_threshold_ && shrink_count_ > buffer_shrink_threshold_ && recv_buffer_size_ == 0) {
                // In this case, make the resize to recv_buffer_size_initial_
                recv_buffer_.resize(recv_buffer_size_initial_, 0x0);
                recv_buffer_.shrink_to_fit();
                // And set buffer_size to recv_buffer_size_initial_, the same of our resize
                left_buffer_size = recv_buffer_size_initial_;
                shrink_count_ = 0;
            }
        } catch (const std::exception& e) {
            its_lock.unlock();
            handle_recv_buffer_exception(e);
            // don't start receiving again
            return;
        }
        socket_.async_receive(boost::asio::buffer(&recv_buffer_[recv_buffer_size_], left_buffer_size),
                              std::bind(&tcp_server_endpoint_impl::connection::receive_cbk, shared_from_this(), std::placeholders::_1,
                                        std::placeholders::_2));
    }
}

void tcp_server_endpoint_impl::connection::stop() {
    std::scoped_lock its_lock(socket_mutex_);

    if (socket_.is_open()) {
        boost::system::error_code its_error;

        socket_.shutdown(socket_.shutdown_both, its_error);
        if (its_error) {
            VSOMEIP_WARNING << instance_name_ << __func__ << ": " << get_address_port_remote() << ">:shutting down socket failed ("
                            << its_error.message() << ")";
        }

        socket_.close(its_error);
        if (its_error) {
            VSOMEIP_WARNING << instance_name_ << __func__ << ": " << get_address_port_remote() << " closing socket failed ("
                            << its_error.message() << ")";
        }

        VSOMEIP_INFO << instance_name_ << __func__ << ": socket closed, endpoint > " << this;

    } else {
        VSOMEIP_INFO << instance_name_ << __func__ << ": socket was already closed, endpoint > " << this;
    }
}

void tcp_server_endpoint_impl::connection::send_queued(const target_data_iterator_type _it) {

    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": couldn't lock server_";
        return;
    }
    message_buffer_ptr_t its_buffer = _it->second.queue_.front().first;
    const service_t its_service = bithelper::read_uint16_be(&(*its_buffer)[VSOMEIP_SERVICE_POS_MIN]);
    const method_t its_method = bithelper::read_uint16_be(&(*its_buffer)[VSOMEIP_METHOD_POS_MIN]);
    const client_t its_client = bithelper::read_uint16_be(&(*its_buffer)[VSOMEIP_CLIENT_POS_MIN]);
    const session_t its_session = bithelper::read_uint16_be(&(*its_buffer)[VSOMEIP_SESSION_POS_MIN]);
    if (use_magic_cookies_) {
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cookie_sent_) > std::chrono::milliseconds(10000)) {
            if (send_magic_cookie(its_buffer)) {
                last_cookie_sent_ = now;
                _it->second.queue_size_ += sizeof(SERVICE_COOKIE);
            }
        }
    }

    {
        std::scoped_lock its_lock(socket_mutex_);
        _it->second.is_sending_ = true;

        boost::asio::async_write(
                socket_, boost::asio::buffer(*its_buffer),
                std::bind(&tcp_server_endpoint_impl::connection::write_completion_condition, shared_from_this(), std::placeholders::_1,
                          std::placeholders::_2, its_buffer->size(), its_service, its_method, its_client, its_session,
                          std::chrono::steady_clock::now()),
                std::bind(&tcp_server_endpoint_base_impl::send_cbk, its_server, _it->first, std::placeholders::_1, std::placeholders::_2));
    }
}

bool tcp_server_endpoint_impl::connection::send_magic_cookie(message_buffer_ptr_t& _buffer) {
    if (max_message_size_ == MESSAGE_SIZE_UNLIMITED
        || max_message_size_ - _buffer->size() >= VSOMEIP_SOMEIP_HEADER_SIZE + VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE) {
        _buffer->insert(_buffer->begin(), SERVICE_COOKIE, SERVICE_COOKIE + sizeof(SERVICE_COOKIE));
        return true;
    }
    return false;
}

bool tcp_server_endpoint_impl::connection::is_magic_cookie(size_t _offset) const {
    return (0 == std::memcmp(CLIENT_COOKIE, &recv_buffer_[_offset], sizeof(CLIENT_COOKIE)));
}

void tcp_server_endpoint_impl::connection::receive_cbk(boost::system::error_code const& _error, std::size_t _bytes) {
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
        msg << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<int>( recv_buffer_[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    std::unique_lock its_lock(socket_mutex_);
    std::shared_ptr<routing_host> its_host = its_server->routing_host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            if (recv_buffer_size_ + _bytes < recv_buffer_size_) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": receive buffer overflow in tcp client endpoint ~> abort!";
                return;
            }
            recv_buffer_size_ += _bytes;

            size_t its_iteration_gap = 0;
            bool has_full_message;
            do {
                uint64_t read_message_size = utility::get_message_size(&recv_buffer_[its_iteration_gap], recv_buffer_size_);
                if (read_message_size > MESSAGE_SIZE_UNLIMITED) {
                    VSOMEIP_ERROR << instance_name_ << __func__ << ": message size exceeds allowed maximum!";
                    return;
                }
                uint32_t current_message_size = static_cast<uint32_t>(read_message_size);
                has_full_message = (current_message_size > VSOMEIP_RETURN_CODE_POS && current_message_size <= recv_buffer_size_);
                if (has_full_message) {
                    bool needs_forwarding(true);
                    if (is_magic_cookie(its_iteration_gap)) {
                        use_magic_cookies_ = true;
                    } else {
                        if (use_magic_cookies_) {
                            uint32_t its_offset = its_server->find_magic_cookie(&recv_buffer_[its_iteration_gap], recv_buffer_size_);
                            if (its_offset < current_message_size) {
                                VSOMEIP_ERROR << instance_name_ << __func__
                                              << ": detected Magic Cookie within message data. "
                                                 "Resyncing."
                                              << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();

                                if (!is_magic_cookie(its_iteration_gap)) {
                                    auto its_endpoint_host = its_server->endpoint_host_.lock();
                                    if (its_endpoint_host) {
                                        its_endpoint_host->on_error(&recv_buffer_[its_iteration_gap],
                                                                    static_cast<length_t>(recv_buffer_size_), its_server.get(),
                                                                    remote_address_, remote_port_);
                                    }
                                }
                                current_message_size = its_offset;
                                needs_forwarding = false;
                            }
                        }
                    }
                    if (needs_forwarding) {
                        if (utility::is_request(recv_buffer_[its_iteration_gap + VSOMEIP_MESSAGE_TYPE_POS])) {
                            const client_t its_client =
                                    bithelper::read_uint16_be(&recv_buffer_[its_iteration_gap + VSOMEIP_CLIENT_POS_MIN]);
                            if (its_client != MAGIC_COOKIE_CLIENT) {
                                const service_t its_service =
                                        bithelper::read_uint16_be(&recv_buffer_[its_iteration_gap + VSOMEIP_SERVICE_POS_MIN]);
                                const method_t its_method =
                                        bithelper::read_uint16_be(&recv_buffer_[its_iteration_gap + VSOMEIP_METHOD_POS_MIN]);
                                std::scoped_lock its_clients_lock{its_server->clients_mutex_};
                                its_server->clients_to_target_[to_clients_key(its_service, its_method, its_client)] = remote_;
                            }
                        }
                        if (!use_magic_cookies_) {
                            its_lock.unlock();
                            its_host->on_message(&recv_buffer_[its_iteration_gap], current_message_size, its_server.get(), false,
                                                 VSOMEIP_ROUTING_CLIENT, nullptr, remote_address_, remote_port_);
                            its_lock.lock();
                        } else {
                            // Only call on_message without a magic cookie in front of the buffer!
                            if (!is_magic_cookie(its_iteration_gap)) {
                                its_lock.unlock();
                                its_host->on_message(&recv_buffer_[its_iteration_gap], current_message_size, its_server.get(), false,
                                                     VSOMEIP_ROUTING_CLIENT, nullptr, remote_address_, remote_port_);
                                its_lock.lock();
                            }
                        }
                    }
                    calculate_shrink_count();
                    missing_capacity_ = 0;
                    recv_buffer_size_ -= current_message_size;
                    its_iteration_gap += current_message_size;
                } else if (use_magic_cookies_ && recv_buffer_size_ > 0) {
                    uint32_t its_offset = its_server->find_magic_cookie(&recv_buffer_[its_iteration_gap], recv_buffer_size_);
                    if (its_offset < recv_buffer_size_) {
                        VSOMEIP_ERROR << instance_name_ << __func__ << ": detected Magic Cookie within message data. Resyncing."
                                      << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();

                        if (!is_magic_cookie(its_iteration_gap)) {
                            auto its_endpoint_host = its_server->endpoint_host_.lock();
                            if (its_endpoint_host) {
                                its_lock.unlock();
                                its_endpoint_host->on_error(&recv_buffer_[its_iteration_gap], static_cast<length_t>(recv_buffer_size_),
                                                            its_server.get(), remote_address_, remote_port_);
                                its_lock.lock();
                            }
                        }
                        recv_buffer_size_ -= its_offset;
                        its_iteration_gap += its_offset;
                        has_full_message = true; // trigger next loop
                        if (!is_magic_cookie(its_iteration_gap)) {
                            auto its_endpoint_host = its_server->endpoint_host_.lock();
                            if (its_endpoint_host) {
                                its_lock.unlock();
                                its_endpoint_host->on_error(&recv_buffer_[its_iteration_gap], static_cast<length_t>(recv_buffer_size_),
                                                            its_server.get(), remote_address_, remote_port_);
                                its_lock.lock();
                            }
                        }
                    }
                }

                if (!has_full_message) {
                    if (recv_buffer_size_ > VSOMEIP_RETURN_CODE_POS
                        && (recv_buffer_[its_iteration_gap + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION
                            || !utility::is_valid_message_type(
                                    static_cast<message_type_e>(recv_buffer_[its_iteration_gap + VSOMEIP_MESSAGE_TYPE_POS]))
                            || !utility::is_valid_return_code(
                                    static_cast<return_code_e>(recv_buffer_[its_iteration_gap + VSOMEIP_RETURN_CODE_POS])))) {
                        if (recv_buffer_[its_iteration_gap + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION) {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": wrong protocol version: 0x" << std::hex << std::setfill('0')
                                          << std::setw(2) << std::uint32_t(recv_buffer_[its_iteration_gap + VSOMEIP_PROTOCOL_VERSION_POS])
                                          << " local: " << get_address_port_local() << " remote: " << get_address_port_remote()
                                          << ". Closing connection due to missing/broken data TCP "
                                             "stream.";

                            // ensure to send back a error message w/ wrong protocol version
                            its_lock.unlock();
                            its_host->on_message(&recv_buffer_[its_iteration_gap], VSOMEIP_SOMEIP_HEADER_SIZE + 8, its_server.get(), false,
                                                 VSOMEIP_ROUTING_CLIENT, nullptr, remote_address_, remote_port_);
                            its_lock.lock();
                        } else if (!utility::is_valid_message_type(
                                           static_cast<message_type_e>(recv_buffer_[its_iteration_gap + VSOMEIP_MESSAGE_TYPE_POS]))) {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": invalid message type: 0x" << std::hex << std::setfill('0')
                                          << std::setw(2) << std::uint32_t(recv_buffer_[its_iteration_gap + VSOMEIP_MESSAGE_TYPE_POS])
                                          << " local: " << get_address_port_local() << " remote: " << get_address_port_remote()
                                          << ". Closing connection due to missing/broken data TCP "
                                             "stream.";
                        } else if (!utility::is_valid_return_code(
                                           static_cast<return_code_e>(recv_buffer_[its_iteration_gap + VSOMEIP_RETURN_CODE_POS]))) {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": invalid return code: 0x" << std::hex << std::setfill('0')
                                          << std::setw(2) << std::uint32_t(recv_buffer_[its_iteration_gap + VSOMEIP_RETURN_CODE_POS])
                                          << " local: " << get_address_port_local() << " remote: " << get_address_port_remote()
                                          << ". Closing connection due to missing/broken data TCP "
                                             "stream.";
                        }

                        its_lock.unlock();
                        wait_until_sent(boost::asio::error::operation_aborted);
                        return;
                    } else if (max_message_size_ != MESSAGE_SIZE_UNLIMITED && current_message_size > max_message_size_) {
                        recv_buffer_size_ = 0;
                        recv_buffer_.resize(recv_buffer_size_initial_, 0x0);
                        recv_buffer_.shrink_to_fit();
                        if (use_magic_cookies_) {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": received a TCP message which exceeds "
                                          << "maximum message size (" << std::dec << current_message_size << " > " << std::dec
                                          << max_message_size_ << "). Magic Cookies are enabled: "
                                          << "Resetting receiver. local: " << get_address_port_local()
                                          << " remote: " << get_address_port_remote();
                        } else {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": received a TCP message which exceeds "
                                          << "maximum message size (" << std::dec << current_message_size << " > " << std::dec
                                          << max_message_size_ << ") Magic cookies are disabled: "
                                          << "Connection will be closed! local: " << get_address_port_local()
                                          << " remote: " << get_address_port_remote();

                            its_lock.unlock();
                            wait_until_sent(boost::asio::error::operation_aborted);
                            return;
                        }
                    } else if (current_message_size > recv_buffer_size_) {
                        missing_capacity_ = current_message_size - static_cast<std::uint32_t>(recv_buffer_size_);
                    } else if (VSOMEIP_SOMEIP_HEADER_SIZE > recv_buffer_size_) {
                        missing_capacity_ = VSOMEIP_SOMEIP_HEADER_SIZE - static_cast<std::uint32_t>(recv_buffer_size_);
                    } else if (use_magic_cookies_ && recv_buffer_size_ > 0) {
                        // no need to check for magic cookie here again: has_full_message
                        // would have been set to true if there was one present in the data
                        recv_buffer_size_ = 0;
                        recv_buffer_.resize(recv_buffer_size_initial_, 0x0);
                        recv_buffer_.shrink_to_fit();
                        missing_capacity_ = 0;
                        VSOMEIP_ERROR << instance_name_ << __func__ << ": didn't find magic cookie in broken"
                                      << " data, trying to resync."
                                      << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();
                    } else {
                        VSOMEIP_ERROR << instance_name_ << __func__ << ": recv_buffer_size is: " << std::dec << recv_buffer_size_
                                      << " but couldn't read "
                                         "out message_size. recv_buffer_capacity: "
                                      << recv_buffer_.capacity() << " its_iteration_gap: " << its_iteration_gap
                                      << "local: " << get_address_port_local() << " remote: " << get_address_port_remote()
                                      << ". Closing connection due to missing/broken data TCP "
                                         "stream.";

                        its_lock.unlock();
                        wait_until_sent(boost::asio::error::operation_aborted);
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
                if (missing_capacity_ && missing_capacity_ <= recv_buffer_.capacity() - recv_buffer_size_) {
                    missing_capacity_ = 0;
                }
            }

            its_lock.unlock();
            receive();
        }
    }
    if (_error == boost::asio::error::eof || _error == boost::asio::error::connection_reset || _error == boost::asio::error::timed_out) {
        if (_error == boost::asio::error::timed_out) {
            VSOMEIP_WARNING << instance_name_ << __func__ << ": " << _error.message() << " local: " << get_address_port_local()
                            << " remote: " << get_address_port_remote();
        }

        its_lock.unlock();
        wait_until_sent(boost::asio::error::operation_aborted);
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

void tcp_server_endpoint_impl::connection::set_remote_info(const endpoint_type& _remote) {
    if (remote_ != _remote) {
        instance_name_ += _remote.address().to_string();
        instance_name_ += ":";
        instance_name_ += std::to_string(_remote.port());
        instance_name_ += "::";

        VSOMEIP_INFO << instance_name_ << __func__ << ": endpoint > " << this;

        remote_ = _remote;
        remote_address_ = _remote.address();
        remote_port_ = _remote.port();
    }
}

std::string tcp_server_endpoint_impl::connection::get_address_port_remote() const {
    std::string its_address_port;
    its_address_port.reserve(21);
    its_address_port += remote_address_.to_string();
    its_address_port += ":";
    its_address_port += std::to_string(remote_port_);
    return its_address_port;
}

std::string tcp_server_endpoint_impl::connection::get_address_port_local() const {
    std::string its_address_port;
    its_address_port.reserve(21);
    boost::system::error_code ec;
    if (socket_.is_open()) {
        endpoint_type its_local_endpoint = socket_.local_endpoint(ec);
        if (!ec) {
            its_address_port += its_local_endpoint.address().to_string();
            its_address_port += ":";
            its_address_port += std::to_string(its_local_endpoint.port());
        }
    }
    return its_address_port;
}

void tcp_server_endpoint_impl::connection::handle_recv_buffer_exception(const std::exception& _e) {
    std::stringstream its_message;
    its_message << instance_name_ << __func__ << ": catched exception" << _e.what() << " local: " << get_address_port_local()
                << " remote: " << get_address_port_remote() << " shutting down connection. Start of buffer: " << std::hex
                << std::setfill('0');

    for (std::size_t i = 0; i < recv_buffer_size_ && i < 16; i++) {
        its_message << std::setw(2) << static_cast<int>(recv_buffer_[i]) << " ";
    }

    its_message << " Last 16 Bytes captured: ";
    for (int i = 15; recv_buffer_size_ > 15 && i >= 0; i--) {
        its_message << std::setw(2) << static_cast<int>(recv_buffer_[static_cast<size_t>(i)]) << " ";
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
        its_server->remove_connection(remote_, this);
    }
}

std::size_t tcp_server_endpoint_impl::connection::get_recv_buffer_capacity() const {
    return recv_buffer_.capacity();
}

std::size_t tcp_server_endpoint_impl::connection::write_completion_condition(const boost::system::error_code& _error,
                                                                             std::size_t _bytes_transferred, std::size_t _bytes_to_send,
                                                                             service_t _service, method_t _method, client_t _client,
                                                                             session_t _session,
                                                                             const std::chrono::steady_clock::time_point _start) {
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": " << _error.message() << "(" << std::dec << _error.value()
                      << ") bytes transferred: " << std::dec << _bytes_transferred << " bytes to sent: " << std::dec << _bytes_to_send
                      << " "
                      << "remote:" << get_address_port_remote() << " (" << std::hex << std::setfill('0') << std::setw(4) << _client
                      << "): [" << std::setw(4) << _service << "." << std::setw(4) << _method << "." << std::setw(4) << _session << "]";
        stop_and_remove_connection();
        return 0;
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const std::chrono::milliseconds passed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start);
    if (passed > send_timeout_warning_) {
        if (passed > send_timeout_) {
            VSOMEIP_ERROR << instance_name_ << __func__ << ": " << _error.message() << "(" << std::dec << _error.value()
                          << ") took longer than " << send_timeout_.count() << "ms bytes transferred: " << _bytes_transferred
                          << " bytes to sent: " << _bytes_to_send << " remote:" << get_address_port_remote() << " (" << std::hex
                          << std::setfill('0') << std::setw(4) << _client << "): [" << std::setw(4) << _service << "." << std::setw(4)
                          << _method << "." << std::setw(4) << _session << "]";
        } else {
            VSOMEIP_WARNING << instance_name_ << __func__ << ": " << _error.message() << "(" << std::dec << _error.value()
                            << ") took longer than " << send_timeout_warning_.count() << "ms bytes transferred: " << _bytes_transferred
                            << " bytes to sent: " << _bytes_to_send << " remote:" << get_address_port_remote() << " (" << std::hex
                            << std::setfill('0') << std::setw(4) << _client << "): [" << std::setw(4) << _service << "." << std::setw(4)
                            << _method << "." << std::setw(4) << _session << "]";
        }
    }
    return _bytes_to_send - _bytes_transferred;
}

void tcp_server_endpoint_impl::connection::stop_and_remove_connection() {
    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": couldn't lock server_";
        return;
    }
    {
        std::scoped_lock its_lock(its_server->connections_mutex_);
        stop();
    }
    its_server->remove_connection(remote_, this);
}

// Dummies
void tcp_server_endpoint_impl::receive() {
    // intentionally left empty
}

void tcp_server_endpoint_impl::print_status() {
    std::scoped_lock its_lock(mutex_);
    connections_t its_connections;
    {
        std::scoped_lock its_lock_inner(connections_mutex_);
        its_connections = connections_;
    }

    VSOMEIP_INFO << instance_name_ << __func__ << ": " << std::dec << local_.port() << " connections: " << std::dec
                 << its_connections.size() << " targets: " << std::dec << targets_.size();
    for (const auto& c : its_connections) {
        std::size_t its_data_size(0);
        std::size_t its_queue_size(0);
        std::size_t its_recv_size(0);
        {
            std::unique_lock<std::mutex> c_s_lock(c.second->get_socket_lock());
            its_recv_size = c.second->get_recv_buffer_capacity();
        }
        auto found_queue = targets_.find(c.first);
        if (found_queue != targets_.end()) {
            its_queue_size = found_queue->second.queue_.size();
            its_data_size = found_queue->second.queue_size_;
        }
        VSOMEIP_INFO << instance_name_ << __func__ << ": client: " << c.second->get_address_port_remote() << " queue: " << std::dec
                     << its_queue_size << " data: " << std::dec << its_data_size << " recv_buffer: " << std::dec << its_recv_size;
    }
}

std::string tcp_server_endpoint_impl::get_remote_information(const target_data_iterator_type _it) const {
    return _it->first.address().to_string() + ":" + std::to_string(_it->first.port());
}

std::string tcp_server_endpoint_impl::get_remote_information(const endpoint_type& _remote) const {
    return _remote.address().to_string() + ":" + std::to_string(_remote.port());
}

void tcp_server_endpoint_impl::connection::wait_until_sent(const boost::system::error_code& _error) {
    std::shared_ptr<tcp_server_endpoint_impl> its_server(server_.lock());
    if (!its_server)
        return;

    std::scoped_lock its_lock(its_server->mutex_);

    auto it = its_server->targets_.find(remote_);
    if (it != its_server->targets_.end()) {
        auto& its_data = it->second;
        if (its_data.is_sending_ && _error) {
            std::chrono::milliseconds its_timeout(VSOMEIP_MAX_TCP_SENT_WAIT_TIME);
            its_data.sent_timer_.expires_after(its_timeout);
            its_data.sent_timer_.async_wait(std::bind(&tcp_server_endpoint_impl::connection::wait_until_sent,
                                                      std::dynamic_pointer_cast<tcp_server_endpoint_impl::connection>(shared_from_this()),
                                                      std::placeholders::_1));
            return;
        } else {
            std::scoped_lock its_lock_inner{socket_mutex_};
            VSOMEIP_WARNING << instance_name_ << __func__ << ": maximum wait time for send operation exceeded for tse."
                            << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();
        }
    }
    {
        std::scoped_lock its_lock_inner(its_server->connections_mutex_);
        stop();
    }
    its_server->remove_connection(remote_, this);
}

} // namespace vsomeip_v3

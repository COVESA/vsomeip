// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <atomic>
#include <iomanip>
#include <sstream>
#if defined(__linux__)
#include <netinet/tcp.h>
#endif

#include <vsomeip/defines.hpp>
#include <vsomeip/internal/logger.hpp>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif
#include "../include/tcp_socket.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/local_tcp_client_endpoint_impl.hpp"
#include "../include/local_tcp_server_endpoint_impl.hpp"
#include "../../protocol/include/protocol.hpp"
#include "../../routing/include/routing_host.hpp"

namespace vsomeip_v3 {

local_tcp_client_endpoint_impl::local_tcp_client_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                                                               const std::shared_ptr<routing_host>& _routing_host,
                                                               const endpoint_type& _local, const endpoint_type& _remote,
                                                               boost::asio::io_context& _io,
                                                               const std::shared_ptr<configuration>& _configuration) :
    local_tcp_client_endpoint_base_impl(_endpoint_host, _routing_host, _local, _remote, _io, _configuration),
    recv_buffer_(VSOMEIP_LOCAL_CLIENT_ENDPOINT_RECV_BUFFER_SIZE, 0) {

    is_supporting_magic_cookies_ = false;

    this->max_message_size_ = _configuration->get_max_message_size_local();
    this->queue_limit_ = _configuration->get_endpoint_queue_limit_local();
}

local_tcp_client_endpoint_impl::~local_tcp_client_endpoint_impl() {
    // ensure socket close() before boost destructor
    // otherwise boost asio removes linger, which may leave connection in TIME_WAIT
    VSOMEIP_INFO << "ltcei::~ltcei: endpoint > " << this << ", state_ > " << static_cast<int>(state_.load());
    shutdown_and_close_socket(false);
}

bool local_tcp_client_endpoint_impl::is_local() const {
    return true;
}
void local_tcp_client_endpoint_impl::restart(bool _force) {

    if (!_force && state_ == cei_state_e::CONNECTING) {
        return;
    }
    {
        std::lock_guard<std::recursive_mutex> its_lock(mutex_);
        sending_blocked_ = false;
        queue_.clear();
        queue_size_ = 0;
        is_sending_ = false;
    }
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        shutdown_and_close_socket_unlocked(true);
    }
    auto its_host = endpoint_host_.lock();
    if (its_host) {
        its_host->on_disconnect(shared_from_this());
    }
    state_ = cei_state_e::CONNECTING;
    was_not_connected_ = true;
    reconnect_counter_ = 0;
    start_connect_timer();
}

void local_tcp_client_endpoint_impl::start() {
    if (state_ == cei_state_e::CLOSED) {
        {
            std::lock_guard<std::recursive_mutex> its_lock(mutex_);
            sending_blocked_ = false;
            is_stopping_ = false;
        }
        connect();
    }
}

void local_tcp_client_endpoint_impl::stop() {
    {
        std::lock_guard<std::recursive_mutex> its_lock(mutex_);
        sending_blocked_ = true;
        is_stopping_ = true;
    }
    {
        std::lock_guard<std::mutex> its_lock(connect_timer_mutex_);
        connect_timer_.cancel();
    }
    connect_timeout_ = VSOMEIP_DEFAULT_CONNECT_TIMEOUT;

    bool is_open(false);
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        is_open = socket_->is_open();
    }
    if (is_open) {
        bool send_queue_empty(false);
        std::uint32_t times_slept(0);

        while (times_slept <= LOCAL_TCP_WAIT_SEND_QUEUE_ON_STOP) {
            std::unique_lock<std::recursive_mutex> its_lock(mutex_);
            send_queue_empty = (queue_.size() == 0);
            if (send_queue_empty) {
                break;
            } else {
                queue_cv_.wait_for(its_lock, std::chrono::milliseconds(10), [this] { return queue_.size() == 0; });
                times_slept++;
            }
        }
    }

    shutdown_and_close_socket(false);
}

void local_tcp_client_endpoint_impl::connect() {
    std::unique_lock<std::mutex> its_lock(socket_mutex_);
    boost::system::error_code its_error;
    socket_->open(remote_.protocol(), its_error);
    if (!its_error || its_error == boost::asio::error::already_open) {
        // Nagle algorithm off
        socket_->set_option(boost::asio::ip::tcp::no_delay(true), its_error);
        if (its_error) {
            VSOMEIP_WARNING << "ltcei::connect: couldn't disable "
                            << "Nagle algorithm: " << its_error.message() << " remote: " << remote_.port() << " endpoint: " << this
                            << " state_: " << to_string(state_.load());
        }

        // connection in the same host (and not across a host and guest or similar)
        // important, as we can (MUST!) be lax in the socket options - no keep alive necessary
        if (local_.address() == remote_.address()) {
            // disable keep alive
            socket_->set_option(boost::asio::socket_base::keep_alive(false), its_error);
            if (its_error) {
                VSOMEIP_WARNING << "ltcei::connect: couldn't disable "
                                << "keep_alive: " << its_error.message() << " remote:" << remote_.port() << " endpoint > " << this
                                << " state_ > " << to_string(state_.load());
            }
        } else {
            // enable keep alive
            socket_->set_option(boost::asio::socket_base::keep_alive(true), its_error);
            if (its_error) {
                VSOMEIP_WARNING << "ltcei::connect: couldn't enable "
                                << "keep_alive: " << its_error.message() << " remote:" << remote_.port() << " endpoint > " << this
                                << " state_ > " << to_string(state_.load());
            }

#if defined(__linux__)
            // set a user timeout
            // along the keep alives, this ensures connection closes if endpoint is unreachable
            if (!socket_->set_user_timeout(configuration_->get_local_tcp_user_timeout())) {
                VSOMEIP_WARNING << "ltcei::connect: could not setsockopt(TCP_USER_TIMEOUT), errno " << errno;
            }

            // override kernel settings
            // unfortunate, but there are plenty of custom keep-alive settings, and need to
            // enforce some sanity here
            if (!socket_->set_keepidle(configuration_->get_local_tcp_keepidle())) {
                VSOMEIP_WARNING << "ltcei::connect: could not setsockopt(TCP_KEEPIDLE), errno " << errno;
            }
            if (!socket_->set_keepintvl(configuration_->get_local_tcp_keepintvl())) {
                VSOMEIP_WARNING << "ltcei::connect: could not setsockopt(TCP_KEEPINTVL), errno " << errno;
            }
            if (!socket_->set_keepcnt(configuration_->get_local_tcp_keepcnt())) {
                VSOMEIP_WARNING << "ltcei::connect: could not setsockopt(TCP_KEEPCNT), errno " << errno;
            }
#endif
        }

        // force always TCP RST on close/shutdown, in order to:
        // 1) avoid issues with TIME_WAIT, which otherwise lasts for 120 secs with a
        // non-responding endpoint (see also 4396812d2)
        // 2) handle by default what needs to happen at suspend/shutdown
        socket_->set_option(boost::asio::socket_base::linger(true, 0), its_error);
        if (its_error) {
            VSOMEIP_WARNING << "ltcei::connect: couldn't enable "
                            << "SO_LINGER: " << its_error.message() << " remote:" << remote_.port() << " endpoint > " << this
                            << " state_ > " << to_string(state_.load());
        }
        socket_->set_option(boost::asio::socket_base::reuse_address(true), its_error);
        if (its_error) {
            VSOMEIP_WARNING << "ltcei::" << __func__ << ": Cannot enable SO_REUSEADDR" << "(" << its_error.message() << ")"
                            << " endpoint > " << this << " state_ > " << to_string(state_.load());
        }
        socket_->bind(local_, its_error);
        if (its_error) {
            VSOMEIP_WARNING << "ltcei::" << __func__ << ": Cannot bind to client port " << local_.port() << "(" << its_error.message()
                            << ")"
                            << " endpoint > " << this << " state_ > " << to_string(state_.load());
            try {
                boost::asio::post(strand_, std::bind(&client_endpoint_impl::connect_cbk, shared_from_this(), its_error));
            } catch (const std::exception& e) {
                VSOMEIP_ERROR << "ltcei::connect: " << e.what() << " endpoint > " << this << " state_ > " << to_string(state_.load());
            }
            return;
        }
        state_ = cei_state_e::CONNECTING;
        connecting_timer_state_ = connecting_timer_state_e::IN_PROGRESS;
        start_connecting_timer();
        socket_->async_connect(remote_,
                               strand_.wrap(std::bind(&local_tcp_client_endpoint_impl::cancel_and_connect_cbk, shared_from_this(),
                                                      std::placeholders::_1)));
    } else {
        VSOMEIP_WARNING << "ltcei::connect: Error opening socket: " << its_error.message() << " (" << std::dec << its_error.value() << ")"
                        << " endpoint > " << this;
        try {
            boost::asio::post(strand_, std::bind(&client_endpoint_impl::connect_cbk, shared_from_this(), its_error));
        } catch (const std::exception& e) {
            VSOMEIP_ERROR << "ltcei::connect: " << e.what() << " endpoint > " << this;
        }
    }
}

void local_tcp_client_endpoint_impl::receive() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (socket_->is_open()) {
        socket_->async_receive(boost::asio::buffer(recv_buffer_),
                               strand_.wrap(std::bind(&local_tcp_client_endpoint_impl::receive_cbk,
                                                      std::dynamic_pointer_cast<local_tcp_client_endpoint_impl>(shared_from_this()),
                                                      std::placeholders::_1, std::placeholders::_2)));
    }
}

// this overrides client_endpoint_impl::send to disable the pull method
// for local communication
bool local_tcp_client_endpoint_impl::send(const uint8_t* _data, uint32_t _size) {
    std::lock_guard<std::recursive_mutex> its_lock(mutex_);

    if (endpoint_impl::sending_blocked_ || !check_queue_limit(_data, _size)) {
        return false;
    }
    if (!check_message_size(_size)) {
        return false;
    }
    if (!check_packetizer_space(_size)) {
        return false;
    }
#if 0
    std::stringstream msg;
    msg << "lce::send: ";
    for (uint32_t i = 0; i < _size; i++)
        msg << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(_data[i]) << " ";
    VSOMEIP_INFO << msg.str();
#endif
    queue_train_buffer(_size);
    train_->buffer_->insert(train_->buffer_->end(), _data, _data + _size);
    queue_train(train_);
    train_->buffer_ = std::make_shared<message_buffer_t>();
    return true;
}

void local_tcp_client_endpoint_impl::send_queued(std::pair<message_buffer_ptr_t, uint32_t>& _entry) {

    static const byte_t its_start_tag[] = {0x67, 0x37, 0x6D, 0x07};
    static const byte_t its_end_tag[] = {0x07, 0x6D, 0x37, 0x67};
    std::vector<boost::asio::const_buffer> bufs;

    bufs.push_back(boost::asio::buffer(its_start_tag));
    bufs.push_back(boost::asio::buffer(*_entry.first));
    bufs.push_back(boost::asio::buffer(its_end_tag));

    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        if (socket_->is_open()) {
            socket_->async_write(bufs,
                                 // ensure the callback keeps the buffer alive by copying the
                                 // shared_ptr of the buffer
                                 strand_.wrap([self = shared_from_this(), buffer = _entry.first](auto ec, size_t length) {
                                     self->send_cbk(ec, length, buffer);
                                 }));
        } else {
            VSOMEIP_WARNING << "ltcei::" << __func__ << ": try to send while socket was not open | endpoint > " << this;
            was_not_connected_ = true;
            is_sending_ = false;
        }
    }
}

void local_tcp_client_endpoint_impl::get_configured_times_from_endpoint(service_t _service, method_t _method,
                                                                        std::chrono::nanoseconds* _debouncing,
                                                                        std::chrono::nanoseconds* _maximum_retention) const {

    (void)_service;
    (void)_method;
    (void)_debouncing;
    (void)_maximum_retention;
    VSOMEIP_ERROR << "ltcei::get_configured_times_from_endpoint called." << " endpoint > " << this;
}

void local_tcp_client_endpoint_impl::send_magic_cookie() { }

void local_tcp_client_endpoint_impl::receive_cbk(boost::system::error_code const& _error, std::size_t _bytes) {

    if (_error) {
        VSOMEIP_INFO << "ltcei::" << __func__ << " Error: " << _error.message() << " endpoint > " << this << " state_ > "
                     << to_string(state_.load());
        if (_error == boost::asio::error::operation_aborted) {
            // endpoint was stopped
            return;
        } else if (_error == boost::asio::error::eof) {
            std::scoped_lock its_lock(mutex_);
            sending_blocked_ = false;
            queue_.clear();
            queue_size_ = 0;

            if (is_stopping_) {
                queue_cv_.notify_all();
            }
        }
        error_handler_t handler;
        {
            std::lock_guard<std::mutex> its_lock(error_handler_mutex_);
            handler = error_handler_;
        }
        if (handler)
            handler();
    } else {

#if 0
        std::stringstream msg;
        msg << "lce<" << this << ">::recv: ";
        for (std::size_t i = 0; i < recv_buffer_.size(); i++)
            msg << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<int>(recv_buffer_[i]) << " ";
        VSOMEIP_INFO << msg.str();
#endif

        // We only handle a single message here. Check whether the message
        // format matches what we do expect.
        // TODO: Replace the magic numbers.
        if (_bytes == VSOMEIP_LOCAL_CLIENT_ENDPOINT_RECV_BUFFER_SIZE && recv_buffer_[0] == 0x67 && recv_buffer_[1] == 0x37
            && recv_buffer_[2] == 0x6d && recv_buffer_[3] == 0x07 && recv_buffer_[4] == byte_t(protocol::id_e::ASSIGN_CLIENT_ACK_ID)
            && recv_buffer_[15] == 0x07 && recv_buffer_[16] == 0x6d && recv_buffer_[17] == 0x37 && recv_buffer_[18] == 0x67) {

            auto its_routing_host = routing_host_.lock();
            if (its_routing_host)
                its_routing_host->on_message(&recv_buffer_[4], static_cast<length_t>(recv_buffer_.size() - 8), this);
        }

        receive();
    }
}

std::uint16_t local_tcp_client_endpoint_impl::get_local_port() const {
    return local_.port();
}

std::uint16_t local_tcp_client_endpoint_impl::get_remote_port() const {
    return remote_.port();
}

bool local_tcp_client_endpoint_impl::get_remote_address(boost::asio::ip::address& _address) const {
    if (remote_.address().is_unspecified()) {
        return false;
    }
    _address = remote_.address();
    return true;
}

void local_tcp_client_endpoint_impl::print_status() {

    std::string its_path("");
    std::size_t its_data_size(0);
    std::size_t its_queue_size(0);
    {
        std::lock_guard<std::recursive_mutex> its_lock(mutex_);
        its_queue_size = queue_.size();
        its_data_size = queue_size_;
    }

    VSOMEIP_INFO << "status lce: " << its_path << " queue: " << its_queue_size << " data: " << its_data_size;
}

std::string local_tcp_client_endpoint_impl::get_remote_information() const {
    return remote_.address().to_string() + ":" + std::to_string(remote_.port());
}

bool local_tcp_client_endpoint_impl::check_packetizer_space(std::uint32_t _size) const {
    if (train_->buffer_->size() + _size < train_->buffer_->size()) {
        VSOMEIP_ERROR << "ltcei: Overflow in packetizer addition ~> abort sending!"
                      << " endpoint > " << this;
        return false;
    }
    return true;
}

bool local_tcp_client_endpoint_impl::queue_train_buffer(std::uint32_t _size) {
    if (train_->buffer_->size() + _size > max_message_size_ && !train_->buffer_->empty()) {
        queue_.push_back(std::make_pair(train_->buffer_, 0));
        queue_size_ += train_->buffer_->size();
        train_->buffer_ = std::make_shared<message_buffer_t>();
        return true;
    }
    return false;
}

bool local_tcp_client_endpoint_impl::is_reliable() const {

    return true;
}

std::uint32_t local_tcp_client_endpoint_impl::get_max_allowed_reconnects() const {

    return MAX_RECONNECTS_LOCAL_TCP;
}

void local_tcp_client_endpoint_impl::max_allowed_reconnects_reached() {

    VSOMEIP_ERROR << "ltcei::max_allowed_reconnects_reached: " << get_remote_information() << " endpoint > " << this;
    error_handler_t handler;
    {
        std::lock_guard<std::mutex> its_lock(error_handler_mutex_);
        handler = error_handler_;
    }
    if (handler)
        handler();
}

} // namespace vsomeip_v3

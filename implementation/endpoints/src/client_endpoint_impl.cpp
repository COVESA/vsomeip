// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>

#include "../include/client_endpoint_impl.hpp"
#include "../include/endpoint_host.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

template<typename Protocol>
client_endpoint_impl<Protocol>::client_endpoint_impl(
        std::shared_ptr<endpoint_host> _host,
        endpoint_type _local,
        endpoint_type _remote,
        boost::asio::io_service &_io,
        std::uint32_t _max_message_size)
        : endpoint_impl<Protocol>(_host, _local, _io, _max_message_size),
          socket_(_io), remote_(_remote),
          flush_timer_(_io), connect_timer_(_io),
          connect_timeout_(VSOMEIP_DEFAULT_CONNECT_TIMEOUT), // TODO: use config variable
          is_connected_(false),
          packetizer_(std::make_shared<message_buffer_t>()),
          was_not_connected_(false) {
}

template<typename Protocol>
client_endpoint_impl<Protocol>::~client_endpoint_impl() {
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::is_client() const {
    return true;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::is_connected() const {
    return is_connected_;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::stop() {
    if (socket_.is_open()) {
        connect_timer_.cancel();
        connect_timeout_ = VSOMEIP_DEFAULT_CONNECT_TIMEOUT;
        endpoint_impl<Protocol>::sending_blocked_ = true;
        bool send_queue_empty(false);
        std::uint32_t times_slept(0);

        while (times_slept <= 50) {
            mutex_.lock();
            send_queue_empty = (queue_.size() == 0);
            mutex_.unlock();
            if (send_queue_empty) {
                break;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                times_slept++;
            }
        }

        boost::system::error_code its_error;
        socket_.close(its_error);
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::restart() {
    is_connected_ = false;
    connect_timer_.expires_from_now(
            std::chrono::milliseconds(connect_timeout_));
    connect_timer_.async_wait(
            std::bind(&client_endpoint_impl<Protocol>::wait_connect_cbk,
                      this->shared_from_this(), std::placeholders::_1));
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::send_to(
        const std::shared_ptr<endpoint_definition> _target, const byte_t *_data,
        uint32_t _size, bool _flush) {
    (void)_target;
    (void)_data;
    (void)_size;
    (void)_flush;

    VSOMEIP_ERROR << "Clients endpoints must not be used to "
            << "send to explicitely specified targets";
    return false;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::send(const uint8_t *_data,
        uint32_t _size, bool _flush) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (endpoint_impl<Protocol>::sending_blocked_) {
        return false;
    }
#if 0
    std::stringstream msg;
    msg << "cei::send: ";
    for (uint32_t i = 0; i < _size; i++)
    msg << std::hex << std::setw(2) << std::setfill('0')
    << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif

    const bool queue_size_zero_on_entry(queue_.empty());
    if (packetizer_->size() + _size > endpoint_impl<Protocol>::max_message_size_
            && !packetizer_->empty()) {
        queue_.push_back(packetizer_);
        packetizer_ = std::make_shared<message_buffer_t>();
    }

    packetizer_->insert(packetizer_->end(), _data, _data + _size);

    if (_flush) {
        flush_timer_.cancel();
        queue_.push_back(packetizer_);
        packetizer_ = std::make_shared<message_buffer_t>();
    } else {
        flush_timer_.expires_from_now(
                std::chrono::milliseconds(VSOMEIP_DEFAULT_FLUSH_TIMEOUT)); // TODO: use config variable
        flush_timer_.async_wait(
                        std::bind(
                            &client_endpoint_impl<Protocol>::flush_cbk,
                            this->shared_from_this(),
                            std::placeholders::_1));
    }

    if (queue_size_zero_on_entry && !queue_.empty()) { // no writing in progress
        send_queued();
    }

    return (true);
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::flush() {
    bool is_successful(true);

    if (!packetizer_->empty()) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        queue_.push_back(packetizer_);
        packetizer_ = std::make_shared<message_buffer_t>();
        if (queue_.size() == 1) { // no writing in progress
            send_queued();
        }
    } else {
        is_successful = false;
    }

    return is_successful;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::connect_cbk(
        boost::system::error_code const &_error) {
    std::shared_ptr<endpoint_host> its_host = this->host_.lock();
    if (its_host) {
        if (_error && _error != boost::asio::error::already_connected) {
            if(socket_.is_open()) {
                boost::system::error_code its_error;
                socket_.close(its_error);
            }

            connect_timer_.expires_from_now(
                    std::chrono::milliseconds(connect_timeout_));
            connect_timer_.async_wait(
                    std::bind(&client_endpoint_impl<Protocol>::wait_connect_cbk,
                              this->shared_from_this(), std::placeholders::_1));

            // Double the timeout as long as the maximum allowed is larger
            if (connect_timeout_ < VSOMEIP_MAX_CONNECT_TIMEOUT)
            	connect_timeout_ <<= 1;

            if (is_connected_) {
                is_connected_ = false;
                its_host->on_disconnect(this->shared_from_this());
            }
        } else {
            connect_timer_.cancel();
            connect_timeout_ = VSOMEIP_DEFAULT_CONNECT_TIMEOUT; // TODO: use config variable

            if (!is_connected_) {
                is_connected_ = true;
                its_host->on_connect(this->shared_from_this());
            }

            receive();

            std::lock_guard<std::mutex> its_lock(mutex_);
            if (queue_.size() > 0 && was_not_connected_) {
                was_not_connected_ = false;
                send_queued();
            }
        }
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::wait_connect_cbk(
        boost::system::error_code const &_error) {
    if (!_error) {
        connect();
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::send_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_bytes;
    if (!_error) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        if (queue_.size() > 0) {
            queue_.pop_front();
            send_queued();
        }
    } else if (_error == boost::asio::error::broken_pipe) {
        is_connected_ = false;
        if (endpoint_impl<Protocol>::sending_blocked_) {
        	std::lock_guard<std::mutex> its_lock(mutex_);
        	queue_.clear();
        }
        if (socket_.is_open()) {
            boost::system::error_code its_error;
            socket_.close(its_error);
        }
        connect();
    } else if (_error == boost::asio::error::not_connected
            || _error == boost::asio::error::bad_descriptor) {
        was_not_connected_ = true;
        connect();
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::flush_cbk(
        boost::system::error_code const &_error) {
    if (!_error) {
        (void) flush();
    }
}

// Instantiate template
#ifndef WIN32
template class client_endpoint_impl<boost::asio::local::stream_protocol>;
#endif
template class client_endpoint_impl<boost::asio::ip::tcp>;
template class client_endpoint_impl<boost::asio::ip::udp>;
}  // namespace vsomeip


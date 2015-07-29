// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>
#include <sstream>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/client_endpoint_impl.hpp"
#include "../include/endpoint_host.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

template<typename Protocol, int MaxBufferSize>
client_endpoint_impl<Protocol, MaxBufferSize>::client_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _remote,
        boost::asio::io_service &_io)
        : endpoint_impl<MaxBufferSize>(_host, _io), socket_(_io),
          connect_timer_(_io), flush_timer_(_io), remote_(_remote),
          packetizer_(std::make_shared<message_buffer_t>()),
          connect_timeout_(VSOMEIP_DEFAULT_CONNECT_TIMEOUT), // TODO: use config variable
          is_connected_(false) {
}

template<typename Protocol, int MaxBufferSize>
client_endpoint_impl<Protocol, MaxBufferSize>::~client_endpoint_impl() {
}

template<typename Protocol, int MaxBufferSize>
bool client_endpoint_impl<Protocol, MaxBufferSize>::is_client() const {
    return true;
}

template<typename Protocol, int MaxBufferSize>
bool client_endpoint_impl<Protocol, MaxBufferSize>::is_connected() const {
    return is_connected_;
}

template<typename Protocol, int MaxBufferSize>
void client_endpoint_impl<Protocol, MaxBufferSize>::stop() {
    if (socket_.is_open()) {
        socket_.close();
    }
}

template<typename Protocol, int MaxBufferSize>
void client_endpoint_impl<Protocol, MaxBufferSize>::restart() {
    receive();
}

template<typename Protocol, int MaxBufferSize>
bool client_endpoint_impl<Protocol, MaxBufferSize>::send_to(
        const std::shared_ptr<endpoint_definition> _target, const byte_t *_data,
        uint32_t _size, bool _flush) {
    VSOMEIP_ERROR<< "Clients endpoints must not be used to "
    << "send to explicitely specified targets";
    return false;
}

template<typename Protocol, int MaxBufferSize>
bool client_endpoint_impl<Protocol, MaxBufferSize>::send(const uint8_t *_data,
        uint32_t _size, bool _flush) {
    std::lock_guard<std::mutex> its_lock(mutex_);
#if 0
    std::stringstream msg;
    msg << "cei::send: ";
    for (uint32_t i = 0; i < _size; i++)
    msg << std::hex << std::setw(2) << std::setfill('0')
    << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    if (packetizer_->size() + _size > MaxBufferSize) {
        send_queued(packetizer_);
        packetizer_ = std::make_shared<message_buffer_t>();
    }

    packetizer_->insert(packetizer_->end(), _data, _data + _size);

    if (_flush) {
        flush_timer_.cancel();
        send_queued(packetizer_);
        packetizer_ = std::make_shared<message_buffer_t>();
    } else {
        flush_timer_.expires_from_now(
                std::chrono::milliseconds(VSOMEIP_DEFAULT_FLUSH_TIMEOUT)); // TODO: use config variable
        flush_timer_.async_wait(
                std::bind(
                        &client_endpoint_impl<Protocol, MaxBufferSize>::flush_cbk,
                        this->shared_from_this(), std::placeholders::_1));
    }

    return (true);
}

template<typename Protocol, int MaxBufferSize>
bool client_endpoint_impl<Protocol, MaxBufferSize>::flush() {
    bool is_successful(true);

    if (!packetizer_->empty()) {
        send_queued(packetizer_);
        packetizer_ = std::make_shared<message_buffer_t>();
    } else {
        is_successful = false;
    }

    return is_successful;
}

template<typename Protocol, int MaxBufferSize>
void client_endpoint_impl<Protocol, MaxBufferSize>::connect_cbk(
        boost::system::error_code const &_error) {
    std::shared_ptr<endpoint_host> its_host = this->host_.lock();
    if (its_host) {
        if (_error) {
            socket_.close();

            connect_timer_.expires_from_now(
                    std::chrono::milliseconds(connect_timeout_));
            connect_timer_.async_wait(
                    std::bind(&client_endpoint_impl<
                                Protocol, MaxBufferSize>::wait_connect_cbk,
                              this->shared_from_this(), std::placeholders::_1));

            // next time we wait longer
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
        }
    }
}

template<typename Protocol, int MaxBufferSize>
void client_endpoint_impl<Protocol, MaxBufferSize>::wait_connect_cbk(
        boost::system::error_code const &_error) {
    if (!_error) {
        connect();
    }
}

template<typename Protocol, int MaxBufferSize>
void client_endpoint_impl<Protocol, MaxBufferSize>::send_cbk(
        message_buffer_ptr_t _buffer, boost::system::error_code const &_error,
        std::size_t _bytes) {
#if 0
    std::stringstream msg;
    msg << "cei<" << this << ">::scb (" << _error.message() << "): ";
    for (std::size_t i = 0; i < _data->size(); ++i)
    msg << std::hex << std::setw(2) << std::setfill('0')
    << (int)(*_buffer)[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    if (_error == boost::asio::error::broken_pipe) {
        is_connected_ = false;
        socket_.close();
        connect();
    }
}

template<typename Protocol, int MaxBufferSize>
void client_endpoint_impl<Protocol, MaxBufferSize>::flush_cbk(
        boost::system::error_code const &_error) {
    if (!_error) {
        (void) flush();
    }
}

// Instantiate template
#ifndef WIN32
template class client_endpoint_impl<boost::asio::local::stream_protocol,
VSOMEIP_MAX_LOCAL_MESSAGE_SIZE> ;
#endif
template class client_endpoint_impl<boost::asio::ip::tcp,
VSOMEIP_MAX_TCP_MESSAGE_SIZE> ;
template class client_endpoint_impl<boost::asio::ip::udp,
VSOMEIP_MAX_UDP_MESSAGE_SIZE> ;

}  // namespace vsomeip


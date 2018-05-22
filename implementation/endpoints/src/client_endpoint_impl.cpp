// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <limits>

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
#include "../../utility/include/byteorder.hpp"

namespace vsomeip {

template<typename Protocol>
client_endpoint_impl<Protocol>::client_endpoint_impl(
        std::shared_ptr<endpoint_host> _host,
        endpoint_type _local,
        endpoint_type _remote,
        boost::asio::io_service &_io,
        std::uint32_t _max_message_size,
        configuration::endpoint_queue_limit_t _queue_limit)
        : endpoint_impl<Protocol>(_host, _local, _io, _max_message_size, _queue_limit),
          socket_(new socket_type(_io)), remote_(_remote),
          flush_timer_(_io), connect_timer_(_io),
          connect_timeout_(VSOMEIP_DEFAULT_CONNECT_TIMEOUT), // TODO: use config variable
          state_(cei_state_e::CLOSED),
          packetizer_(std::make_shared<message_buffer_t>()),
          queue_size_(0),
          was_not_connected_(false),
          local_port_(0) {
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
    return state_ == cei_state_e::ESTABLISHED;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::set_connected(bool _connected) {
    if (_connected) {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        if (socket_->is_open()) {
            state_ = cei_state_e::ESTABLISHED;
        } else {
            state_ = cei_state_e::CLOSED;
        }
    } else {
        state_ = cei_state_e::CLOSED;
    }}
template<typename Protocol>void client_endpoint_impl<Protocol>::stop() {
    {
        std::lock_guard<std::mutex> its_lock(mutex_);
        endpoint_impl<Protocol>::sending_blocked_ = true;
        // delete unsent messages
        queue_.clear();
        queue_size_ = 0;
    }
    {
        std::lock_guard<std::mutex> its_lock(connect_timer_mutex_);
        boost::system::error_code ec;
        connect_timer_.cancel(ec);
    }
    connect_timeout_ = VSOMEIP_DEFAULT_CONNECT_TIMEOUT;
    shutdown_and_close_socket(false);
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
    bool ret(true);
    const bool queue_size_zero_on_entry(queue_.empty());
    if (endpoint_impl<Protocol>::sending_blocked_ ||
        !check_message_size(_size) ||
        !check_packetizer_space(_size) ||
        !check_queue_limit(_data, _size)) {
        ret = false;
    } else {
#if 0
        std::stringstream msg;
        msg << "cei::send: ";
        for (uint32_t i = 0; i < _size; i++)
        msg << std::hex << std::setw(2) << std::setfill('0')
        << (int)_data[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif
        packetizer_->insert(packetizer_->end(), _data, _data + _size);
        send_or_start_flush_timer(_flush, queue_size_zero_on_entry);
    }
    return ret;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::send(const std::vector<byte_t>& _cmd_header,
                                      const byte_t *_data, uint32_t _size,
                                      bool _flush) {
    (void) _cmd_header;
    (void) _data;
    (void) _size;
    (void) _flush;
    return false;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::flush() {
    bool is_successful(true);
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (!packetizer_->empty()) {
        queue_.push_back(packetizer_);
        queue_size_ += packetizer_->size();
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
    if (_error == boost::asio::error::operation_aborted) {
        // endpoint was stopped
        shutdown_and_close_socket(false);
        return;
    }
    std::shared_ptr<endpoint_host> its_host = this->host_.lock();
    if (its_host) {
        if (_error && _error != boost::asio::error::already_connected) {
            shutdown_and_close_socket(true);

            if (state_ != cei_state_e::ESTABLISHED) {
                state_ = cei_state_e::CLOSED;
                its_host->on_disconnect(this->shared_from_this());
            }
            start_connect_timer();
            // Double the timeout as long as the maximum allowed is larger
            if (connect_timeout_ < VSOMEIP_MAX_CONNECT_TIMEOUT)
                connect_timeout_ = (connect_timeout_ << 1);
        } else {
            {
                std::lock_guard<std::mutex> its_lock(connect_timer_mutex_);
                connect_timer_.cancel();
            }
            connect_timeout_ = VSOMEIP_DEFAULT_CONNECT_TIMEOUT; // TODO: use config variable
            set_local_port();
            if (state_ != cei_state_e::ESTABLISHED) {
                its_host->on_connect(this->shared_from_this());
            }

            receive();

            if (was_not_connected_) {
                was_not_connected_ = false;
                std::lock_guard<std::mutex> its_lock(mutex_);
                if (queue_.size() > 0) {
                    send_queued();
                    VSOMEIP_WARNING << __func__ << ": resume sending to: "
                            << get_remote_information();
                }
            }
        }
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::wait_connect_cbk(
        boost::system::error_code const &_error) {
    if (!_error && !client_endpoint_impl<Protocol>::sending_blocked_) {
        connect();
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::send_cbk(
        boost::system::error_code const &_error, std::size_t _bytes,
        message_buffer_ptr_t _sent_msg) {
    (void)_bytes;
    if (!_error) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        if (queue_.size() > 0) {
            queue_size_ -= queue_.front()->size();
            queue_.pop_front();
            send_queued();
        }
    } else if (_error == boost::asio::error::broken_pipe) {
        state_ = cei_state_e::CLOSED;
        bool stopping(false);
        {
            std::lock_guard<std::mutex> its_lock(mutex_);
            stopping = endpoint_impl<Protocol>::sending_blocked_;
            if (stopping) {
                queue_.clear();
                queue_size_ = 0;
            } else {
                service_t its_service(0);
                method_t its_method(0);
                client_t its_client(0);
                session_t its_session(0);
                if (_sent_msg && _sent_msg->size() > VSOMEIP_SESSION_POS_MAX) {
                    its_service = VSOMEIP_BYTES_TO_WORD(
                            (*_sent_msg)[VSOMEIP_SERVICE_POS_MIN],
                            (*_sent_msg)[VSOMEIP_SERVICE_POS_MAX]);
                    its_method = VSOMEIP_BYTES_TO_WORD(
                            (*_sent_msg)[VSOMEIP_METHOD_POS_MIN],
                            (*_sent_msg)[VSOMEIP_METHOD_POS_MAX]);
                    its_client = VSOMEIP_BYTES_TO_WORD(
                            (*_sent_msg)[VSOMEIP_CLIENT_POS_MIN],
                            (*_sent_msg)[VSOMEIP_CLIENT_POS_MAX]);
                    its_session = VSOMEIP_BYTES_TO_WORD(
                            (*_sent_msg)[VSOMEIP_SESSION_POS_MIN],
                            (*_sent_msg)[VSOMEIP_SESSION_POS_MAX]);
                }
                VSOMEIP_WARNING << "cei::send_cbk received error: "
                        << _error.message() << " (" << std::dec
                        << _error.value() << ") " << get_remote_information()
                        << " " << std::dec << queue_.size()
                        << " " << std::dec << queue_size_ << " ("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_session << "]";
            }
        }
        if (!stopping) {
            print_status();
        }
        was_not_connected_ = true;
        shutdown_and_close_socket(true);
        connect();
    } else if (_error == boost::asio::error::not_connected
            || _error == boost::asio::error::bad_descriptor) {
        state_ = cei_state_e::CLOSED;
        was_not_connected_ = true;
        shutdown_and_close_socket(true);
        connect();
    } else if (_error == boost::asio::error::operation_aborted) {
        // endpoint was stopped
        shutdown_and_close_socket(false);
    } else if (_error == boost::system::errc::destination_address_required) {
        VSOMEIP_WARNING << "cei::send_cbk received error: " << _error.message()
                << " (" << std::dec << _error.value() << ") "
                << get_remote_information();
        was_not_connected_ = true;
    } else {
        service_t its_service(0);
        method_t its_method(0);
        client_t its_client(0);
        session_t its_session(0);
        if (_sent_msg && _sent_msg->size() > VSOMEIP_SESSION_POS_MAX) {
            its_service = VSOMEIP_BYTES_TO_WORD(
                    (*_sent_msg)[VSOMEIP_SERVICE_POS_MIN],
                    (*_sent_msg)[VSOMEIP_SERVICE_POS_MAX]);
            its_method = VSOMEIP_BYTES_TO_WORD(
                    (*_sent_msg)[VSOMEIP_METHOD_POS_MIN],
                    (*_sent_msg)[VSOMEIP_METHOD_POS_MAX]);
            its_client = VSOMEIP_BYTES_TO_WORD(
                    (*_sent_msg)[VSOMEIP_CLIENT_POS_MIN],
                    (*_sent_msg)[VSOMEIP_CLIENT_POS_MAX]);
            its_session = VSOMEIP_BYTES_TO_WORD(
                    (*_sent_msg)[VSOMEIP_SESSION_POS_MIN],
                    (*_sent_msg)[VSOMEIP_SESSION_POS_MAX]);
        }
        VSOMEIP_WARNING << "cei::send_cbk received error: " << _error.message()
                << " (" << std::dec << _error.value() << ") "
                << get_remote_information() << " "
                << " " << std::dec << queue_.size()
                << " " << std::dec << queue_size_ << " ("
                << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_session << "]";
        print_status();
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::flush_cbk(
        boost::system::error_code const &_error) {
    if (!_error) {
        (void) flush();
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::shutdown_and_close_socket(bool _recreate_socket) {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    shutdown_and_close_socket_unlocked(_recreate_socket);
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::shutdown_and_close_socket_unlocked(bool _recreate_socket) {
    local_port_ = 0;
    if (socket_->is_open()) {
        boost::system::error_code its_error;
        socket_->shutdown(Protocol::socket::shutdown_both, its_error);
        socket_->close(its_error);
    }
    if (_recreate_socket) {
        socket_.reset(new socket_type(endpoint_impl<Protocol>::service_));
    }
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::get_remote_address(
        boost::asio::ip::address &_address) const {
    (void)_address;
    return false;
}

template<typename Protocol>
std::uint16_t client_endpoint_impl<Protocol>::get_remote_port() const {
    return 0;
}

template<typename Protocol>
std::uint16_t client_endpoint_impl<Protocol>::get_local_port() const {
    return local_port_;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::start_connect_timer() {
    std::lock_guard<std::mutex> its_lock(connect_timer_mutex_);
    connect_timer_.expires_from_now(
            std::chrono::milliseconds(connect_timeout_));
    connect_timer_.async_wait(
            std::bind(&client_endpoint_impl<Protocol>::wait_connect_cbk,
                      this->shared_from_this(), std::placeholders::_1));
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::check_message_size(std::uint32_t _size) const {
    if (endpoint_impl<Protocol>::max_message_size_ != MESSAGE_SIZE_UNLIMITED
            && _size > endpoint_impl<Protocol>::max_message_size_) {
        VSOMEIP_ERROR << "cei::check_message_size: Dropping to big message ("
                << std::dec << _size << " Bytes). Maximum allowed message size is: "
                << endpoint_impl<Protocol>::max_message_size_ << " Bytes.";
        return false;
    }
    return true;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::check_packetizer_space(std::uint32_t _size) {
    if (packetizer_->size() + _size < packetizer_->size()) {
        VSOMEIP_ERROR << "Overflow in packetizer addition ~> abort sending!";
        return false;
    }
    if (packetizer_->size() + _size > endpoint_impl<Protocol>::max_message_size_
            && !packetizer_->empty()) {
        queue_.push_back(packetizer_);
        queue_size_ += packetizer_->size();
        packetizer_ = std::make_shared<message_buffer_t>();
    }
    return true;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::check_queue_limit(const uint8_t *_data, std::uint32_t _size) const {
    if (endpoint_impl<Protocol>::queue_limit_ != QUEUE_SIZE_UNLIMITED
            && queue_size_ + _size > endpoint_impl<Protocol>::queue_limit_) {
        service_t its_service(0);
        method_t its_method(0);
        client_t its_client(0);
        session_t its_session(0);
        if (_size >= VSOMEIP_SESSION_POS_MAX) {
            // this will yield wrong IDs for local communication as the commands
            // are prepended to the actual payload
            // it will print:
            // (lowbyte service ID + highbyte methoid)
            // [(Command + lowerbyte sender's client ID).
            //  highbyte sender's client ID + lowbyte command size.
            //  lowbyte methodid + highbyte vsomeipd length]
            its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
                                                _data[VSOMEIP_SERVICE_POS_MAX]);
            its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
                                               _data[VSOMEIP_METHOD_POS_MAX]);
            its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN],
                                               _data[VSOMEIP_CLIENT_POS_MAX]);
            its_session = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SESSION_POS_MIN],
                                                _data[VSOMEIP_SESSION_POS_MAX]);
        }
        VSOMEIP_ERROR << "cei::check_queue_limit: queue size limit (" << std::dec
                << endpoint_impl<Protocol>::queue_limit_
                << ") reached. Dropping message ("
                << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_session << "] "
                << "queue_size: " << std::dec << queue_size_
                << " data size: " << std::dec << _size;
        return false;
    }
    return true;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::send_or_start_flush_timer(
        bool _flush, bool _queue_size_zero_on_entry) {
    if (_flush) {
        flush_timer_.cancel();
        queue_.push_back(packetizer_);
        queue_size_ += packetizer_->size();
        packetizer_ = std::make_shared<message_buffer_t>();

        if (_queue_size_zero_on_entry && !queue_.empty()) { // no writing in progress
            send_queued();
        }
    } else {
        flush_timer_.expires_from_now(
                std::chrono::milliseconds(VSOMEIP_DEFAULT_FLUSH_TIMEOUT)); // TODO: use config variable
        flush_timer_.async_wait(
            std::bind(
                &client_endpoint_impl<Protocol>::flush_cbk,
                this->shared_from_this(),
                std::placeholders::_1
            )
        );
    }
}

// Instantiate template
#ifndef _WIN32
template class client_endpoint_impl<boost::asio::local::stream_protocol>;
#endif
template class client_endpoint_impl<boost::asio::ip::tcp>;
template class client_endpoint_impl<boost::asio::ip::udp>;
}  // namespace vsomeip


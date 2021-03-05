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
#include <vsomeip/internal/logger.hpp>

#include "../include/client_endpoint_impl.hpp"
#include "../include/endpoint_host.hpp"
#include "../../utility/include/utility.hpp"
#include "../../utility/include/byteorder.hpp"

namespace vsomeip_v3 {

template<typename Protocol>
client_endpoint_impl<Protocol>::client_endpoint_impl(
        const std::shared_ptr<endpoint_host>& _endpoint_host,
        const std::shared_ptr<routing_host>& _routing_host,
        const endpoint_type& _local,
        const endpoint_type& _remote,
        boost::asio::io_service &_io,
        std::uint32_t _max_message_size,
        configuration::endpoint_queue_limit_t _queue_limit,
        const std::shared_ptr<configuration>& _configuration)
        : endpoint_impl<Protocol>(_endpoint_host, _routing_host, _local, _io,
                _max_message_size, _queue_limit, _configuration),
          socket_(new socket_type(_io)), remote_(_remote),
          flush_timer_(_io), connect_timer_(_io),
          connect_timeout_(VSOMEIP_DEFAULT_CONNECT_TIMEOUT), // TODO: use config variable
          state_(cei_state_e::CLOSED),
          reconnect_counter_(0),
          train_(_io),
          queue_size_(0),
          was_not_connected_(false),
          local_port_(0),
          strand_(_io) {
}

template<typename Protocol>
client_endpoint_impl<Protocol>::~client_endpoint_impl() {
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::is_client() const {
    return true;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::is_established() const {
    return state_ == cei_state_e::ESTABLISHED;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::is_established_or_connected() const {
    return (state_ == cei_state_e::ESTABLISHED
            || state_ == cei_state_e::CONNECTED);
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::set_established(bool _established) {
    if (_established) {
        if (state_ != cei_state_e::CONNECTING) {
            std::lock_guard<std::mutex> its_lock(socket_mutex_);
            if (socket_->is_open()) {
                state_ = cei_state_e::ESTABLISHED;
            } else {
                state_ = cei_state_e::CLOSED;
            }
        }
    } else {
        state_ = cei_state_e::CLOSED;
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::set_connected(bool _connected) {
    if (_connected) {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        if (socket_->is_open()) {
            state_ = cei_state_e::CONNECTED;
        } else {
            state_ = cei_state_e::CLOSED;
        }
    } else {
        state_ = cei_state_e::CLOSED;
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::prepare_stop(
        endpoint::prepare_stop_handler_t _handler, service_t _service) {
    (void) _handler;
    (void) _service;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::stop() {
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
        uint32_t _size) {
    (void)_target;
    (void)_data;
    (void)_size;
    VSOMEIP_ERROR << "Clients endpoints must not be used to "
            << "send to explicitely specified targets";
    return false;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::send_error(
        const std::shared_ptr<endpoint_definition> _target, const byte_t *_data,
        uint32_t _size) {
    (void)_target;
    (void)_data;
    (void)_size;
    VSOMEIP_ERROR << "Clients endpoints must not be used to "
            << "send errors to explicitly specified targets";
    return false;
}


template<typename Protocol>
bool client_endpoint_impl<Protocol>::send(const uint8_t *_data, uint32_t _size) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    bool must_depart(false);
    const bool queue_size_zero_on_entry(queue_.empty());
#if 0
    std::stringstream msg;
    msg << "cei::send: ";
    for (uint32_t i = 0; i < _size; i++)
    msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif

    if (endpoint_impl<Protocol>::sending_blocked_ ||
        !check_queue_limit(_data, _size)) {
        return false;
    }
    switch (check_message_size(_data, _size)) {
        case endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT:
            return true;
            break;
        case endpoint_impl<Protocol>::cms_ret_e::MSG_TOO_BIG:
            return false;
            break;
        case endpoint_impl<Protocol>::cms_ret_e::MSG_OK:
        default:
            break;
    }

    // STEP 1: Determine elapsed time and update the departure time and cancel the departure timer
    train_.update_departure_time_and_stop_departure();

    // STEP 3: Get configured timings
    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
            _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
    const method_t its_method = VSOMEIP_BYTES_TO_WORD(
            _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
    std::chrono::nanoseconds its_debouncing(0), its_retention(0);
    get_configured_times_from_endpoint(its_service, its_method,
                                       &its_debouncing, &its_retention);

    // STEP 4: Check if the passenger enters an empty train
    const std::pair<service_t, method_t> its_identifier = std::make_pair(
            its_service, its_method);
    if (train_.passengers_.empty()) {
        train_.departure_ = its_retention;
    } else {
        // STEP 4.1: Check whether the current train already contains the message
        if (train_.passengers_.end() != train_.passengers_.find(its_identifier)) {
            must_depart = true;
        } else {
            // STEP 5: Check whether the current message fits into the current train
            if (train_.buffer_->size() + _size > endpoint_impl<Protocol>::max_message_size_) {
                must_depart = true;
            } else {
                // STEP 6: Check debouncing time
                if (its_debouncing > train_.minimal_max_retention_time_) {
                    // train's latest departure would already undershot new
                    // passenger's debounce time
                    must_depart = true;
                } else {
                    if (its_debouncing > train_.departure_) {
                        // train departs earlier as the new passenger's debounce
                        // time allows
                        must_depart = true;
                    } else {
                        // STEP 7: Check maximum retention time
                        if (its_retention < train_.minimal_debounce_time_) {
                            // train's earliest departure would already exceed
                            // the new passenger's retention time.
                            must_depart = true;
                        } else {
                            if (its_retention < train_.departure_) {
                                train_.departure_ = its_retention;
                            }
                        }
                    }
                }
            }
        }
    }

    // STEP 8: if necessary, send current buffer and create a new one
    if (must_depart) {
        // STEP 8.1: check if debounce time would be undershot here if the train
        // departs. Block sending until train is allowed to depart.
        wait_until_debounce_time_reached();
        train_.passengers_.clear();
        queue_train(queue_size_zero_on_entry);
        train_.last_departure_ = std::chrono::steady_clock::now();
        train_.departure_ = its_retention;
        train_.minimal_debounce_time_ = std::chrono::nanoseconds::max();
        train_.minimal_max_retention_time_ = std::chrono::nanoseconds::max();
    }

    // STEP 9: insert current message buffer
    train_.buffer_->insert(train_.buffer_->end(), _data, _data + _size);
    train_.passengers_.insert(its_identifier);
    // STEP 9.1: update the trains minimal debounce time if necessary
    if (its_debouncing < train_.minimal_debounce_time_) {
        train_.minimal_debounce_time_ = its_debouncing;
    }
    // STEP 9.2: update the trains minimal maximum retention time if necessary
    if (its_retention < train_.minimal_max_retention_time_) {
        train_.minimal_max_retention_time_ = its_retention;
    }

    // STEP 10: restart timer with current departure time
#ifndef _WIN32
    train_.departure_timer_->expires_from_now(train_.departure_);
#else
    train_.departure_timer_->expires_from_now(
            std::chrono::duration_cast<
                std::chrono::steady_clock::duration>(train_.departure_));
#endif
    train_.departure_timer_->async_wait(
            std::bind(&client_endpoint_impl<Protocol>::flush_cbk,
                      this->shared_from_this(), std::placeholders::_1));

    return true;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::send_segments(
        const tp::tp_split_messages_t &_segments) {
    if (_segments.size() == 0) {
        return;
    }
    const bool queue_size_zero_on_entry(queue_.empty());

    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
            (*(_segments[0]))[VSOMEIP_SERVICE_POS_MIN],
            (*(_segments[0]))[VSOMEIP_SERVICE_POS_MAX]);
    const method_t its_method = VSOMEIP_BYTES_TO_WORD(
            (*(_segments[0]))[VSOMEIP_METHOD_POS_MIN],
            (*(_segments[0]))[VSOMEIP_METHOD_POS_MAX]);
    std::chrono::nanoseconds its_debouncing(0), its_retention(0);
    get_configured_times_from_endpoint(its_service, its_method,
                                       &its_debouncing, &its_retention);
    // update the trains minimal debounce time if necessary
    if (its_debouncing < train_.minimal_debounce_time_) {
        train_.minimal_debounce_time_ = its_debouncing;
    }
    // update the trains minimal maximum retention time if necessary
    if (its_retention < train_.minimal_max_retention_time_) {
        train_.minimal_max_retention_time_ = its_retention;
    }

    // We only need to respect the debouncing. There is no need to wait for further
    // messages as we will send several now anyway.
    if (!train_.passengers_.empty()) {
        wait_until_debounce_time_reached();
        train_.passengers_.clear();
        queue_train(queue_size_zero_on_entry);
        train_.last_departure_ = std::chrono::steady_clock::now();
        train_.departure_ = its_retention;
        train_.minimal_debounce_time_ = std::chrono::nanoseconds::max();
        train_.minimal_max_retention_time_ = std::chrono::nanoseconds::max();
    }
    const bool queue_size_still_zero(queue_.empty());
    for (const auto& s : _segments) {
        queue_.emplace_back(s);
        queue_size_ += s->size();
    }

    if (queue_size_still_zero && !queue_.empty()) { // no writing in progress
        // respect minimal debounce time
        wait_until_debounce_time_reached();
        // ignore retention time and send immediately as the train is full anyway
        send_queued();
    }
    train_.last_departure_ = std::chrono::steady_clock::now();
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::wait_until_debounce_time_reached() const {
    const std::chrono::nanoseconds time_since_last_departure =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - train_.last_departure_);
    if (time_since_last_departure < train_.minimal_debounce_time_) {
        std::this_thread::sleep_for(
                train_.minimal_debounce_time_ - time_since_last_departure);
    }
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::send(const std::vector<byte_t>& _cmd_header,
                                      const byte_t *_data, uint32_t _size) {
    (void) _cmd_header;
    (void) _data;
    (void) _size;
    return false;
}

template<typename Protocol>
bool client_endpoint_impl<Protocol>::flush() {
    bool is_successful(true);
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (!train_.buffer_->empty()) {
        queue_train(!queue_.size());
        train_.last_departure_ = std::chrono::steady_clock::now();
        train_.passengers_.clear();
        train_.minimal_debounce_time_ = std::chrono::nanoseconds::max();
        train_.minimal_max_retention_time_ = std::chrono::nanoseconds::max();
    } else {
        is_successful = false;
    }

    return is_successful;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::connect_cbk(
        boost::system::error_code const &_error) {
    if (_error == boost::asio::error::operation_aborted
            || endpoint_impl<Protocol>::sending_blocked_) {
        // endpoint was stopped
        shutdown_and_close_socket(false);
        return;
    }
    std::shared_ptr<endpoint_host> its_host = this->endpoint_host_.lock();
    if (its_host) {
        if (_error && _error != boost::asio::error::already_connected) {
            shutdown_and_close_socket(true);

            if (state_ != cei_state_e::ESTABLISHED) {
                state_ = cei_state_e::CLOSED;
                its_host->on_disconnect(this->shared_from_this());
            }
            if (get_max_allowed_reconnects() == MAX_RECONNECTS_UNLIMITED ||
                get_max_allowed_reconnects() >= ++reconnect_counter_) {
                start_connect_timer();
            } else {
                max_allowed_reconnects_reached();
            }
            // Double the timeout as long as the maximum allowed is larger
            if (connect_timeout_ < VSOMEIP_MAX_CONNECT_TIMEOUT)
                connect_timeout_ = (connect_timeout_ << 1);
        } else {
            {
                std::lock_guard<std::mutex> its_lock(connect_timer_mutex_);
                connect_timer_.cancel();
            }
            connect_timeout_ = VSOMEIP_DEFAULT_CONNECT_TIMEOUT; // TODO: use config variable
            reconnect_counter_ = 0;
            set_local_port();
            if (was_not_connected_) {
                was_not_connected_ = false;
                std::lock_guard<std::mutex> its_lock(mutex_);
                if (queue_.size() > 0) {
                    send_queued();
                    VSOMEIP_WARNING << __func__ << ": resume sending to: "
                            << get_remote_information();
                }
            }
            if (state_ != cei_state_e::ESTABLISHED) {
                its_host->on_connect(this->shared_from_this());
            }
            receive();
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
        const message_buffer_ptr_t& _sent_msg) {
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
            || _error == boost::asio::error::bad_descriptor
            || _error == boost::asio::error::no_permission) {
        state_ = cei_state_e::CLOSED;
        if (_error == boost::asio::error::no_permission) {
            VSOMEIP_WARNING << "cei::send_cbk received error: " << _error.message()
                    << " (" << std::dec << _error.value() << ") "
                    << get_remote_information();
            std::lock_guard<std::mutex> its_lock(mutex_);
            queue_.clear();
            queue_size_ = 0;
        }
        was_not_connected_ = true;
        shutdown_and_close_socket(true);
        connect();
    } else if (_error == boost::asio::error::operation_aborted) {
        VSOMEIP_WARNING << "cei::send_cbk received error: " << _error.message();
        // endpoint was stopped
        endpoint_impl<Protocol>::sending_blocked_ = true;
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
#ifndef _WIN32
        if (-1 == fcntl(socket_->native_handle(), F_GETFD)) {
            VSOMEIP_ERROR << "cei::shutdown_and_close_socket_unlocked: socket/handle closed already '"
                    << std::string(std::strerror(errno))
                    << "' (" << errno << ") " << get_remote_information();
        }
#endif
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
typename endpoint_impl<Protocol>::cms_ret_e client_endpoint_impl<Protocol>::check_message_size(
        const std::uint8_t * const _data, std::uint32_t _size) {
    typename endpoint_impl<Protocol>::cms_ret_e ret(endpoint_impl<Protocol>::cms_ret_e::MSG_OK);
    if (endpoint_impl<Protocol>::max_message_size_ != MESSAGE_SIZE_UNLIMITED
            && _size > endpoint_impl<Protocol>::max_message_size_) {
        if (endpoint_impl<Protocol>::is_supporting_someip_tp_ && _data != nullptr) {
            const service_t its_service = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_SERVICE_POS_MIN],
                    _data[VSOMEIP_SERVICE_POS_MAX]);
            const method_t its_method = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_METHOD_POS_MIN],
                    _data[VSOMEIP_METHOD_POS_MAX]);
            if (tp_segmentation_enabled(its_service, its_method)) {
                send_segments(tp::tp::tp_split_message(_data, _size));
                return endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT;
            }
        }
        VSOMEIP_ERROR << "cei::check_message_size: Dropping too big message ("
                << std::dec << _size << " Bytes). Maximum allowed message size is: "
                << endpoint_impl<Protocol>::max_message_size_ << " Bytes.";
        ret = endpoint_impl<Protocol>::cms_ret_e::MSG_TOO_BIG;
    }
    return ret;
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
            //  lowbyte methodid + highbyte vsomeip length]
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
void client_endpoint_impl<Protocol>::queue_train(bool _queue_size_zero_on_entry) {
    queue_.push_back(train_.buffer_);
    queue_size_ += train_.buffer_->size();
    train_.buffer_ = std::make_shared<message_buffer_t>();
    if (_queue_size_zero_on_entry && !queue_.empty()) { // no writing in progress
        send_queued();
    }
}

template<typename Protocol>
size_t client_endpoint_impl<Protocol>::get_queue_size() const {
    std::lock_guard<std::mutex> its_lock(mutex_);
    return queue_size_;
}

// Instantiate template
#ifndef _WIN32
template class client_endpoint_impl<boost::asio::local::stream_protocol>;
#endif
template class client_endpoint_impl<boost::asio::ip::tcp>;
template class client_endpoint_impl<boost::asio::ip::udp>;

}  // namespace vsomeip_v3


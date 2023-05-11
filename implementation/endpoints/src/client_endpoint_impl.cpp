// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
        boost::asio::io_context &_io,
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
          connecting_timer_(_io), connecting_timeout_(VSOMEIP_DEFAULT_CONNECTING_TIMEOUT),
          train_(std::make_shared<train>()),
          dispatch_timer_(_io),
          has_last_departure_(false),
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
        const endpoint::prepare_stop_handler_t &_handler, service_t _service) {

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

    // bind to strand as stop() might be called from different thread
    strand_.dispatch(std::bind(&client_endpoint_impl::shutdown_and_close_socket,
            this->shared_from_this(),
            false)
    );
}

template<typename Protocol>
std::pair<message_buffer_ptr_t, uint32_t>
client_endpoint_impl<Protocol>::get_front() {

    std::pair<message_buffer_ptr_t, uint32_t> its_entry;
    if (queue_.size())
        its_entry = queue_.front();

    return (its_entry);
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
    auto its_now(std::chrono::steady_clock::now());

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

    // STEP 1: Cancel dispatch timer
    cancel_dispatch_timer();

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
    if (train_->passengers_.empty()) {
        train_->departure_ = its_now + its_retention; // latest possible
    } else {
        // STEP 4.1: Check whether the current train already contains the message
        if (train_->passengers_.end() != train_->passengers_.find(its_identifier)) {
            must_depart = true;
        } else {
            // STEP 5: Check whether the current message fits into the current train
            if (train_->buffer_->size() + _size > endpoint_impl<Protocol>::max_message_size_) {
                must_depart = true;
            } else {
                // STEP 6: Check debouncing time
                if (its_debouncing > train_->minimal_max_retention_time_) {
                    // train's latest departure would already undershot new
                    // passenger's debounce time
                    must_depart = true;
                } else {
                    if (its_now + its_debouncing > train_->departure_) {
                        // train departs earlier as the new passenger's debounce
                        // time allows
                        must_depart = true;
                    } else {
                        // STEP 7: Check maximum retention time
                        if (its_retention < train_->minimal_debounce_time_) {
                            // train's earliest departure would already exceed
                            // the new passenger's retention time.
                            must_depart = true;
                        } else {
                            if (its_now + its_retention < train_->departure_) {
                                train_->departure_ = its_now + its_retention;
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
        // departs. Schedule departure of current train and create a new one.
        schedule_train();

        train_ = std::make_shared<train>();
        train_->departure_ = its_now + its_retention;
    }

    // STEP 9: insert current message buffer
    train_->buffer_->insert(train_->buffer_->end(), _data, _data + _size);
    train_->passengers_.insert(its_identifier);
    // STEP 9.1: update the trains minimal debounce time if necessary
    if (its_debouncing < train_->minimal_debounce_time_) {
        train_->minimal_debounce_time_ = its_debouncing;
    }
    // STEP 9.2: update the trains minimal maximum retention time if necessary
    if (its_retention < train_->minimal_max_retention_time_) {
        train_->minimal_max_retention_time_ = its_retention;
    }

    // STEP 10: restart dispatch timer with next departure time
    start_dispatch_timer(its_now);

    return true;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::send_segments(
        const tp::tp_split_messages_t &_segments, std::uint32_t _separation_time) {

    auto its_now(std::chrono::steady_clock::now());

    if (_segments.size() == 0) {
        return;
    }

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
    if (its_debouncing < train_->minimal_debounce_time_) {
        train_->minimal_debounce_time_ = its_debouncing;
    }
    // update the trains minimal maximum retention time if necessary
    if (its_retention < train_->minimal_max_retention_time_) {
        train_->minimal_max_retention_time_ = its_retention;
    }

    // We only need to respect the debouncing. There is no need to wait for further
    // messages as we will send several now anyway.
    if (!train_->passengers_.empty()) {
        schedule_train();
        train_->departure_ = its_now + its_retention;
    }

    const bool queue_size_still_zero(queue_.empty());
    for (const auto& s : _segments) {
        queue_.emplace_back(std::make_pair(s, _separation_time));
        queue_size_ += s->size();
    }

    if (queue_size_still_zero && !queue_.empty()) { // no writing in progress
        // respect minimal debounce time
        schedule_train();
        // ignore retention time and send immediately as the train is full anyway
        auto its_entry = get_front();
        if (its_entry.first) {
            strand_.dispatch(std::bind(&client_endpoint_impl::send_queued,
                this->shared_from_this(), its_entry));
        }
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::schedule_train() {

    if (has_last_departure_) {
        if (last_departure_ + train_->minimal_debounce_time_ > train_->departure_) {
            train_->departure_ = last_departure_ + train_->minimal_debounce_time_;
        }
    }

    dispatched_trains_[train_->departure_].push_back(train_);
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

    bool has_queued(true);
    bool is_current_train(true);

    std::lock_guard<std::mutex> its_lock(mutex_);

    std::shared_ptr<train> its_train(train_);
    if (!dispatched_trains_.empty()) {

        auto its_dispatched = dispatched_trains_.begin();
        if (its_dispatched->first <= its_train->departure_) {

            is_current_train = false;
            its_train = its_dispatched->second.front();
            its_dispatched->second.pop_front();
            if (its_dispatched->second.empty()) {

                dispatched_trains_.erase(its_dispatched);
            }
        }
    }

    if (!its_train->buffer_->empty()) {

        queue_train(its_train, !queue_.size());

        // Reset current train if necessary
        if (is_current_train) {
            its_train->reset();
        }
    } else {
        has_queued = false;
    }

    if (!is_current_train || !dispatched_trains_.empty()) {

        auto its_now(std::chrono::steady_clock::now());
        start_dispatch_timer(its_now);
    }

    return (has_queued);
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::connect_cbk(
        boost::system::error_code const &_error) {

    if (_error != boost::asio::error::timed_out) {
        std::lock_guard<std::mutex> its_lock(connecting_timer_mutex_);
        connecting_timer_.cancel();
    }

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
                auto its_entry = get_front();
                if (its_entry.first) {
                    strand_.dispatch(std::bind(&client_endpoint_impl::send_queued,
                            this->shared_from_this(), its_entry));
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
        auto self = this->shared_from_this();
        strand_.dispatch(std::bind(&client_endpoint_impl::connect,
                this->shared_from_this()));
    }
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::wait_connecting_cbk(
        boost::system::error_code const &_error) {
    if (!_error && !client_endpoint_impl<Protocol>::sending_blocked_) {
        connect_cbk(boost::asio::error::timed_out);
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
            queue_size_ -= queue_.front().first->size();
            queue_.pop_front();

            update_last_departure();

            auto its_entry = get_front();
            if (its_entry.first) {
                send_queued(its_entry);
            }
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
                        << " " << queue_size_ << " ("
                        << std::hex << std::setfill('0')
                        << std::setw(4) << its_client << "): ["
                        << std::setw(4) << its_service << "."
                        << std::setw(4) << its_method << "."
                        << std::setw(4) << its_session << "]";
            }
        }
        if (!stopping) {
            print_status();
        }
        was_not_connected_ = true;
        shutdown_and_close_socket(true);
        strand_.dispatch(std::bind(&client_endpoint_impl::connect,
                this->shared_from_this()));
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
        strand_.dispatch(std::bind(&client_endpoint_impl::connect,
                this->shared_from_this()));
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
                << " " << queue_.size()
                << " " << queue_size_ << " ("
                << std::hex << std::setfill('0')
                << std::setw(4) << its_client << "): ["
                << std::setw(4) << its_service << "."
                << std::setw(4) << its_method << "."
                << std::setw(4) << its_session << "]";
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
#if defined(__linux__) || defined(ANDROID)
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
        socket_.reset(new socket_type(endpoint_impl<Protocol>::io_));
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
void client_endpoint_impl<Protocol>::set_local_port(uint16_t _port) {

    local_port_ = _port;
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
void client_endpoint_impl<Protocol>::start_connecting_timer() {

    std::lock_guard<std::mutex> its_lock(connecting_timer_mutex_);
    connecting_timer_.expires_from_now(
            std::chrono::milliseconds(connecting_timeout_));
    connecting_timer_.async_wait(
            std::bind(&client_endpoint_impl<Protocol>::wait_connecting_cbk,
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
                instance_t its_instance = this->get_instance(its_service);
                if (its_instance != 0xFFFF) {
                    std::uint16_t its_max_segment_length;
                    std::uint32_t its_separation_time;
                    this->configuration_->get_tp_configuration(
                                its_service, its_instance, its_method, true,
                                its_max_segment_length, its_separation_time);
                    send_segments(tp::tp::tp_split_message(_data, _size,
                            its_max_segment_length), its_separation_time);
                    return endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT;
                }
            }
        }
        VSOMEIP_ERROR << "cei::check_message_size: Dropping to big message ("
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
                << std::hex << std::setfill('0')
		<< std::setw(4) << its_client << "): ["
                << std::setw(4) << its_service << "."
                << std::setw(4) << its_method << "."
                << std::setw(4) << its_session << "] "
                << "queue_size: " << std::dec << queue_size_
                << " data size: " << _size;
        return false;
    }
    return true;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::queue_train(
        const std::shared_ptr<train> &_train, bool _queue_size_zero_on_entry) {

    queue_.push_back(std::make_pair(_train->buffer_, 0));
    queue_size_ += _train->buffer_->size();

    if (_queue_size_zero_on_entry && !queue_.empty()) { // no writing in progress
        auto its_entry = get_front();
        if (its_entry.first) {
            strand_.dispatch(std::bind(&client_endpoint_impl::send_queued,
                this->shared_from_this(), its_entry));
        }
    }
}

template<typename Protocol>
size_t client_endpoint_impl<Protocol>::get_queue_size() const {

    std::lock_guard<std::mutex> its_lock(mutex_);
    return queue_size_;
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::start_dispatch_timer(
        const std::chrono::steady_clock::time_point &_now) {

    // Choose the next train
    std::shared_ptr<train> its_train(train_);
    if (!dispatched_trains_.empty()) {

        auto its_dispatched = dispatched_trains_.begin();
        if (its_dispatched->first < its_train->departure_) {

            its_train = its_dispatched->second.front();
        }
    }

    std::chrono::nanoseconds its_offset;
    if (its_train->departure_ > _now) {

        its_offset = std::chrono::duration_cast<std::chrono::nanoseconds>(
                its_train->departure_ - _now);
    } else { // already departure time

        its_offset = std::chrono::nanoseconds::zero();
    }

#if defined(__linux__) || defined(ANDROID)
    dispatch_timer_.expires_from_now(its_offset);
#else
    dispatch_timer_.expires_from_now(
            std::chrono::duration_cast<
                std::chrono::steady_clock::duration>(its_offset));
#endif
    dispatch_timer_.async_wait(
            std::bind(&client_endpoint_impl<Protocol>::flush_cbk,
                      this->shared_from_this(), std::placeholders::_1));
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::cancel_dispatch_timer() {

    boost::system::error_code ec;
    dispatch_timer_.cancel(ec);
}

template<typename Protocol>
void client_endpoint_impl<Protocol>::update_last_departure() {

    last_departure_ = std::chrono::steady_clock::now();
    has_last_departure_ = true;
}

// Instantiate template
#if defined(__linux__) || defined(ANDROID)
template class client_endpoint_impl<boost::asio::local::stream_protocol>;
#endif
template class client_endpoint_impl<boost::asio::ip::tcp>;
template class client_endpoint_impl<boost::asio::ip::udp>;

}  // namespace vsomeip_v3

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>
#include <limits>
#include <thread>
#include <algorithm>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp_ext.hpp>
#include <boost/asio/local/stream_protocol_ext.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/server_endpoint_impl.hpp"
#include "../include/endpoint_definition.hpp"

#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../../service_discovery/include/defines.hpp"

namespace vsomeip_v3 {

template<typename Protocol>
server_endpoint_impl<Protocol>::server_endpoint_impl(
        const std::shared_ptr<endpoint_host>& _endpoint_host,
        const std::shared_ptr<routing_host>& _routing_host, endpoint_type _local,
        boost::asio::io_service &_io, std::uint32_t _max_message_size,
        configuration::endpoint_queue_limit_t _queue_limit,
        const std::shared_ptr<configuration>& _configuration)
    : endpoint_impl<Protocol>(_endpoint_host, _routing_host, _local, _io, _max_message_size,
                              _queue_limit, _configuration),
                              sent_timer_(_io) {
    is_sending_ = false;
}

template<typename Protocol>
server_endpoint_impl<Protocol>::~server_endpoint_impl() {
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::prepare_stop(
        endpoint::prepare_stop_handler_t _handler, service_t _service) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    bool queued_train(false);
    if (_service == ANY_SERVICE) { // endpoint is shutting down completely
        endpoint_impl<Protocol>::sending_blocked_ = true;
        boost::system::error_code ec;
        for (auto const& train_iter : trains_) {
            train_iter.second->departure_timer_->cancel(ec);
            if (train_iter.second->buffer_->size() > 0) {
                auto target_queue_iter = queues_.find(train_iter.first);
                if (target_queue_iter != queues_.end()) {
                    auto& its_qpair = target_queue_iter->second;
                    const bool queue_size_zero_on_entry(its_qpair.second.empty());
                    queue_train(target_queue_iter, train_iter.second,
                            queue_size_zero_on_entry);
                    queued_train = true;
                }
            }
        }
    } else {
        for (auto const& train_iter : trains_) {
            for (auto const& passenger_iter : train_iter.second->passengers_) {
                if (passenger_iter.first == _service) {
                    // cancel departure timer
                    boost::system::error_code ec;
                    train_iter.second->departure_timer_->cancel(ec);
                    // queue train
                    auto target_queue_iter = queues_.find(train_iter.first);
                    if (target_queue_iter != queues_.end()) {
                        const auto& its_qpair = target_queue_iter->second;
                        const bool queue_size_zero_on_entry(its_qpair.second.empty());
                        queue_train(target_queue_iter, train_iter.second,
                                queue_size_zero_on_entry);
                        queued_train = true;
                    }
                    break;
                }
            }
        }
    }
    if (!queued_train) {
        if (_service == ANY_SERVICE) {
            if (std::all_of(queues_.begin(), queues_.end(),
                            [&](const typename queue_type::value_type& q)
                                { return q.second.second.empty(); })) {
                // nothing was queued and all queues are empty -> ensure cbk is called
                auto ptr = this->shared_from_this();
                endpoint_impl<Protocol>::service_.post([ptr, _handler, _service](){
                                                            _handler(ptr, _service);
                                                        });
            } else {
                prepare_stop_handlers_[_service] = _handler;
            }
        } else {
            // check if any of the queues contains a message of to be stopped service
            bool found_service_msg(false);
            for (const auto& q : queues_) {
                for (const auto& msg : q.second.second ) {
                    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
                                            (*msg)[VSOMEIP_SERVICE_POS_MIN],
                                            (*msg)[VSOMEIP_SERVICE_POS_MAX]);
                    if (its_service == _service) {
                        found_service_msg = true;
                        break;
                    }
                }
                if (found_service_msg) {
                    break;
                }
            }
            if (found_service_msg) {
                prepare_stop_handlers_[_service] = _handler;
            } else { // no messages of the to be stopped service are or have been queued
                auto ptr = this->shared_from_this();
                endpoint_impl<Protocol>::service_.post([ptr, _handler, _service](){
                                                            _handler(ptr, _service);
                                                        });
            }
        }
    } else {
        prepare_stop_handlers_[_service] = _handler;
    }
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::stop() {
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::is_client() const {
    return false;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::restart(bool _force) {
    (void)_force;
    // intentionally left blank
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::is_established() const {
    return true;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::is_established_or_connected() const {
    return true;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::set_established(bool _established) {
    (void) _established;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::set_connected(bool _connected) {
    (void) _connected;
}

template<typename Protocol>bool server_endpoint_impl<Protocol>::send(const uint8_t *_data,
        uint32_t _size) {
#if 0
    std::stringstream msg;
    msg << "sei::send ";
    for (uint32_t i = 0; i < _size; i++)
        msg << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    endpoint_type its_target;
    bool is_valid_target(false);

    if (VSOMEIP_SESSION_POS_MAX < _size) {
        std::lock_guard<std::mutex> its_lock(mutex_);

        if(endpoint_impl<Protocol>::sending_blocked_) {
            return false;
        }

        const service_t its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        const client_t its_client = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_CLIENT_POS_MIN], _data[VSOMEIP_CLIENT_POS_MAX]);
        const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SESSION_POS_MIN], _data[VSOMEIP_SESSION_POS_MAX]);

        clients_mutex_.lock();
        auto found_client = clients_.find(its_client);
        if (found_client != clients_.end()) {
            auto found_session = found_client->second.find(its_session);
            if (found_session != found_client->second.end()) {
                its_target = found_session->second;
                is_valid_target = true;
                found_client->second.erase(its_session);
            } else {
                VSOMEIP_WARNING << "server_endpoint::send: session_id 0x"
                        << std::hex << its_session
                        << " not found for client 0x" << its_client;
                const method_t its_method =
                        VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
                                             _data[VSOMEIP_METHOD_POS_MAX]);
                if (its_service == VSOMEIP_SD_SERVICE
                        && its_method == VSOMEIP_SD_METHOD) {
                    VSOMEIP_ERROR << "Clearing clients map as a request was "
                            "received on SD port";
                    clients_.clear();
                    is_valid_target = get_default_target(its_service, its_target);
                }
            }
        } else {
            is_valid_target = get_default_target(its_service, its_target);
        }
        clients_mutex_.unlock();

        if (is_valid_target) {
            is_valid_target = send_intern(its_target, _data, _size);
        }
    }
    return is_valid_target;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::send(
        const std::vector<byte_t>& _cmd_header, const byte_t *_data,
        uint32_t _size) {
    (void) _cmd_header;
    (void) _data;
    (void) _size;
    return false;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::send_intern(
        endpoint_type _target, const byte_t *_data, uint32_t _size) {

    switch (check_message_size(_data, _size, _target)) {
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
    if (!prepare_stop_handlers_.empty()) {
        const service_t its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        if (prepare_stop_handlers_.find(its_service) != prepare_stop_handlers_.end()) {
            const method_t its_method = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
            const client_t its_client = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_CLIENT_POS_MIN], _data[VSOMEIP_CLIENT_POS_MAX]);
            const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_SESSION_POS_MIN], _data[VSOMEIP_SESSION_POS_MAX]);
            VSOMEIP_WARNING << "server_endpoint::send: Service is stopping, ignoring message: ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_session << "]";
            return false;
        }
    }

    const queue_iterator_type target_queue_iterator = find_or_create_queue_unlocked(_target);

    bool must_depart(false);

#if 0
    std::stringstream msg;
    msg << "sei::send_intern: ";
    for (uint32_t i = 0; i < _size; i++)
    msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    // STEP 1: determine the correct train
    std::shared_ptr<train> target_train = find_or_create_train_unlocked(_target);

    const bool queue_size_zero_on_entry(target_queue_iterator->second.second.empty());
    if (!check_queue_limit(_data, _size, target_queue_iterator->second.first)) {
        return false;
    }
    // STEP 2: Determine elapsed time and update the departure time and cancel the timer
    target_train->update_departure_time_and_stop_departure();

    // STEP 3: Get configured timings
    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
            _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
    const method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
            _data[VSOMEIP_METHOD_POS_MAX]);

    std::chrono::nanoseconds its_debouncing(0), its_retention(0);
    if (its_service != VSOMEIP_SD_SERVICE && its_method != VSOMEIP_SD_METHOD) {
        get_configured_times_from_endpoint(its_service, its_method,
                &its_debouncing, &its_retention);
    }

    // STEP 4: Check if the passenger enters an empty train
    const std::pair<service_t, method_t> its_identifier = std::make_pair(
            its_service, its_method);
    if (target_train->passengers_.empty()) {
        target_train->departure_ = its_retention;
    } else {
        if (target_train->passengers_.end()
                != target_train->passengers_.find(its_identifier)) {
            must_depart = true;
        } else {
            // STEP 5: Check whether the current message fits into the current train
            if (target_train->buffer_->size() + _size > endpoint_impl<Protocol>::max_message_size_) {
                must_depart = true;
            } else {
                // STEP 6: Check debouncing time
                if (its_debouncing > target_train->minimal_max_retention_time_) {
                    // train's latest departure would already undershot new
                    // passenger's debounce time
                    must_depart = true;
                } else {
                    if (its_debouncing > target_train->departure_) {
                        // train departs earlier as the new passenger's debounce
                        // time allows
                        must_depart = true;
                    } else {
                        // STEP 7: Check maximum retention time
                        if (its_retention < target_train->minimal_debounce_time_) {
                            // train's earliest departure would already exceed
                            // the new passenger's retention time.
                            must_depart = true;
                        } else {
                            if (its_retention < target_train->departure_) {
                                target_train->departure_ = its_retention;
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
        wait_until_debounce_time_reached(target_train);
        queue_train(target_queue_iterator, target_train,
                queue_size_zero_on_entry);
        target_train->departure_ = its_retention;
    }

    // STEP 9: insert current message buffer
    target_train->buffer_->insert(target_train->buffer_->end(), _data, _data + _size);
    target_train->passengers_.insert(its_identifier);
    // STEP 9.1: update the trains minimal debounce time if necessary
    if (its_debouncing < target_train->minimal_debounce_time_) {
        target_train->minimal_debounce_time_ = its_debouncing;
    }
    // STEP 9.2: update the trains minimal maximum retention time if necessary
    if (its_retention < target_train->minimal_max_retention_time_) {
        target_train->minimal_max_retention_time_ = its_retention;
    }

    // STEP 10: restart timer with current departure time
#ifndef _WIN32
    target_train->departure_timer_->expires_from_now(target_train->departure_);
#else
    target_train->departure_timer_->expires_from_now(
            std::chrono::duration_cast<
                std::chrono::steady_clock::duration>(target_train->departure_));
#endif
    target_train->departure_timer_->async_wait(
        std::bind(&server_endpoint_impl<Protocol>::flush_cbk,
                  this->shared_from_this(), _target,
                  target_train, std::placeholders::_1));

    return (true);
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::send_segments(
        const tp::tp_split_messages_t &_segments, const endpoint_type &_target) {

    if (_segments.size() == 0)
        return;

    const queue_iterator_type target_queue_iterator = find_or_create_queue_unlocked(_target);
    const bool queue_size_zero_on_entry(target_queue_iterator->second.second.empty());

    std::shared_ptr<train> target_train = find_or_create_train_unlocked(_target);
    target_train->update_departure_time_and_stop_departure();

    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
            (*(_segments[0]))[VSOMEIP_SERVICE_POS_MIN], (*(_segments[0]))[VSOMEIP_SERVICE_POS_MAX]);
    const method_t its_method = VSOMEIP_BYTES_TO_WORD(
            (*(_segments[0]))[VSOMEIP_METHOD_POS_MIN], (*(_segments[0]))[VSOMEIP_METHOD_POS_MAX]);

    std::chrono::nanoseconds its_debouncing(0), its_retention(0);
    if (its_service != VSOMEIP_SD_SERVICE && its_method != VSOMEIP_SD_METHOD) {
        get_configured_times_from_endpoint(its_service, its_method,
                &its_debouncing, &its_retention);
    }
    // update the trains minimal debounce time if necessary
    if (its_debouncing < target_train->minimal_debounce_time_) {
        target_train->minimal_debounce_time_ = its_debouncing;
    }
    // update the trains minimal maximum retention time if necessary
    if (its_retention < target_train->minimal_max_retention_time_) {
        target_train->minimal_max_retention_time_ = its_retention;
    }
    // We only need to respect the debouncing. There is no need to wait for further
    // messages as we will send several now anyway.
    if (!target_train->passengers_.empty()) {
        wait_until_debounce_time_reached(target_train);
        queue_train(target_queue_iterator, target_train, queue_size_zero_on_entry);
    }

    const bool queue_size_still_zero(target_queue_iterator->second.second.empty());
    for (const auto &s : _segments) {
        target_queue_iterator->second.second.emplace_back(s);
        target_queue_iterator->second.first += s->size();
    }
    if (queue_size_still_zero && !target_queue_iterator->second.second.empty()) { // no writing in progress
        // respect minimal debounce time
        wait_until_debounce_time_reached(target_train);
        // ignore retention time and send immediately as the train is full anyway
        send_queued(target_queue_iterator);
    }
    target_train->last_departure_ = std::chrono::steady_clock::now();
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::wait_until_debounce_time_reached(
        const std::shared_ptr<train>& _train) const {
    const std::chrono::nanoseconds time_since_last_departure =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - _train->last_departure_);

    if (time_since_last_departure < _train->minimal_debounce_time_) {
        std::this_thread::sleep_for(
                _train->minimal_debounce_time_ - time_since_last_departure);
    }
}


template<typename Protocol>
typename endpoint_impl<Protocol>::cms_ret_e server_endpoint_impl<Protocol>::check_message_size(
        const std::uint8_t * const _data, std::uint32_t _size,
        const endpoint_type& _target) {
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
                send_segments(tp::tp::tp_split_message(_data, _size), _target);
                return endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT;
            }
        }
        VSOMEIP_ERROR << "sei::send_intern: Dropping too big message (" << _size
                << " Bytes). Maximum allowed message size is: "
                << endpoint_impl<Protocol>::max_message_size_ << " Bytes.";
        ret = endpoint_impl<Protocol>::cms_ret_e::MSG_TOO_BIG;
    }
    return ret;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::check_queue_limit(const uint8_t *_data, std::uint32_t _size,
                       std::size_t _current_queue_size) const {
    if (endpoint_impl<Protocol>::queue_limit_ != QUEUE_SIZE_UNLIMITED
            && _current_queue_size + _size
                    > endpoint_impl<Protocol>::queue_limit_) {
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
        VSOMEIP_ERROR << "sei::send_intern: queue size limit (" << std::dec
                << endpoint_impl<Protocol>::queue_limit_
                << ") reached. Dropping message ("
                << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_session << "]"
                << " queue_size: " << std::dec << _current_queue_size
                << " data size: " << std::dec << _size;
        return false;
    }
    return true;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::queue_train(
                     const queue_iterator_type _queue_iterator,
                     const std::shared_ptr<train>& _train,
                     bool _queue_size_zero_on_entry) {
    _queue_iterator->second.second.emplace_back(_train->buffer_);
    _queue_iterator->second.first += _train->buffer_->size();
    _train->last_departure_ = std::chrono::steady_clock::now();
    _train->passengers_.clear();
    _train->buffer_ = std::make_shared<message_buffer_t>();
    _train->minimal_debounce_time_ = std::chrono::nanoseconds::max();
    _train->minimal_max_retention_time_ = std::chrono::nanoseconds::max();
    if (_queue_size_zero_on_entry && !_queue_iterator->second.second.empty()) { // no writing in progress
        send_queued(_queue_iterator);
    }
}

template<typename Protocol>
typename server_endpoint_impl<Protocol>::queue_iterator_type
server_endpoint_impl<Protocol>::find_or_create_queue_unlocked(const endpoint_type& _target) {
    queue_iterator_type target_queue_iterator = queues_.find(_target);
    if (target_queue_iterator == queues_.end()) {
        target_queue_iterator = queues_.insert(queues_.begin(),
                                    std::make_pair(
                                        _target,
                                        std::make_pair(std::size_t(0),
                                                       std::deque<message_buffer_ptr_t>())
                                    ));
    }
    return target_queue_iterator;
}

template<typename Protocol>
std::shared_ptr<train> server_endpoint_impl<Protocol>::find_or_create_train_unlocked(
        const endpoint_type& _target) {
    auto train_iter = trains_.find(_target);
    if (train_iter == trains_.end()) {
        train_iter = trains_.insert(trains_.begin(),
                                    std::make_pair(_target, std::make_shared<train>(this->service_)));
    }
    return train_iter->second;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::flush(
        endpoint_type _target,
        const std::shared_ptr<train>& _train) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    bool is_flushed = false;
    if (!_train->buffer_->empty()) {
        const queue_iterator_type target_queue_iterator = queues_.find(_target);
        if (target_queue_iterator != queues_.end()) {
            const bool queue_size_zero_on_entry(target_queue_iterator->second.second.empty());
            queue_train(target_queue_iterator, _train, queue_size_zero_on_entry);
            is_flushed = true;
        } else {
            std::stringstream ss;
            ss << "sei::flush couldn't find target queue, won't  queue train to: "
                    << get_remote_information(_target) << " passengers: ";
            for (const auto& p : _train->passengers_) {
                ss << "["  << std::hex << std::setw(4) << std::setfill('0')
                    << p.first << ":" << p.second << "] ";
            }
            VSOMEIP_WARNING << ss.str();
        }
    }
    return is_flushed;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::connect_cbk(
        boost::system::error_code const &_error) {
    (void)_error;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::send_cbk(
        const queue_iterator_type _queue_iterator,
        boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_bytes;

    {
        std::lock_guard<std::mutex> its_sent_lock(sent_mutex_);
        is_sending_ = false;

        boost::system::error_code ec;
        sent_timer_.cancel(ec);
    }

    std::lock_guard<std::mutex> its_lock(mutex_);

    auto check_if_all_msgs_for_stopped_service_are_sent = [&]() {
        bool found_service_msg(false);
        service_t its_stopped_service(ANY_SERVICE);
        for (auto stp_hndlr_iter = prepare_stop_handlers_.begin();
                  stp_hndlr_iter != prepare_stop_handlers_.end();) {
            its_stopped_service = stp_hndlr_iter->first;
            if (its_stopped_service == ANY_SERVICE) {
                ++stp_hndlr_iter;
                continue;
            }
            for (const auto& q : queues_) {
                for (const auto& msg : q.second.second ) {
                    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
                                            (*msg)[VSOMEIP_SERVICE_POS_MIN],
                                            (*msg)[VSOMEIP_SERVICE_POS_MAX]);
                    if (its_service == its_stopped_service) {
                        found_service_msg = true;
                        break;
                    }
                }
                if (found_service_msg) {
                    break;
                }
            }
            if (found_service_msg) {
                ++stp_hndlr_iter;
                found_service_msg = false;
            } else { // all messages of the to be stopped service have been sent
                auto handler = stp_hndlr_iter->second;
                auto ptr = this->shared_from_this();
                #ifndef _WIN32
                endpoint_impl<Protocol>::
                #endif
                    service_.post([ptr, handler, its_stopped_service](){
                        handler(ptr, its_stopped_service);
                    });
                stp_hndlr_iter = prepare_stop_handlers_.erase(stp_hndlr_iter);
            }
        }
    };

    auto check_if_all_queues_are_empty = [&](){
        if (prepare_stop_handlers_.size() > 1) {
            // before the endpoint was stopped completely other
            // prepare_stop_handlers have been queued ensure to call them as well
            check_if_all_msgs_for_stopped_service_are_sent();
        }
        if (std::all_of(queues_.begin(), queues_.end(), [&]
                        #ifndef _WIN32
                           (const typename queue_type::value_type& q)
                        #else
                           (const std::pair<endpoint_type,std::pair<size_t, std::deque<message_buffer_ptr_t>>>& q)
                        #endif
                           { return q.second.second.empty(); })) {
            // all outstanding response have been sent.
            auto found_cbk = prepare_stop_handlers_.find(ANY_SERVICE);
            if (found_cbk != prepare_stop_handlers_.end()) {
                auto handler = found_cbk->second;
                auto ptr = this->shared_from_this();
                #ifndef _WIN32
                endpoint_impl<Protocol>::
                #endif
                    service_.post([ptr, handler](){
                            handler(ptr, ANY_SERVICE);
                    });
                prepare_stop_handlers_.erase(found_cbk);
            }
        }
    };

    auto& its_qpair = _queue_iterator->second;
    if (!_error) {
        its_qpair.first -= its_qpair.second.front()->size();
        its_qpair.second.pop_front();

        if (!prepare_stop_handlers_.empty() && !endpoint_impl<Protocol>::sending_blocked_) {
            // only one service instance is stopped
            check_if_all_msgs_for_stopped_service_are_sent();
        }

        if (its_qpair.second.size() > 0) {
            send_queued(_queue_iterator);
        } else if (!prepare_stop_handlers_.empty() && endpoint_impl<Protocol>::sending_blocked_) {
            // endpoint is shutting down completely
            queues_.erase(_queue_iterator);
            check_if_all_queues_are_empty();
        }
    } else {
        message_buffer_ptr_t its_buffer;
        if (_queue_iterator->second.second.size()) {
            its_buffer = _queue_iterator->second.second.front();
        }
        service_t its_service(0);
        method_t its_method(0);
        client_t its_client(0);
        session_t its_session(0);
        if (its_buffer && its_buffer->size() > VSOMEIP_SESSION_POS_MAX) {
            its_service = VSOMEIP_BYTES_TO_WORD(
                    (*its_buffer)[VSOMEIP_SERVICE_POS_MIN],
                    (*its_buffer)[VSOMEIP_SERVICE_POS_MAX]);
            its_method = VSOMEIP_BYTES_TO_WORD(
                    (*its_buffer)[VSOMEIP_METHOD_POS_MIN],
                    (*its_buffer)[VSOMEIP_METHOD_POS_MAX]);
            its_client = VSOMEIP_BYTES_TO_WORD(
                    (*its_buffer)[VSOMEIP_CLIENT_POS_MIN],
                    (*its_buffer)[VSOMEIP_CLIENT_POS_MAX]);
            its_session = VSOMEIP_BYTES_TO_WORD(
                    (*its_buffer)[VSOMEIP_SESSION_POS_MIN],
                    (*its_buffer)[VSOMEIP_SESSION_POS_MAX]);
        }
        // error: sending of outstanding responses isn't started again
        // delete remaining outstanding responses
        VSOMEIP_WARNING << "sei::send_cbk received error: " << _error.message()
                << " (" << std::dec << _error.value() << ") "
                << get_remote_information(_queue_iterator) << " "
                << std::dec << _queue_iterator->second.second.size() << " "
                << std::dec << _queue_iterator->second.first << " ("
                << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_session << "]";
        queues_.erase(_queue_iterator);
        if (!prepare_stop_handlers_.empty()) {
            if (endpoint_impl<Protocol>::sending_blocked_) {
                // endpoint is shutting down completely, ensure to call
                // prepare_stop_handlers even in error cases
                check_if_all_queues_are_empty();
            } else {
                // only one service instance is stopped
                check_if_all_msgs_for_stopped_service_are_sent();
            }
        }

    }
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::flush_cbk(
        endpoint_type _target,
        const std::shared_ptr<train>& _train, const boost::system::error_code &_error_code) {
    if (!_error_code) {
        (void) flush(_target, _train);
    }
}

template<typename Protocol>
size_t server_endpoint_impl<Protocol>::get_queue_size() const {
    size_t its_queue_size(0);

    {
        std::lock_guard<std::mutex> its_lock(mutex_);
        for (const auto& q : queues_) {
            its_queue_size += q.second.second.size();
        }
    }

    return its_queue_size;
}

// Instantiate template
#ifndef _WIN32
template class server_endpoint_impl<boost::asio::local::stream_protocol_ext>;
#endif
template class server_endpoint_impl<boost::asio::ip::tcp>;
template class server_endpoint_impl<boost::asio::ip::udp_ext>;

}  // namespace vsomeip_v3

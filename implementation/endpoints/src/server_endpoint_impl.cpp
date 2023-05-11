// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#if VSOMEIP_BOOST_VERSION < 106600
#include <boost/asio/local/stream_protocol_ext.hpp>
#include <boost/asio/ip/udp_ext.hpp>
#else
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ip/udp.hpp>
#endif

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
        boost::asio::io_context &_io, std::uint32_t _max_message_size,
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
        const endpoint::prepare_stop_handler_t &_handler, service_t _service) {

    std::lock_guard<std::mutex> its_lock(mutex_);
    bool queued_train(false);
    std::vector<target_data_iterator_type> its_erased;
    boost::system::error_code ec;

    if (_service == ANY_SERVICE) { // endpoint is shutting down completely
        endpoint_impl<Protocol>::sending_blocked_ = true;
        for (auto t = targets_.begin(); t != targets_.end(); t++) {
            auto its_train (t->second.train_);
            // cancel dispatch timer
            t->second.dispatch_timer_->cancel(ec);
            if (its_train->buffer_->size() > 0) {
                const bool queue_size_zero_on_entry(t->second.queue_.empty());
                if (queue_train(t, its_train, queue_size_zero_on_entry))
                    its_erased.push_back(t);
                queued_train = true;
            }
        }
    } else {
        for (auto t = targets_.begin(); t != targets_.end(); t++) {
            auto its_train(t->second.train_);
            for (auto const& passenger_iter : its_train->passengers_) {
                if (passenger_iter.first == _service) {
                    // cancel dispatch timer
                    t->second.dispatch_timer_->cancel(ec);
                    // queue train
                    const bool queue_size_zero_on_entry(t->second.queue_.empty());
                    // TODO: Queue all(!) trains here...
                    if (queue_train(t, its_train, queue_size_zero_on_entry))
                        its_erased.push_back(t);
                    queued_train = true;
                    break;
                }
            }
        }
    }

    for (const auto t : its_erased)
        targets_.erase(t);

    if (!queued_train) {
        if (_service == ANY_SERVICE) {
            if (std::all_of(targets_.begin(), targets_.end(),
                            [&](const typename target_data_type::value_type &_t)
                                { return _t.second.queue_.empty(); })) {
                // nothing was queued and all queues are empty -> ensure cbk is called
                auto ptr = this->shared_from_this();
                endpoint_impl<Protocol>::io_.post([ptr, _handler, _service](){
                                                            _handler(ptr, _service);
                                                        });
            } else {
                prepare_stop_handlers_[_service] = _handler;
            }
        } else {
            // check if any of the queues contains a message of to be stopped service
            bool found_service_msg(false);
            for (const auto &t : targets_) {
                for (const auto &q : t.second.queue_) {
                    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
                                            (*q.first)[VSOMEIP_SERVICE_POS_MIN],
                                            (*q.first)[VSOMEIP_SERVICE_POS_MAX]);
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
                endpoint_impl<Protocol>::io_.post([ptr, _handler, _service](){
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
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
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
                    << std::hex << std::setfill('0')
                    << std::setw(4) << its_service << "."
                    << std::setw(4) << its_method << "."
                    << std::setw(4) << its_client << "."
                    << std::setw(4) << its_session << "]";
            return false;
        }
    }

    const auto its_target_iterator = find_or_create_target_unlocked(_target);
    auto &its_data(its_target_iterator->second);

    bool must_depart(false);
    auto its_now(std::chrono::steady_clock::now());

#if 0
    std::stringstream msg;
    msg << "sei::send_intern: ";
    for (uint32_t i = 0; i < _size; i++)
    msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    // STEP 1: Check queue limit
    if (!check_queue_limit(_data, _size, its_data.queue_size_)) {
        return false;
    }
    // STEP 2: Cancel the dispatch timer
    cancel_dispatch_timer(its_target_iterator);

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
    const std::pair<service_t, method_t> its_identifier
        = std::make_pair(its_service, its_method);
    if (its_data.train_->passengers_.empty()) {
        its_data.train_->departure_ = its_now + its_retention;
    } else {
        if (its_data.train_->passengers_.end()
                != its_data.train_->passengers_.find(its_identifier)) {
            must_depart = true;
        } else {
            // STEP 5: Check whether the current message fits into the current train
            if (its_data.train_->buffer_->size() + _size > endpoint_impl<Protocol>::max_message_size_) {
                must_depart = true;
            } else {
                // STEP 6: Check debouncing time
                if (its_debouncing > its_data.train_->minimal_max_retention_time_) {
                    // train's latest departure would already undershot new
                    // passenger's debounce time
                    must_depart = true;
                } else {
                    if (its_now + its_debouncing > its_data.train_->departure_) {
                        // train departs earlier as the new passenger's debounce
                        // time allows
                        must_depart = true;
                    } else {
                        // STEP 7: Check maximum retention time
                        if (its_retention < its_data.train_->minimal_debounce_time_) {
                            // train's earliest departure would already exceed
                            // the new passenger's retention time.
                            must_depart = true;
                        } else {
                            if (its_now + its_retention < its_data.train_->departure_) {
                                its_data.train_->departure_ = its_now + its_retention;
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
        schedule_train(its_data);

        its_data.train_ = std::make_shared<train>();
        its_data.train_->departure_ = its_now + its_retention;
    }

    // STEP 9: insert current message buffer
    its_data.train_->buffer_->insert(its_data.train_->buffer_->end(), _data, _data + _size);
    its_data.train_->passengers_.insert(its_identifier);
    // STEP 9.1: update the trains minimal debounce time if necessary
    if (its_debouncing < its_data.train_->minimal_debounce_time_) {
        its_data.train_->minimal_debounce_time_ = its_debouncing;
    }
    // STEP 9.2: update the trains minimal maximum retention time if necessary
    if (its_retention < its_data.train_->minimal_max_retention_time_) {
        its_data.train_->minimal_max_retention_time_ = its_retention;
    }

    // STEP 10: restart timer with current departure time
    start_dispatch_timer(its_target_iterator, its_now);

    return (true);
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::send_segments(
        const tp::tp_split_messages_t &_segments, std::uint32_t _separation_time,
        const endpoint_type &_target) {

    if (_segments.size() == 0)
        return;

    const auto its_target_iterator = find_or_create_target_unlocked(_target);
    auto &its_data = its_target_iterator->second;

    auto its_now(std::chrono::steady_clock::now());

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
    if (its_debouncing < its_data.train_->minimal_debounce_time_) {
        its_data.train_->minimal_debounce_time_ = its_debouncing;
    }
    // update the trains minimal maximum retention time if necessary
    if (its_retention < its_data.train_->minimal_max_retention_time_) {
        its_data.train_->minimal_max_retention_time_ = its_retention;
    }
    // We only need to respect the debouncing. There is no need to wait for further
    // messages as we will send several now anyway.
    if (!its_data.train_->passengers_.empty()) {
        schedule_train(its_data);
        its_data.train_->departure_ = its_now + its_retention;
    }

    const bool queue_size_still_zero(its_data.queue_.empty());
    for (const auto &s : _segments) {
        its_data.queue_.emplace_back(std::make_pair(s, _separation_time));
        its_data.queue_size_ += s->size();
    }

    if (queue_size_still_zero && !its_data.queue_.empty()) { // no writing in progress
        // respect minimal debounce time
        schedule_train(its_data);
        // ignore retention time and send immediately as the train is full anyway
        (void)send_queued(its_target_iterator);
    }
}

template<typename Protocol>
typename server_endpoint_impl<Protocol>::target_data_iterator_type
server_endpoint_impl<Protocol>::find_or_create_target_unlocked(endpoint_type _target) {

    auto its_iterator = targets_.find(_target);
    if (its_iterator == targets_.end()) {

        auto its_result = targets_.emplace(
                std::make_pair(_target, endpoint_data_type(this->io_)));
        its_iterator = its_result.first;
    }

    return (its_iterator);
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::schedule_train(endpoint_data_type &_data) {

    if (_data.has_last_departure_) {
        if (_data.last_departure_ + _data.train_->minimal_debounce_time_
                > _data.train_->departure_) {
            _data.train_->departure_ = _data.last_departure_
                    + _data.train_->minimal_debounce_time_;
        }
    }

    _data.dispatched_trains_[_data.train_->departure_]
                             .push_back(_data.train_);
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
                instance_t its_instance = this->get_instance(its_service);
                if (its_instance != 0xFFFF) {
                    std::uint16_t its_max_segment_length;
                    std::uint32_t its_separation_time;

                    this->configuration_->get_tp_configuration(
                                its_service, its_instance, its_method, false,
                                its_max_segment_length, its_separation_time);
                    send_segments(tp::tp::tp_split_message(_data, _size,
                            its_max_segment_length), its_separation_time, _target);
                    return endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT;
                }
            }
        }
        VSOMEIP_ERROR << "sei::send_intern: Dropping to big message (" << _size
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
                << std::hex << std::setfill('0')
                << std::setw(4) << its_client << "): ["
                << std::setw(4) << its_service << "."
                << std::setw(4) << its_method << "."
                << std::setw(4) << its_session << "]"
                << " queue_size: " << std::dec << _current_queue_size
                << " data size: " << _size;
        return false;
    }
    return true;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::queue_train(
        target_data_iterator_type _it, const std::shared_ptr<train> &_train,
        bool _queue_size_zero_on_entry) {

    bool must_erase(false);

    auto &its_data = _it->second;
    its_data.queue_.push_back(std::make_pair(_train->buffer_, 0));
    its_data.queue_size_ += _train->buffer_->size();

    if (_queue_size_zero_on_entry && !its_data.queue_.empty()) { // no writing in progress
        must_erase = send_queued(_it);
    }

    return must_erase;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::flush(target_data_iterator_type _it) {

    bool has_queued(true);
    bool is_current_train(true);
    auto &its_data = _it->second;

    std::lock_guard<std::mutex> its_lock(mutex_);

    auto its_train(its_data.train_);
    if (!its_data.dispatched_trains_.empty()) {

        auto its_dispatched = its_data.dispatched_trains_.begin();
        if (its_dispatched->first <= its_train->departure_) {

            is_current_train = false;
            its_train = its_dispatched->second.front();
            its_dispatched->second.pop_front();
            if (its_dispatched->second.empty()) {

                its_data.dispatched_trains_.erase(its_dispatched);
            }
        }
    }

    if (!its_train->buffer_->empty()) {

        queue_train(_it, its_train, its_data.queue_.empty());

        // Reset current train if necessary
        if (is_current_train) {
            its_train->reset();
        }
    } else {
        has_queued = false;
    }

    if (!is_current_train || !its_data.dispatched_trains_.empty()) {

        auto its_now(std::chrono::steady_clock::now());
        start_dispatch_timer(_it, its_now);
    }

    return (has_queued);
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::connect_cbk(
        boost::system::error_code const &_error) {
    (void)_error;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::send_cbk(
        const target_data_iterator_type _it,
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
            for (const auto& t : targets_) {
                for (const auto& e : t.second.queue_ ) {
                    const service_t its_service = VSOMEIP_BYTES_TO_WORD(
                                            (*e.first)[VSOMEIP_SERVICE_POS_MIN],
                                            (*e.first)[VSOMEIP_SERVICE_POS_MAX]);
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
            } else { // all messages of the to be stopped service have been sent
                auto handler = stp_hndlr_iter->second;
                auto ptr = this->shared_from_this();
#if defined(__linux__) || defined(ANDROID)
                endpoint_impl<Protocol>::
#endif
                    io_.post([ptr, handler, its_stopped_service](){
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
        if (std::all_of(targets_.begin(), targets_.end(),
                        [&](const typename target_data_type::value_type &_t)
                        { return _t.second.queue_.empty(); })) {
            // all outstanding response have been sent.
            auto found_cbk = prepare_stop_handlers_.find(ANY_SERVICE);
            if (found_cbk != prepare_stop_handlers_.end()) {
                auto handler = found_cbk->second;
                auto ptr = this->shared_from_this();
#if defined(__linux__) || defined(ANDROID)
                endpoint_impl<Protocol>::
#endif
                    io_.post([ptr, handler](){
                            handler(ptr, ANY_SERVICE);
                    });
                prepare_stop_handlers_.erase(found_cbk);
            }
        }
    };

    auto& its_data = _it->second;
    if (!_error) {
        its_data.queue_size_ -= its_data.queue_.front().first->size();
        its_data.queue_.pop_front();

        update_last_departure(its_data);

        if (!prepare_stop_handlers_.empty() && !endpoint_impl<Protocol>::sending_blocked_) {
            // only one service instance is stopped
            check_if_all_msgs_for_stopped_service_are_sent();
        }

        if (!its_data.queue_.empty()) {
            (void)send_queued(_it);
        } else if (!prepare_stop_handlers_.empty() && endpoint_impl<Protocol>::sending_blocked_) {
            // endpoint is shutting down completely
            cancel_dispatch_timer(_it);
            targets_.erase(_it);
            check_if_all_queues_are_empty();
        }
    } else {
        message_buffer_ptr_t its_buffer;
        if (its_data.queue_.size()) {
            its_buffer = its_data.queue_.front().first;
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
                << get_remote_information(_it) << " "
                << std::dec << its_data.queue_.size() << " "
                << its_data.queue_size_ << " ("
                << std::hex << std::setfill('0')
                << std::setw(4) << its_client << "): ["
                << std::setw(4) << its_service << "."
                << std::setw(4) << its_method << "."
                << std::setw(4) << its_session << "]";
        cancel_dispatch_timer(_it);
        targets_.erase(_it);
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
        target_data_iterator_type _it,
        const boost::system::error_code &_error_code) {

    if (!_error_code) {

        (void) flush(_it);
    }
}

template<typename Protocol>
size_t server_endpoint_impl<Protocol>::get_queue_size() const {
    size_t its_queue_size(0);
    {
        std::lock_guard<std::mutex> its_lock(mutex_);
        for (const auto &t : targets_) {
            its_queue_size += t.second.queue_size_;
        }
    }
    return (its_queue_size);
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::start_dispatch_timer(
        target_data_iterator_type _it,
        const std::chrono::steady_clock::time_point &_now) {

    auto &its_data = _it->second;
    std::shared_ptr<train> its_train(its_data.train_);

    if (!its_data.dispatched_trains_.empty()) {

        auto its_dispatched = its_data.dispatched_trains_.begin();
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
    its_data.dispatch_timer_->expires_from_now(its_offset);
#else
    its_data.dispatch_timer_->expires_from_now(
            std::chrono::duration_cast<
                std::chrono::steady_clock::duration>(its_offset));
#endif
    its_data.dispatch_timer_->async_wait(
            std::bind(&server_endpoint_impl<Protocol>::flush_cbk,
                      this->shared_from_this(), _it, std::placeholders::_1));
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::cancel_dispatch_timer(
        target_data_iterator_type _it) {

    boost::system::error_code ec;
    _it->second.dispatch_timer_->cancel(ec);
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::update_last_departure(
        endpoint_data_type &_data) {

    _data.last_departure_ = std::chrono::steady_clock::now();
    _data.has_last_departure_ = true;
}

// Instantiate template
#if defined(__linux__) || defined(ANDROID)
#if VSOMEIP_BOOST_VERSION < 106600
template class server_endpoint_impl<boost::asio::local::stream_protocol_ext>;
template class server_endpoint_impl<boost::asio::ip::udp_ext>;
#else
template class server_endpoint_impl<boost::asio::local::stream_protocol>;
template class server_endpoint_impl<boost::asio::ip::udp>;
#endif
#endif
template class server_endpoint_impl<boost::asio::ip::tcp>;

}  // namespace vsomeip_v3

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <memory>
#include <sstream>
#include <limits>
#include <thread>
#include <algorithm>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ip/udp.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/server_endpoint_impl.hpp"
#include "../include/endpoint_definition.hpp"

#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/utility.hpp"
#include "../../service_discovery/include/defines.hpp"

#define VSOMEIP_LOG_PREFIX "sei"

namespace vsomeip_v3 {

template<typename Protocol>
server_endpoint_impl<Protocol>::server_endpoint_impl(const std::shared_ptr<boardnet_endpoint_host>& _boardnet_endpoint_host,
                                                     const std::shared_ptr<routing_host>& _routing_host, boost::asio::io_context& _io,
                                                     const std::shared_ptr<configuration>& _configuration) :
    endpoint_impl<Protocol>(_boardnet_endpoint_host, _routing_host, _io, _configuration) { }

template<typename Protocol>
void server_endpoint_impl<Protocol>::stop(bool /*_due_to_error*/) { }

template<typename Protocol>
bool server_endpoint_impl<Protocol>::is_client() const {
    return false;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::restart(bool _force) {
    (void)_force;

    boost::system::error_code its_error;
    this->stop(false);
    this->init(server_endpoint_impl<Protocol>::local_, its_error);
    this->start();
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
    (void)_established;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::set_connected(bool _connected) {
    (void)_connected;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::send(const uint8_t* _data, uint32_t _size) {
    endpoint_type its_target;
    bool is_valid_target(false);

    if (VSOMEIP_SESSION_POS_MAX < _size) {
        std::scoped_lock its_lock{mutex_};

        if (endpoint_impl<Protocol>::sending_blocked_) {
            return false;
        }

        const service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
        const method_t its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
        const client_t its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);

        auto clients_key = to_clients_key(its_service, its_method, its_client);
        if (auto target = get_client_target(clients_key); target) {
            its_target = target.value();
            is_valid_target = true;
        } else {
            is_valid_target = get_default_target(its_service, its_target);
        }

        if (is_valid_target) {
            is_valid_target = send_intern(its_target, _data, _size);
        }
    }
    return is_valid_target;
}

template<typename Protocol>
typename server_endpoint_impl<Protocol>::clients_key_t
server_endpoint_impl<Protocol>::to_clients_key(service_t its_service, method_t its_method, client_t its_client) {
    return (static_cast<clients_key_t>(its_service) << 48) | (static_cast<clients_key_t>(its_method) << 32)
            | (static_cast<clients_key_t>(its_client) << 16);
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::send(const std::vector<byte_t>& _cmd_header, const byte_t* _data, uint32_t _size) {
    (void)_cmd_header;
    (void)_data;
    (void)_size;
    return false;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::send_intern(endpoint_type _target, const byte_t* _data, uint32_t _size) {

    if (!check_message_size(_size)) {
        return segment_message(_data, _size, _target) == endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT;
    }

    const auto its_target_iterator = find_or_create_target_unlocked(_target);
    auto& its_data(its_target_iterator->second);

    bool must_depart(false);
    auto its_now(std::chrono::steady_clock::now());

    // STEP 1: Check queue limit
    if (!check_queue_limit(_data, _size, its_data)) {
        return false;
    }

    // STEP 2: Cancel the dispatch timer
    cancel_dispatch_timer(its_target_iterator);

    // STEP 3: Get configured timings
    const service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
    const method_t its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);

    std::chrono::nanoseconds its_debouncing(0), its_retention(0);
    if (its_service != VSOMEIP_SD_SERVICE && its_method != VSOMEIP_SD_METHOD) {
        get_configured_times_from_endpoint(its_service, its_method, &its_debouncing, &its_retention);
    }

    // STEP 4: Check if the passenger enters an empty train
    const std::pair<service_t, method_t> its_identifier = std::make_pair(its_service, its_method);
    if (its_data.train_->passengers_.empty()) {
        its_data.train_->departure_ = its_now + its_retention;
    } else {
        if (its_data.train_->passengers_.end() != its_data.train_->passengers_.find(its_identifier)) {
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

    return true;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::set_client_target(const clients_key_t _client, const endpoint_type& _target) {
    std::scoped_lock its_clients_lock{clients_mutex_};
    if (auto it = clients_to_target_.find(_client); it != clients_to_target_.end()) {
        auto& [id, current_target] = *it;
        if (current_target != _target) {
            const auto service = static_cast<uint16_t>((_client >> 48) & 0xffff);
            const auto method = static_cast<uint16_t>((_client >> 32) & 0xffff);
            const auto client = static_cast<uint16_t>((_client >> 16) & 0xffff);
            VSOMEIP_ERROR << "sei::" << __func__ << ": Target replaced. May cause pending response to be lost. message=" << hex4(service)
                          << "." << hex4(method) << " client=" << hex4(client) << " old=" << current_target << " new=" << _target;
        }
    }
    clients_to_target_[_client] = _target;
}

template<typename Protocol>
std::optional<typename Protocol::endpoint> server_endpoint_impl<Protocol>::get_client_target(const clients_key_t _client) {
    std::scoped_lock its_clients_lock{clients_mutex_};
    if (auto it = clients_to_target_.find(_client); it != clients_to_target_.end()) {
        auto& [id, target] = *it;
        return std::optional(target);
    }
    return std::nullopt;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::tp_segmentation_enabled(service_t /*_service*/, instance_t /*_instance*/, method_t /*_method*/) const {

    return false;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::send_segments(const tp::tp_split_messages_t& _segments, std::uint32_t _separation_time,
                                                   const endpoint_type& _target) {

    if (_segments.size() == 0)
        return;

    const auto its_target_iterator = find_or_create_target_unlocked(_target);
    auto& its_data = its_target_iterator->second;

    auto its_now(std::chrono::steady_clock::now());

    const service_t its_service = bithelper::read_uint16_be(&(*(_segments[0]))[VSOMEIP_SERVICE_POS_MIN]);
    const method_t its_method = bithelper::read_uint16_be(&(*(_segments[0]))[VSOMEIP_METHOD_POS_MIN]);

    std::chrono::nanoseconds its_debouncing(0), its_retention(0);
    if (its_service != VSOMEIP_SD_SERVICE && its_method != VSOMEIP_SD_METHOD) {
        get_configured_times_from_endpoint(its_service, its_method, &its_debouncing, &its_retention);
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
        its_data.train_ = std::make_shared<train>();
        its_data.train_->departure_ = its_now + its_retention;
    }

    for (const auto& s : _segments) {
        its_data.queue_.emplace_back(s, _separation_time);
        its_data.queue_size_ += s->size();
    }

    if (!its_data.is_sending_ && !its_data.queue_.empty()) { // no writing in progress
        // ignore retention time and send immediately as the train is full anyway
        (void)send_queued(its_target_iterator);
    }
}

template<typename Protocol>
typename server_endpoint_impl<Protocol>::target_data_iterator_type
server_endpoint_impl<Protocol>::find_or_create_target_unlocked(endpoint_type _target) {

    auto its_iterator = targets_.find(_target);
    if (its_iterator == targets_.end()) {
        auto its_result = targets_.emplace(std::make_pair(_target, endpoint_data_type(this->io_)));
        its_iterator = its_result.first;
    }

    return its_iterator;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::schedule_train(endpoint_data_type& _data) {

    if (_data.has_last_departure_) {
        if (_data.last_departure_ + _data.train_->minimal_debounce_time_ > _data.train_->departure_) {
            _data.train_->departure_ = _data.last_departure_ + _data.train_->minimal_debounce_time_;
        }
    }

    _data.dispatched_trains_[_data.train_->departure_].push_back(_data.train_);
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::check_message_size(std::uint32_t _size) const {
    return !(endpoint_impl<Protocol>::max_message_size_ != MESSAGE_SIZE_UNLIMITED && _size > endpoint_impl<Protocol>::max_message_size_);
}

template<typename Protocol>
typename endpoint_impl<Protocol>::cms_ret_e
server_endpoint_impl<Protocol>::segment_message(const std::uint8_t* const _data, std::uint32_t _size, const endpoint_type& _target) {

    if (endpoint_impl<Protocol>::is_supporting_someip_tp_ && _data != nullptr) {
        const service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
        const method_t its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
        instance_t its_instance = this->get_instance(its_service);

        if (its_instance != ANY_INSTANCE) {
            if (tp_segmentation_enabled(its_service, its_instance, its_method)) {
                std::uint16_t its_max_segment_length;
                std::uint32_t its_separation_time;

                this->configuration_->get_tp_configuration(its_service, its_instance, its_method, false, its_max_segment_length,
                                                           its_separation_time);
                send_segments(tp::tp::tp_split_message(_data, _size, its_max_segment_length), its_separation_time, _target);
                return endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT;
            }
        }
    }
    VSOMEIP_ERROR_P << "Dropping to big message (" << _size
                    << " Bytes). Maximum allowed message size is: " << endpoint_impl<Protocol>::max_message_size_ << " Bytes.";
    return endpoint_impl<Protocol>::cms_ret_e::MSG_TOO_BIG;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::recalculate_queue_size(endpoint_data_type& _data) const {
    _data.queue_size_ = 0;
    for (const auto& q : _data.queue_) {
        if (q.first) {
            _data.queue_size_ += q.first->size();
        }
    }
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::check_queue_limit(const uint8_t* _data, std::uint32_t _size,
                                                       endpoint_data_type& _endpoint_data) const {

    // No queue limit --> Fine
    if (endpoint_impl<Protocol>::queue_limit_ == QUEUE_SIZE_UNLIMITED) {
        return true;
    }

    // Current queue size is bigger than the maximum queue size
    if (_endpoint_data.queue_size_ > endpoint_impl<Protocol>::queue_limit_) {
        size_t its_error_queue_size{_endpoint_data.queue_size_};
        recalculate_queue_size(_endpoint_data);

        VSOMEIP_WARNING_P << "Detected possible queue size underflow (" << std::dec << its_error_queue_size << "). Recalculating it ("
                          << std::dec << _endpoint_data.queue_size_ << ")";
    }

    if (_endpoint_data.queue_size_ + _size > endpoint_impl<Protocol>::queue_limit_
        || _endpoint_data.queue_size_ + _size < _size) { // overflow protection
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
            its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
            its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
            its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);
            its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
        }
        VSOMEIP_ERROR_P << "Queue size limit (" << std::dec << endpoint_impl<Protocol>::queue_limit_ << ") reached. Dropping message ("
                        << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_method) << "." << hex4(its_session) << "]"
                        << " queue_size: " << std::dec << _endpoint_data.queue_size_ << " data size: " << _size;
        return false;
    }
    return true;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::queue_train(target_data_iterator_type _it, const std::shared_ptr<train>& _train) {

    bool must_erase(false);

    auto& its_data = _it->second;
    its_data.queue_size_ += _train->buffer_->size();
    its_data.queue_.emplace_back(_train->buffer_, 0);

    if (!its_data.is_sending_) { // no writing in progress
        must_erase = send_queued(_it);
    } else if (get_local_port() == this->configuration_->get_sd_port()
               && is_reliable() == (this->configuration_->get_sd_protocol() == "tcp")) {
        VSOMEIP_WARNING_P << "SD endpoint is currently sending " << its_data.is_sending_ << " queue size: " << its_data.queue_size_
                          << " target address: " << _it->first;
    }

    return must_erase;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::flush(endpoint_type _key) {

    bool has_queued(true);
    bool is_current_train(true);

    std::scoped_lock its_lock(mutex_);

    auto it = targets_.find(_key);
    if (it == targets_.end())
        return false;

    auto& its_data = it->second;
    auto its_train(its_data.train_);
    if (!its_data.dispatched_trains_.empty()) {

        auto its_dispatched = its_data.dispatched_trains_.begin();
        if (its_dispatched->first <= its_train->departure_) {

            is_current_train = false;
            if (!its_dispatched->second.empty()) {
                its_train = its_dispatched->second.front();
                its_dispatched->second.pop_front();
                if (its_dispatched->second.empty()) {

                    its_data.dispatched_trains_.erase(its_dispatched);
                }
            }
        }
    }

    if (!its_train->buffer_->empty()) {

        queue_train(it, its_train);

        // Reset current train if necessary
        if (is_current_train) {
            its_train->reset();
        }
    } else {
        has_queued = false;
    }

    if (!is_current_train || !its_data.dispatched_trains_.empty()) {

        auto its_now(std::chrono::steady_clock::now());
        start_dispatch_timer(it, its_now);
    }

    return has_queued;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::connect_cbk(boost::system::error_code const& _error) {
    (void)_error;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::send_cbk(const endpoint_type _key, boost::system::error_code const& _error, std::size_t _bytes) {
    (void)_bytes;

    std::scoped_lock its_lock(mutex_);

    auto it = targets_.find(_key);
    if (it == targets_.end())
        return;

    auto& its_data = it->second;

    its_data.sent_timer_.cancel();

    // Extracts some information for logging puposes.
    //
    // TODO(brunoldsilva): Code like this is used in a lot of places. It might be worth moving this
    // into a proper function.
    auto parse_message_ids = [](const message_buffer_ptr_t& buffer, service_t& its_service, method_t& its_method, client_t& its_client,
                                session_t& its_session) {
        if (buffer && buffer->size() > VSOMEIP_SESSION_POS_MAX) {
            its_service = bithelper::read_uint16_be(&(*buffer)[VSOMEIP_SERVICE_POS_MIN]);
            its_method = bithelper::read_uint16_be(&(*buffer)[VSOMEIP_METHOD_POS_MIN]);
            its_client = bithelper::read_uint16_be(&(*buffer)[VSOMEIP_CLIENT_POS_MIN]);
            its_session = bithelper::read_uint16_be(&(*buffer)[VSOMEIP_SESSION_POS_MIN]);
        }
    };

    message_buffer_ptr_t its_buffer;
    if (its_data.queue_.size()) {
        its_buffer = its_data.queue_.front().first;
    }

    if (!its_buffer) {
        // Pointer not initialized.
        its_buffer = std::make_shared<message_buffer_t>();
        VSOMEIP_WARNING_P << "Prevented nullptr de-reference by initializing queue buffer";
    }

    service_t its_service(0);
    method_t its_method(0);
    client_t its_client(0);
    session_t its_session(0);

    if (!_error) {
        const std::size_t payload_size = its_buffer->size();
        if (payload_size <= its_data.queue_size_) {
            its_data.queue_size_ -= payload_size;
            its_data.queue_.pop_front();
        } else {
            parse_message_ids(its_buffer, its_service, its_method, its_client, its_session);
            VSOMEIP_WARNING_P << "Prevented queue_size underflow. queue_size: " << its_data.queue_size_ << " payload_size: " << payload_size
                              << " payload: (" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_method) << "."
                              << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_method) << "." << hex4(its_session)
                              << "]";
            its_data.queue_.pop_front();
            recalculate_queue_size(its_data);
        }

        update_last_departure(its_data);

        if (its_data.queue_.empty() || !send_queued(it)) {
            its_data.is_sending_ = false;
        }
    } else {
        // error: sending of outstanding responses isn't started again
        // delete remaining outstanding responses
        parse_message_ids(its_buffer, its_service, its_method, its_client, its_session);
        VSOMEIP_WARNING_P << "Received error: " << _error.message() << " (" << std::dec << _error.value() << ") "
                          << get_remote_information(it) << " " << its_data.queue_.size() << " " << its_data.queue_size_ << " ("
                          << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_method) << "." << hex4(its_session)
                          << "]  endpoint -> " << this;
        cancel_dispatch_timer(it);
        targets_.erase(it);
    }
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::flush_cbk(endpoint_type _key, const boost::system::error_code& _error_code) {

    if (!_error_code) {

        (void)flush(_key);
    }
}

template<typename Protocol>
size_t server_endpoint_impl<Protocol>::get_queue_size() const {
    size_t its_queue_size(0);
    {
        std::scoped_lock its_lock(mutex_);
        for (const auto& t : targets_) {
            its_queue_size += t.second.queue_size_;
        }
    }
    return its_queue_size;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::start_dispatch_timer(target_data_iterator_type _it,
                                                          const std::chrono::steady_clock::time_point& _now) {

    auto& its_data = _it->second;
    std::shared_ptr<train> its_train(its_data.train_);

    if (!its_data.dispatched_trains_.empty()) {

        auto its_dispatched = its_data.dispatched_trains_.begin();
        if (its_dispatched->first < its_train->departure_) {

            its_train = its_dispatched->second.front();
        }
    }

    std::chrono::nanoseconds its_offset;
    if (its_train->departure_ > _now) {

        its_offset = std::chrono::duration_cast<std::chrono::nanoseconds>(its_train->departure_ - _now);
    } else { // already departure time

        its_offset = std::chrono::nanoseconds::zero();
    }

#if defined(__linux__) || defined(__QNX__)
    its_data.dispatch_timer_->expires_after(its_offset);
#else
    its_data.dispatch_timer_->expires_after(std::chrono::duration_cast<std::chrono::steady_clock::duration>(its_offset));
#endif
    its_data.dispatch_timer_->async_wait(
            std::bind(&server_endpoint_impl<Protocol>::flush_cbk, this->shared_from_this(), _it->first, std::placeholders::_1));
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::cancel_dispatch_timer(target_data_iterator_type _it) {
    _it->second.dispatch_timer_->cancel();
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::update_last_departure(endpoint_data_type& _data) {
    _data.last_departure_ = std::chrono::steady_clock::now();
    _data.has_last_departure_ = true;
}

// Instantiate template
#if defined(__linux__) || defined(__QNX__)
template class server_endpoint_impl<boost::asio::local::stream_protocol>;
#endif

template class server_endpoint_impl<boost::asio::ip::tcp>;
template class server_endpoint_impl<boost::asio::ip::udp>;

} // namespace vsomeip_v3

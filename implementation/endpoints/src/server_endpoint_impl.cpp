// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>
#include <limits>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp_ext.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>

#include "../include/server_endpoint_impl.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

template<typename Protocol>
server_endpoint_impl<Protocol>::server_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _local,
        boost::asio::io_service &_io, std::uint32_t _max_message_size,
        configuration::endpoint_queue_limit_t _queue_limit)
    : endpoint_impl<Protocol>(_host, _local, _io, _max_message_size,
                              _queue_limit),
      flush_timer_(_io) {
}

template<typename Protocol>
server_endpoint_impl<Protocol>::~server_endpoint_impl() {
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::stop() {
    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_impl<Protocol>::sending_blocked_ = true;
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
bool server_endpoint_impl<Protocol>::is_connected() const {
    return true;
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::set_connected(bool _connected) {    (void) _connected;}
template<typename Protocol>bool server_endpoint_impl<Protocol>::send(const uint8_t *_data,
        uint32_t _size, bool _flush) {
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

        service_t its_service;
        std::memcpy(&its_service, &_data[VSOMEIP_SERVICE_POS_MIN],
                sizeof(service_t));

        client_t its_client;
        std::memcpy(&its_client, &_data[VSOMEIP_CLIENT_POS_MIN],
                sizeof(client_t));
        session_t its_session;
        std::memcpy(&its_session, &_data[VSOMEIP_SESSION_POS_MIN],
                sizeof(session_t));

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
            }
        } else {
            is_valid_target = get_default_target(its_service, its_target);
        }
        clients_mutex_.unlock();

        if (is_valid_target) {
            is_valid_target = send_intern(its_target, _data, _size, _flush);
        }
    }
    return is_valid_target;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::send(
        const std::vector<byte_t>& _cmd_header, const byte_t *_data,
        uint32_t _size, bool _flush) {
    (void) _cmd_header;
    (void) _data;
    (void) _size;
    (void) _flush;
    return false;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::send_intern(
        endpoint_type _target, const byte_t *_data, uint32_t _size,
        bool _flush) {

    message_buffer_ptr_t target_packetizer;
    queue_iterator_type target_queue_iterator;

    if (endpoint_impl<Protocol>::max_message_size_ != MESSAGE_SIZE_UNLIMITED
            && _size > endpoint_impl<Protocol>::max_message_size_) {
        VSOMEIP_ERROR << "sei::send_intern: Dropping to big message (" << _size
                << " Bytes). Maximum allowed message size is: "
                << endpoint_impl<Protocol>::max_message_size_ << " Bytes.";
        return false;
    }

    auto found_packetizer = packetizer_.find(_target);
    if (found_packetizer != packetizer_.end()) {
        target_packetizer = found_packetizer->second;
    } else {
        target_packetizer = std::make_shared<message_buffer_t>();
        packetizer_.insert(std::make_pair(_target, target_packetizer));
    }

    target_queue_iterator = queues_.find(_target);
    if (target_queue_iterator == queues_.end()) {
        target_queue_iterator = queues_.insert(queues_.begin(),
                                    std::make_pair(
                                        _target,
                                        std::make_pair(std::size_t(0),
                                                       std::deque<message_buffer_ptr_t>())
                                    ));
    }

    // TODO compare against value from configuration here
    const bool queue_size_zero_on_entry(target_queue_iterator->second.second.empty());
    if (target_packetizer->size() + _size
            > endpoint_impl<Protocol>::max_message_size_
            && !target_packetizer->empty()) {
        target_queue_iterator->second.second.push_back(target_packetizer);
        target_queue_iterator->second.first += target_packetizer->size();
        target_packetizer = std::make_shared<message_buffer_t>();
        packetizer_[_target] = target_packetizer;
    }

    if (endpoint_impl<Protocol>::queue_limit_ != QUEUE_SIZE_UNLIMITED
            && target_queue_iterator->second.first + _size >
                                        endpoint_impl<Protocol>::queue_limit_) {
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
        VSOMEIP_ERROR << "sei::send_intern: queue size limit (" << std::dec
                << endpoint_impl<Protocol>::queue_limit_
                << ") reached. Dropping message ("
                << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_session << "]"
                << " queue_size: " << std::dec << target_queue_iterator->second.first
                << " data size: " << std::dec << _size;
        return false;
    }


    target_packetizer->insert(target_packetizer->end(), _data, _data + _size);

    if (_flush) {
        flush_timer_.cancel();
        target_queue_iterator->second.second.push_back(target_packetizer);
        target_queue_iterator->second.first += target_packetizer->size();
        packetizer_[_target] = std::make_shared<message_buffer_t>();
    } else {
        std::chrono::milliseconds flush_timeout(VSOMEIP_DEFAULT_FLUSH_TIMEOUT);
        flush_timer_.expires_from_now(flush_timeout); // TODO: use configured value
        flush_timer_.async_wait(
                std::bind(&server_endpoint_impl<Protocol>::flush_cbk,
                          this->shared_from_this(),
                          _target,
                          std::placeholders::_1));
    }

    if (queue_size_zero_on_entry && !target_queue_iterator->second.second.empty()) { // no writing in progress
        send_queued(target_queue_iterator);
    }

    return true;
}

template<typename Protocol>
bool server_endpoint_impl<Protocol>::flush(
        endpoint_type _target) {
    bool is_flushed = false;
    std::lock_guard<std::mutex> its_lock(mutex_);
    auto queue_iterator = queues_.find(_target);
    if (queue_iterator != queues_.end() && !queue_iterator->second.second.empty()) {
        send_queued(queue_iterator);
        is_flushed = true;
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

    std::lock_guard<std::mutex> its_lock(mutex_);
    if (!_error) {
        _queue_iterator->second.first -=
                _queue_iterator->second.second.front()->size();
        _queue_iterator->second.second.pop_front();
        if (_queue_iterator->second.second.size() > 0) {
            send_queued(_queue_iterator);
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
    }
}

template<typename Protocol>
void server_endpoint_impl<Protocol>::flush_cbk(
        endpoint_type _target, const boost::system::error_code &_error_code) {
    if (!_error_code) {
        (void) flush(_target);
    }
}

// Instantiate template
#ifndef _WIN32
template class server_endpoint_impl<boost::asio::local::stream_protocol>;
#endif
template class server_endpoint_impl<boost::asio::ip::tcp>;
template class server_endpoint_impl<boost::asio::ip::udp_ext>;

}  // namespace vsomeip

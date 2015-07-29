// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/server_endpoint_impl.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

template<typename Protocol, int MaxBufferSize>
server_endpoint_impl<Protocol, MaxBufferSize>::server_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _local,
        boost::asio::io_service &_io)
    : endpoint_impl<MaxBufferSize>(_host, _io), flush_timer_(_io) {
}

template<typename Protocol, int MaxBufferSize>
bool server_endpoint_impl<Protocol, MaxBufferSize>::is_client() const {
    return false;
}

template<typename Protocol, int MaxBufferSize>
bool server_endpoint_impl<Protocol, MaxBufferSize>::is_connected() const {
    return true;
}

template<typename Protocol, int MaxBufferSize>
bool server_endpoint_impl<Protocol, MaxBufferSize>::send(const uint8_t *_data,
        uint32_t _size, bool _flush) {
#if 0
    std::stringstream msg;
    msg << "sei::send ";
    for (uint32_t i = 0; i < _size; i++)
    msg << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    endpoint_type its_target;
    bool is_valid_target(false);

    if (VSOMEIP_SESSION_POS_MAX < _size) {
        std::lock_guard<std::mutex> its_lock(mutex_);

        service_t its_service;
        std::memcpy(&its_service, &_data[VSOMEIP_SERVICE_POS_MIN],
                sizeof(service_t));

        client_t its_client;
        std::memcpy(&its_client, &_data[VSOMEIP_CLIENT_POS_MIN],
                sizeof(client_t));
        session_t its_session;
        std::memcpy(&its_session, &_data[VSOMEIP_SESSION_POS_MIN],
                sizeof(session_t));

        auto found_client = clients_.find(its_client);
        if (found_client != clients_.end()) {
            auto found_session = found_client->second.find(its_session);
            if (found_session != found_client->second.end()) {
                its_target = found_session->second;
                is_valid_target = true;
            }
        } else {
            event_t its_event = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_METHOD_POS_MIN],
                    _data[VSOMEIP_METHOD_POS_MAX]);
            is_valid_target
                = get_multicast(its_service, its_event, its_target);
        }

        if (is_valid_target) {
            is_valid_target = send_intern(its_target, _data, _size, _flush);
        }
    }
    return is_valid_target;
}

template<typename Protocol, int MaxBufferSize>
bool server_endpoint_impl<Protocol, MaxBufferSize>::send_intern(
        endpoint_type _target, const byte_t *_data, uint32_t _size,
        bool _flush) {

    std::shared_ptr<std::vector<byte_t> > target_packetizer;
    auto found_packetizer = packetizer_.find(_target);
    if (found_packetizer != packetizer_.end()) {
        target_packetizer = found_packetizer->second;
    } else {
        target_packetizer = std::make_shared<message_buffer_t>();
        packetizer_.insert(std::make_pair(_target, target_packetizer));
    }

    if (target_packetizer->size() + _size > MaxBufferSize) {
        send_queued(_target, target_packetizer);
        packetizer_[_target] = std::make_shared<message_buffer_t>();
    }

    target_packetizer->insert(target_packetizer->end(), _data, _data + _size);

    if (_flush) {
        flush_timer_.cancel();
        send_queued(_target, target_packetizer);
        packetizer_[_target] = std::make_shared<message_buffer_t>();
    } else {
        std::chrono::milliseconds flush_timeout(VSOMEIP_DEFAULT_FLUSH_TIMEOUT);
        flush_timer_.expires_from_now(flush_timeout); // TODO: use configured value
        flush_timer_.async_wait(
                std::bind(&server_endpoint_impl<
                              Protocol, MaxBufferSize
                          >::flush_cbk,
                          this->shared_from_this(),
                          _target,
                          std::placeholders::_1));
    }
    return true;
}

template<typename Protocol, int MaxBufferSize>
bool server_endpoint_impl<Protocol, MaxBufferSize>::flush(
        endpoint_type _target) {
    bool is_flushed = false;
    std::lock_guard<std::mutex> its_lock(mutex_);
    auto i = packetizer_.find(_target);
    if (i != packetizer_.end() && !i->second->empty()) {
        send_queued(_target, i->second);
        i->second = std::make_shared<message_buffer_t>();
        is_flushed = true;
    }

    return is_flushed;
}

template<typename Protocol, int MaxBufferSize>
void server_endpoint_impl<Protocol, MaxBufferSize>::connect_cbk(
        boost::system::error_code const &_error) {
}

template<typename Protocol, int MaxBufferSize>
void server_endpoint_impl<Protocol, MaxBufferSize>::send_cbk(
        message_buffer_ptr_t _buffer, boost::system::error_code const &_error,
        std::size_t _bytes) {
#if 0
    std::stringstream msg;
    msg << "sei::scb (" << _error.message() << "): ";
    for (std::size_t i = 0; i < _buffer->size(); ++i)
    msg << std::hex << std::setw(2) << std::setfill('0')
    << (int)(*_buffer)[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
}

template<typename Protocol, int MaxBufferSize>
void server_endpoint_impl<Protocol, MaxBufferSize>::flush_cbk(
        endpoint_type _target, const boost::system::error_code &_error_code) {
    if (!_error_code) {
        (void) flush(_target);
    }
}

// Instantiate template
#ifndef WIN32
template class server_endpoint_impl<boost::asio::local::stream_protocol,
VSOMEIP_MAX_LOCAL_MESSAGE_SIZE> ;
#else
// TODO: put instantiation for windows here!
//template class server_endpoint_impl<boost::asio::ip::tcp,
//    VSOMEIP_MAX_TCP_MESSAGE_SIZE>;
#endif
template class server_endpoint_impl<boost::asio::ip::tcp,
VSOMEIP_MAX_TCP_MESSAGE_SIZE> ;
template class server_endpoint_impl<boost::asio::ip::udp,
VSOMEIP_MAX_UDP_MESSAGE_SIZE> ;

}  // namespace vsomeip

// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/endpoint_impl.hpp"

namespace vsomeip {

template<int MaxBufferSize>
endpoint_impl<MaxBufferSize>::endpoint_impl(
        std::shared_ptr<endpoint_host> _host, boost::asio::io_service &_io)
    : host_(_host),
      service_(_io),
      is_supporting_magic_cookies_(false),
      has_enabled_magic_cookies_(false) {
}

template<int MaxBufferSize>
endpoint_impl<MaxBufferSize>::~endpoint_impl() {
}

template<int MaxBufferSize>
void endpoint_impl<MaxBufferSize>::enable_magic_cookies() {
    has_enabled_magic_cookies_ = is_supporting_magic_cookies_;
}

template<int MaxBufferSize>
bool endpoint_impl<MaxBufferSize>::is_magic_cookie() const {
    return false;
}

template<int MaxBufferSize>
uint32_t endpoint_impl<MaxBufferSize>::find_magic_cookie(
        message_buffer_t &_buffer) {
    bool is_found(false);
    uint32_t its_offset = 0xFFFFFFFF;
    if (has_enabled_magic_cookies_) {
        uint8_t its_cookie_identifier, its_cookie_type;

        if (is_client()) {
            its_cookie_identifier =
                    static_cast<uint8_t>(MAGIC_COOKIE_SERVICE_MESSAGE);
            its_cookie_type =
                    static_cast<uint8_t>(MAGIC_COOKIE_SERVICE_MESSAGE_TYPE);
        } else {
            its_cookie_identifier =
                    static_cast<uint8_t>(MAGIC_COOKIE_CLIENT_MESSAGE);
            its_cookie_type =
                    static_cast<uint8_t>(MAGIC_COOKIE_CLIENT_MESSAGE_TYPE);
        }

        do {
            its_offset++; // --> first loop has "its_offset = 0"
            if (_buffer.size() > its_offset + 16) {
                is_found = (_buffer[its_offset] == 0xFF
                         && _buffer[its_offset + 1] == 0xFF
                         && _buffer[its_offset + 2] == its_cookie_identifier
                         && _buffer[its_offset + 3] == 0x00
                         && _buffer[its_offset + 4] == 0x00
                         && _buffer[its_offset + 5] == 0x00
                         && _buffer[its_offset + 6] == 0x00
                         && _buffer[its_offset + 7] == 0x08
                         && _buffer[its_offset + 8] == 0xDE
                         && _buffer[its_offset + 9] == 0xAD
                         && _buffer[its_offset + 10] == 0xBE
                         && _buffer[its_offset + 11] == 0xEF
                         && _buffer[its_offset + 12] == 0x01
                         && _buffer[its_offset + 13] == 0x01
                         && _buffer[its_offset + 14] == its_cookie_type
                         && _buffer[its_offset + 15] == 0x00);
            } else {
                break;
            }

        } while (!is_found);
    }

    return (is_found ? its_offset : 0xFFFFFFFF);
}

template<int MaxBufferSize>
void endpoint_impl<MaxBufferSize>::join(const std::string &) {
}

template<int MaxBufferSize>
void endpoint_impl<MaxBufferSize>::leave(const std::string &) {
}

template<int MaxBufferSize>
void endpoint_impl<MaxBufferSize>::add_multicast(
        service_t, event_t, const std::string &, uint16_t) {
}

template<int MaxBufferSize>
void endpoint_impl<MaxBufferSize>::remove_multicast(service_t, event_t) {
}

template<int MaxBufferSize>
bool endpoint_impl<MaxBufferSize>::get_remote_address(
        boost::asio::ip::address &_address) const {
    return false;
}

template<int MaxBufferSize>
unsigned short endpoint_impl<MaxBufferSize>::get_local_port() const {
    return 0;
}

template<int MaxBufferSize>
unsigned short endpoint_impl<MaxBufferSize>::get_remote_port() const {
    return 0;
}

template<int MaxBufferSize>
bool endpoint_impl<MaxBufferSize>::is_reliable() const {
    return false;
}

// Instantiate template
template class endpoint_impl< VSOMEIP_MAX_LOCAL_MESSAGE_SIZE> ;
template class endpoint_impl< VSOMEIP_MAX_TCP_MESSAGE_SIZE> ;
template class endpoint_impl< VSOMEIP_MAX_UDP_MESSAGE_SIZE> ;

} // namespace vsomeip

// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/endpoint_impl.hpp"
#include "../../message/include/byteorder.hpp"

namespace vsomeip {

template < int MaxBufferSize >
endpoint_impl< MaxBufferSize >::endpoint_impl(
		std::shared_ptr< endpoint_host > _host, boost::asio::io_service &_io)
	: host_(_host.get()),
	  service_(_io),
	  is_supporting_magic_cookies_(false),
	  has_enabled_magic_cookies_(false) {
}

template < int MaxBufferSize >
endpoint_impl< MaxBufferSize >::~endpoint_impl() {
}

template < int MaxBufferSize >
void endpoint_impl< MaxBufferSize >::open_filter(service_t _service) {
	auto find_service = opened_.find(_service);
	if (find_service != opened_.end()) {
		find_service->second++;
	} else {
		opened_[_service] = 1;
	}
}

template < int MaxBufferSize >
void endpoint_impl< MaxBufferSize >::close_filter(service_t _service) {
	auto find_service = opened_.find(_service);
	if (find_service != opened_.end()) {
		find_service->second--;
		if (0 == find_service->second)
			opened_.erase(_service);
	}
}

template < int MaxBufferSize >
void endpoint_impl< MaxBufferSize >::enable_magic_cookies() {
	has_enabled_magic_cookies_ = is_supporting_magic_cookies_;
}

template < int MaxBufferSize >
bool endpoint_impl< MaxBufferSize >::is_magic_cookie() const {
	return false;
}

template < int MaxBufferSize >
bool endpoint_impl< MaxBufferSize >::resync_on_magic_cookie(message_buffer_t &_buffer) {
	bool is_resynced = false;
	if (has_enabled_magic_cookies_) {
		uint32_t its_offset = 0xFFFFFFFF;
		uint8_t its_cookie_identifier, its_cookie_type;

		if (is_client()) {
			its_cookie_identifier
				= static_cast< uint8_t >(magic_cookie_service_message);
			its_cookie_type
				= static_cast< uint8_t >(magic_cookie_service_message_type);
		} else {
			its_cookie_identifier
				= static_cast< uint8_t >(magic_cookie_client_message);
			its_cookie_type
				= static_cast< uint8_t >(magic_cookie_client_message_type);
		}

		do {
			 its_offset++;
			 if (_buffer.size() > its_offset + 16) {
				 is_resynced = (
						 _buffer[its_offset] == 0xFF &&
						 _buffer[its_offset+1] == 0xFF &&
						 _buffer[its_offset+2] == its_cookie_identifier &&
						 _buffer[its_offset+3] == 0x00 &&
						 _buffer[its_offset+4] == 0x00 &&
						 _buffer[its_offset+5] == 0x00 &&
						 _buffer[its_offset+6] == 0x00 &&
						 _buffer[its_offset+7] == 0x08 &&
						 _buffer[its_offset+8] == 0xDE &&
						 _buffer[its_offset+9] == 0xAD &&
						 _buffer[its_offset+10] == 0xBE &&
						 _buffer[its_offset+11] == 0xEF &&
						 _buffer[its_offset+12] == 0x01 &&
						 _buffer[its_offset+13] == 0x01 &&
						 _buffer[its_offset+14] == its_cookie_type &&
						 _buffer[its_offset+15] == 0x00
				 );
			 } else {
				 break;
			 }

		} while (!is_resynced);

		if (is_resynced) {
			_buffer.erase(_buffer.begin(),
					   	  _buffer.begin() + its_offset +
								VSOMEIP_SOMEIP_HEADER_SIZE +
								VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE);
		}
	}

	return is_resynced;
}

template < int MaxBufferSize >
bool endpoint_impl< MaxBufferSize >::is_v4() const {
	return false;
}

template < int MaxBufferSize >
bool endpoint_impl< MaxBufferSize >::get_address(std::vector< byte_t > &_address) const {
	return false;
}

template < int MaxBufferSize >
unsigned short endpoint_impl< MaxBufferSize >::get_port() const {
	return 0;
}

template < int MaxBufferSize >
bool endpoint_impl< MaxBufferSize >::is_udp() const {
	return false;
}

// Instantiate template
template class endpoint_impl< VSOMEIP_MAX_LOCAL_MESSAGE_SIZE >;
template class endpoint_impl< VSOMEIP_MAX_TCP_MESSAGE_SIZE >;
template class endpoint_impl< VSOMEIP_MAX_UDP_MESSAGE_SIZE >;

} // namespace vsomeip



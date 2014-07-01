// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>

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
	has_enabled_magic_cookies_ = (true && is_supporting_magic_cookies_);
}

template < int MaxBufferSize >
void endpoint_impl< MaxBufferSize >::disable_magic_cookies() {
	has_enabled_magic_cookies_ = (false && is_supporting_magic_cookies_);
}

template < int MaxBufferSize >
bool endpoint_impl< MaxBufferSize >::is_magic_cookie() const {
	return false;
}

/*
template < int MaxBufferSize >
bool endpoint_impl< MaxBufferSize >::resync_on_magic_cookie() {
	bool is_resynced = false;
	if (has_enabled_magic_cookies_) {
		uint32_t offset = 0xFFFFFFFF;
		uint8_t cookie_identifier = (
				is_client() ?
					VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_ID :
					VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_ID
		);
		uint8_t cookie_type = static_cast<uint8_t>(
				is_client() ?
					VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_TYPE :
					VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_TYPE
		);

		do {
			 offset++;

			 if (message_.size() > offset + 16) {
				 is_resynced = (
						 message_[offset] == 0xFF &&
						 message_[offset+1] == 0xFF &&
						 message_[offset+2] == cookie_identifier &&
						 message_[offset+3] == 0x00 &&
						 message_[offset+4] == 0x00 &&
						 message_[offset+5] == 0x00 &&
						 message_[offset+6] == 0x00 &&
						 message_[offset+7] == 0x08 &&
						 message_[offset+8] == 0xDE &&
						 message_[offset+9] == 0xAD &&
						 message_[offset+10] == 0xBE &&
						 message_[offset+11] == 0xEF &&
						 message_[offset+12] == 0x01 &&
						 message_[offset+13] == 0x01 &&
						 message_[offset+14] == cookie_type &&
						 message_[offset+15] == 0x00
				 );
			 };

		} while (!is_resynced);

		if (is_resynced) {
			message_.erase(message_.begin(),
						   message_.begin() + offset +
								VSOMEIP_SOMEIP_HEADER_SIZE +
								VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE);
		} else {
			message_.clear();
		}

	} else {
		message_.clear();
	}

	return is_resynced;
}
*/

// Instantiate template
template class endpoint_impl< VSOMEIP_MAX_LOCAL_MESSAGE_SIZE >;
template class endpoint_impl< VSOMEIP_MAX_TCP_MESSAGE_SIZE >;
template class endpoint_impl< VSOMEIP_MAX_UDP_MESSAGE_SIZE >;

} // namespace vsomeip



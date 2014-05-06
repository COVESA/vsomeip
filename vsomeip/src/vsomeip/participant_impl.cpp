//
// participant_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>

#include <vsomeip/config.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/participant_impl.hpp>
#include <vsomeip_internal/managing_proxy_impl.hpp>

namespace vsomeip {

template < int MaxBufferSize >
participant_impl< MaxBufferSize >::participant_impl(managing_proxy_impl *_owner, const endpoint *_location)
	: log_user(*_owner),
	  owner_(_owner),
	  location_(_location),
	  service_(_owner->get_service()),
	  is_supporting_magic_cookies_(false),
	  has_enabled_magic_cookies_(false) {
}

template < int MaxBufferSize >
participant_impl< MaxBufferSize >::~participant_impl() {
}

template < int MaxBufferSize >
void participant_impl< MaxBufferSize >::open_filter(service_id _service) {
	auto find_service = opened_.find(_service);
	if (find_service != opened_.end()) {
		find_service->second++;
	} else {
		opened_[_service] = 1;
	}
}

template < int MaxBufferSize >
void participant_impl< MaxBufferSize >::close_filter(service_id _service) {
	auto find_service = opened_.find(_service);
	if (find_service != opened_.end()) {
		find_service->second--;
		if (0 == find_service->second)
			opened_.erase(_service);
	}
}

template < int MaxBufferSize >
void participant_impl< MaxBufferSize >::enable_magic_cookies() {
	has_enabled_magic_cookies_ = (true && is_supporting_magic_cookies_);
}

template < int MaxBufferSize >
void participant_impl< MaxBufferSize >::disable_magic_cookies() {
	has_enabled_magic_cookies_ = (false && is_supporting_magic_cookies_);
}

template < int MaxBufferSize >
uint32_t participant_impl< MaxBufferSize >::get_message_size() const {
	if (message_.size() < VSOMEIP_STATIC_HEADER_SIZE)
			return 0;

	return VSOMEIP_STATIC_HEADER_SIZE +
		   VSOMEIP_BYTES_TO_LONG(
				message_[4], message_[5], message_[6], message_[7]);
}

template < int MaxBufferSize >
void participant_impl< MaxBufferSize >::receive_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {

	static uint32_t message_counter = 0;

	if (!_error && 0 < _bytes) {
		#ifdef USE_VSOMEIP_STATISTICS
		statistics_.received_bytes_ += _bytes;
		#endif

		const uint8_t *buffer = get_buffer();
		message_.insert(message_.end(), buffer, buffer + _bytes);

		bool has_full_message;
		do {
			uint32_t current_message_size = get_message_size();
			has_full_message = (current_message_size > 0 && current_message_size <= message_.size());
			if (has_full_message) {
				endpoint *sender = factory::get_instance()->get_endpoint(
					get_remote_address(), get_remote_port(), get_protocol()
				);

				owner_->receive(&message_[0], current_message_size, sender, location_);
				message_.erase(message_.begin(), message_.begin() + current_message_size);
			}
		} while (has_full_message);

		restart();
	} else {
		receive();
	}
}

template < int MaxBufferSize >
bool participant_impl< MaxBufferSize >::is_magic_cookie() const {
	return false;
}

template < int MaxBufferSize >
bool participant_impl< MaxBufferSize >::resync_on_magic_cookie() {
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
								VSOMEIP_STATIC_HEADER_SIZE +
								VSOMEIP_MAGIC_COOKIE_SIZE);
		} else {
			message_.clear();
		}

	} else {
		message_.clear();
	}

	return is_resynced;
}

// Instantiate template
template class participant_impl< VSOMEIP_MAX_TCP_MESSAGE_SIZE >;
template class participant_impl< VSOMEIP_MAX_UDP_MESSAGE_SIZE >;

} // namespace vsomeip



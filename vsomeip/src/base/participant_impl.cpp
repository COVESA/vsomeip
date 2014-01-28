//
// participant_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <algorithm>
#include <iomanip>
#include <iostream>

#include <boost/shared_ptr.hpp>

#include <vsomeip/factory.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/receiver.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/internal/participant_impl.hpp>

namespace vsomeip {

participant_impl::participant_impl(uint32_t _max_message_size)
	: max_message_size_(_max_message_size),
	  has_magic_cookies_(false),
	  has_enabled_magic_cookies_(false)
{
	serializer_ = factory::get_default_factory()->create_serializer();
	deserializer_ = factory::get_default_factory()->create_deserializer();
	if (serializer_)
		serializer_->create_data(max_message_size_);
}

participant_impl::~participant_impl() {
	delete serializer_;
	delete deserializer_;
}

std::size_t participant_impl::poll_one() {
	return is_.poll_one();
}

std::size_t participant_impl::poll() {
	return is_.poll();
}

std::size_t participant_impl::run() {
	return is_.run();
}

void participant_impl::register_for(
		receiver *_receiver,
		service_id _service_id, method_id _method_id) {

	std::map< service_id, std::set< receiver * > >& service_registry
		= receiver_registry_[_service_id];
	service_registry[_method_id].insert(_receiver);
}

void participant_impl::unregister_for(
			receiver * _receiver,
			service_id _service_id, method_id _method_id) {

	auto i = receiver_registry_.find(_service_id);
	if (i != receiver_registry_.end()) {
		auto j = i->second.find(_method_id);
		if (j != i->second.end()) {
			j->second.erase(_receiver);
		}
	}
}

void participant_impl::enable_magic_cookies() {
	has_enabled_magic_cookies_ = true;
}

void participant_impl::disable_magic_cookies() {
	has_enabled_magic_cookies_ = false;
}

void participant_impl::receive(const message_base *_message) const {
	service_id requested_service = _message->get_service_id();
	auto i = receiver_registry_.find(requested_service);
	if (i != receiver_registry_.end()) {
		method_id requested_method = _message->get_message_id();
		auto j = i->second.find(requested_method);
		if (j != i->second.end()) {
			std::for_each(j->second.begin(), j->second.end(),
					[_message](receiver* const& _receiver) {
						_receiver->receive(_message); });
		}
	}
}

void participant_impl::received(
		boost::system::error_code const &_error_code,
		std::size_t _transferred_bytes) {

	if (!_error_code && 0 < _transferred_bytes) {
		#ifdef USE_VSOMEIP_STATISTICS
		statistics_.received_bytes_ += _transferred_bytes;
		#endif
		deserializer_->append_data(get_received(), _transferred_bytes);

		bool has_deserialized;
		do {
			has_deserialized = false;

			uint32_t message_length = 0;
			deserializer_->look_ahead(VSOMEIP_LENGTH_POSITION, message_length);

			if (message_length + VSOMEIP_STATIC_HEADER_LENGTH
					<= deserializer_->get_available()) {
				deserializer_->set_remaining(message_length
											 + VSOMEIP_STATIC_HEADER_LENGTH);
				boost::shared_ptr<message_base>
					received_message(deserializer_->deserialize_message());
				if (0 != received_message) {
					if (is_magic_cookie(received_message.get())) {
						// TODO: log message "Magic Cookie dropped. Already synced."
					} else {
						endpoint *sender
							= factory::get_default_factory()->create_endpoint(
									get_remote_address(), get_remote_port(),
									get_protocol(), get_version());

						received_message->set_endpoint(sender);
						receive(received_message.get());

						#ifdef USE_VSOMEIP_STATISTICS
						statistics_.received_messages_++;
						#endif
					}
					has_deserialized = true;
				}
				deserializer_->reset();
			} else {
				has_deserialized = resync_on_magic_cookie();
			}

		} while (has_deserialized);

		restart();
	} else {
		std::cerr << _error_code.message() << std::endl;
	}
}

bool participant_impl::is_magic_cookie(message_base *_message) const {
	return (_message ?
			 is_magic_cookie(_message->get_message_id(),
			  				 _message->get_length(),
							 _message->get_request_id(),
							 _message->get_protocol_version(),
							 _message->get_interface_version(),
							 _message->get_message_type(),
							 _message->get_return_code()) :
			 false);
}

bool participant_impl::is_magic_cookie(
		message_id _message_id,
		length _length,
		request_id _request_id,
		protocol_version _protocol_version,
		interface_version _interface_version,
		message_type _message_type,
		return_code _return_code) const {
	return false; // as we do not know about magic cookies
}

bool participant_impl::resync_on_magic_cookie() {
	bool is_resynced = false;
	if (has_magic_cookies_) {
		bool is_successful;
		uint32_t cookie_message_id, cookie_length, cookie_request_id;
		uint8_t cookie_protocol_version, cookie_interface_version,
				cookie_message_type, cookie_return_code;
		uint32_t offset = 0xFFFFFFFF;
		do {
			 offset++;
			 is_successful
			 	 = deserializer_->look_ahead(offset, cookie_message_id) &&
				   deserializer_->look_ahead(offset+4, cookie_length) &&
				   deserializer_->look_ahead(offset+8, cookie_request_id) &&
			 	   deserializer_->look_ahead(offset+12, cookie_protocol_version) &&
			 	   deserializer_->look_ahead(offset+13, cookie_interface_version) &&
			 	   deserializer_->look_ahead(offset+14, cookie_message_type) &&
			 	   deserializer_->look_ahead(offset+15, cookie_return_code);

			 is_resynced = is_magic_cookie(
					  cookie_message_id,
					  cookie_length,
					  cookie_request_id,
					  cookie_protocol_version,
					  cookie_interface_version,
					  static_cast<message_type>(cookie_message_type),
					  static_cast<return_code>(cookie_return_code));

		} while (is_successful && !is_resynced);

		if (is_successful) {
			deserializer_->drop_data(offset +
									 VSOMEIP_STATIC_HEADER_LENGTH +
									 VSOMEIP_MAGIC_COOKIE_LENGTH);
			is_resynced = true;
		} else {
			deserializer_->set_data(NULL, 0);
		}

	} else {
		deserializer_->set_data(NULL, 0);
	}

	return is_resynced;
}

} // namespace vsomeip



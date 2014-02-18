//
// client_base_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <chrono>

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/internal/client_base_impl.hpp>

namespace vsomeip {

client_base_impl::client_base_impl(
		factory *_factory,
		uint32_t _max_message_size,
		boost::asio::io_service& _is)
	: participant_impl(_factory, _max_message_size, _is), flush_timer_(is_) {
}

client_base_impl::~client_base_impl() {
}

bool client_base_impl::send(const message_base *_message,  bool _flush) {
#ifdef VSOMEIP_USE_STATISTICS
	statistics_.sent_messages_++;
#endif
	uint32_t message_size = VSOMEIP_STATIC_HEADER_LENGTH
							+ _message->get_length();

	if (message_size > max_message_size_) {
		std::cerr << "Message too large" << std::endl;
		return false;
	}

	serializer_->reset();
	bool is_successful = serializer_->serialize(_message);
	if (!is_successful) {
		std::cerr << "Message too long or deserialization failed" << std::endl;
		return false;
	}

	return send(serializer_->get_data(), serializer_->get_size(), _flush);
}

bool client_base_impl::send(const uint8_t *_data, const uint32_t _size, bool _flush) {
	bool is_queue_empty(packet_queue_.empty());

	if (packetizer_.size() + _size > max_message_size_) {
		// TODO: log "implicit flush because new message cannot be buffered"
		packet_queue_.push_back(packetizer_);
		packetizer_.clear();
		if (is_queue_empty) // looks weird, but be just inserted an element....
			send_queued();
	}

	packetizer_.insert(packetizer_.end(), _data, _data + _size);

	if (_flush) {
		flush_timer_.cancel();

		packet_queue_.push_back(packetizer_);
		packetizer_.clear();
		if (is_queue_empty)
			send_queued();
	} else {
		flush_timer_.expires_from_now(
				std::chrono::milliseconds(VSOMEIP_FLUSH_TIMEOUT));
		flush_timer_.async_wait(boost::bind(&client_base_impl::flush, this,
											boost::asio::placeholders::error));
	}

	return true;
}

void client_base_impl::flush(const boost::system::error_code &_error_code) {
	if (!_error_code) {
		(void)flush();
	}
}

bool client_base_impl::flush() {
	bool is_successful = true;

	if (!packetizer_.empty()) {
		packet_queue_.push_back(packetizer_);
		packetizer_.clear();

		send_queued();
	} else {
		is_successful = false;
	}

	return is_successful;
}

//
// Private
//
void client_base_impl::connected(
		boost::system::error_code const &_error_code) {
	if (_error_code) {
		connect();
	}
}

void client_base_impl::sent(
		boost::system::error_code const &_error_code,
		std::size_t _sent_bytes) {

	if (!_error_code && _sent_bytes > 0) {
		packet_queue_.pop_front();

#ifdef USE_VSOMEIP_STATISTICS
		statistics_.sent_messages_++;
		statistics_.sent_bytes_ += _sent_bytes;
#endif
		if (!packet_queue_.empty()) {
			send_queued();
		}
	} else {
		receive();
	}
}

} // namespace vsomeip

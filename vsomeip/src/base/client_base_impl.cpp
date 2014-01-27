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

#include <vsomeip/constants.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/impl/client_base_impl.hpp>

namespace vsomeip {

client_base_impl::client_base_impl(uint32_t _max_message_size,
										bool _is_supporting_resync)
	: participant_impl(_max_message_size, _is_supporting_resync){
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

	std::cout << "Serialized message has " << serializer_->get_size() << " bytes." << std::endl;

	if (!is_successful) {
		std::cerr << "Message too long or deserialization failed" << std::endl;
		return false;
	}

	bool is_queue_empty(packet_queue_.empty());

	if (packetizer_.size() + message_size > max_message_size_) {
		// TODO: log "implicit flush because new message cannot be buffered"
		packet_queue_.push_back(packetizer_);
		packetizer_.clear();
		if (is_queue_empty) // looks weird, but be just inserted an element....
			send_queued();
	}

	packetizer_.insert(packetizer_.end(),
			serializer_->get_data(),
			serializer_->get_data() + serializer_->get_size());

	if (_flush) {
		packet_queue_.push_back(packetizer_);
		packetizer_.clear();
		if (is_queue_empty)
			send_queued();
	}

	return true;
}


//
// Private
//
void client_base_impl::connected(
		boost::system::error_code const &_error_code) {
}

void client_base_impl::sent(
		boost::system::error_code const &_error_code,
		std::size_t _sent_bytes) {

	if (!_error_code && _sent_bytes > 0) {
		packet_queue_.pop_front();

		std::cout << "Sent: " << _sent_bytes << " bytes." << std::endl;

#ifdef USE_VSOMEIP_STATISTICS
		statistics_.sent_messages_++;
		statistics_.sent_bytes_ += _sent_bytes;
#endif
		if (!packet_queue_.empty()) {
			send_queued();
		}
	} else {
		std::cout << _error_code.message() << std::endl;
	}
}

} // namespace vsomeip

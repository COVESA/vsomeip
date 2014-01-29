//
// service_base_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/thread/lock_guard.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/message_base.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/internal/service_base_impl.hpp>

namespace vsomeip {

service_base_impl::service_base_impl(uint32_t _max_message_size)
	: participant_impl(_max_message_size),
	  current_queue_(packet_queues_.end()) {
}

service_base_impl::~service_base_impl() {
}

bool service_base_impl::send(const message_base *_message,  bool _flush) {
	uint32_t message_size = VSOMEIP_STATIC_HEADER_LENGTH
							+ _message->get_length();

	if (message_size > max_message_size_) {
		// TODO: log error "message too large"
		return false;
	}

	serializer_->reset();
	bool is_successful = serializer_->serialize(_message);

	if (!is_successful) {
		// TODO: log "message to long or deserialization failed"
		return false;
	}

	return send(serializer_->get_data(), serializer_->get_size(),
				 _message->get_endpoint(), _flush);
}

bool service_base_impl::send(const uint8_t *_data, uint32_t _size,
								 endpoint *_target, bool _flush) {

	if (0 == _target)
		return false;

	bool is_queue_empty(packet_queues_.empty());

	// find queue and packetizer (buffer)
	std::deque< std::vector< uint8_t > >& target_packet_queue
		= packet_queues_[_target];
	std::vector< uint8_t >& target_packetizer
		= packetizer_[_target];

	// if the current_queue is not yet set, set it to newly created one
	if (current_queue_ == packet_queues_.end())
		current_queue_ = packet_queues_.find(_target);

	if (target_packetizer.size() + _size > max_message_size_) {
		// TODO: log "implicit flush because new message cannot be buffered"
		target_packet_queue.push_back(target_packetizer);
		target_packetizer.clear();

		if (is_queue_empty)
			send_queued();
	}

	target_packetizer.insert(target_packetizer.end(), _data, _data + _size);

	if (_flush) {
		target_packet_queue.push_back(target_packetizer);
		target_packetizer.clear();

		if (is_queue_empty)
			send_queued();
	}

	return true;
}

//
// Private
//
void service_base_impl::connected(
		boost::system::error_code const &_error_code) {

}

void service_base_impl::sent(
		boost::system::error_code const &_error_code,
		std::size_t _sent_bytes) {

	current_queue_->second.pop_front();
	bool is_message_available(set_next_queue());

	if (!_error_code) {
#ifdef USE_VSOMEIP_STATISTICS
		statistics_.sent_messages_++;
		statistics_.sent_bytes_ += _sent_bytes;
#endif
		if (is_message_available) {
			send_queued();
		}
	} else {
		std::cout << _error_code.message() << std::endl;
	}
}

bool service_base_impl::set_next_queue() {
	if (current_queue_->second.empty())
		current_queue_ = packet_queues_.erase(current_queue_);

	if (packet_queues_.empty())
		return false;

	if (current_queue_ == packet_queues_.end())
		current_queue_ = packet_queues_.begin();

	if (!current_queue_->second.empty())
		return true;

	auto saved_current_queue = current_queue_;
	do {
		current_queue_++;
		if (current_queue_ == packet_queues_.end())
			current_queue_ = packet_queues_.begin();
	} while (current_queue_->second.empty() &&
			  current_queue_ != saved_current_queue);

	return !current_queue_->second.empty();
}

} // namespace vsomeip

//
// service_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip_internal/managing_application.hpp>
#include <vsomeip_internal/service_impl.hpp>

namespace vsomeip {

template < typename Protocol, int MaxBufferSize >
service_impl< Protocol, MaxBufferSize >::service_impl(managing_application *_owner, const endpoint *_location)
	: participant_impl<MaxBufferSize>(_owner, _location),
	  current_queue_(packet_queues_.end()),
	  flush_timer_(_owner->get_io_service()) {
}

template < typename Protocol, int MaxBufferSize >
bool service_impl<Protocol, MaxBufferSize>::is_client() const {
	return false;
}

template < typename Protocol, int MaxBufferSize >
bool service_impl< Protocol, MaxBufferSize >::send(
		const uint8_t *_data, uint32_t _size, const endpoint *_target, bool _flush) {

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

	if (target_packetizer.size() + _size > MaxBufferSize) {
		// TODO: log "implicit flush because new message cannot be buffered"
		target_packet_queue.push_back(target_packetizer);
		target_packetizer.clear();

		if (is_queue_empty)
			send_queued();
	}

	target_packetizer.insert(target_packetizer.end(), _data, _data + _size);

	if (_flush) {
		flush_timer_.cancel();

		target_packet_queue.push_back(target_packetizer);
		target_packetizer.clear();

		if (is_queue_empty)
			send_queued();
	} else {
		flush_timer_.expires_from_now(
				std::chrono::milliseconds(VSOMEIP_FLUSH_TIMEOUT));
		flush_timer_.async_wait(
			boost::bind(
				&service_impl<Protocol, MaxBufferSize>::flush_cbk,
				this,
				_target,
				boost::asio::placeholders::error
			)
		);
	}

	return true;
}

template < typename Protocol, int MaxBufferSize >
bool service_impl< Protocol, MaxBufferSize >::flush(const endpoint *_target) {
	bool is_successful = false;
	if (_target) {
		auto i = packetizer_.find(_target);
		if (i != packetizer_.end() && !i->second.empty()) {
			std::deque< std::vector< uint8_t > >& target_packet_queue
					= packet_queues_[i->first];
			target_packet_queue.push_back(i->second);
			i->second.clear();

			is_successful = true;
		}
	} else {
		for (auto i = packetizer_.begin(); i != packetizer_.end(); ++i) {
			if (!i->second.empty()) {
				std::deque< std::vector< uint8_t > >& target_packet_queue
							= packet_queues_[i->first];
				target_packet_queue.push_back(i->second);
				i->second.clear();

				is_successful = true;
			}
		}
	}

	if (is_successful)
		send_queued();

	return is_successful;
}

template < typename Protocol, int MaxBufferSize >
bool service_impl< Protocol, MaxBufferSize >::set_next_queue() {
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

template < typename Protocol, int MaxBufferSize >
void service_impl< Protocol, MaxBufferSize >::connect_cbk(
		boost::system::error_code const &_error) {
}

template < typename Protocol, int MaxBufferSize >
void service_impl< Protocol, MaxBufferSize >::send_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {

	current_queue_->second.pop_front();
	bool is_message_available(set_next_queue());

    if (!_error) {
    	if (is_message_available) {
    		send_queued();
    	}
    }
}

template < typename Protocol, int MaxBufferSize >
void service_impl< Protocol, MaxBufferSize >::flush_cbk(
		const endpoint *_target, const boost::system::error_code &_error_code) {
	if (!_error_code) {
		(void)flush(_target);
	}
}

// Instatiate template
template class service_impl<boost::asio::ip::tcp, VSOMEIP_MAX_TCP_MESSAGE_SIZE>;
template class service_impl<boost::asio::ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE>;

} // namespace vsomeip


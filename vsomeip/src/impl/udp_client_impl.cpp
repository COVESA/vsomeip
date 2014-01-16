//
// udp_client_impl.cpp
//
// Date: 	Jan 14, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <boost/bind.hpp>

#include <vsomeip/factory.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/impl/udp_client_impl.hpp>

#include <iostream>
#include <iomanip>

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_client_impl::udp_client_impl(const endpoint &_endpoint) :
		io_(), socket_(io_),
		endpoint_(ip::address::from_string(_endpoint.get_address()), _endpoint.get_port()),
		version_(_endpoint.get_version() == ip_version::V6 ? ip::udp::v6() : ip::udp::v4()) {

	serializer_ = factory::get_default_factory()->create_serializer();
	serializer_->create_data(VSOMEIP_MAX_UDP_MESSAGE_SIZE);

	deserializer_ = factory::get_default_factory()->create_deserializer();
}

udp_client_impl::~udp_client_impl() {
	close();
}

void udp_client_impl::open() {
	socket_.open(version_);
}

void udp_client_impl::close() {
	if (socket_.is_open())
		socket_.close();
}

void udp_client_impl::connect() {
	socket_.async_connect(endpoint_,
			boost::bind(&udp_client_impl::connect_callback,
						this,
						boost::asio::placeholders::error));
}

void udp_client_impl::disconnect() {

}

void udp_client_impl::send(const message &_message, bool _flush) {
	uint32_t message_size = VSOMEIP_MESSAGE_HEADER_LENGTH + _message.get_length();

	if (message_size > VSOMEIP_MAX_UDP_MESSAGE_SIZE) {
		// TODO: log error "message too large"
		return;
	}

	serializer_->reset();
	bool is_successful = serializer_->serialize(_message);

	if (!is_successful) {
		// TODO: log "message to long or deserialization failed"
		return;
	}

	bool is_queue_empty(queue_.empty());

	if (current_send_buffer_.size() + message_size > VSOMEIP_MAX_UDP_MESSAGE_SIZE) {
		// TODO: log "implicit flush because new message cannot be buffered"
		queue_.push_back(current_send_buffer_);
		current_send_buffer_.clear();
		if (is_queue_empty) send();
	}

	current_send_buffer_.insert(current_send_buffer_.end(),
			serializer_->get_data(),
			serializer_->get_data() + serializer_->get_size());

	if (_flush) {
		queue_.push_back(current_send_buffer_);
		current_send_buffer_.clear();
		if (is_queue_empty) send();
	}
}


void udp_client_impl::register_receiver(receiver *_receiver) {
	receiver_.insert(_receiver);
}

void udp_client_impl::unregister_receiver(receiver *_receiver) {
	receiver_.erase(_receiver);
}

size_t udp_client_impl::poll_one() {
	return io_.poll_one();
}

size_t udp_client_impl::poll() {
	return io_.poll();
}

size_t udp_client_impl::run() {
	return io_.run();
}

void udp_client_impl::send() {
	std::cout << "Current number of queued message: " << queue_.size() << std::endl;
	std::cout << "Sending " << (int)queue_.front().size() << " bytes." << std::endl;
	socket_.async_send(
		boost::asio::buffer(&queue_.front()[0], queue_.front().size()),
		boost::bind(&udp_client_impl::send_callback,
					this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void udp_client_impl::connect_callback(const boost::system::error_code &error) {
	if (!error)
		std::cout << "udp_client_impl: connect operation succeeded!" << std::endl;
}

void udp_client_impl::send_callback(boost::system::error_code const &_error, std::size_t _transferred_bytes) {
	std::cout << "udp_client_impl::send_callback: transferred " << std::dec << (int)_transferred_bytes << " bytes." << std::endl;
	queue_.pop_front();
	if (_error) {
		// TODO: whatever????
	} else if (!queue_.empty()) {
		send();
	}
}

void udp_client_impl::receive_callback(boost::system::error_code const &_error, std::size_t _transferred_bytes) {

}


} // namespace vsomeip


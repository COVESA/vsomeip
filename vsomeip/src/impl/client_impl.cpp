//
// client_impl.cpp
//
// Date: 	Jan 14, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <boost/asio.hpp>

#include <vsomeip/factory.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/service.hpp>
#include <vsomeip/impl/client_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

client_impl::client_impl() :
		io_() {
	serializer_ = factory::get_default_factory()->create_serializer();
}

client_impl::~client_impl() {

}

void client_impl::send(const service &_service, const message &_message,
bool _flush) {
	if (_service.get_protocol() == ip_protocol::TCP) {
		send_tcp(_service, _message, _flush);
	} else if (_service.get_protocol() == ip_protocol::UDP) {
		send_udp(_service, _message, _flush);
	} else {
		// TODO: log "cannot send because of unknown protocol"
	}
}

void client_impl::register_receiver(receiver *_receiver) {
	receiver_.insert(_receiver);
}

void client_impl::unregister_receiver(receiver *_receiver) {
	receiver_.erase(_receiver);
}

void client_impl::send_tcp(const service &_service, const message &_message,
		bool _flush) {
}

void client_impl::send_udp(const service &_service, const message &_message,
		bool _flush) {
	ip::udp::endpoint ep(ip::address::from_string(_service.get_address()),
			_service.get_port());
	ip::udp::socket sckt(io_);

	std::vector<uint8_t> &buffer = udp_buffers_[ep];

	if (VSOMEIP_MESSAGE_HEADER_LENGTH
			+ _message.get_length() > VSOMEIP_MAX_UDP_MESSAGE_SIZE) {
		sckt.send_to(boost::asio::buffer(buffer), ep);
		buffer.clear();
	}

	serializer_->serialize(_message);
	buffer.insert(buffer.end(),
			serializer_->get_data(),
			serializer_->get_data() + serializer_->get_size());

	if (_flush) {
		sckt.send_to(boost::asio::buffer(buffer), ep);
		buffer.clear();
	}
}

} // namespace vsomeip


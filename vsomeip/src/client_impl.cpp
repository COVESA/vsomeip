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

#include <vsomeip/message.hpp>
#include <vsomeip/service.hpp>
#include <vsomeip/impl/client_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

client_impl::client_impl()
	: io_() {

}

client_impl::~client_impl() {

}

void client_impl::send(const service &_service, const message &_message) {
	if (_service.get_protocol() == ip_protocol::TCP) {
		send_tcp(_service, _message);
	} else if (_service.get_protocol() == ip_protocol::UDP) {
		send_udp(_service, _message);
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

void client_impl::send_tcp(const service &_service, const message &_message) {
}

void client_impl::send_udp(const service &_service, const message &_message) {
}


} // namespace vsomeip




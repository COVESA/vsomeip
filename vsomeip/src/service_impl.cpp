//
// service_impl.cpp
//
// Date: 	Jan 14, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/impl/service_impl.hpp>

namespace vsomeip {

service_impl::service_impl()
	: address_(""), port_(0), protocol_(ip_protocol::UNKNOWN) {

}

service_impl::service_impl(const service_impl &_impl)
	: address_(_impl.address_), port_(_impl.port_), protocol_(_impl.protocol_) {
}

service_impl::~service_impl() {
}

std::string service_impl::get_address() const {
	return address_;
}

void service_impl::set_address(const std::string &_address) {
	address_ = _address;
}

uint16_t service_impl::get_port() const {
	return port_;
}

void service_impl::set_port(uint16_t _port) {
	port_ = _port;
}

ip_protocol service_impl::get_protocol() const {
	return protocol_;
}

void service_impl::set_protocol(ip_protocol _protocol) {
	protocol_ = _protocol;
}

} // namespace vsomeip


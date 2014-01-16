//
// endpoint_impl.cpp
//
// Date: 	Jan 14, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/impl/endpoint_impl.hpp>

namespace vsomeip {

endpoint_impl::endpoint_impl()
	: address_(""), port_(0), protocol_(ip_protocol::UNKNOWN), version_(ip_version::UNKNOWN) {

}

endpoint_impl::endpoint_impl(const endpoint_impl &_impl)
	: address_(_impl.address_), port_(_impl.port_), protocol_(_impl.protocol_), version_(_impl.version_) {
}

endpoint_impl::~endpoint_impl() {
}

std::string endpoint_impl::get_address() const {
	return address_;
}

void endpoint_impl::set_address(const std::string &_address) {
	address_ = _address;
}

uint16_t endpoint_impl::get_port() const {
	return port_;
}

void endpoint_impl::set_port(uint16_t _port) {
	port_ = _port;
}

ip_protocol endpoint_impl::get_protocol() const {
	return protocol_;
}

void endpoint_impl::set_protocol(ip_protocol _protocol) {
	protocol_ = _protocol;
}

ip_version endpoint_impl::get_version() const {
	return version_;
}

void endpoint_impl::set_version(ip_version _version) {
	version_ = _version;
}

} // namespace vsomeip


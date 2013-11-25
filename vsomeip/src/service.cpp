//
// service.cpp
//
// Date: 	Nov 21, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/service.h>

#define DEFAULT_PROTOCOL 0

namespace vsomeip {

Service::Service() {
	m_domain = -1;
	m_type = -1;
	m_protocol = -1;

	m_port = -1;

	m_socket = -1;
}

Service::Service(int a_domain, int a_type, int a_protocol, int a_port) {
	m_domain = a_domain;
	m_type = a_type;
	m_protocol = a_protocol;

	m_port = a_port;

	m_socket = -1;
}

Service::~Service() {
}

void Service::init() {
}

void Service::start() {

}

int Service::getDomain() const {
	return m_domain;
}

void Service::setDomain(int a_domain) {
	m_domain = a_domain;
}

int Service::getType() const {
	return m_type;
}

void Service::setType(int a_type) {
	m_type = a_type;
}

int Service::getProtocol() const {
	return m_protocol;
}

void Service::setProtocol(int a_protocol) {
	m_protocol = a_protocol;
}

} // namespace vsomeip


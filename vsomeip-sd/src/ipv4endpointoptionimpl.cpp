//
// ipv4endpointoption.cpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/serializer.h>
#include <vsomeip/deserializer.h>

#include <vsomeip/sd/ipv4endpointoptionimpl.h>

namespace vsomeip {

namespace sd {

IPv4EndpointOptionImpl::IPv4EndpointOptionImpl() {
	m_length = (1 + 4 + 1 + 1 + 2);
	m_type = OptionType::IP4_ENDPOINT;
	m_address = 0;
	m_port = 0;
	m_protocol = IPProtocol::UNKNOWN;
}

IPv4EndpointOptionImpl::~IPv4EndpointOptionImpl() {
}

bool IPv4EndpointOptionImpl::operator ==(const Option& a_option) const {
	if (a_option.getType() != OptionType::IP4_ENDPOINT)
		return false;

	const IPv4EndpointOptionImpl& l_ipEndpointOption
		= reinterpret_cast<const IPv4EndpointOptionImpl&>(a_option);

	return (m_address == l_ipEndpointOption.m_address
		 && m_port == l_ipEndpointOption.m_port
		 && m_protocol == l_ipEndpointOption.m_protocol);
}


IPv4Address IPv4EndpointOptionImpl::getAddress() const {
	return m_address;
}

void IPv4EndpointOptionImpl::setAddress(IPv4Address a_address) {
	m_address = a_address;
}

IPPort IPv4EndpointOptionImpl::getPort() const {
	return m_port;
}

void IPv4EndpointOptionImpl::setPort(IPPort a_port) {
	m_port = a_port;
}

IPProtocol IPv4EndpointOptionImpl::getProtocol() const {
	return m_protocol;
}

void IPv4EndpointOptionImpl::setProtocol(IPProtocol a_protocol) {
	m_protocol = a_protocol;
}

bool IPv4EndpointOptionImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful = OptionImpl::serialize(a_serializer);

	l_isSuccessful = l_isSuccessful && a_serializer->serialize32(m_address);
	l_isSuccessful = l_isSuccessful && a_serializer->serialize8(VSOMEIP_PROTOCOL_RESERVED);
	l_isSuccessful = l_isSuccessful && a_serializer->serialize8(static_cast<uint16_t>(m_protocol));
	l_isSuccessful = l_isSuccessful && a_serializer->serialize16(m_port);

	return l_isSuccessful;
}

bool IPv4EndpointOptionImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = OptionImpl::deserialize(a_deserializer);

	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize32(m_address);

	uint8_t l_reserved;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize8(l_reserved);

	uint16_t l_protocol;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize16(l_protocol);
	m_protocol = static_cast<IPProtocol>(l_protocol);

	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize16(m_port);

	return l_isSuccessful;
}

} // namespace sd

} // namespace vsomeip




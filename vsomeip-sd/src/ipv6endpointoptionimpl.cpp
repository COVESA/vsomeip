//
// ipv6endpointoption.cpp
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

#include <vsomeip/sd/ipv6endpointoptionimpl.h>

namespace vsomeip {

namespace sd {

IPv6EndpointOptionImpl::IPv6EndpointOptionImpl() {
	m_length = (1 + 16 + 1 + 1 + 2);
	m_type = OptionType::IP6_ENDPOINT;
	memset(m_address, 0, sizeof(m_address));
	m_port = 0;
	m_protocol = IPProtocol::UNKNOWN;
}

IPv6EndpointOptionImpl::~IPv6EndpointOptionImpl() {
}

bool IPv6EndpointOptionImpl::operator ==(const Option& a_option) const {
	if (a_option.getType() != OptionType::IP6_ENDPOINT)
		return false;

	const IPv6EndpointOptionImpl& l_ipEndpointOption
		= reinterpret_cast<const IPv6EndpointOptionImpl&>(a_option);

	return (m_address == l_ipEndpointOption.m_address
		 && m_port == l_ipEndpointOption.m_port
		 && m_protocol == l_ipEndpointOption.m_protocol);
}

const IPv6Address& IPv6EndpointOptionImpl::getAddress() const {
	return m_address;
}

void IPv6EndpointOptionImpl::setAddress(const IPv6Address& a_address) {
	memcpy(m_address, a_address, sizeof(m_address));
}

IPPort IPv6EndpointOptionImpl::getPort() const {
	return m_port;
}

void IPv6EndpointOptionImpl::setPort(IPPort a_port) {
	m_port = a_port;
}

IPProtocol IPv6EndpointOptionImpl::getProtocol() const {
	return m_protocol;
}

void IPv6EndpointOptionImpl::setProtocol(IPProtocol a_protocol) {
	m_protocol = a_protocol;
}

bool IPv6EndpointOptionImpl::serialize(Serializer *a_serializer) const {
	bool l_result;

	l_result = OptionImpl::serialize(a_serializer);
	l_result = l_result && a_serializer->serializeX(m_address, sizeof(m_address));
	l_result = l_result && a_serializer->serialize8(VSOMEIP_PROTOCOL_RESERVED);
	l_result = l_result && a_serializer->serialize8(static_cast<uint16_t>(m_protocol));
	l_result = l_result && a_serializer->serialize16(m_port);

	return l_result;
}

bool IPv6EndpointOptionImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = OptionImpl::deserialize(a_deserializer);

	l_isSuccessful = l_isSuccessful && a_deserializer->deserializeX(m_address, sizeof(m_address));

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




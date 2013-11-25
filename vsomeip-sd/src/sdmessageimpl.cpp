//
// sdmessage.cpp
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

#include <vsomeip/sd/deserializer.h>

#include <vsomeip/sd/eventgroupentryimpl.h>
#include <vsomeip/sd/serviceentryimpl.h>

#include <vsomeip/sd/configurationoptionimpl.h>
#include <vsomeip/sd/ipv4endpointoptionimpl.h>
#include <vsomeip/sd/ipv6endpointoptionimpl.h>
#include <vsomeip/sd/loadbalancingoptionimpl.h>
#include <vsomeip/sd/protectionoptionimpl.h>

#include <vsomeip/sd/sdmessageimpl.h>

namespace vsomeip {

namespace sd {

SdMessageImpl::SdMessageImpl() {
	setServiceId(0xFFFF);
	setMethodId(0x8100);
	setProtocolVersion(0x01);
	m_flags = 0x00;
}

SdMessageImpl::~SdMessageImpl() {
}

Flags SdMessageImpl::getFlags() const {
	return m_flags;
}

void SdMessageImpl::setFlags(Flags a_flags) {
	m_flags = a_flags;
}

EventGroupEntry& SdMessageImpl::createEventGroupEntry() {
	EventGroupEntryImpl* l_entry = new EventGroupEntryImpl;
	//TODO: throw OutOfMemoryException if allocation fails
	l_entry->setMessage(this);
	m_entries.push_back(l_entry);
	return *l_entry;
}

ServiceEntry& SdMessageImpl::createServiceEntry() {
	ServiceEntryImpl* l_entry = new ServiceEntryImpl;
	//TODO: throw OutOfMemoryException if allocation fails
	l_entry->setMessage(this);
	m_entries.push_back(l_entry);
	return *l_entry;
}

ConfigurationOption& SdMessageImpl::createConfigurationOption() {
	ConfigurationOptionImpl* l_option = new ConfigurationOptionImpl;
	//TODO: throw OutOfMemoryException if allocation fails
	l_option->setMessage(this);
	m_options.push_back(l_option);
	return *l_option;
}

IPv4EndpointOption& SdMessageImpl::createIPv4EndPointOption() {
	IPv4EndpointOptionImpl* l_option = new IPv4EndpointOptionImpl;
	//TODO: throw OutOfMemoryException if allocation fails
	l_option->setMessage(this);
	m_options.push_back(l_option);
	return *l_option;
}

IPv6EndpointOption& SdMessageImpl::createIPv6EndPointOption() {
	IPv6EndpointOptionImpl* l_option = new IPv6EndpointOptionImpl;
	//TODO: throw OutOfMemoryException if allocation fails
	l_option->setMessage(this);
	m_options.push_back(l_option);
	return *l_option;
}

LoadBalancingOption& SdMessageImpl::createLoadBalancingOption() {
	LoadBalancingOptionImpl* l_option = new LoadBalancingOptionImpl;
	//TODO: throw OutOfMemoryException if allocation fails
	l_option->setMessage(this);
	m_options.push_back(l_option);
	return *l_option;
}

ProtectionOption& SdMessageImpl::createProtectionOption() {
	ProtectionOptionImpl* l_option = new ProtectionOptionImpl;
	//TODO: throw OutOfMemoryException if allocation fails
	l_option->setMessage(this);
	m_options.push_back(l_option);
	return *l_option;
}

// TODO: throw exception to signal "OptionNotFound"
int16_t SdMessageImpl::getOptionIndex(const Option& a_option) const {
	int16_t i = 0;

	while (i < m_options.size()) {
		if (*(m_options[i]) == a_option)
			return i;
		i++;
	}

	return -1;
}

bool SdMessageImpl::serialize(vsomeip::Serializer *a_serializer) const {
	bool l_result = MessageImpl::serialize(a_serializer);

	l_result = l_result && a_serializer->serialize8(m_flags);
	l_result = l_result && a_serializer->serialize24(VSOMEIP_PROTOCOL_RESERVED);

	uint32_t l_entriesLength = (m_entries.size() * VSOMEIP_ENTRY_LENGTH);
	l_result = l_result && a_serializer->serialize32(l_entriesLength);

	for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
		l_result = l_result && (*it)->serialize(a_serializer);

	uint32_t l_optionsLength = 0;
	for (size_t i = 0; i < m_options.size(); ++i)
		l_optionsLength += (m_options[i]->getLength()
				+ VSOMEIP_OPTION_HEADER_LENGTH);
	l_result = l_result && a_serializer->serialize32(l_optionsLength);

	for (auto it = m_options.begin(); it != m_options.end(); ++it)
		l_result = l_result && (*it)->serialize(a_serializer);

	return l_result;
}

bool SdMessageImpl::deserialize(vsomeip::Deserializer *a_deserializer) {

	Deserializer *l_deserializer =
			dynamic_cast<vsomeip::sd::Deserializer*>(a_deserializer);
	if (NULL == l_deserializer)
		return false;

	bool l_isSuccessful = MessageImpl::deserialize(l_deserializer);
	l_isSuccessful = l_isSuccessful && l_deserializer->deserialize8(m_flags);

	// Read three reserved bytes
	uint32_t l_reserved;
	l_isSuccessful = l_isSuccessful
			&& l_deserializer->deserialize24(l_reserved);

	// deserialize the entries
	uint32_t l_entriesLength = 0;
	l_isSuccessful = l_isSuccessful
			&& l_deserializer->deserialize32(l_entriesLength);

	Deserializer* l_entriesDeserializer =
			dynamic_cast<vsomeip::sd::Deserializer*>(l_deserializer->copy(false));
	l_entriesDeserializer->setContentLength(l_entriesLength);

	while (l_isSuccessful && l_entriesDeserializer->getContentLength()) {
		Entry* l_entry = l_entriesDeserializer->deserializeEntry();
		if (l_entry) {
			m_entries.push_back(l_entry);
		} else {
			l_isSuccessful = false;
		}
	}

	l_deserializer->setContentLength(
			l_deserializer->getContentLength() - l_entriesLength);

	// deserialize the options
	uint32_t l_optionsLength = 0;
	l_isSuccessful = l_isSuccessful
			&& l_deserializer->deserialize32(l_optionsLength);

	while (l_isSuccessful && l_deserializer->getContentLength()) {
		Option *l_option = l_deserializer->deserializeOption();
		if (l_option) {
			m_options.push_back(l_option);
		} else {
			l_isSuccessful = false;
		}
	}

	return l_isSuccessful;
}

} // namespace sd

} // namespace vsomeip


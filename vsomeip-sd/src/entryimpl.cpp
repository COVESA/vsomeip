//
// entryimpl.cpp
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <algorithm>

#include <vsomeip/serializer.h>
#include <vsomeip/deserializer.h>

#include <vsomeip/sd/entryimpl.h>
#include <vsomeip/sd/sdmessageimpl.h>

namespace vsomeip {

namespace sd {

// TODO: throw exception if this constructor is used
EntryImpl::EntryImpl() {
	m_type = EntryType::UNKNOWN;
	m_majorVersion = 0;
	m_serviceId = 0x0;
	m_instanceId = 0x0;
	m_timeToLive = 0x0;
}

EntryImpl::EntryImpl(const EntryImpl& a_entry) :
		SdMessagePartImpl(a_entry) {
	m_type = EntryType::UNKNOWN;
	m_majorVersion = a_entry.m_majorVersion;
	m_serviceId = a_entry.m_serviceId;
	m_instanceId = a_entry.m_instanceId;
	m_timeToLive = a_entry.m_timeToLive;
}

EntryImpl::~EntryImpl() {
}

EntryType EntryImpl::getType() const {
	return m_type;
}

void EntryImpl::setType(EntryType a_type) {
	m_type = a_type;
}

ServiceId EntryImpl::getServiceId() const {
	return m_serviceId;
}

void EntryImpl::setServiceId(ServiceId a_serviceId) {
	m_serviceId = a_serviceId;
}

InstanceId EntryImpl::getInstanceId() const {
	return m_instanceId;
}

void EntryImpl::setInstanceId(InstanceId a_instanceId) {
	m_instanceId = a_instanceId;
}

MajorVersion EntryImpl::getMajorVersion() const {
	return m_majorVersion;
}

void EntryImpl::setMajorVersion(MajorVersion a_majorVersion) {
	m_majorVersion = a_majorVersion;
}

TimeToLive EntryImpl::getTimeToLive() const {
	return m_timeToLive;
}

void EntryImpl::setTimeToLive(TimeToLive a_timeToLive) {
	m_timeToLive = a_timeToLive;
}

void EntryImpl::assignOption(const Option& a_option, uint8_t a_run) {
	if (a_run > 0 && a_run <= VSOMEIP_MAX_OPTION_RUN) {
		a_run--; // Index = Run-1

		uint8_t l_optionIndex = getMessage()->getOptionIndex(a_option);
		if (0x10 > l_optionIndex) { // as we have only a nibble for the option counter
			m_options[a_run].push_back(l_optionIndex);
			std::sort(m_options[a_run].begin(), m_options[a_run].end());
		} else {
			// TODO: decide what to do if option does not belong to the message.
		}
	} else {
		// TODO: decide what to do if an illegal index for the option run is provided
	}
}

bool EntryImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful = a_serializer->serialize8(
			static_cast<uint8_t>(m_type));

	uint8_t l_indexFirstOptionRun = 0;
	if (m_options[0].size() > 0)
		l_indexFirstOptionRun = m_options[0][0];
	l_isSuccessful = l_isSuccessful
			&& a_serializer->serialize8(l_indexFirstOptionRun);

	uint8_t l_indexSecondOptionRun = 0;
	if (m_options[1].size() > 0)
		l_indexSecondOptionRun = m_options[1][0];
	l_isSuccessful = l_isSuccessful
			&& a_serializer->serialize8(l_indexSecondOptionRun);

	uint8_t l_numberOfOptions = ((((uint8_t) m_options[0].size()) << 4)
			| (((uint8_t) m_options[1].size()) & 0x0F));
	l_isSuccessful = l_isSuccessful
			&& a_serializer->serialize8(l_numberOfOptions);

	l_isSuccessful = l_isSuccessful
			&& a_serializer->serialize16(static_cast<uint16_t>(m_serviceId));

	l_isSuccessful = l_isSuccessful
			&& a_serializer->serialize16(static_cast<uint16_t>(m_instanceId));

	return l_isSuccessful;
}

bool EntryImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = true;

	uint8_t l_type;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize8(l_type);
	m_type = static_cast<EntryType>(l_type);

	uint32_t l_options;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize24(l_options);

	uint16_t l_serviceId;
	l_isSuccessful = l_isSuccessful
			&& a_deserializer->deserialize16(l_serviceId);
	m_serviceId = static_cast<ServiceId>(l_serviceId);

	uint16_t l_instanceId;
	l_isSuccessful = l_isSuccessful
			&& a_deserializer->deserialize16(l_instanceId);
	m_instanceId = static_cast<InstanceId>(l_instanceId);

	return l_isSuccessful;
}

bool EntryImpl::isServiceEntry() const {
	return (m_type <= EntryType::REQUEST_SERVICE);
}

bool EntryImpl::isEventGroupEntry() const {
	return (m_type >= EntryType::FIND_EVENT_GROUP
			&& m_type <= EntryType::SUBSCRIBE_EVENTGROUP_ACK);
}

} // namespace sd

} // namespace vsomeip

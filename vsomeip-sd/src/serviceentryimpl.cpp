//
// serviceentryimpl.cpp
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

#include <vsomeip/sd/serviceentryimpl.h>

namespace vsomeip {

namespace sd {

ServiceEntryImpl::ServiceEntryImpl() {
	m_minorVersion =0x00;
}

ServiceEntryImpl::~ServiceEntryImpl() {
}

MinorVersion ServiceEntryImpl::getMinorVersion() const {
	return m_minorVersion;
}

void ServiceEntryImpl::setMinorVersion(MinorVersion a_minorVersion) {
	m_minorVersion = a_minorVersion;
}

bool ServiceEntryImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful = EntryImpl::serialize(a_serializer);

	l_isSuccessful = l_isSuccessful && a_serializer->serialize8(static_cast<uint8_t>(m_majorVersion));
	l_isSuccessful = l_isSuccessful && a_serializer->serialize24(static_cast<uint32_t>(m_timeToLive));
	l_isSuccessful = l_isSuccessful && a_serializer->serialize32(static_cast<uint32_t>(m_minorVersion));

	return l_isSuccessful;
}

bool ServiceEntryImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = EntryImpl::deserialize(a_deserializer);

	uint8_t l_majorVersion;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize8(l_majorVersion);
	m_majorVersion = static_cast<MajorVersion>(l_majorVersion);

	uint32_t l_timeToLive;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize32(l_timeToLive);
	m_timeToLive = static_cast<TimeToLive>(l_timeToLive);

	uint32_t l_minorVersion;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize32(l_minorVersion);
	m_minorVersion = static_cast<MinorVersion>(l_minorVersion);

	return l_isSuccessful;
}

} // namespace sd

} // namespace vsomeip


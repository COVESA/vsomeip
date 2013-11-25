//
// eventgroupentryimpl.cpp
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

#include <vsomeip/sd/eventgroupentryimpl.h>

namespace vsomeip {

namespace sd {

EventGroupEntryImpl::EventGroupEntryImpl() {
	m_eventGroupId = 0xFFFF;
}

EventGroupEntryImpl::EventGroupEntryImpl(const EventGroupEntryImpl& a_entry) :
		EntryImpl(a_entry) {
	m_eventGroupId = a_entry.m_eventGroupId;
}

EventGroupEntryImpl::~EventGroupEntryImpl() {
}

EventGroupId EventGroupEntryImpl::getEventGroupId() const {
	return m_eventGroupId;
}

void EventGroupEntryImpl::setEventGroupId(EventGroupId a_eventGroupId) {
	m_eventGroupId = a_eventGroupId;
}

bool EventGroupEntryImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful = EntryImpl::serialize(a_serializer);

	l_isSuccessful = l_isSuccessful
				&& a_serializer->serialize8(VSOMEIP_PROTOCOL_RESERVED);

	l_isSuccessful = l_isSuccessful
				&& a_serializer->serialize24(static_cast<uint32_t>(m_timeToLive));

	l_isSuccessful = l_isSuccessful
			&& a_serializer->serialize16(VSOMEIP_PROTOCOL_RESERVED);

	l_isSuccessful = l_isSuccessful
			&& a_serializer->serialize16(static_cast<uint16_t>(m_eventGroupId));

	return l_isSuccessful;
}

bool EventGroupEntryImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = EntryImpl::deserialize(a_deserializer);

	uint16_t l_reserved;
	l_isSuccessful = l_isSuccessful
			&& a_deserializer->deserialize16(l_reserved);

	uint16_t l_eventGroupId;
	l_isSuccessful = l_isSuccessful
			&& a_deserializer->deserialize16(l_eventGroupId);
	m_eventGroupId = static_cast<EventGroupId>(l_eventGroupId);

	return l_isSuccessful;
}

} // namespace sd

} // namespace vsomeip


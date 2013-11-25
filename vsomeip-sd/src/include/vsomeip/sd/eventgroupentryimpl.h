//
// eventgroupentryimpl.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_EVENTGROUPENTRY_H__
#define __VSOMEIP_LIBRARY_SD_EVENTGROUPENTRY_H__

#include <vsomeip/sd/eventgroupentry.h>
#include <vsomeip/sd/entryimpl.h>

namespace vsomeip {

namespace sd {

class EventGroupEntryImpl : virtual public EventGroupEntry, virtual public EntryImpl {
public:
	EventGroupEntryImpl();
	EventGroupEntryImpl(const EventGroupEntryImpl& a_entry);
	virtual ~EventGroupEntryImpl();

	EventGroupId getEventGroupId() const;
	void setEventGroupId(EventGroupId a_eventGroupId);

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

private:
	EventGroupId m_eventGroupId;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_LIBRARY_SD_EVENTGROUPENTRY_H__

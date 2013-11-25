//
// entry.h
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_ENTRY_H__
#define __VSOMEIP_SD_ENTRY_H__

#include <vsomeip/types.h>
#include <vsomeip/serializable.h>
#include <vsomeip/sd/types.h>

namespace vsomeip {

namespace sd {

class Option;

enum class EntryType {
	FIND_SERVICE = 0x00,
	OFFER_SERVICE = 0x01,
	STOP_OFFER_SERVICE = 0x01,
	REQUEST_SERVICE = 0x2,
	FIND_EVENT_GROUP = 0x4,
	PUBLISH_EVENTGROUP = 0x5,
	STOP_PUBLISH_EVENTGROUP = 0x5,
	SUBSCRIBE_EVENTGROUP = 0x06,
	STOP_SUBSCRIBE_EVENTGROUP = 0x06,
	SUBSCRIBE_EVENTGROUP_ACK = 0x07,
	STOP_SUBSCRIBE_EVENTGROUP_ACK = 0x07,
	UNKNOWN = 0xFF
};

class Entry : virtual public Serializable {
public:
	virtual ~Entry() {};

	virtual EntryType getType() const = 0;
	virtual void setType(EntryType a_type) = 0;

	virtual ServiceId getServiceId() const = 0;
	virtual void setServiceId(ServiceId a_serviceId) = 0;

	virtual InstanceId getInstanceId() const = 0;
	virtual void setInstanceId(InstanceId a_instanceId) = 0;

	virtual MajorVersion getMajorVersion() const = 0;
	virtual void setMajorVersion(MajorVersion a_majorVersion) = 0;

	virtual TimeToLive getTimeToLive() const = 0;
	virtual void setTimeToLive(TimeToLive a_timeToLive) = 0;

	virtual void assignOption(const Option& a_option, uint8_t a_run) = 0;

	virtual bool isServiceEntry() const = 0;
	virtual bool isEventGroupEntry() const = 0;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_ENTRY_H__

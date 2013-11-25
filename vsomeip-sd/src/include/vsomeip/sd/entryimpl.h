//
// entryimpl.h
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_ENTRY_H__
#define __VSOMEIP_LIBRARY_SD_ENTRY_H__

#include <vector>

#include <vsomeip/serializable.h>
#include <vsomeip/sd/entry.h>

#include "sdmessagepartimpl.h"

namespace vsomeip {

namespace sd {

class Option;
class SdMessageImpl;

class EntryImpl : virtual public Entry, virtual public SdMessagePartImpl {
public:
	virtual ~EntryImpl();

	// public interface
	EntryType getType() const;

	ServiceId getServiceId() const;
	void setServiceId(ServiceId a_serviceId);

	InstanceId getInstanceId() const;
	void setInstanceId(InstanceId a_instanceId);

	MajorVersion getMajorVersion() const;
	void setMajorVersion(MajorVersion a_majorVersion);

	TimeToLive getTimeToLive() const;
	void setTimeToLive(TimeToLive a_timeToLive);

	void assignOption(const Option& a_option, uint8_t a_run);

	bool isServiceEntry() const;
	bool isEventGroupEntry() const;

	void setType(EntryType a_type);

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

protected:
	EntryType m_type;
	ServiceId m_serviceId;
	InstanceId m_instanceId;
	MajorVersion m_majorVersion;
	TimeToLive m_timeToLive;

	std::vector< uint8_t > m_options[VSOMEIP_MAX_OPTION_RUN];

	EntryImpl();
	EntryImpl(const EntryImpl& a_entry);
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_LIBRARY_SD_ENTRY_H__

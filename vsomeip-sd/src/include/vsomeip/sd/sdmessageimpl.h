//
// sdmessageimpl.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_SDMESSAGEIMPL_H__
#define __VSOMEIP_LIBRARY_SD_SDMESSAGEIMPL_H__

#include <vector>

#include <vsomeip/sd/sdmessage.h>

#include <vsomeip/messageimpl.h>

namespace vsomeip {

namespace sd {

class Entry;
class Option;

class SdMessageImpl: virtual public SdMessage, virtual public vsomeip::MessageImpl {
public:
	SdMessageImpl();
	virtual ~SdMessageImpl();

	Flags getFlags() const;
	void setFlags(Flags a_flags);

	EventGroupEntry& createEventGroupEntry();
	ServiceEntry& createServiceEntry();

	ConfigurationOption& createConfigurationOption();
	IPv4EndpointOption& createIPv4EndPointOption();
	IPv6EndpointOption& createIPv6EndPointOption();
	LoadBalancingOption& createLoadBalancingOption();
	ProtectionOption& createProtectionOption();

	int16_t getOptionIndex(const Option& a_option) const;

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

private:
	Flags m_flags;

	std::vector<Entry*> m_entries;
	std::vector<Option*> m_options;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_LIBRARY_SD_SDMESSAGEIMPL_H__

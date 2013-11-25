//
// sdmessage.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_SDMESSAGE_H__
#define __VSOMEIP_SD_SDMESSAGE_H__

#include <vsomeip/message.h>
#include <vsomeip/sd/types.h>

namespace vsomeip {

namespace sd {

class EventGroupEntry;
class ServiceEntry;

class ConfigurationOption;
class IPv4EndpointOption;
class IPv6EndpointOption;
class LoadBalancingOption;
class ProtectionOption;

class SdMessage : virtual public Message {
public:
	virtual ~SdMessage() {};

	virtual Flags getFlags() const = 0;
	virtual void setFlags(Flags a_flags) = 0;

	virtual EventGroupEntry& createEventGroupEntry() = 0;
	virtual ServiceEntry& createServiceEntry() = 0;

	virtual ConfigurationOption& createConfigurationOption() = 0;
	virtual IPv4EndpointOption& createIPv4EndPointOption() = 0;
	virtual IPv6EndpointOption& createIPv6EndPointOption() = 0;
	virtual LoadBalancingOption& createLoadBalancingOption() = 0;
	virtual ProtectionOption& createProtectionOption() = 0;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_SDMESSAGE_H__

//
// ipv6endpointoption.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_IPV6ENDPOINTOPTION_H__
#define __VSOMEIP_SD_IPV6ENDPOINTOPTION_H__

#include <cstring>

#include <vsomeip/sd/option.h>
#include <vsomeip/sd/types.h>

namespace vsomeip {

namespace sd {

class IPv6EndpointOption : virtual public Option {
public:
	virtual ~IPv6EndpointOption() {};

	virtual const IPv6Address& getAddress() const = 0;
	virtual void setAddress(const IPv6Address& a_address) = 0;

	virtual IPPort getPort() const = 0;
	virtual void setPort(IPPort a_port) = 0;

	virtual IPProtocol getProtocol() const = 0;
	virtual void setProtocol(IPProtocol a_protocol) = 0;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_IPV6ENDPOINTOPTION_H__

//
// ip4endpointoption.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_IP4ENDPOINTOPTION_H__
#define __VSOMEIP_SD_IP4ENDPOINTOPTION_H__

#include <vsomeip/sd/option.h>
#include <vsomeip/sd/types.h>

namespace vsomeip {

namespace sd {

class IPv4EndpointOption : virtual public Option {
public:
	virtual ~IPv4EndpointOption() {};

	virtual IPv4Address getAddress() const = 0;
	virtual void setAddress(IPv4Address a_address) = 0;

	virtual IPPort getPort() const = 0;
	virtual void setPort(IPPort a_port) = 0;

	virtual IPProtocol getProtocol() const = 0;
	virtual void setProtocol(IPProtocol a_protocol) = 0;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOME_SD_IP4ENDPOINTOPTION_H__

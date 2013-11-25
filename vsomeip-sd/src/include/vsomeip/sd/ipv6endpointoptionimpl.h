//
// ipv6endpointoptionimpl.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_IPV6ENDPOINTOPTION_H__
#define __VSOMEIP_LIBRARY_SD_IPV6ENDPOINTOPTION_H__

#include <vsomeip/sd/ipv6endpointoption.h>
#include "optionimpl.h"

namespace vsomeip {

namespace sd {

class IPv6EndpointOptionImpl : virtual public IPv6EndpointOption, virtual public OptionImpl {
public:
	IPv6EndpointOptionImpl();
	virtual ~IPv6EndpointOptionImpl();
	bool operator == (const Option& a_option) const;

	const IPv6Address& getAddress() const;
	void setAddress(const IPv6Address& a_address);

	IPPort getPort() const;
	void setPort(IPPort a_port);

	IPProtocol getProtocol() const;
	void setProtocol(IPProtocol a_protocol);

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

private:
	IPv6Address m_address;
	IPPort m_port;
	IPProtocol m_protocol;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_LIBRARY_SD_IPV6ENDPOINTOPTION_H__

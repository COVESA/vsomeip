//
// protection_option.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_PROTECTION_OPTION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_PROTECTION_OPTION_HPP

#include <vsomeip/service_discovery/option.hpp>
#include <vsomeip/service_discovery/primitive_types.hpp>

namespace vsomeip {
namespace service_discovery {

class protection_option
		: virtual public option {
public:
	virtual ~protection_option() {};

	virtual alive_counter get_alive_counter() const = 0;
	virtual void set_alive_counter(alive_counter a) = 0;

	virtual crc get_crc() const = 0;
	virtual void set_crc(crc c) = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_PROTECTION_OPTION_HPP

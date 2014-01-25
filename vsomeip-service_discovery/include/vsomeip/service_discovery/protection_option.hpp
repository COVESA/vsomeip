//
// protection_option.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_PROTECTION_OPTION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_PROTECTION_OPTION_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/service_discovery/option.hpp>

namespace vsomeip {
namespace service_discovery {

class protection_option
		: virtual public option {
public:
	virtual ~protection_option() {};

	virtual alive_counter get_alive_counter() const = 0;
	virtual void set_alive_counter(alive_counter _counter) = 0;

	virtual crc get_crc() const = 0;
	virtual void set_crc(crc _crc) = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_PROTECTION_OPTION_HPP

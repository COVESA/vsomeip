//
// service_entry.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_SERVICE_ENTRY_HPP
#define VSOMEIP_SERVICE_DISCOVERY_SERVICE_ENTRY_HPP

#include <vsomeip/service_discovery/entry.hpp>

namespace vsomeip {
namespace service_discovery {

class service_entry
		: virtual public entry {
public:
	virtual ~service_entry() {};

	virtual minor_version get_minor_version() const = 0;
	virtual void set_minor_version(minor_version version) = 0;
};

} // namespace service_discovery
} // namespace vsomeip


#endif // VSOMEIP_SERVICE_DISCOVERY_SERVICE_ENTRY_HPP

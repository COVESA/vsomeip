//
// eventgroup_entry.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_EVENTGROUP_ENTRY_HPP
#define VSOMEIP_SERVICE_DISCOVERY_EVENTGROUP_ENTRY_HPP

#include <vsomeip/service_discovery/entry.hpp>

namespace vsomeip {
namespace service_discovery {

class eventgroup_entry
		: virtual public entry {
public:
	virtual ~eventgroup_entry() {};

	virtual eventgroup_id get_eventgroup_id() const = 0;
	virtual void set_eventgroup_id(eventgroup_id _id) = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_EVENTGROUP_ENTRY_HPP

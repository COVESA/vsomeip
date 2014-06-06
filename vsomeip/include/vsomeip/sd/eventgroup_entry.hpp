//
// eventgroup_entry.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SD_EVENTGROUP_ENTRY_HPP
#define VSOMEIP_SD_EVENTGROUP_ENTRY_HPP

#include <vsomeip/sd/entry.hpp>

namespace vsomeip {
namespace sd {

class eventgroup_entry
		: virtual public entry {
public:
	virtual ~eventgroup_entry() {};

	virtual eventgroup_id get_eventgroup_id() const = 0;
	virtual void set_eventgroup_id(eventgroup_id _id) = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_EVENTGROUP_ENTRY_HPP

//
// eventgroup_entry_impl.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL_EVENTGROUP_ENTRY_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_IMPL_EVENTGROUP_ENTRY_IMPL_HPP

#include <vsomeip/service_discovery/eventgroup_entry.hpp>
#include <vsomeip/service_discovery/impl/entry_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class eventgroup_entry_impl
		: virtual public eventgroup_entry,
		  virtual public entry_impl {
public:
	eventgroup_entry_impl();
	eventgroup_entry_impl(const eventgroup_entry_impl &entry_);
	virtual ~eventgroup_entry_impl();

	eventgroup_id get_eventgroup_id() const;
	void set_eventgroup_id(eventgroup_id _id);

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

private:
	eventgroup_id eventgroup_id_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL_EVENTGROUP_ENTRY_IMPL_HPP

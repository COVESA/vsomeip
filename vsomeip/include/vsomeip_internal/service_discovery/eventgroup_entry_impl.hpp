//
// eventgroup_entry_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SERVICE_DISCOVERY_EVENTGROUP_ENTRY_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_DISCOVERY_EVENTGROUP_ENTRY_IMPL_HPP

#include <vsomeip/service_discovery/eventgroup_entry.hpp>
#include <vsomeip_internal/service_discovery/entry_impl.hpp>

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

#endif // VSOMEIP_INTERNAL_SERVICE_DISCOVERY_EVENTGROUP_ENTRY_IMPL_HPP

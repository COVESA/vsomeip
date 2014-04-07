//
// eventgroup_entry_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_EVENTGROUP_ENTRY_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_EVENTGROUP_ENTRY_IMPL_HPP

#include <vsomeip/sd/eventgroup_entry.hpp>
#include <vsomeip_internal/sd/entry_impl.hpp>

namespace vsomeip {
namespace sd {

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

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_EVENTGROUP_ENTRY_IMPL_HPP

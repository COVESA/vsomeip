//
// eventgroup_entry_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/service_discovery/eventgroup_entry_impl.hpp>

namespace vsomeip {
namespace service_discovery {

eventgroup_entry_impl::eventgroup_entry_impl() {
	eventgroup_id_ = 0xFFFF;
}

eventgroup_entry_impl::eventgroup_entry_impl(const eventgroup_entry_impl &_entry)
	: entry_impl(_entry) {

	eventgroup_id_ = _entry.eventgroup_id_;
}

eventgroup_entry_impl::~eventgroup_entry_impl() {
}

eventgroup_id eventgroup_entry_impl::get_eventgroup_id() const {
	return eventgroup_id_;
}

void eventgroup_entry_impl::set_eventgroup_id(eventgroup_id _eventgroup_id) {
	eventgroup_id_ = _eventgroup_id;
}

bool eventgroup_entry_impl::serialize(vsomeip::serializer *_to) const {
	bool is_successful = entry_impl::serialize(_to);

	is_successful = is_successful
				&& _to->serialize(vsomeip_protocol_reserved_byte);

	is_successful = is_successful
				&& _to->serialize(static_cast<uint32_t>(time_to_live_), true);

	is_successful = is_successful
			&& _to->serialize(vsomeip_protocol_reserved_word);

	is_successful = is_successful
			&& _to->serialize(static_cast<uint16_t>(eventgroup_id_));

	return is_successful;
}

bool eventgroup_entry_impl::deserialize(vsomeip::deserializer *_from) {
	bool is_successful = entry_impl::deserialize(_from);

	uint16_t reserved;
	is_successful = is_successful
			&& _from->deserialize(reserved);

	uint16_t tmp_eventgroup_id = 0;
	is_successful = is_successful
			&& _from->deserialize(tmp_eventgroup_id);
	eventgroup_id_ = static_cast<eventgroup_id>(tmp_eventgroup_id);

	return is_successful;
}

} // namespace service_discovery
} // namespace vsomeip

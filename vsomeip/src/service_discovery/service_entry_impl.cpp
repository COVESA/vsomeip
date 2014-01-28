//
// service_entry_impl.cpp
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
#include <vsomeip/service_discovery/internal/service_entry_impl.hpp>

namespace vsomeip {
namespace service_discovery {

service_entry_impl::service_entry_impl() {
	minor_version_ = 0;
}

service_entry_impl::~service_entry_impl() {
}

minor_version service_entry_impl::get_minor_version() const {
	return minor_version_;
}

void service_entry_impl::set_minor_version(minor_version _version) {
	minor_version_ = _version;
}

bool service_entry_impl::serialize(vsomeip::serializer *_to) const {
	bool is_successful = entry_impl::serialize(_to);

	is_successful = is_successful && _to->serialize(static_cast<uint8_t>(major_version_));
	is_successful = is_successful && _to->serialize(static_cast<uint32_t>(time_to_live_), true);
	is_successful = is_successful && _to->serialize(static_cast<uint32_t>(minor_version_));

	return is_successful;
}

bool service_entry_impl::deserialize(vsomeip::deserializer *_from) {
	bool is_successful = entry_impl::deserialize(_from);

	uint8_t tmp_major_version;
	is_successful = is_successful && _from->deserialize(tmp_major_version);
	major_version_ = static_cast<major_version>(tmp_major_version);

	uint32_t tmp_time_to_live;
	is_successful = is_successful && _from->deserialize(tmp_time_to_live, true);
	time_to_live_ = static_cast<time_to_live>(tmp_time_to_live);

	uint32_t tmp_minor_version;
	is_successful = is_successful && _from->deserialize(tmp_minor_version);
	minor_version_ = static_cast<minor_version>(tmp_minor_version);

	return is_successful;
}

} // namespace service_discovery
} // namespace vsomeip


//
// entry_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <algorithm>

#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/serializer.hpp>
#include <vsomeip_internal/sd/entry_impl.hpp>
#include <vsomeip_internal/sd/message_impl.hpp>

namespace vsomeip {
namespace sd {

// TODO: throw exception if this constructor is used
entry_impl::entry_impl() {
	type_ = entry_type::UNKNOWN;
	major_version_ = 0;
	service_id_ = 0x0;
	instance_id_ = 0x0;
	time_to_live_ = 0x0;
}

entry_impl::entry_impl(const entry_impl &_entry) {
	type_ = _entry.type_;
	major_version_ = _entry.major_version_;
	service_id_ = _entry.service_id_;
	instance_id_ = _entry.instance_id_;
	time_to_live_ = _entry.time_to_live_;
}

entry_impl::~entry_impl() {
}

entry_type entry_impl::get_type() const {
	return type_;
}

void entry_impl::set_type(entry_type _type) {
	type_ = _type;
}

service_id entry_impl::get_service_id() const {
	return service_id_;
}

void entry_impl::set_service_id(service_id _service_id) {
	service_id_ = _service_id;
}

instance_id entry_impl::get_instance_id() const {
	return instance_id_;
}

void entry_impl::set_instance_id(instance_id _instance_id) {
	instance_id_ = _instance_id;
}

major_version entry_impl::get_major_version() const {
	return major_version_;
}

void entry_impl::set_major_version(major_version _major_version) {
	major_version_ = _major_version;
}

time_to_live entry_impl::get_time_to_live() const {
	return time_to_live_;
}

void entry_impl::set_time_to_live(time_to_live _ttl) {
	time_to_live_ = _ttl;
}

void entry_impl::assign_option(const option &_option, uint8_t _run) {
	if (_run > 0 && _run <= VSOMEIP_MAX_OPTION_RUN) {
		_run--; // Index = Run-1

		uint8_t option_index = get_owning_message()->get_option_index(_option);
		if (0x10 > option_index) { // as we have only a nibble for the option counter
			options_[_run].push_back(option_index);
			std::sort(options_[_run].begin(), options_[_run].end());
		} else {
			// TODO: decide what to do if option does not belong to the message.
		}
	} else {
		// TODO: decide what to do if an illegal index for the option run is provided
	}
}

bool entry_impl::serialize(vsomeip::serializer *_to) const {
	bool is_successful = (0 != _to && _to->serialize(static_cast<uint8_t>(type_)));

	uint8_t index_first_option_run = 0;
	if (options_[0].size() > 0)
		index_first_option_run = options_[0][0];
	is_successful = is_successful
			&& _to->serialize(index_first_option_run);

	uint8_t index_second_option_run = 0;
	if (options_[1].size() > 0)
		index_second_option_run = options_[1][0];
	is_successful = is_successful
			&& _to->serialize(index_second_option_run);

	uint8_t number_of_options = ((((uint8_t)options_[0].size()) << 4)
			| (((uint8_t)options_[1].size()) & 0x0F));
	is_successful = is_successful
			&& _to->serialize(number_of_options);

	is_successful = is_successful
			&& _to->serialize(static_cast<uint16_t>(service_id_));

	is_successful = is_successful
			&& _to->serialize(static_cast<uint16_t>(instance_id_));

	return is_successful;
}

bool entry_impl::deserialize(vsomeip::deserializer *_from) {
	bool is_successful = (0 != _from);

	uint8_t tmp_type;
	is_successful = is_successful && _from->deserialize(tmp_type);
	type_ = static_cast<entry_type>(tmp_type);

	uint32_t tmp_options;
	is_successful = is_successful && _from->deserialize(tmp_options, true);

	uint16_t tmp_id;
	is_successful = is_successful && _from->deserialize(tmp_id);
	service_id_ = static_cast<service_id>(tmp_id);

	is_successful = is_successful && _from->deserialize(tmp_id);
	instance_id_ = static_cast<instance_id>(tmp_id);

	return is_successful;
}

bool entry_impl::is_service_entry() const {
	return (type_ <= entry_type::REQUEST_SERVICE);
}

bool entry_impl::is_eventgroup_entry() const {
	return (type_ >= entry_type::FIND_EVENT_GROUP
			&& type_ <= entry_type::SUBSCRIBE_EVENTGROUP_ACK);
}

} // namespace sd
} // namespace vsomeip

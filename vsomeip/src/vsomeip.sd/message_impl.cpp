//
// message_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/config.hpp>
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/serializer.hpp>
#include <vsomeip_internal/sd/eventgroup_entry_impl.hpp>
#include <vsomeip_internal/sd/service_entry_impl.hpp>
#include <vsomeip_internal/sd/configuration_option_impl.hpp>
#include <vsomeip_internal/sd/ipv4_endpoint_option_impl.hpp>
#include <vsomeip_internal/sd/ipv4_multicast_option_impl.hpp>
#include <vsomeip_internal/sd/ipv6_endpoint_option_impl.hpp>
#include <vsomeip_internal/sd/ipv6_multicast_option_impl.hpp>
#include <vsomeip_internal/sd/load_balancing_option_impl.hpp>
#include <vsomeip_internal/sd/protection_option_impl.hpp>
#include <vsomeip_internal/sd/message_impl.hpp>

#include <iostream>
#include <typeinfo>

namespace vsomeip {
namespace sd {

message_impl::message_impl() {
	header_.service_id_ = 0xFFFF;
	header_.method_id_ = 0x8100;
	header_.protocol_version_ = 0x01;
	flags_ = 0x00;
}

message_impl::~message_impl() {
}

length message_impl::get_length() const {
	length current_length = VSOMEIP_STATIC_HEADER_SIZE
			+ VSOMEIP_STATIC_SERVICE_DISCOVERY_DATA_LENGTH;

	current_length += (entries_.size() * VSOMEIP_ENTRY_LENGTH);

	for (size_t i = 0; i < options_.size(); ++i)
		current_length += (options_[i]->get_length()
				+ VSOMEIP_OPTION_HEADER_LENGTH);

	return current_length;
}

#define VSOMEIP_REBOOT_FLAG 0x80

bool message_impl::get_reboot_flag() const {
	return ((flags_ & VSOMEIP_REBOOT_FLAG) != 0);
}

void message_impl::set_reboot_flag(bool _is_set) {
	if (_is_set)
		flags_ |= VSOMEIP_REBOOT_FLAG;
	else
		flags_ &= ~VSOMEIP_REBOOT_FLAG;
}

#define VSOMEIP_UNICAST_FLAG 0x40

bool message_impl::get_unicast_flag() const {
	return ((flags_ & VSOMEIP_UNICAST_FLAG) != 0);
}

void message_impl::set_unicast_flag(bool _is_set) {
	if (_is_set)
		flags_ |= VSOMEIP_UNICAST_FLAG;
	else
		flags_ &= ~VSOMEIP_UNICAST_FLAG;
}

void message_impl::set_length(length _length) {
}

eventgroup_entry& message_impl::create_eventgroup_entry() {
	eventgroup_entry_impl *tmp_entry = new eventgroup_entry_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_entry->set_owning_message(this);
	entries_.push_back(tmp_entry);
	return *tmp_entry;
}

service_entry& message_impl::create_service_entry() {
	service_entry_impl *tmp_entry = new service_entry_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_entry->set_owning_message(this);
	entries_.push_back(tmp_entry);
	return *tmp_entry;
}

configuration_option& message_impl::create_configuration_option() {
	configuration_option_impl *tmp_option = new configuration_option_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_option->set_owning_message(this);
	options_.push_back(tmp_option);
	return *tmp_option;
}

ipv4_endpoint_option& message_impl::create_ipv4_endpoint_option() {
	ipv4_endpoint_option_impl *tmp_option = new ipv4_endpoint_option_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_option->set_owning_message(this);
	options_.push_back(tmp_option);
	return *tmp_option;
}

ipv4_multicast_option& message_impl::create_ipv4_multicast_option() {
	ipv4_multicast_option_impl *tmp_option = new ipv4_multicast_option_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_option->set_owning_message(this);
	options_.push_back(tmp_option);
	return *tmp_option;
}

ipv6_endpoint_option& message_impl::create_ipv6_endpoint_option() {
	ipv6_endpoint_option_impl *tmp_option = new ipv6_endpoint_option_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_option->set_owning_message(this);
	options_.push_back(tmp_option);
	return *tmp_option;
}

ipv6_multicast_option& message_impl::create_ipv6_multicast_option() {
	ipv6_multicast_option_impl *tmp_option = new ipv6_multicast_option_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_option->set_owning_message(this);
	options_.push_back(tmp_option);
	return *tmp_option;
}

load_balancing_option& message_impl::create_load_balancing_option() {
	load_balancing_option_impl *tmp_option = new load_balancing_option_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_option->set_owning_message(this);
	options_.push_back(tmp_option);
	return *tmp_option;
}

protection_option& message_impl::create_protection_option() {
	protection_option_impl *tmp_option = new protection_option_impl;
	//TODO: throw OutOfMemoryException if allocation fails
	tmp_option->set_owning_message(this);
	options_.push_back(tmp_option);
	return *tmp_option;
}

const std::vector<entry *> message_impl::get_entries() const {
	return entries_;
}

const std::vector<option *> message_impl::get_options() const {
	return options_;
}

// TODO: throw exception to signal "OptionNotFound"
int16_t message_impl::get_option_index(const option &_option) const {
	int16_t i = 0;

	while (i < options_.size()) {
		if (*(options_[i]) == _option)
			return i;
		i++;
	}

	return -1;
}

bool message_impl::serialize(vsomeip::serializer *_to) const {
	bool is_successful = header_.serialize(_to);

	is_successful = is_successful && _to->serialize(flags_);
	is_successful = is_successful
			&& _to->serialize(vsomeip_protocol_reserved_long, true);

	uint32_t entries_length = (entries_.size() * VSOMEIP_ENTRY_LENGTH);
	is_successful = is_successful && _to->serialize(entries_length);

	for (auto it = entries_.begin(); it != entries_.end(); ++it)
		is_successful = is_successful && (*it)->serialize(_to);

	uint32_t options_length = 0;
	for (size_t i = 0; i < options_.size(); ++i)
		options_length += (options_[i]->get_length()
				+ VSOMEIP_OPTION_HEADER_LENGTH);
	is_successful = is_successful && _to->serialize(options_length);

	for (auto it = options_.begin(); it != options_.end(); ++it)
		is_successful = is_successful && (*it)->serialize(_to);

	return is_successful;
}

bool message_impl::deserialize(vsomeip::deserializer *_from) {
	bool is_successful;

	// header
	is_successful = header_.deserialize(_from);

	// flags
	is_successful = is_successful && _from->deserialize(flags_);

	// reserved
	uint32_t reserved;
	is_successful = is_successful && _from->deserialize(reserved, true);

	// entries
	uint32_t entries_length = 0;
	is_successful = is_successful && _from->deserialize(entries_length);

	// backup the current remaining length
	uint32_t save_remaining = _from->get_remaining();

	// set remaining bytes to length of entries array
	_from->set_remaining(entries_length);

	// deserialize the entries
	while (is_successful && _from->get_remaining()) {
		entry* tmp_entry = deserialize_entry(_from);
		if (tmp_entry) {
			entries_.push_back(tmp_entry);
		} else {
			is_successful = false;
		}
	}

	// set length to remaining bytes after entries array
	_from->set_remaining(save_remaining - entries_length);

	// deserialize the options
	uint32_t options_length = 0;
	is_successful = is_successful && _from->deserialize(options_length);

	while (is_successful && _from->get_remaining()) {
		option *tmp_option = deserialize_option(_from);
		if (tmp_option) {
			options_.push_back(tmp_option);
		} else {
			is_successful = false;
		}
	}

	return is_successful;
}

entry * message_impl::deserialize_entry(vsomeip::deserializer *_from) {
	entry *deserialized_entry = 0;
	uint8_t tmp_entry_type;

	if (_from->look_ahead(0, tmp_entry_type)) {
		entry_type deserialized_entry_type =
				static_cast<entry_type>(tmp_entry_type);

		switch (deserialized_entry_type) {
		case entry_type::FIND_SERVICE:
		case entry_type::OFFER_SERVICE:
		//case entry_type::STOP_OFFER_SERVICE:
		case entry_type::REQUEST_SERVICE:
			deserialized_entry = new service_entry_impl;
			break;

		case entry_type::FIND_EVENT_GROUP:
		case entry_type::PUBLISH_EVENTGROUP:
		//case entry_type::STOP_PUBLISH_EVENTGROUP:
		case entry_type::SUBSCRIBE_EVENTGROUP:
		//case entry_type::STOP_SUBSCRIBE_EVENTGROUP:
		case entry_type::SUBSCRIBE_EVENTGROUP_ACK:
		//case entry_type::STOP_SUBSCRIBE_EVENTGROUP_ACK:
			deserialized_entry = new eventgroup_entry_impl;
			break;

		default:
			break;
		};

		// deserialize object
		if (0 != deserialized_entry) {
			if (!deserialized_entry->deserialize(_from)) {
				delete deserialized_entry;
				deserialized_entry = 0;
			};
		}
	}

	return deserialized_entry;
}

option * message_impl::deserialize_option(vsomeip::deserializer *_from) {
	option *deserialized_option = 0;
	uint8_t tmp_option_type;

	if (_from->look_ahead(2, tmp_option_type)) {

		option_type deserialized_option_type =
				static_cast<option_type>(tmp_option_type);

		switch (deserialized_option_type) {

		case option_type::CONFIGURATION:
			deserialized_option = new configuration_option_impl;
			break;
		case option_type::LOAD_BALANCING:
			deserialized_option = new load_balancing_option_impl;
			break;
		case option_type::PROTECTION:
			deserialized_option = new protection_option_impl;
			break;
		case option_type::IP4_ENDPOINT:
			deserialized_option = new ipv4_endpoint_option_impl;
			break;
		case option_type::IP4_MULTICAST:
			deserialized_option = new ipv4_multicast_option_impl;
			break;
		case option_type::IP6_ENDPOINT:
			deserialized_option = new ipv6_endpoint_option_impl;
			break;
		case option_type::IP6_MULTICAST:
			deserialized_option = new ipv6_multicast_option_impl;
			break;

		default:
			break;
		};

		// deserialize object
		if (0 != deserialized_option
				&& !deserialized_option->deserialize(_from)) {
			delete deserialized_option;
			deserialized_option = 0;
		};
	}

	return deserialized_option;
}

} // namespace sd
} // namespace vsomeip


//
// entry_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL_ENTRY_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_IMPL_ENTRY_IMPL_HPP

#include <vector>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/serializable.hpp>
#include <vsomeip/service_discovery/entry.hpp>
#include <vsomeip/service_discovery/impl/message_element_impl.hpp>

#define VSOMEIP_MAX_OPTION_RUN	2

namespace vsomeip {
namespace service_discovery {

class option;
class message_impl;

class entry_impl
		: virtual public entry,
		  virtual public message_element_impl {
public:
	virtual ~entry_impl();

	// public interface
	entry_type get_type() const;

	service_id get_service_id() const;
	void set_service_id(service_id _id);

	instance_id get_instance_id() const;
	void set_instance_id(instance_id _id);

	major_version get_major_version() const;
	void set_major_version(major_version _version);

	time_to_live get_time_to_live() const;
	void set_time_to_live(time_to_live _time_to_live);

	void assign_option(const option& _option, uint8_t _run);

	bool is_service_entry() const;
	bool is_eventgroup_entry() const;

	void set_type(entry_type _type);

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

protected:
	entry_type type_;
	service_id service_id_;
	instance_id instance_id_;
	major_version major_version_;
	time_to_live time_to_live_;

	std::vector< uint8_t > options_[VSOMEIP_MAX_OPTION_RUN];

	entry_impl();
	entry_impl(const entry_impl &entry_);
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL_ENTRY_IMPL_HPP

//
// entry.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_ENTRY_HPP
#define VSOMEIP_SERVICE_DISCOVERY_ENTRY_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/serializable.hpp>
#include <vsomeip/service_discovery/enumeration_types.hpp>

namespace vsomeip {
namespace service_discovery {

class option;

class entry : virtual public serializable {
public:
	virtual ~entry() {};

	virtual entry_type get_type() const = 0;
	virtual void set_type(entry_type _type) = 0;

	virtual service_id get_service_id() const = 0;
	virtual void set_service_id(service_id _id) = 0;

	virtual instance_id get_instance_id() const = 0;
	virtual void set_instance_id(instance_id _id) = 0;

	virtual major_version get_major_version() const = 0;
	virtual void set_major_version(major_version _version) = 0;

	virtual time_to_live get_time_to_live() const = 0;
	virtual void set_time_to_live(time_to_live _ttl) = 0;

	virtual void assign_option(const option& _option, uint8_t _run) = 0;

	virtual bool is_service_entry() const = 0;
	virtual bool is_eventgroup_entry() const = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_ENTRY_HPP

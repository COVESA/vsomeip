//
// service_manager.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_HPP

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace sd {

class service {
public:
	service(service_id _service, instance_id _instance);

	service_id get_service_id() const;
	instance_id get_instance_id() const;

	const std::set< eventgroup_id > & get_eventgroups() const;
	void add_eventgroup(eventgroup_id _eventgroup);
	void remove_eventgroup(eventgroup_id _eventgroup);

	bool is_local() const;

	bool is_active() const;
	void set_active(bool _is_active);

private:
	service_id service_;
	instance_id instance_;

	std::set< eventgroup_id > eventgroups_;

	bool is_local_;
	bool is_active_;
	std::set< client_id > requesters_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_SERVICE_HPP

//
// request_info.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_REQUEST_INFO_HPP
#define VSOMEIP_DAEMON_REQUEST_INFO_HPP

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

struct request_info {
	request_info(client_id _id, service_id _service, instance_id _instance, const endpoint *_location)
		: id_(_id), service_(_service), instance_(_instance), location_(_location) {
	}

	client_id id_;
	service_id service_;
	instance_id instance_;
	const endpoint *location_;

	// as it is used in a set
	bool operator <(const request_info &_other) const {
		return (id_ < _other.id_
			 || service_ < _other.service_
			 || instance_ < _other.instance_
			 || location_ < _other.location_);
	}
};

} // namespace vsomeip

#endif // VSOMEIP_DAEMON_REQUEST_INFO_HPP

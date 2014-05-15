//
// message_queue.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_METHOD_INFO_HPP
#define VSOMEIP_INTERNAL_METHOD_INFO_HPP

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

struct method_info {
	service_id service_;
	instance_id instance_;
	method_id method_;

	bool operator<(const method_info &_other) const {
		return (service_ < _other.service_ ||
				(service_ == _other.service_ && instance_ < _other.instance_) ||
				(service_ == _other.service_ && instance_ == _other.instance_ && method_ < _other.method_));
	}
};

};

#endif // VSOMEIP_INTERNAL_METHOD_INFO_HPP

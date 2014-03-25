//
// service_info.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_SERVICE_INFO_HPP
#define VSOMEIP_DAEMON_SERVICE_INFO_HPP

#include <map>
#include <set>

namespace vsomeip {

class endpoint;

struct service_info {
	uint32_t application_id_;
	std::set< uint32_t > clients_;
	bool is_started_;
	bool has_service_discovery;
	std::set< const endpoint *> locations_;
};

} // namespace vsomeip

#endif // VSOMEIP_DAEMON_SERVICE_INFO_HPP

//
// service_info.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_INFO_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_INFO_HPP

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace service_discovery {

struct service_info {
	service_id service_;
	instance_id instance_;
	major_version major_version_;
	minor_version minor_version_;
	time_to_live time_to_live_;
};

} // namespace service_discovery
} // namespace vsomeip


#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_INFO_HPP

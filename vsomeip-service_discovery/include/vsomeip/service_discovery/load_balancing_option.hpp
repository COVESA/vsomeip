//
// loadbalancingoption.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_LOAD_BALANCING_OPTION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_LOAD_BALANCING_OPTION_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/service_discovery/option.hpp>

namespace vsomeip {
namespace service_discovery {

class load_balancing_option
		: virtual public option {
public:
	virtual ~load_balancing_option() {};

	virtual priority get_priority() const = 0;
	virtual void set_priority(priority _priority) = 0;

	virtual weight get_weight() const = 0;
	virtual void set_weight(weight _weight) = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_LOAD_BALANCING_OPTION_HPP

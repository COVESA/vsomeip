//
// load_balancing_option.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SD_LOAD_BALANCING_OPTION_HPP
#define VSOMEIP_SD_LOAD_BALANCING_OPTION_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/sd/option.hpp>

namespace vsomeip {
namespace sd {

class load_balancing_option
		: virtual public option {
public:
	virtual ~load_balancing_option() {};

	virtual priority get_priority() const = 0;
	virtual void set_priority(priority _priority) = 0;

	virtual weight get_weight() const = 0;
	virtual void set_weight(weight _weight) = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_LOAD_BALANCING_OPTION_HPP

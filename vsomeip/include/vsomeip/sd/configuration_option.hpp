//
// configuration_option.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SD_CONFIGURATION_OPTION_HPP
#define VSOMEIP_SD_CONFIGURATION_OPTION_HPP

#include <string>
#include <vsomeip/sd/option.hpp>

namespace vsomeip {
namespace sd {

class configuration_option
		: virtual public option {
public:
	virtual ~configuration_option() {};

	virtual void add_item(const std::string &_key, const std::string &_value) = 0;
	virtual void remove_item(const std::string &_key) = 0;

	virtual std::vector<std::string> get_keys() const = 0;
	virtual std::vector<std::string> get_values() const = 0;

	virtual std::string get_value(const std::string &_key) const = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_CONFIGURATION_OPTION_HPP

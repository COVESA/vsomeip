//
// configuration_option.hpp
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_CONFIGURATION_OPTION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_CONFIGURATION_OPTION_HPP

#include <string>
#include <vsomeip/service_discovery/option.hpp>

namespace vsomeip {
namespace service_discovery {

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

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_CONFIGURATION_OPTION_HPP

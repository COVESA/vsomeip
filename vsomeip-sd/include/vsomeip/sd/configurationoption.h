//
// configurationoption.h
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_CONFIGURATIONOPTION_H__
#define __VSOMEIP_SD_CONFIGURATIONOPTION_H__

#include <string>
#include <vsomeip/sd/option.h>

namespace vsomeip {

namespace sd {

class ConfigurationOption : virtual public Option {

public:
	virtual ~ConfigurationOption() {};

	virtual void addItem(const std::string& a_key, const std::string& a_value) = 0;
	virtual void removeItem(const std::string& a_key) = 0;

	virtual std::vector<std::string> getKeys() const = 0;
	virtual std::vector<std::string> getValues() const = 0;

	virtual std::string getValue(const std::string& a_key) const = 0;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_CONFIGURATIONOPTION_H__

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

#ifndef __VSOMEIP_LIBRARY_SD_CONFIGURATIONOPTION_H__
#define __VSOMEIP_LIBRARY_SD_CONFIGURATIONOPTION_H__

#include <map>
#include <string>
#include <vector>

#include <vsomeip/sd/configurationoption.h>
#include "optionimpl.h"

namespace vsomeip {

class Serializer;
class Deserializer;

namespace sd {

class ConfigurationOptionImpl: virtual public ConfigurationOption, virtual public OptionImpl {

public:
	ConfigurationOptionImpl();
	virtual ~ConfigurationOptionImpl();
	bool operator==(const Option& a_option) const;

	void addItem(const std::string& a_key, const std::string& a_value);
	void removeItem(const std::string& a_key);

	std::vector<std::string> getKeys() const;
	std::vector<std::string> getValues() const;
	std::string getValue(const std::string& a_key) const;

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

private:
	std::map< std::string, std::string> m_configuration;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_LIBRARY_SD_CONFIGURATIONOPTION_H__

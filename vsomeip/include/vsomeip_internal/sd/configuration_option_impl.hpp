//
// configuration_option_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_CONFIGURATION_OPTION_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_CONFIGURATION_OPTION_IMPL_HPP

#include <map>
#include <string>
#include <vector>

#include <vsomeip/sd/configuration_option.hpp>
#include <vsomeip_internal/sd/option_impl.hpp>

namespace vsomeip {

class serializer;
class deserializer;

namespace sd {

class configuration_option_impl :
		virtual public configuration_option,
		virtual public option_impl {

public:
	configuration_option_impl();
	virtual ~configuration_option_impl();
	bool operator==(const option& _other) const;

	void add_item(const std::string &_key, const std::string &_value);
	void remove_item(const std::string &_key);

	std::vector<std::string> get_keys() const;
	std::vector<std::string> get_values() const;
	std::string get_value(const std::string &_key) const;

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

private:
	std::map< std::string, std::string> configuration_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_CONFIGURATION_OPTION_IMPL_HPP

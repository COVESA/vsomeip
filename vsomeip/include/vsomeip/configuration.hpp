//
// configuration.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_CONFIGURATION_HPP
#define VSOMEIP_CONFIGURATION_HPP

#include <string>

#include <boost/log/trivial.hpp>

namespace vsomeip {

class configuration {
public:
	static configuration * get_instance();
	virtual ~configuration() {};

	virtual void init(int _count, char **_options) = 0;

	virtual bool use_console_logger() const = 0;
	virtual bool use_file_logger() const = 0;
	virtual bool use_dlt_logger() const = 0;

	virtual boost::log::trivial::severity_level get_loglevel() const = 0;

	virtual const std::string & get_configuration_file_path() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_CONFIGURATION_HPP

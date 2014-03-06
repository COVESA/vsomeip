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

#ifndef VSOMEIP_INTERNAL_CONFIGURATION_IMPL_HPP
#define VSOMEIP_INTERNAL_CONFIGURATION_IMPL_HPP

#include <string>

#include <boost/log/trivial.hpp>

#include <vsomeip/configuration.hpp>

namespace vsomeip {

class configuration_impl
		: virtual public configuration {
public:
	static configuration * get_instance();
	configuration_impl();
	~configuration_impl();

	void init(int _count, char **_options);

	bool use_console_logger() const;
	bool use_file_logger() const;
	bool use_dlt_logger() const;

	boost::log::trivial::severity_level get_loglevel() const;

	const std::string & get_configuration_file_path() const;

private:
	bool use_console_logger_;
	bool use_file_logger_;
	bool use_dlt_logger_;

	boost::log::trivial::severity_level loglevel_;

protected:
	std::string configuration_file_path_;

protected:
	void read();

private:
	void parse_options(int, char **);
	void parse_loggers(const std::string &);
	void parse_loglevel(const std::string &);
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_CONFIGURATION_IMPL_HPP

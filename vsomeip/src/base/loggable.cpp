//
// loggable.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <vsomeip/configuration.hpp>
#include <vsomeip/internal/loggable.hpp>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace expressions = boost::log::expressions;

using namespace boost::log::trivial;

namespace vsomeip {

void loggable::init() {
	configuration * c = configuration::get_instance();
	severity_level loglevel = c->get_loglevel();

	logging::add_common_attributes();

	auto daemon_log_format = expressions::stream
			<< expressions::format_date_time<boost::posix_time::ptime>(
					"TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << " ["
			<< expressions::attr<severity_level>("Severity") << "] "
			<< expressions::smessage;

	if (c->use_console_logger()) {
		logging::add_console_log(std::clog,
				keywords::filter = severity >= loglevel, keywords::format =
						daemon_log_format);
	}

	if (c->use_file_logger()) {
		logging::add_file_log(keywords::file_name = "vsomeipd_%N.log",
				keywords::rotation_size = 10 * 1024 * 1024,
				keywords::time_based_rotation =
						logging::sinks::file::rotation_at_time_point(0, 0, 0),
				keywords::filter = severity >= loglevel, keywords::format =
						daemon_log_format);
	}

	// TODO: DLT

	boost::log::core::get()->set_filter(severity >= loglevel);
}

} // namespace vsomeip





//
// log_owner.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <fstream>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility/empty_deleter.hpp>

#include <vsomeip_internal/log_owner.hpp>

namespace logging = boost::log;
namespace sources = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expressions = boost::log::expressions;
namespace attributes = boost::log::attributes;

using namespace boost::log::trivial;

namespace vsomeip {

log_owner::log_owner() {
	logging::add_common_attributes();
}

void log_owner::set_id(const std::string &_id) {
	id_ = _id;
	logger_.add_attribute("Owner-Id", attributes::constant< std::string >(_id));
}

void log_owner::set_loglevel(const std::string &_loglevel) {
	if (_loglevel == "fatal")
		loglevel_ = fatal;
	else if (_loglevel == "error")
		loglevel_ = error;
	else if (_loglevel == "warning")
		loglevel_ = warning;
	else if (_loglevel == "debug")
		loglevel_ = debug;
	else if (_loglevel == "verbose")
		loglevel_ = trace;
	else
		loglevel_ = info;
}

void log_owner::enable_console() {
	if (console_sink_)
		return;

	auto vsomeip_log_format = expressions::stream
				<< expressions::format_date_time<boost::posix_time::ptime>(
						"TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
				<< " ["	<< expressions::attr<severity_level>("Severity") << "] "
				<< expressions::smessage;

	boost::shared_ptr< sinks::text_ostream_backend > backend
		= boost::make_shared< sinks::text_ostream_backend >();
	backend->add_stream(
		boost::shared_ptr< std::ostream >(&std::clog, boost::empty_deleter()));

	console_sink_ = new sink_t(backend);
	boost::shared_ptr< sink_t > sink(console_sink_);

	sink->set_filter(
		boost::phoenix::bind(
			&log_owner::filter,
			this,
			severity.or_none(),
			owner_id.or_none()
		)
	);
	sink->set_formatter(vsomeip_log_format);

	logging::core::get()->add_sink(sink);
}

void log_owner::enable_file(const std::string &_name) {
	if (file_sink_)
		return;

	auto vsomeip_log_format = expressions::stream
				<< expressions::format_date_time<boost::posix_time::ptime>(
						"TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
				<< " ["	<< expressions::attr<severity_level>("Severity") << "] "
				<< expressions::smessage;

	boost::shared_ptr< sinks::text_ostream_backend > backend
		= boost::make_shared< sinks::text_ostream_backend >();
	backend->add_stream(
		boost::shared_ptr< std::ostream >(new std::ofstream(_name + ".log")));

	file_sink_ = new sink_t(backend);
	boost::shared_ptr< sink_t > sink(file_sink_);

	sink->set_filter(
		boost::phoenix::bind(
			&log_owner::filter,
			this,
			severity.or_none(),
			owner_id.or_none()
		)
	);
	sink->set_formatter(vsomeip_log_format);

	logging::core::get()->add_sink(sink);
}

void log_owner::enable_dlt() {
	// TODO: implement
}

bool log_owner::filter(
		logging::value_ref< severity_level, tag::severity > const &_loglevel,
		logging::value_ref< std::string, tag::owner_id > const &_id) {
	return _loglevel >= loglevel_ && _id == id_;
}


} // namespace vsomeip




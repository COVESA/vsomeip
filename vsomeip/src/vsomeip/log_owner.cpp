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

// The "empty_deleter"-struct was moved from the log-package
// to the more generic "utility"-package in V1.55. If we'd
// use the "old" include, we get a "deprecation" warning
// when compiling with the newer boost version. Therefore a
// version dependent include handling is done here, which
// can/should be removed in case GPT is updating Boost to V1.55.
#if BOOST_VERSION < 105500
#include <boost/log/utility/empty_deleter.hpp>
#else
#include <boost/utility/empty_deleter.hpp>
#endif

#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/log_owner.hpp>

namespace logging = boost::log;
namespace sources = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expressions = boost::log::expressions;
namespace attributes = boost::log::attributes;

using namespace boost::log::trivial;

namespace vsomeip {

log_owner::log_owner(const std::string &_name)
	: owner_base(_name) {
	logging::add_common_attributes();
}

void log_owner::configure_logging(bool _use_console, bool _use_file, bool _use_dlt) {
	if (_use_console)
		enable_console();

	if (_use_file)
		enable_file();

	if (_use_dlt)
		enable_file();

	if (!_use_console && !_use_file && !_use_dlt) {
		use_null_logger();
	}
}

void log_owner::set_channel(const std::string &_channel) {
	channel_ = _channel;
	logger_.add_attribute("Channel", attributes::constant< std::string >(_channel));
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
		boost::shared_ptr< std::ostream >(
			&std::clog,
#if BOOST_VERSION < 105500
			boost::log::empty_deleter()
#else
			boost::empty_deleter()
#endif
		)
	);

	console_sink_ = new sink_t(backend);
	boost::shared_ptr< sink_t > sink(console_sink_);

	sink->set_filter(
		boost::phoenix::bind(
			&log_owner::filter,
			this,
			severity.or_none(),
			channel.or_none()
		)
	);
	sink->set_formatter(vsomeip_log_format);

	logging::core::get()->add_sink(sink);
}

void log_owner::enable_file() {
	if (file_sink_)
		return;

	auto vsomeip_log_format = expressions::stream
				<< expressions::format_date_time<boost::posix_time::ptime>(
						"TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
				<< " ["	<< expressions::attr<severity_level>("Severity") << "] "
				<< expressions::smessage;

	configuration * vsomeip_configuration
		= configuration::request(name_);

	boost::shared_ptr< sinks::text_ostream_backend > backend
		= boost::make_shared< sinks::text_ostream_backend >();
	backend->add_stream(
		boost::shared_ptr< std::ostream >(
			new std::ofstream(
				vsomeip_configuration->get_logfile_path()
			)
		)
	);

	configuration::release(name_);

	file_sink_ = new sink_t(backend);
	boost::shared_ptr< sink_t > sink(file_sink_);

	sink->set_filter(
		boost::phoenix::bind(
			&log_owner::filter,
			this,
			severity.or_none(),
			channel.or_none()
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
		logging::value_ref< std::string, tag::channel > const &_channel) {
	return _channel == channel_ && _loglevel >= loglevel_;
}

void log_owner::use_null_logger() {
	boost::shared_ptr< sinks::text_ostream_backend > backend
		= boost::make_shared< sinks::text_ostream_backend >();
	backend->add_stream(
		boost::shared_ptr< std::ostream >(
			new std::ofstream("/dev/null") // TODO: how to call this on windows
		)
	);

	file_sink_ = new sink_t(backend);
	boost::shared_ptr< sink_t > sink(file_sink_);

	logging::core::get()->add_sink(sink);
}

} // namespace vsomeip




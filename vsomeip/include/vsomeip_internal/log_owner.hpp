//
// log_owner.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_INTERNAL_LOG_OWNER_HPP
#define VSOMEIP_INTERNAL_LOG_OWNER_HPP

#include <string>

#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

BOOST_LOG_ATTRIBUTE_KEYWORD(owner_id, "Owner-Id",
		std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity",
		boost::log::trivial::severity_level)

typedef boost::log::sinks::synchronous_sink<
			boost::log::sinks::text_ostream_backend > sink_t;

class log_owner {
public:
	log_owner();

	void set_id(const std::string &_id);
	void set_loglevel(const std::string &_loglevel);

	void enable_console();
	void enable_file(const std::string &_name);
	void enable_dlt();

protected:
	boost::log::sources::severity_logger<
		boost::log::trivial::severity_level > logger_;
	boost::log::trivial::severity_level loglevel_;
	std::string id_;

	sink_t *console_sink_;
	sink_t *file_sink_;
	//dlt_sink *dlt_sink_;

private:
	bool filter(boost::log::value_ref< boost::log::trivial::severity_level,
										tag::severity > const &_loglevel,
			     boost::log::value_ref< std::string,
			     	 	 	 	 	    tag::owner_id > const &_id);
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_LOG_OWNER_HPP

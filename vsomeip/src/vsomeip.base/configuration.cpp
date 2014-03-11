//
// configuration.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <vsomeip_internal/config.hpp>
#include <vsomeip_internal/configuration.hpp>

namespace options = boost::program_options;

namespace vsomeip {

configuration * configuration::get_instance() {
	static configuration the_configuration;
	return &the_configuration;
}

configuration::configuration()
	: use_console_logger_(false),
	  use_file_logger_(false),
	  use_dlt_logger_(false),
	  loglevel_("info"),
	  configuration_file_path_(VSOMEIP_DEFAULT_CONFIGURATION_FILE_PATH) {
}

configuration::~configuration() {
}

void configuration::init(int _option_count, char **_options) {
	read_options(_option_count, _options);
	read_configuration();
}

void configuration::read_options(int _options_count, char **_options) {
	options::variables_map command_line_options;

	try {
		options::options_description valid_options("valid_options");
		valid_options.add_options()
			("help", "Print help message")
			("config",
			 options::value< std::string >()->default_value("vsomeip.conf"),
			 "Path to configuration file");

		options::store(
			options::parse_command_line(
				_options_count,
				_options,
				valid_options
			),
			command_line_options
		);

		options::notify(command_line_options);
	}

	catch (...) {
		// intentionally left empty
	}

	if (command_line_options.count("config")) {
		configuration_file_path_
			= command_line_options["config"].as< std::string >();
	}
}

void configuration::read_configuration() {
	static bool has_read = false;
	if (!has_read) {
		options::options_description vsomeip_options_description;
		vsomeip_options_description.add_options()
			(
				"someip.loglevel",
				options::value< std::string >(),
				"Loglevel to be used by the vsomeip daemon"
			)
			(
				"someip.loggers",
				options::value< std::string >(),
				"Loggers (Console, File, DLT) to be used by vsomeip daemon"
			)
			(
				"someip.daemon.service_discovery_enabled",
				options::value<bool>(),
				"Enable service discovery by vsomeip daemon"
			)
			(
				"someip.daemon.virtual_mode_enabled",
				options::value<bool>(),
				"Enable virtual mode by vsomeip daemon"
			)
			(
				"someip.daemon.registry",
				options::value<std::string>(),
				"Access point to internal registry"
			)
			(
				"someip.service_discovery.port",
				options::value<int>(),
				"Service discovery port"
			)
			(
				"someip.service_discovery.min_initial_delay",
				options::value<int>(),
				"Minimum initial delay before first offer message"
			)
			(
				"someip.service_discovery.max_initial_delay",
				options::value<int>(),
				"Maximum initial delay before first offer message"
			)
			(
				"someip.service_discovery.repetition_base_delay",
				options::value<int>(),
				"Base delay for the repetition phase."
			)
			(
				"someip.service_discovery.repetition_max",
				options::value<int>(),
				"Maximum number of repetitions in the repetition phase."
			)
			(
				"someip.service_discovery.ttl",
				options::value<int>(),
				"Lifetime of service discovery entry"
			)
			(
				"someip.service_discovery.cyclic_offer_delay",
				options::value<int>(),
				"Cycle of the OfferService messages in the main phase"
			)
			(
				"someip.service_discovery.cyclic_request_delay",
				options::value<int>(),
				"Cycle of the FindService messages in the main phase"
			)
			(
				"someip.service_discovery.request_response_delay",
				options::value<int>(),
				"Delay of an unicast answer to a multicast message."
			);

		options::variables_map vsomeip_options;
		std::ifstream vsomeip_configuration_file;
		vsomeip_configuration_file.open(configuration_file_path_.c_str());
		if (vsomeip_configuration_file.is_open()) {
			try {
				options::store(
					options::parse_config_file(
						vsomeip_configuration_file,
						vsomeip_options_description,
						true),
						vsomeip_options
				);
			}

			catch (...) {
				// Intentionally left empty
			}

			// General
			if (vsomeip_options.count("someip.loggers"))
				read_loggers(vsomeip_options["someip.loggers"].as< std::string >());

			if (vsomeip_options.count("someip.loglevel"))
				read_loglevel(vsomeip_options["someip.loglevel"].as< std::string >());

			// Daemon
			if (vsomeip_options.count("someip.daemon.service_discovery_enabled"))
				use_service_discovery_
					= vsomeip_options["someip.daemon.service_discovery_enabled"].as< bool >();

			if (vsomeip_options.count("someip.daemon.virtual_mode_enabled"))
				use_virtual_mode_
					= vsomeip_options["someip.daemon.virtual_mode_enabled"].as< bool >();

			// Service Discovery
			if (vsomeip_options.count("someip.service_discovery.unicast_address"))
				unicast_address_ = vsomeip_options["someip.service_discovery.unicast_address"].as< std::string >();

			if (vsomeip_options.count("someip.service_discovery.multicast_address"))
				multicast_address_ = vsomeip_options["someip.service_discovery.multicast_address"].as< std::string >();

			if (vsomeip_options.count("someip.service_discovery.port"))
				port_ = vsomeip_options["someip.service_discovery.port"].as< int >();

			if (vsomeip_options.count("someip.service_discovery.min_initial_delay"))
				min_initial_delay_
					= vsomeip_options["someip.service_discovery.min_initial_delay"].as< int >();

			if (vsomeip_options.count("someip.service_discovery.max_initial_delay"))
				max_initial_delay_
					= vsomeip_options["someip.service_discovery.max_initial_delay"].as< int >();

			if (vsomeip_options.count("someip.service_discovery.repetition_base_delay"))
				repetition_base_delay_
					= vsomeip_options["someip.service_discovery.repetition_base_delay"].as< int >();

			if (vsomeip_options.count("someip.service_discovery.repetition_max"))
				repetition_max_
					= vsomeip_options["someip.service_discovery.repetition_max"].as< int >();

			if (vsomeip_options.count("someip.service_discovery.cyclic_offer_delay"))
				cyclic_offer_delay_
					= vsomeip_options["someip.service_discovery.cyclic_offer_delay"].as< int >();

			if (vsomeip_options.count("someip.service_discovery.cyclic_request_delay"))
				cyclic_request_delay_
					= vsomeip_options["someip.service_discovery.cyclic_request_delay"].as< int >();

			if (vsomeip_options.count("someip.service_discovery.request_response_delay"))
				request_response_delay_
					= vsomeip_options["someip.service_discovery.request_response_delay"].as< int >();
		}
	}
}


const std::string & configuration::get_configuration_file_path() const {
	return configuration_file_path_;
}

bool configuration::use_console_logger() const {
	return use_console_logger_;
}

bool configuration::use_file_logger() const {
	return use_file_logger_;
}

bool configuration::use_dlt_logger() const {
	return use_dlt_logger_;
}

const std::string &  configuration::get_loglevel() const {
	return loglevel_;
}

bool configuration::use_service_discovery() const {
	return use_service_discovery_;
}

bool configuration::use_virtual_mode() const {
	return use_virtual_mode_;
}

const std::string & configuration::get_unicast_address() const {
	return unicast_address_;
}
const std::string & configuration::get_multicast_address() const {
	return multicast_address_;
}

uint16_t configuration::get_port() const {
	return port_;
}

uint32_t configuration::get_min_initial_delay() const {
	return min_initial_delay_;
}

uint32_t configuration::get_max_initial_delay() const {
	return max_initial_delay_;
}

uint32_t configuration::get_repetition_base_delay() const {
	return repetition_base_delay_;
}

uint8_t configuration::get_repetition_max() const {
	return repetition_max_;
}

uint32_t configuration::get_ttl() const {
	return ttl_;
}

uint32_t configuration::get_cyclic_offer_delay() const {
	return cyclic_offer_delay_;
}

uint32_t configuration::get_cyclic_request_delay() const {
	return cyclic_request_delay_;
}

uint32_t configuration::get_request_response_delay() const {
	return request_response_delay_;
}

void configuration::read_loggers(const std::string &_logger_configuration) {
	std::string logger_configuration(_logger_configuration);
	boost::algorithm::trim(logger_configuration);

	std::vector<std::string> loggers;
	boost::split(loggers, logger_configuration, boost::is_any_of("|"));

	for (auto i: loggers) {
		boost::algorithm::trim(i);
		if ("console" == i) use_console_logger_ = true;
		else if ("file" == i) use_file_logger_ = true;
		else if ("dlt" == i) use_dlt_logger_ = true;
	}
}

void configuration::read_loglevel(const std::string &_loglevel) {
	if (_loglevel == "info"
	 || _loglevel == "debug"
	 || _loglevel == "warning"
	 || _loglevel == "error"
	 || _loglevel == "fatal"
	 || _loglevel == "verbose") {
		loglevel_ = _loglevel;
	} else {
		loglevel_ = "info";
	}
}

} // namespace vsomeip


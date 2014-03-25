//
// configuration.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <fstream>
#include <map>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip_internal/configuration.hpp>

namespace options = boost::program_options;

namespace vsomeip {

std::string configuration::configuration_file_path_(VSOMEIP_DEFAULT_CONFIGURATION_FILE_PATH);
std::map< std::string, configuration * > configuration::configurations__;

void configuration::init(int _options_count, char **_options) {
	options::variables_map command_line_options;

	try {
		options::options_description valid_options("valid_options");
		valid_options.add_options()
			(
				"vsomeip-config",
				options::value< std::string >()->default_value("/etc/vsomeip.conf"),
				"Path to configuration file"
			);

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

	if (command_line_options.count("vsomeip-config")) {
		configuration_file_path_
			= command_line_options["vsomeip-config"].as< std::string >();
	}
}

// TODO: check whether ref_ could be replaced using shared pointers
configuration * configuration::request(const std::string &_name) {
	configuration * requested_configuration = 0;
	auto found_configuration = configurations__.find(_name);
	if (found_configuration == configurations__.end()) {
		requested_configuration = new configuration;
		requested_configuration->read_configuration(_name);
		configurations__[_name] = requested_configuration;
	} else {
		requested_configuration = found_configuration->second;
	}

	requested_configuration->ref_ ++;
	return requested_configuration;
}

void configuration::release(const std::string &_name) {
	configuration * requested_configuration = 0;
	auto found_configuration = configurations__.find(_name);
	if (found_configuration != configurations__.end()) {
		requested_configuration = found_configuration->second;
		requested_configuration->ref_ --;
		if (requested_configuration->ref_ == 0) {
			configurations__.erase(_name);
			delete requested_configuration;
		}
	}
}

configuration::configuration()
	: use_console_logger_(false),
	  use_file_logger_(false),
	  use_dlt_logger_(false),
	  loglevel_("info"),
	  logfile_path_("./vsomeip.log"),
	  use_service_discovery_(false),
	  use_virtual_mode_(false),
	  receiver_slots_(10),
	  protocol_("udp.v4"),
	  unicast_address_("127.0.0.1"),
	  multicast_address_("223.0.0.0"),
	  port_(30490),
	  min_initial_delay_(10),
	  max_initial_delay_(100),
	  repetition_base_delay_(200),
	  repetition_max_(3),
	  ttl_(0xFFFFFFFF),
	  cyclic_offer_delay_(3000),
	  cyclic_request_delay_(3000),
	  request_response_delay_(2000) { // TODO: symbolic constants instead of magic values
}

configuration::~configuration() {
}

void configuration::read_configuration(const std::string &_name) {
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
				"someip.application.slots",
				options::value<int>(),
				"Maximum number of received messages an application can buffer"
			)
			(
				"someip.service_discovery.protocol",
				options::value<std::string>(),
				"Protocol used to communicate to Service Discovery"
			)
			(
				"someip.service_discovery.unicast",
				options::value<std::string>(),
				"Unicast address of Service Discovery"
			)
			(
				"someip.service_discovery.multicast",
				options::value<std::string>(),
				"Multicast address to be used by Service Discovery"
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

		// Local overrides
		if (_name != "") {
			std::string local_override("someip.application." + _name + ".slots");
			vsomeip_options_description.add_options()
				(
					local_override.c_str(),
					options::value< int >(),
					"Local override for number of receiver slots."
				);
		};

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

			// Application
			if (vsomeip_options.count("someip.application.slots"))
				receiver_slots_
					= vsomeip_options["someip.application.slots"].as< int >();

			// Service Discovery
			if (vsomeip_options.count("someip.service_discovery.protocol"))
				protocol_ = vsomeip_options["someip.service_discovery.protocol"].as< std::string >();

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

			if (_name != "") {
				if (vsomeip_options.count("someip.application." + _name + ".slots"))
					receiver_slots_
						= vsomeip_options["someip.application." + _name + ".slots"].as< int >();
			}
		}
	}
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

const std::string &  configuration::get_logfile_path() const {
	return logfile_path_;
}

bool configuration::use_service_discovery() const {
	return use_service_discovery_;
}

bool configuration::use_virtual_mode() const {
	return use_virtual_mode_;
}

uint8_t configuration::get_receiver_slots() const {
	return receiver_slots_;
}

const std::string & configuration::get_protocol() const {
	return protocol_;
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


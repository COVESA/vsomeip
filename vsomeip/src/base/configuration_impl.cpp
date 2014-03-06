//
// configuration_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/internal/configuration_impl.hpp>
#include <vsomeip/internal/loggable.hpp>

using namespace boost::log::trivial;
namespace options = boost::program_options;

namespace vsomeip {

configuration * configuration_impl::get_instance() {
	static configuration_impl the_configuration;
	return &the_configuration;
}

configuration_impl::configuration_impl()
	: use_console_logger_(true), use_file_logger_(false), use_dlt_logger_(false),
	  loglevel_(info), configuration_file_path_(VSOMEIP_DEFAULT_CONFIGURATION_FILE) {
}

configuration_impl::~configuration_impl() {
}

void configuration_impl::init(int _count, char **_options) {
	parse_options(_count, _options);
	read();
	loggable::init();
}

void configuration_impl::parse_options(int count, char **options) {
	options::variables_map commandline_configuration;

	try {
		options::options_description valid_options("allowed");
		valid_options.add_options()
			 ("help", "Print help message")
			 ("config", options::value<std::string>()->default_value("vsomeip.conf"), "Path to configuration file");

		options::store(options::parse_command_line(count, options, valid_options), commandline_configuration);
		options::notify(commandline_configuration);
	}

	catch (...) {
		// intentionally doing nothing here as the command line may contain other parameters...
	}

	if (commandline_configuration.count("config"))
		configuration_file_path_ = commandline_configuration["config"].as<std::string>();

}

void configuration_impl::read() {
	static bool has_read = false;
	if (!has_read) {
		options::variables_map configuration;
		options::options_description configuration_description;
		configuration_description.add_options()
			("someip.daemon.loglevel", options::value<std::string>(),"Log level to be used by vsomeip daemon")
			("someip.daemon.logger", options::value<std::string>(),	"Logger(s) to be used");

		std::ifstream configuration_file;
		configuration_file.open(configuration_file_path_.c_str());
		if (configuration_file.is_open()) {
			try {
				options::store(
					options::parse_config_file(configuration_file,
						configuration_description, true), configuration);
				options::notify(configuration);
			}

			catch (...) {
				// Intentionally left empty!
			}

			if (configuration.count("someip.daemon.logger")) {
				parse_loggers(configuration["someip.daemon.logger"].as< std::string >());
			}

			if (configuration.count("someip.daemon.loglevel"))
				parse_loglevel(configuration["someip.daemon.loglevel"].as< std::string>());
		}
	}
}

bool configuration_impl::use_console_logger() const {
	return use_console_logger_;
}

bool configuration_impl::use_file_logger() const {
	return use_file_logger_;
}

bool configuration_impl::use_dlt_logger() const {
	return use_dlt_logger_;
}

boost::log::trivial::severity_level configuration_impl::get_loglevel() const {
	return loglevel_;
}

const std::string & configuration_impl::get_configuration_file_path() const {
	return configuration_file_path_;
}

void configuration_impl::parse_loggers(const std::string &_logger_configuration) {
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

void configuration_impl::parse_loglevel(const std::string &_loglevel) {
	if (_loglevel == "debug")
		loglevel_ = debug;
	else if (_loglevel == "trace")
		loglevel_ = trace;
	else if (_loglevel == "warning")
		loglevel_ = warning;
	else if (_loglevel == "error")
		loglevel_ = error;
	else if (_loglevel == "fatal")
		loglevel_ = fatal;
	else
		loglevel_ = info;
}

} // namespace vsomeip

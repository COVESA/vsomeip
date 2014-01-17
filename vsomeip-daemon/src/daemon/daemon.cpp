//
// daemon.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/program_options.hpp>

#include "daemon-config.h"
#include "daemon.h"

#define VSOMEIP_DAEMON_FOREVER	0x1

namespace options = boost::program_options;

namespace vsomeip {

std::string logLevelToString(Daemon::LogLevel a_logLevel);
Daemon::LogLevel stringToLogLevel(const std::string &a_logLevel);
int stringToInt(const std::string& a_value);

Daemon*
Daemon::getInstance() {
	static Daemon s_daemon;
	return &s_daemon;
}

Daemon::Daemon() {
	m_socket = 0;
	m_port = stringToInt(VSOMEIP_KEY_PORT_DEFAULT);
	m_logLevel = stringToLogLevel(VSOMEIP_KEY_LOGLEVEL_DEFAULT);
	m_isVirtualMode = (std::string("false") == VSOMEIP_KEY_VIRTUALMODE_DEFAULT);
	m_isServiceDiscoveryMode = (std::string("false")
			== VSOMEIP_KEY_SERVICEDISCOVERYMODE_DEFAULT);
}

void Daemon::init(int argc, char **argv) {
	options::variables_map l_commandLineConfiguration;

	try {
		options::options_description l_allowedOptions("Allowed");
		l_allowedOptions.add_options()("help", "Print help message")("config",
				options::value<std::string>()->default_value("vsomeip.conf"),
				"Path to configuration file")("loglevel",
				options::value<std::string>(),
				"Log level to be used by vsomeip daemon")("port",
				options::value<int>(), "IP port to be used by vsomeip daemon")(
				"service_discovery", options::value<bool>(),
				"Enable/Disable service discovery by vsomeip daemon")(
				"virtual_mode", options::value<bool>(),
				"Enable/Disable virtual mode by vsomeip daemon");

		options::store(
				options::parse_command_line(argc, argv, l_allowedOptions),
				l_commandLineConfiguration);
		options::notify(l_commandLineConfiguration);
	}

	catch (...) {
		log(LogLevel::Warn, "Command line configuration (partly) invalid.");
	}

	std::string l_configurationFilePath(VSOMEIP_DEFAULT_CONFIGURATION_FILE);
	if (l_commandLineConfiguration.count("config")) {
		l_configurationFilePath = l_commandLineConfiguration["config"].as<
				std::string>();
	}

	std::stringstream l_logMessage;
	l_logMessage << "Using configuration file \"" << l_configurationFilePath
			<< "\"";
	log(LogLevel::Info, l_logMessage.str());

	options::variables_map l_configuration;
	options::options_description l_configurationDescription;
	l_configurationDescription.add_options()("someip.daemon.loglevel",
			options::value<std::string>(),
			"Log level to be used by vsomeip daemon")("someip.daemon.port",
			options::value<int>(), "IP port to be used by vsomeip daemon")(
			"someip.daemon.service_discovery", options::value<bool>(),
			"Enable service discovery by vsomeip daemon")(
			"someip.daemon.virtual_mode", options::value<bool>(),
			"Enable virtual mode by vsomeip daemon");

	std::ifstream l_configurationFile;

	l_configurationFile.open(l_configurationFilePath.c_str());
	if (l_configurationFile.is_open()) {

		try {
			options::store(
					options::parse_config_file(l_configurationFile,
							l_configurationDescription, true), l_configuration);
			options::notify(l_configuration);
		}

		catch (...) {
			log(LogLevel::Warn,
					"Configuration file settings (partly) invalid.");
		}

		if (l_configuration.count("someip.daemon.loglevel"))
			m_logLevel =
					stringToLogLevel(
							l_configuration["someip.daemon.loglevel"].as<
									std::string>());

		if (l_configuration.count("someip.daemon.port"))
			m_port = l_configuration["someip.daemon.port"].as<int>();

		if (l_configuration.count("someip.daemon.virtual_mode"))
			m_isVirtualMode = l_configuration["someip.daemon.virtual_mode"].as<
					bool>();

		if (l_configuration.count("someip.daemon.service_discovery"))
			m_isServiceDiscoveryMode =
					l_configuration["someip.daemon.service_discovery"].as<bool>();

	} else {
		l_logMessage.str("");
		l_logMessage << "Could not open configuration file \""
				<< l_configurationFilePath << "\"";
		log(LogLevel::Warn, l_logMessage.str());
	}

	// Command line overwrites configuration file
	if (l_commandLineConfiguration.count("loglevel"))
		m_logLevel = stringToLogLevel(
				l_configuration["loglevel"].as<std::string>());

	if (l_commandLineConfiguration.count("port"))
		m_port = l_commandLineConfiguration["port"].as<int>();

	if (l_commandLineConfiguration.count("virtual_mode"))
		m_isVirtualMode = l_commandLineConfiguration["virtual_mode"].as<bool>();

	if (l_commandLineConfiguration.count("service_discovery"))
		m_isServiceDiscoveryMode =
				l_commandLineConfiguration["service_discovery"].as<bool>();
}

void Daemon::start() {
	// Check whether the daemon makes any sense
	if (!m_isVirtualMode && !m_isServiceDiscoveryMode) {
		log(LogLevel::Error,
				"Neither virtual mode nor service discovery mode active, exitting.");
		stop();
	}

	// log configuration
	std::stringstream l_message;
	l_message << "Using port " << m_port;
	log(LogLevel::Info, l_message.str());

	l_message.str("");
	l_message << "Using log level " << logLevelToString(m_logLevel);
	log(LogLevel::Info, l_message.str());

	if (m_isVirtualMode) {
		l_message.str("");
		l_message << "Supporting virtual mode";
		log(LogLevel::Info, l_message.str());
	}

	if (m_isServiceDiscoveryMode) {
		l_message.str("");
		l_message << "Supporting service discovery mode";
		log(LogLevel::Info, l_message.str());
	}

	// create the socket
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (0 > m_socket) {
		log(LogLevel::Error, "Could not open socket!");
		stop();
	}

	// bind it to address & port
	struct sockaddr_in l_serverAddress = { 0 };
	l_serverAddress.sin_family = AF_INET;
	l_serverAddress.sin_addr.s_addr = INADDR_ANY;
	l_serverAddress.sin_port = m_port;
	if (0
			> bind(m_socket, (struct sockaddr *) &l_serverAddress,
					sizeof(l_serverAddress))) {
		log(LogLevel::Error, "Binding to server socket failed!");
		stop();
	}

	listen(m_socket, VSOMEIP_MAX_CLIENTS);

	log(LogLevel::Info, "Some/IP server started.");

	run();
}

void Daemon::stop() {
	log(LogLevel::Info, "Some/IP server stopped.");
	exit(0);
}

///// PRIVATE /////
int Daemon::accept() {
	struct sockaddr_in l_clientAddress;
	socklen_t l_clientLength = sizeof(l_clientAddress);

	return ::accept(m_socket, (struct sockaddr *) &l_clientAddress,
			&l_clientLength);
}

void Daemon::run() {
	while (VSOMEIP_DAEMON_FOREVER) {

		log(LogLevel::Info, "Waiting for data");
		int l_clientSocket = accept();
	}
}

///// Logging /////
void Daemon::log(LogLevel a_logLevel, const std::string& a_message) {
	if (a_logLevel <= m_logLevel) {
		std::cout << logLevelToString(a_logLevel) << " " << a_message
				<< std::endl;
	}
}

///// Private helper functions /////
std::string logLevelToString(Daemon::LogLevel a_logLevel) {
	std::string l_logLevel;

	switch (a_logLevel) {

	case Daemon::LogLevel::Fatal:
		l_logLevel = "[FATAL]";
		break;

	case Daemon::LogLevel::Error:
		l_logLevel = "[ERROR]";
		break;

	case Daemon::LogLevel::Warn:
		l_logLevel = "[WARN]";
		break;

	case Daemon::LogLevel::Info:
		l_logLevel = "[INFO]";
		break;

	case Daemon::LogLevel::Debug:
		l_logLevel = "[DEBUG]";
		break;

	case Daemon::LogLevel::Verbose:
		l_logLevel = "[VERBOSE]";
		break;

	default:
		l_logLevel = "[UNKNOWN]";
	}

	return l_logLevel;
}

//
// Helper functions
//
Daemon::LogLevel stringToLogLevel(const std::string& a_logLevel) {
	Daemon::LogLevel l_logLevel = Daemon::LogLevel::Unknown;

	std::string l_tmpLogLevel(a_logLevel);

	std::transform(l_tmpLogLevel.begin(), l_tmpLogLevel.end(),
			l_tmpLogLevel.begin(), ::toupper);

	if (l_tmpLogLevel == "FATAL") {
		l_logLevel = Daemon::LogLevel::Fatal;
	} else if (l_tmpLogLevel == "ERROR") {
		l_logLevel = Daemon::LogLevel::Error;
	} else if (l_tmpLogLevel == "WARN") {
		l_logLevel = Daemon::LogLevel::Warn;
	} else if (l_tmpLogLevel == "INFO") {
		l_logLevel = Daemon::LogLevel::Info;
	} else if (l_tmpLogLevel == "DEBUG") {
		l_logLevel = Daemon::LogLevel::Debug;
	} else if (l_tmpLogLevel == "VERBOSE") {
		l_logLevel = Daemon::LogLevel::Verbose;
	}

	return l_logLevel;
}

int stringToInt(const std::string& a_value) {
	std::stringstream l_converter;
	int l_value;

	l_converter << a_value;
	l_converter >> l_value;

	return l_value;
}

} // namespace vsomeip


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

#include <algorithm>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include <vsomeip/service.hpp>
#include <vsomeip/internal/endpoint_impl.hpp>
#include <vsomeip/internal/tcp_service_impl.hpp>
#include <vsomeip/internal/udp_service_impl.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/entry.hpp>
#include <vsomeip/service_discovery/eventgroup_entry.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/service_discovery/internal/daemon-config.hpp>
#include <vsomeip/service_discovery/internal/daemon.hpp>
#include <vsomeip/service_discovery/internal/service_registration.hpp>

namespace options = boost::program_options;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace expressions = boost::log::expressions;

using namespace boost::log::trivial;

#define SERVICE_DISCOVERY_SERVICE_ID 0xFFFF
#define SERVICE_DISCOVERY_METHOD_ID  0x8100

namespace vsomeip {
namespace service_discovery {

severity_level loglevel_from_string(const std::string &_loglevel);

daemon*
daemon::get_instance() {
	static daemon daemon__;
	return &daemon__;
}

daemon::daemon() {
	port_ = VSOMEIP_VALUE_PORT_DEFAULT;
	loglevel_ = info;
	is_virtual_mode_ = VSOMEIP_VALUE_IS_VIRTUAL_MODE_DEFAULT;
	is_service_discovery_mode_
		= VSOMEIP_VALUE_IS_SERVICE_DISCOVERY_MODE_DEFAULT;
}

void daemon::init(int argc, char **argv) {
	// logging first
	logging::add_common_attributes();

	auto daemon_log_format
		= expressions::stream <<
		  	  expressions::format_date_time< boost::posix_time::ptime >(
		  		  "TimeStamp", "%Y-%m-%d %H:%M:%S.%f") <<
		  	  " [" <<
		  	  expressions::attr< severity_level >("Severity") <<
		  	  "] " <<
		  	  expressions::smessage;

	logging::add_console_log(
			std::clog,
			keywords::filter = severity >= loglevel_,
			keywords::format = daemon_log_format
	);

	logging::add_file_log(
			keywords::file_name = "vsomeipd_%N.log",
			keywords::rotation_size = 10 * 1024 * 1024,
			keywords::time_based_rotation
				= logging::sinks::file::rotation_at_time_point(0, 0, 0),
			keywords::filter = severity >= loglevel_,
			keywords::format = daemon_log_format
	);

	boost::log::core::get()->set_filter(severity >= info);

	options::variables_map commandline_configuration;

	try {
		options::options_description valid_options("allowed");
		valid_options.add_options()("help", "Print help message")("config",
				options::value<std::string>()->default_value("vsomeipd.conf"),
				"Path to configuration file")("loglevel",
				options::value<std::string>(),
				"Log level to be used by vsomeip daemon")("port",
				options::value<int>(), "IP port to be used by vsomeip daemon")(
				"service_discovery_enabled", options::value<bool>(),
				"Enable/Disable service discovery by vsomeip daemon")(
				"virtual_mode_enabled", options::value<bool>(),
				"Enable/Disable virtual mode by vsomeip daemon");

		options::store(
				options::parse_command_line(argc, argv, valid_options),
				commandline_configuration);
		options::notify(commandline_configuration);
	}

	catch (...) {
		BOOST_LOG_SEV(log_, error)
				<< "Command line configuration (partly) invalid.";
	}

	std::string configuration_path(VSOMEIP_DEFAULT_CONFIGURATION_FILE);
	if (commandline_configuration.count("config")) {
		configuration_path
			= commandline_configuration["config"].as< std::string >();
	}

	BOOST_LOG_SEV(log_, info) << "Using configuration file \""
							   << configuration_path << "\"";

	options::variables_map configuration;
	options::options_description configuration_description;
	configuration_description.add_options()("someip.daemon.loglevel",
			options::value<std::string>(),
			"Log level to be used by vsomeip daemon")("someip.daemon.port",
			options::value<int>(), "IP port to be used by vsomeip daemon")(
			"someip.daemon.service_discovery_enabled", options::value<bool>(),
			"Enable service discovery by vsomeip daemon")(
			"someip.daemon.virtual_mode_enabled", options::value<bool>(),
			"Enable virtual mode by vsomeip daemon");

	std::ifstream configuration_file;

	configuration_file.open(configuration_path.c_str());
	if (configuration_file.is_open()) {

		try {
			options::store(options::parse_config_file(
								configuration_file,
								configuration_description, true),
						   configuration);
			options::notify(configuration);
		}

		catch (...) {
			BOOST_LOG_SEV(log_, error)
					<< "Command line configuration (partly) invalid.";
		}

		if (configuration.count("someip.daemon.loglevel"))
			loglevel_ = loglevel_from_string(
							configuration["someip.daemon.loglevel"].
								as< std::string>());

		if (configuration.count("someip.daemon.port"))
			port_ = configuration["someip.daemon.port"].as< int >();

		if (configuration.count("someip.daemon.virtual_mode_enabled"))
			is_virtual_mode_
				= configuration["someip.daemon.virtual_mode_enabled"].as< bool >();

		if (configuration.count("someip.daemon.service_discovery_enabled"))
			is_service_discovery_mode_
				= configuration["someip.daemon.service_discovery_enabled"].as< bool >();

	} else {
		BOOST_LOG_SEV(log_, error)
				<< "Could not open configuration file \""
				<< configuration_path << "\"";
	}

	// Command line overwrites configuration file
	if (commandline_configuration.count("log_level"))
	loglevel_ = loglevel_from_string(
					configuration["someip.daemon.loglevel"].
						as< std::string>());

	if (commandline_configuration.count("port"))
		port_ = commandline_configuration["port"].as<int>();

	if (commandline_configuration.count("virtual_mode_enabled"))
		is_virtual_mode_ = commandline_configuration["virtual_mode_enabled"].as<bool>();

	if (commandline_configuration.count("service_discovery_enabled"))
		is_service_discovery_mode_ =
				commandline_configuration["service_discovery_enabled"].as<bool>();
}

void daemon::start() {
	// Check whether the daemon makes any sense
	if (!is_virtual_mode_ && !is_service_discovery_mode_) {
		BOOST_LOG_SEV(log_, error)
				<< "Neither virtual mode nor service discovery "
				   "mode active, exiting.";
		stop();
	} else {
		BOOST_LOG_SEV(log_, info) << "Using port " << port_;
		//BOOST_LOG_DEV(log_, info) << "Using log level " << _log_level;

		if (is_virtual_mode_) {
			BOOST_LOG_SEV(log_, info) << "Virtual mode: on";
		}

		if (is_service_discovery_mode_) {
			BOOST_LOG_SEV(log_, info) << "Service discovery mode: on";
		}

		// create a UDP & a TCP service as we want to listen to a port and
		// send/receive Some/IP messages
		endpoint *udp_endpoint = new endpoint_impl("127.0.0.1", port_,
										ip_protocol::UDP, ip_version::V4);

		endpoint *tcp_endpoint = new endpoint_impl("127.0.0.1", port_,
										ip_protocol::TCP, ip_version::V4);

		factory *default_factory = factory::get_default_factory();

		// DEMO
		registry_.add(0x2233, 0x12);

		udp_daemon_
			= new udp_service_impl(default_factory, udp_endpoint, is_);
		tcp_daemon_
			= new tcp_service_impl(default_factory, tcp_endpoint, is_);

		tcp_daemon_->register_for(this,
								 SERVICE_DISCOVERY_SERVICE_ID,
								 SERVICE_DISCOVERY_METHOD_ID);
		udp_daemon_->register_for(this,
								 SERVICE_DISCOVERY_SERVICE_ID,
				 	 	 	 	 SERVICE_DISCOVERY_METHOD_ID);

		udp_daemon_->start();
		tcp_daemon_->start();

		boost::thread io_thread(boost::bind(&daemon::run_service, this));

		is_running_ = true;

		io_thread.join();
	}
}

void daemon::stop() {
	is_running_ = false;
}

void daemon::run_service() {
	while (is_running_) {
		BOOST_LOG_SEV(log_, info) << "(Re-)Starting service";
		is_.run();
	}
}

void daemon::receive(const message_base *_message) {
	const message *requests = dynamic_cast< const message * > (_message);
	if (0 != requests) {
		const std::vector<entry *>& entries = requests->get_entries();
		for (auto e : entries) {
			consume_request(*e, _message->get_endpoint());
		}
	} else {
		BOOST_LOG_SEV(log_, warning)
			<< "Received unstructued Some/IP service discovery message!";
	}
}

void daemon::consume_request(const entry &_entry, endpoint *_target) {
	if (_entry.get_type() == entry_type::FIND_SERVICE) {
		const service_entry& s = reinterpret_cast<const service_entry &>(_entry);
		registry::service *found = registry_.search(s.get_service_id(), s.get_instance_id());

		if (found) {
			send_offer_service(found, _target);
		} else {
			BOOST_LOG_SEV(log_, warning)
				<< "Service " << std::hex
				<< (int)s.get_service_id()
				<< "."
				<< (int)s.get_instance_id()
				<< " not available!";
		}
	}
}

void daemon::send_offer_service(registry::service *_service, endpoint *_target) {
	boost::shared_ptr<message> m(
		factory::get_default_factory()->create_service_discovery_message());
	m->set_endpoint(_target);
	service_entry& s = m->create_service_entry();
	s.set_type(entry_type::OFFER_SERVICE);
	s.set_service_id(_service->service_id_);
	s.set_instance_id(_service->instance_id_);
	udp_daemon_->send(m.get());
}

severity_level loglevel_from_string(const std::string &_loglevel) {

	if (_loglevel == "info")
		return info;

	if (_loglevel == "debug")
		return debug;

	if (_loglevel == "trace")
		return trace;

	if (_loglevel == "warning")
		return warning;

	if (_loglevel == "error")
		return error;

	if (_loglevel == "fatal")
		return fatal;

	return info;
}

} // namespace service_discovery
} // namespace vsomeip


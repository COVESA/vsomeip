//
// configuration.cpp
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

#include <vsomeip/service_discovery/config.hpp>
#include <vsomeip/service_discovery/internal/configuration_impl.hpp>

using namespace boost::log::trivial;
namespace options = boost::program_options;

namespace vsomeip {
namespace service_discovery {

configuration * configuration_impl::get_instance() {
	static configuration_impl the_configuration;
	return &the_configuration;
}

configuration_impl::configuration_impl()
	: use_service_discovery_(VSOMEIP_DEFAULT_DEAMON_SERVICE_DISCOVERY_ENABLED),
	  use_virtual_mode_(VSOMEIP_DEFAULT_DEAMON_VIRTUAL_MODE_ENABLED),
	  registry_path_(VSOMEIP_DEFAULT_DEAMON_REGISTRY_PATH),
	  unicast_address_("127.0.0.1"),
	  multicast_address_("224.223.222.221"),
	  port_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_PORT),
	  min_initial_delay_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_MIN_INITIAL_DELAY),
	  max_initial_delay_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_MAX_INITIAL_DELAY),
	  repetition_base_delay_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_REPETITION_BASE_DELAY),
	  repetition_max_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_REPETITION_MAX),
	  ttl_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_TIME_TO_LIVE),
	  cyclic_offer_delay_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_CYCLIC_OFFER_DELAY),
	  cyclic_request_delay_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_CYCLIC_REQUEST_DELAY),
	  request_response_delay_(VSOMEIP_DEFAULT_SERVICE_DISCOVERY_REQUEST_RESPONSE_DELAY) {
}

configuration_impl::~configuration_impl() {
}

void configuration_impl::read() {
	static bool has_read = false;
	if (!has_read) {
		vsomeip::configuration_impl::read();

		options::variables_map configuration;
		options::options_description configuration_description;
		configuration_description.add_options()(
				"someip.daemon.service_discovery_enabled", options::value<bool>(), "Enable service discovery by vsomeip daemon")(
				"someip.daemon.virtual_mode_enabled", options::value<bool>(), "Enable virtual mode by vsomeip daemon")(
				"someip.daemon.registry", options::value<std::string>(), "Access point to internal registry")(
				"someip.service_discovery.port", options::value<int>(), "Service discovery port")(
				"someip.service_discovery.min_initial_delay", options::value<int>(), "Minimum initial delay before first offer message")(
				"someip.service_discovery.max_initial_delay", options::value<int>(), "Maximum initial delay before first offer message")(
				"someip.service_discovery.repetition_base_delay", options::value<int>(), "Base delay for the repetition phase.")(
				"someip.service_discovery.repetition_max", options::value<int>(), "Maximum number of repetitions in the repetition phase.")(
				"someip.service_discovery.ttl", options::value<int>(), "Lifetime of service discovery entry")(
				"someip.service_discovery.cyclic_offer_delay", options::value<int>(), "Cycle of the OfferService messages in the main phase")(
				"someip.service_discovery.cyclic_request_delay", options::value<int>(), "Cycle of the FindService messages in the main phase")(
				"someip.service_discovery.request_response_delay", options::value<int>(), "Delay of an unicast answer to a multicast message.");

		std::ifstream configuration_file;
		configuration_file.open(configuration_file_path_.c_str());
		if (configuration_file.is_open()) {

			try {
				options::store(options::parse_config_file(
									configuration_file,
									configuration_description, true),
							   configuration);
				options::notify(configuration);
			}

			catch (...) {
				// Intentionally left blank
			}

			// Daemon
			if (configuration.count("someip.daemon.service_discovery_enabled"))
				use_service_discovery_
					= configuration["someip.daemon.service_discovery_enabled"].as< bool >();

			if (configuration.count("someip.daemon.virtual_mode_enabled"))
				use_virtual_mode_
					= configuration["someip.daemon.virtual_mode_enabled"].as< bool >();

			if (configuration.count("someip.daemon.registry"))
				registry_path_
					= configuration["someip.daemon.registry"].as< std::string >();

			// Service Discovery
			if (configuration.count("someip.service_discovery.unicast_address"))
				unicast_address_ = configuration["someip.service_discovery.unicast_address"].as< std::string >();

			if (configuration.count("someip.service_discovery.multicast_address"))
				multicast_address_ = configuration["someip.service_discovery.multicast_address"].as< std::string >();

			if (configuration.count("someip.service_discovery.port"))
				port_ = configuration["someip.service_discovery.port"].as< int >();

			if (configuration.count("someip.service_discovery.min_initial_delay"))
				min_initial_delay_
					= configuration["someip.service_discovery.min_initial_delay"].as< int >();

			if (configuration.count("someip.service_discovery.max_initial_delay"))
				max_initial_delay_
					= configuration["someip.service_discovery.max_initial_delay"].as< int >();

			if (configuration.count("someip.service_discovery.repetition_base_delay"))
				repetition_base_delay_
					= configuration["someip.service_discovery.repetition_base_delay"].as< int >();

			if (configuration.count("someip.service_discovery.repetition_max"))
				repetition_max_
					= configuration["someip.service_discovery.repetition_max"].as< int >();

			if (configuration.count("someip.service_discovery.cyclic_offer_delay"))
				cyclic_offer_delay_
					= configuration["someip.service_discovery.cyclic_offer_delay"].as< int >();

			if (configuration.count("someip.service_discovery.cyclic_request_delay"))
				cyclic_request_delay_
					= configuration["someip.service_discovery.cyclic_request_delay"].as< int >();

			if (configuration.count("someip.service_discovery.request_response_delay"))
				request_response_delay_
					= configuration["someip.service_discovery.request_response_delay"].as< int >();
		}
	}
}


bool configuration_impl::use_service_discovery() const {
	return use_service_discovery_;
}

bool configuration_impl::use_virtual_mode() const {
	return use_virtual_mode_;
}

const std::string & configuration_impl::get_registry_path() const {
	return registry_path_;
}

const std::string & configuration_impl::get_unicast_address() const {
	return unicast_address_;
}
const std::string & configuration_impl::get_multicast_address() const {
	return multicast_address_;
}

uint16_t configuration_impl::get_port() const {
	return port_;
}

uint32_t configuration_impl::get_min_initial_delay() const {
	return min_initial_delay_;
}

uint32_t configuration_impl::get_max_initial_delay() const {
	return max_initial_delay_;
}

uint32_t configuration_impl::get_repetition_base_delay() const {
	return repetition_base_delay_;
}

uint8_t configuration_impl::get_repetition_max() const {
	return repetition_max_;
}

uint32_t configuration_impl::get_ttl() const {
	return ttl_;
}

uint32_t configuration_impl::get_cyclic_offer_delay() const {
	return cyclic_offer_delay_;
}

uint32_t configuration_impl::get_cyclic_request_delay() const {
	return cyclic_request_delay_;
}

uint32_t configuration_impl::get_request_response_delay() const {
	return request_response_delay_;
}

} // namespace service_discovery
} // namespace vsomeip

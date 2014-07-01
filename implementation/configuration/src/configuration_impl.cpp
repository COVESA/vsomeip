// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fstream>
#include <map>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <vsomeip/constants.hpp>

#include "../include/configuration_impl.hpp"
#include "../include/servicegroup.hpp"
#include "../include/service.hpp"
#include "../../logging/include/logger_impl.hpp"

namespace vsomeip {
namespace cfg {

std::map< std::string, configuration * > configuration_impl::the_configurations;
std::mutex configuration_impl::mutex_;

configuration * configuration_impl::get(const std::string &_path) {
	configuration *its_configuration(0);

	bool is_freshly_loaded(false);
	{
		std::unique_lock< std::mutex > its_lock(mutex_);

		auto found_configuration = the_configurations.find(_path);
		if (found_configuration != the_configurations.end()) {
			its_configuration = found_configuration->second;
		} else {
			its_configuration = new configuration_impl;
			if (its_configuration->load(_path)) {
				the_configurations[_path] = its_configuration;
				is_freshly_loaded = true;
			} else {
				delete its_configuration;
				its_configuration = 0;
			}
		}
	}

	if (is_freshly_loaded)
		logger_impl::init(_path);

	return its_configuration;
}

configuration_impl::configuration_impl()
	: has_console_log_(true),
	  has_file_log_(false),
	  has_dlt_log_(false),
	  logfile_("/tmp/vsomeip.log"),
	  loglevel_(boost::log::trivial::severity_level::info),
	  routing_host_("vsomeipd"),
	  is_service_discovery_enabled_(false),
	  service_discovery_host_("vsomeipd") {

	address_ = address_.from_string("127.0.0.1");
}

configuration_impl::~configuration_impl() {
}

bool configuration_impl::load(const std::string &_path) {
	bool is_loaded(true);
	boost::property_tree::ptree its_tree;

	try {
		boost::property_tree::xml_parser::read_xml(_path, its_tree);

		auto its_someip = its_tree.get_child("someip");

		// Read the configuration data
		is_loaded = get_someip_configuration(its_someip);
		is_loaded = get_logging_configuration(its_someip);
		is_loaded = is_loaded && get_services_configuration(its_someip);
		is_loaded = is_loaded && get_routing_configuration(its_someip);
		is_loaded = is_loaded && get_service_discovery_configuration(its_someip);
		is_loaded = is_loaded && get_applications_configuration(its_someip);
	}
	catch (...) {
		is_loaded = false;
	}

	return is_loaded;
}

bool configuration_impl::get_someip_configuration(boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		std::string its_value = _tree.get< std::string >("address");
		address_ = address_.from_string(its_value);
	}
	catch (...) {
		is_loaded = false;
	}
	return is_loaded;
}

bool configuration_impl::get_logging_configuration(boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		auto its_logging = _tree.get_child("logging");
		for (auto i = its_logging.begin(); i != its_logging.end(); ++i) {
			std::string its_key(i->first);
			std::string its_value(i->second.data());

			if (its_key == "console") {
				has_console_log_ = (its_value == "true");
			} else if (its_key == "file") {
				try {
					auto its_file_properties = i->second;
					for (auto j : its_file_properties) {
						std::string its_file_key(j.first);
						std::string its_file_value(j.second.data());

						if (its_file_key == "enabled") {
							has_file_log_ = (its_file_value == "true");
						} else if (its_file_key == "path") {
							logfile_ = its_file_value;
						}
					}
				}
				catch (...) {
				}
			} else if (its_key == "dlt") {
				has_dlt_log_ = (its_value == "true");
			} else if (its_key == "level") {
				loglevel_ = (its_value == "trace" ? boost::log::trivial::severity_level::trace
							 : (its_value == "debug" ? boost::log::trivial::severity_level::debug
							    : (its_value == "info" ? boost::log::trivial::severity_level::info
							       : (its_value == "warning" ? boost::log::trivial::severity_level::warning
							          : (its_value == "error" ? boost::log::trivial::severity_level::error
							             : (its_value == "fatal" ? boost::log::trivial::severity_level::fatal
							                : boost::log::trivial::severity_level::info))))));
			}
		}
	}
	catch (...) {
		is_loaded = false;
	}
	return is_loaded;
}

bool configuration_impl::get_services_configuration(boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		auto its_services = _tree.get_child("services");
		for (auto i = its_services.begin(); i != its_services.end(); ++i) {
			std::string its_key(i->first);

			if (its_key == "servicegroup") {
				is_loaded = is_loaded && get_servicegroup_configuration(i->second);
			}
		}
	}
	catch (...) {
		is_loaded = false;
	}
	return is_loaded;
}

bool configuration_impl::get_servicegroup_configuration(const boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		boost::shared_ptr< servicegroup > its_servicegroup(new servicegroup);
		its_servicegroup->address_ = "local"; // Default
		for (auto i = _tree.begin(); i != _tree.end(); ++i) {
			std::string its_key(i->first);

			if (its_key == "name") {
				its_servicegroup->name_ = i->second.data();
			} else if (its_key == "address") {
				its_servicegroup->address_ = i->second.data();
			} else if (its_key == "delays") {
				is_loaded = is_loaded && get_delays_configuration(its_servicegroup, i->second);
			} else if (its_key == "service") {
				is_loaded = is_loaded && get_service_configuration(its_servicegroup, i->second);
			}
		}
	}
	catch (...) {
		is_loaded = false;
	}
	return is_loaded;
}

bool configuration_impl::get_delays_configuration(boost::shared_ptr< servicegroup > &_group, const boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		std::stringstream its_converter;

		for (auto i = _tree.begin(); i != _tree.end(); ++i) {
			std::string its_key(i->first);

			if (its_key == "initial") {
				_group->initial_delay_min_ = i->second.get< uint32_t >("min");
				_group->initial_delay_max_ = i->second.get< uint32_t >("max");
			} else if (its_key == "repetition-base") {
				its_converter << std::dec << i->second.data();
				its_converter >> _group->repetition_base_delay_;
			} else if (its_key == "repetition-max") {
				int tmp_repetition_max;
				its_converter << std::dec << i->second.data();
				its_converter >> tmp_repetition_max;
				_group->repetition_max_ = tmp_repetition_max;
			} else if (its_key == "cyclic-offer") {
				its_converter << std::dec << i->second.data();
				its_converter >> _group->cyclic_offer_delay_;
			} else if (its_key == "cyclic-request") {
				its_converter << std::dec << i->second.data();
				its_converter >> _group->cyclic_request_delay_;
			}

			its_converter.str("");
			its_converter.clear();
		}
	}
	catch (...) {
		is_loaded = false;
	}
	return is_loaded;
}

bool configuration_impl::get_service_configuration(boost::shared_ptr< servicegroup > &_group, const boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		boost::shared_ptr< service > its_service(new service);
		its_service->group_ = _group;

		for (auto i = _tree.begin(); i != _tree.end(); ++i) {
			std::string its_key(i->first);
			std::string its_value(i->second.data());
			std::stringstream its_converter;

			if (its_key == "multicast") {
				its_service->multicast_ = its_value;
			} else if (its_key == "ports") {
				try {
					its_value = i->second.get_child("reliable").data();
					its_converter << its_value;
					its_converter >> its_service->reliable_;
				}
				catch (...) {
					its_service->reliable_ = VSOMEIP_ILLEGAL_PORT;
				}
				try {
					its_converter.str("");
					its_converter.clear();
					its_value = i->second.get_child("unreliable").data();
					its_converter << its_value;
					its_converter >> its_service->unreliable_;
				}
				catch (...) {
					its_service->unreliable_ = VSOMEIP_ILLEGAL_PORT;
				}
			} else {
				// Trim "its_value"
				if (its_value[0] == '0' && its_value[1] == 'x') {
					its_converter << std::hex << its_value;
				} else {
					its_converter << std::dec << its_value;
				}

				if (its_key == "service-id") {
					its_converter >> its_service->service_;
				} else if (its_key == "instance-id") {
					its_converter >> its_service->instance_;
				}
			}
		}

		auto found_service = services_.find(its_service->service_);
		if (found_service != services_.end()) {
			auto found_instance = found_service->second.find(its_service->instance_);
			if (found_instance != found_service->second.end()) {
				is_loaded = false;
			}
		}

		if (is_loaded) {
			services_[its_service->service_][its_service->instance_] = its_service;
		}
	}
	catch (...) {
		is_loaded = false;
	}
	return is_loaded;
}

bool configuration_impl::get_routing_configuration(boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		std::stringstream its_converter;
		auto its_service_discovery = _tree.get_child("routing");
		for (auto i = its_service_discovery.begin(); i != its_service_discovery.end(); ++i) {
			std::string its_key(i->first);
			std::string its_value(i->second.data());

			if (its_key == "host") {
				routing_host_ = its_value;
			}
		}
	}
	catch (...) {
		is_loaded = false;
	}

	return is_loaded;
}

bool configuration_impl::get_service_discovery_configuration(boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		std::stringstream its_converter;
		auto its_service_discovery = _tree.get_child("service-discovery");
		for (auto i = its_service_discovery.begin(); i != its_service_discovery.end(); ++i) {
			std::string its_key(i->first);
			std::string its_value(i->second.data());

			if (its_key == "enabled") {
				is_service_discovery_enabled_ = (its_value == "true");
			} else if (its_key == "host") {
				service_discovery_host_ = its_value;
			} else if (its_key == "protocol") {
				service_discovery_protocol_ = its_value;
			} else if (its_key == "port") {
				its_converter << its_value;
				its_converter >> service_discovery_port_;
				its_converter.str("");
				its_converter.clear();
			}
		}
	}
	catch (...) {
		is_loaded = false;
	}

	return is_loaded;
}

bool configuration_impl::get_applications_configuration(boost::property_tree::ptree &_tree) {
	bool is_loaded(true);
	try {
		std::stringstream its_converter;
		auto its_applications = _tree.get_child("applications");
		for (auto i = its_applications.begin(); i != its_applications.end(); ++i) {
			std::string its_key(i->first);
			if (its_key == "application") {
				is_loaded = is_loaded && get_application_configuration(i->second);
			}
		}
	}
	catch (...) {
		is_loaded = false;
	}

	return is_loaded;
}

bool configuration_impl::get_application_configuration(const boost::property_tree::ptree &_tree) {
	bool is_loaded(true);

	std::string its_name("");
	client_t its_id;

	for (auto i = _tree.begin(); i != _tree.end(); ++i) {
		std::string its_key(i->first);
		std::string its_value(i->second.data());
		std::stringstream its_converter;

		if (its_key == "name") {
			its_name = its_value;
		} else if (its_key == "id") {
			if (its_value[0] == '0' && its_value[1] == 'x') {
				its_converter << std::hex << its_value;
			} else {
				its_converter << std::dec << its_value;
			}
			its_converter >> its_id;
		}
	}

	if (its_name != "" && its_id != 0) {
		applications_[its_name] = its_id;
	}

	return is_loaded;
}

// Public interface
const boost::asio::ip::address & configuration_impl::get_address() const {
	return address_;
}

bool configuration_impl::is_v4() const {
	return address_.is_v4();
}

bool configuration_impl::is_v6() const {
	return address_.is_v6();
}

bool configuration_impl::has_console_log() const {
	return has_console_log_;
}

bool configuration_impl::has_file_log() const {
	return has_file_log_;
}

bool configuration_impl::has_dlt_log() const {
	return has_dlt_log_;
}

const std::string & configuration_impl::get_logfile() const {
	return logfile_;
}

boost::log::trivial::severity_level configuration_impl::get_loglevel() const {
	return loglevel_;
}

uint32_t configuration_impl::get_min_initial_delay(service_t _service, instance_t _instance) const {
	uint32_t its_delay = 0;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_delay = its_service->group_->initial_delay_min_;

	return its_delay;
}

uint32_t configuration_impl::get_max_initial_delay(service_t _service, instance_t _instance) const {
	uint32_t its_delay = 0xFFFFFFFF;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_delay = its_service->group_->initial_delay_max_;

	return its_delay;
}

uint32_t configuration_impl::get_repetition_base_delay(service_t _service, instance_t _instance) const {
	uint32_t its_delay = 0xFFFFFFFF;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_delay = its_service->group_->repetition_base_delay_;

	return its_delay;
}

uint8_t configuration_impl::get_repetition_max(service_t _service, instance_t _instance) const {
	uint8_t its_max = 0;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_max = its_service->group_->repetition_max_;

	return its_max;
}

uint32_t configuration_impl::get_cyclic_offer_delay(service_t _service, instance_t _instance) const {
	uint32_t its_delay = 0xFFFFFFFF;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_delay = its_service->group_->cyclic_offer_delay_;

	return its_delay;

}

uint32_t configuration_impl::get_cyclic_request_delay(service_t _service, instance_t _instance) const {
	uint32_t its_delay = 0xFFFFFFFF;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_delay = its_service->group_->cyclic_request_delay_;

	return its_delay;
}

std::string configuration_impl::get_address(service_t _service, instance_t _instance) const {
	std::string its_address("");

	service *its_service = find_service(_service, _instance);
	if (its_service)
		its_address = its_service->group_->address_;

	return its_address;
}

uint16_t configuration_impl::get_reliable_port(service_t _service, instance_t _instance) const {
	uint16_t its_reliable = VSOMEIP_ILLEGAL_PORT;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_reliable = its_service->reliable_;

	return its_reliable;
}

uint16_t configuration_impl::get_unreliable_port(service_t _service, instance_t _instance) const {
	uint16_t its_unreliable = VSOMEIP_ILLEGAL_PORT;

	service *its_service = find_service(_service, _instance);
	if (its_service) its_unreliable = its_service->unreliable_;

	return its_unreliable;
}

std::string configuration_impl::get_multicast(service_t _service, instance_t _instance) const {
	std::string its_multicast(VSOMEIP_DEFAULT_MULTICAST);

	service *its_service = find_service(_service, _instance);
	if (its_service) its_multicast = its_service->multicast_;

	return its_multicast;
}

const std::string & configuration_impl::get_routing_host() const {
	return routing_host_;
}

bool configuration_impl::is_service_discovery_enabled() const {
	return is_service_discovery_enabled_;
}

const std::string & configuration_impl::get_service_discovery_host() const {
	return service_discovery_host_;
}

const std::string & configuration_impl::get_service_discovery_protocol() const {
	return service_discovery_protocol_;
}

uint16_t configuration_impl::get_service_discovery_port() const {
	return service_discovery_port_;
}

client_t configuration_impl::get_id(const std::string &_name) const {
	client_t its_client = 0;

	auto found_application = applications_.find(_name);
	if (found_application != applications_.end()) {
		its_client = found_application->second;
	}

	return its_client;
}

std::set< std::pair< service_t, instance_t > > configuration_impl::get_remote_services() const {
	std::set< std::pair< service_t, instance_t > > its_remote_services;
	for (auto i : services_) {
		for (auto j : i.second) {
			if (j.second->group_->address_ != "local")
				its_remote_services.insert(std::make_pair(i.first, j.first));
		}
	}
	return its_remote_services;
}

service *configuration_impl::find_service(service_t _service, instance_t _instance) const {
	service *its_service = 0;
	auto find_service = services_.find(_service);
	if (find_service != services_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			its_service = find_instance->second.get();
		}
	}
	return its_service;
}

} // namespace config
} // namespace vsomeip

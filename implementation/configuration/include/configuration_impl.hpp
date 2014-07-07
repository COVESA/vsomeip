// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_CONFIGURATION_IMPL_HPP
#define VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>

#include <boost/property_tree/ptree.hpp>

#include <vsomeip/configuration.hpp>

namespace vsomeip {
namespace cfg {

class service;
class servicegroup;

class configuration_impl : public configuration {
public:
	static configuration * get(const std::string &_path);

	configuration_impl();
	virtual ~configuration_impl();

	bool load(const std::string &_path);

	const boost::asio::ip::address & get_address() const;
	bool is_v4() const;
	bool is_v6() const;

	bool has_console_log() const;
	bool has_file_log() const;
	bool has_dlt_log() const;
	const std::string & get_logfile() const;
	boost::log::trivial::severity_level get_loglevel() const;

	std::string get_address(service_t _service, instance_t _instance) const;
	uint16_t get_reliable_port(service_t _service, instance_t _instance) const;
	uint16_t get_unreliable_port(service_t _service, instance_t _instance) const;
	std::string get_multicast(service_t _service, instance_t _instance) const;

	std::string get_group(service_t _service, instance_t _instance) const;

	uint32_t get_min_initial_delay(const std::string &_name) const;
	uint32_t get_max_initial_delay(const std::string &_name) const;
	uint32_t get_repetition_base_delay(const std::string &_name) const;
	uint8_t get_repetition_max(const std::string &_name) const;
	uint32_t get_cyclic_offer_delay(const std::string &_name) const;
	uint32_t get_cyclic_request_delay(const std::string &_name) const;

	const std::string & get_routing_host() const;

	bool is_service_discovery_enabled() const;
	const std::string & get_service_discovery_protocol() const;
	uint16_t get_service_discovery_port() const;

	client_t get_id(const std::string &_name) const;

	std::set< std::pair< service_t, instance_t > > get_remote_services() const;

private:
	bool get_someip_configuration(boost::property_tree::ptree &_tree);
	bool get_logging_configuration(boost::property_tree::ptree &_tree);
	bool get_services_configuration(boost::property_tree::ptree &_tree);
	bool get_routing_configuration(boost::property_tree::ptree &_tree);
	bool get_service_discovery_configuration(boost::property_tree::ptree &_tree);
	bool get_applications_configuration(boost::property_tree::ptree &_tree);

	bool get_servicegroup_configuration(const boost::property_tree::ptree &_tree);
	bool get_delays_configuration(std::shared_ptr< servicegroup > &_group, const boost::property_tree::ptree &_tree);
	bool get_service_configuration(std::shared_ptr< servicegroup > &_group, const boost::property_tree::ptree &_tree);
	bool get_application_configuration(const boost::property_tree::ptree &_tree);

	servicegroup * find_servicegroup(const std::string &_name) const;
	service * find_service(service_t _service, instance_t _instance) const;

private:
	static std::map< std::string, configuration * > the_configurations;
	static std::mutex mutex_;

	// Configuration data
	boost::asio::ip::address address_;

	bool has_console_log_;
	bool has_file_log_;
	bool has_dlt_log_;
	std::string logfile_;
	boost::log::trivial::severity_level loglevel_;

	std::map< std::string, std::shared_ptr< servicegroup > > servicegroups_;
	std::map< service_t, std::map< instance_t, std::shared_ptr< service > > > services_;

	std::string routing_host_;

	bool is_service_discovery_enabled_;
	std::string service_discovery_host_;
	std::string service_discovery_protocol_;
	uint16_t service_discovery_port_;

	std::map< std::string, client_t > applications_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

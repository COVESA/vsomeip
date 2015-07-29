// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

struct service;
struct servicegroup;

class configuration_impl: public configuration {
public:
    static configuration * get(const std::string &_path);

    configuration_impl();
    virtual ~configuration_impl();

    bool load(const std::string &_path);

    const boost::asio::ip::address & get_unicast() const;
    bool is_v4() const;
    bool is_v6() const;

    bool has_console_log() const;
    bool has_file_log() const;
    bool has_dlt_log() const;
    const std::string & get_logfile() const;
    boost::log::trivial::severity_level get_loglevel() const;

    std::string get_group(service_t _service, instance_t _instance) const;
    std::set<std::string> get_servicegroups() const;

    bool is_local_servicegroup(const std::string &_name) const;
    int32_t get_min_initial_delay(const std::string &_name) const;
    int32_t get_max_initial_delay(const std::string &_name) const;
    int32_t get_repetition_base_delay(const std::string &_name) const;
    uint8_t get_repetition_max(const std::string &_name) const;
    int32_t get_cyclic_offer_delay(const std::string &_name) const;
    int32_t get_cyclic_request_delay(const std::string &_name) const;

    std::string get_unicast(service_t _service, instance_t _instance) const;
    std::string get_multicast_address(service_t _service,
            instance_t _instance) const;
    uint16_t get_multicast_port(service_t _service,
            instance_t _instance) const;
    uint16_t get_multicast_group(service_t _service,
            instance_t _instance) const;

    uint16_t get_reliable_port(service_t _service, instance_t _instance) const;
    bool has_enabled_magic_cookies(std::string _address, uint16_t _port) const;
    uint16_t get_unreliable_port(service_t _service,
            instance_t _instance) const;

    const std::string & get_routing_host() const;

    bool is_service_discovery_enabled() const;
    const std::string & get_service_discovery_multicast() const;
    uint16_t get_service_discovery_port() const;
    const std::string & get_service_discovery_protocol() const;

    client_t get_id(const std::string &_name) const;
    std::size_t get_num_dispatchers(const std::string &_name) const;

    std::set<std::pair<service_t, instance_t> > get_remote_services() const;

    std::map<service_t,
            std::map<instance_t,
                std::map<eventgroup_t,
                    std::set<event_t> > > > get_eventgroups() const;
    std::map<service_t,
        std::map<instance_t,
            std::set<event_t> > > get_events() const;
    void set_event(std::shared_ptr<event> &_event) const;

private:
    bool get_someip_configuration(boost::property_tree::ptree &_tree);
    bool get_logging_configuration(boost::property_tree::ptree &_tree);
    bool get_services_configuration(boost::property_tree::ptree &_tree);
    bool get_routing_configuration(boost::property_tree::ptree &_tree);
    bool get_service_discovery_configuration(
            boost::property_tree::ptree &_tree);
    bool get_applications_configuration(boost::property_tree::ptree &_tree);

    bool get_servicegroup_configuration(
            const boost::property_tree::ptree &_tree);
    bool get_delays_configuration(std::shared_ptr<servicegroup> &_group,
            const boost::property_tree::ptree &_tree);
    bool get_service_configuration(std::shared_ptr<servicegroup> &_group,
            const boost::property_tree::ptree &_tree);
    bool get_event_configuration(std::shared_ptr<service> &_service,
            const boost::property_tree::ptree &_tree);
    bool get_eventgroup_configuration(std::shared_ptr<service> &_service,
            const boost::property_tree::ptree &_tree);
    bool get_application_configuration(
            const boost::property_tree::ptree &_tree);

    servicegroup * find_servicegroup(const std::string &_name) const;
    service * find_service(service_t _service, instance_t _instance) const;

private:
    static std::map<std::string, configuration *> the_configurations;
    static std::mutex mutex_;

    // Configuration data
    boost::asio::ip::address unicast_;

    bool has_console_log_;
    bool has_file_log_;
    bool has_dlt_log_;
    std::string logfile_;
    boost::log::trivial::severity_level loglevel_;

    std::map<std::string, std::shared_ptr<servicegroup> > servicegroups_;
    std::map<service_t,
        std::map<instance_t,
            std::shared_ptr<service> > > services_;

    std::string routing_host_;

    bool is_service_discovery_enabled_;
    std::string service_discovery_multicast_;
    uint16_t service_discovery_port_;
    std::string service_discovery_protocol_;

    std::map<std::string, std::pair<client_t, std::size_t>> applications_;

    std::map<std::string, std::set<uint16_t> > magic_cookies_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

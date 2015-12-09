// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_CONFIGURATION_IMPL_HPP
#define VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include "configuration.hpp"

namespace vsomeip {
namespace cfg {

struct service;
struct servicegroup;

class configuration_impl: public configuration {
public:
    VSOMEIP_EXPORT static std::shared_ptr<configuration> get(
            const std::set<std::string> &_input);
    VSOMEIP_EXPORT static void reset();

    VSOMEIP_EXPORT configuration_impl();
    VSOMEIP_EXPORT configuration_impl(const configuration_impl &_cfg);
    VSOMEIP_EXPORT virtual ~configuration_impl();

    VSOMEIP_EXPORT void load(const boost::property_tree::ptree &_tree);
    VSOMEIP_EXPORT void load_log(const std::vector<boost::property_tree::ptree> &_trees);

    VSOMEIP_EXPORT const boost::asio::ip::address & get_unicast_address() const;
    VSOMEIP_EXPORT bool is_v4() const;
    VSOMEIP_EXPORT bool is_v6() const;

    VSOMEIP_EXPORT bool has_console_log() const;
    VSOMEIP_EXPORT bool has_file_log() const;
    VSOMEIP_EXPORT bool has_dlt_log() const;
    VSOMEIP_EXPORT const std::string & get_logfile() const;
    VSOMEIP_EXPORT boost::log::trivial::severity_level get_loglevel() const;

    VSOMEIP_EXPORT std::string get_unicast_address(service_t _service, instance_t _instance) const;
    VSOMEIP_EXPORT std::string get_multicast_address(service_t _service,
            instance_t _instance) const;
    VSOMEIP_EXPORT uint16_t get_multicast_port(service_t _service,
            instance_t _instance) const;
    VSOMEIP_EXPORT uint16_t get_multicast_group(service_t _service,
            instance_t _instance) const;

    VSOMEIP_EXPORT uint16_t get_reliable_port(service_t _service, instance_t _instance) const;
    VSOMEIP_EXPORT bool has_enabled_magic_cookies(std::string _address, uint16_t _port) const;
    VSOMEIP_EXPORT uint16_t get_unreliable_port(service_t _service,
            instance_t _instance) const;

    VSOMEIP_EXPORT bool is_someip(service_t _service, instance_t _instance) const;

    VSOMEIP_EXPORT const std::string & get_routing_host() const;

    VSOMEIP_EXPORT client_t get_id(const std::string &_name) const;
    VSOMEIP_EXPORT std::size_t get_num_dispatchers(const std::string &_name) const;

    VSOMEIP_EXPORT std::set<std::pair<service_t, instance_t> > get_remote_services() const;

    VSOMEIP_EXPORT std::uint32_t get_max_message_size_local() const;
    VSOMEIP_EXPORT std::uint32_t get_message_size_reliable(const std::string& _address,
                                           std::uint16_t _port) const;

    // Service Discovery configuration
    VSOMEIP_EXPORT bool is_sd_enabled() const;

    VSOMEIP_EXPORT const std::string & get_sd_multicast() const;
    VSOMEIP_EXPORT uint16_t get_sd_port() const;
    VSOMEIP_EXPORT const std::string & get_sd_protocol() const;

    VSOMEIP_EXPORT int32_t get_sd_initial_delay_min() const;
    VSOMEIP_EXPORT int32_t get_sd_initial_delay_max() const;
    VSOMEIP_EXPORT int32_t get_sd_repetitions_base_delay() const;
    VSOMEIP_EXPORT uint8_t get_sd_repetitions_max() const;
    VSOMEIP_EXPORT ttl_t get_sd_ttl() const;
    VSOMEIP_EXPORT int32_t get_sd_cyclic_offer_delay() const;
    VSOMEIP_EXPORT int32_t get_sd_request_response_delay() const;

private:
    void get_logging_configuration(const boost::property_tree::ptree &_tree);

    void get_someip_configuration(const boost::property_tree::ptree &_tree);
    void get_services_configuration(const boost::property_tree::ptree &_tree);
    void get_payload_sizes_configuration(const boost::property_tree::ptree &_tree);
    void get_routing_configuration(const boost::property_tree::ptree &_tree);
    void get_service_discovery_configuration(
            const boost::property_tree::ptree &_tree);
    void get_applications_configuration(const boost::property_tree::ptree &_tree);

    void get_servicegroup_configuration(
            const boost::property_tree::ptree &_tree);
    void get_delays_configuration(const boost::property_tree::ptree &_tree);
    void get_service_configuration(const boost::property_tree::ptree &_tree,
            const std::string &_unicast_address);
    void get_event_configuration(std::shared_ptr<service> &_service,
            const boost::property_tree::ptree &_tree);
    void get_eventgroup_configuration(std::shared_ptr<service> &_service,
            const boost::property_tree::ptree &_tree);
    void get_application_configuration(
            const boost::property_tree::ptree &_tree);

    servicegroup * find_servicegroup(const std::string &_name) const;
    service * find_service(service_t _service, instance_t _instance) const;

private:
    static std::shared_ptr<configuration_impl> the_configuration;
    static std::mutex mutex_;

protected:
    // Configuration data
    boost::asio::ip::address unicast_;

    bool has_console_log_;
    bool has_file_log_;
    bool has_dlt_log_;
    std::string logfile_;
    boost::log::trivial::severity_level loglevel_;

    std::map<std::string, std::pair<client_t, std::size_t>> applications_;

    std::map<service_t,
        std::map<instance_t,
            std::shared_ptr<service> > > services_;

    std::string routing_host_;

    bool is_sd_enabled_;
    std::string sd_protocol_;
    std::string sd_multicast_;
    uint16_t sd_port_;

    int32_t sd_initial_delay_min_;
    int32_t sd_initial_delay_max_;
    int32_t sd_repetitions_base_delay_;
    uint8_t sd_repetitions_max_;
    ttl_t sd_ttl_;
    int32_t sd_cyclic_offer_delay_;
    int32_t sd_request_response_delay_;

    std::map<std::string, std::set<uint16_t> > magic_cookies_;

    std::map<std::string, std::map<std::uint16_t, std::uint32_t>> message_sizes_;
    std::uint32_t max_configured_message_size_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_CONFIGURATION_IMPL_HPP
#define VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_set>

#include <boost/property_tree/ptree.hpp>

#include "trace.hpp"
#include "configuration.hpp"
#include "watchdog.hpp"
#include "service_instance_range.hpp"
#include "policy.hpp"
#include "../../e2e_protection/include/e2exf/config.hpp"
#include "e2e.hpp"

namespace vsomeip {
namespace cfg {

struct client;
struct service;
struct servicegroup;
struct event;
struct eventgroup;
struct watchdog;

struct element {
    std::string name_;
    boost::property_tree::ptree tree_;

    bool operator<(const element &_other) const {
        return (name_ < _other.name_);
    }
};

class configuration_impl:
        public configuration,
        public plugin_impl<configuration_impl>,
        public std::enable_shared_from_this<configuration_impl> {
public:
    VSOMEIP_EXPORT configuration_impl();
    VSOMEIP_EXPORT configuration_impl(const configuration_impl &_cfg);
    VSOMEIP_EXPORT virtual ~configuration_impl();

    VSOMEIP_EXPORT bool load(const std::string &_name);

    VSOMEIP_EXPORT const std::string &get_network() const;

    VSOMEIP_EXPORT void set_configuration_path(const std::string &_path);

    VSOMEIP_EXPORT const boost::asio::ip::address & get_unicast_address() const;
    VSOMEIP_EXPORT unsigned short get_diagnosis_address() const;
    VSOMEIP_EXPORT bool is_v4() const;
    VSOMEIP_EXPORT bool is_v6() const;

    VSOMEIP_EXPORT bool has_console_log() const;
    VSOMEIP_EXPORT bool has_file_log() const;
    VSOMEIP_EXPORT bool has_dlt_log() const;
    VSOMEIP_EXPORT const std::string & get_logfile() const;
    VSOMEIP_EXPORT boost::log::trivial::severity_level get_loglevel() const;

    VSOMEIP_EXPORT std::string get_unicast_address(service_t _service, instance_t _instance) const;

    VSOMEIP_EXPORT uint16_t get_reliable_port(service_t _service, instance_t _instance) const;
    VSOMEIP_EXPORT bool has_enabled_magic_cookies(std::string _address, uint16_t _port) const;
    VSOMEIP_EXPORT uint16_t get_unreliable_port(service_t _service,
            instance_t _instance) const;

    VSOMEIP_EXPORT bool is_someip(service_t _service, instance_t _instance) const;

    VSOMEIP_EXPORT bool get_client_port(service_t _service, instance_t _instance, bool _reliable,
            std::map<bool, std::set<uint16_t> > &_used, uint16_t &_port) const;

    VSOMEIP_EXPORT const std::string & get_routing_host() const;

    VSOMEIP_EXPORT client_t get_id(const std::string &_name) const;
    VSOMEIP_EXPORT bool is_configured_client_id(client_t _id) const;

    VSOMEIP_EXPORT std::size_t get_max_dispatchers(const std::string &_name) const;
    VSOMEIP_EXPORT std::size_t get_max_dispatch_time(const std::string &_name) const;
    VSOMEIP_EXPORT std::size_t get_io_thread_count(const std::string &_name) const;
    VSOMEIP_EXPORT std::size_t get_request_debouncing(const std::string &_name) const;

    VSOMEIP_EXPORT std::set<std::pair<service_t, instance_t> > get_remote_services() const;

    VSOMEIP_EXPORT bool get_multicast(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, std::string &_address, uint16_t &_port) const;

    VSOMEIP_EXPORT uint8_t get_threshold(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup) const;

    VSOMEIP_EXPORT std::uint32_t get_max_message_size_local() const;
    VSOMEIP_EXPORT std::uint32_t get_max_message_size_reliable(const std::string& _address,
                                           std::uint16_t _port) const;
    VSOMEIP_EXPORT std::uint32_t get_buffer_shrink_threshold() const;

    VSOMEIP_EXPORT bool supports_selective_broadcasts(boost::asio::ip::address _address) const;

    VSOMEIP_EXPORT bool is_offered_remote(service_t _service, instance_t _instance) const;

    VSOMEIP_EXPORT bool log_version() const;
    VSOMEIP_EXPORT uint32_t get_log_version_interval() const;

    VSOMEIP_EXPORT bool is_local_service(service_t _service, instance_t _instance) const;

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
    VSOMEIP_EXPORT std::uint32_t get_sd_offer_debounce_time() const;

    // Trace configuration
    VSOMEIP_EXPORT std::shared_ptr<cfg::trace> get_trace() const;

    VSOMEIP_EXPORT bool is_watchdog_enabled() const;
    VSOMEIP_EXPORT uint32_t get_watchdog_timeout() const;
    VSOMEIP_EXPORT uint32_t get_allowed_missing_pongs() const;

    VSOMEIP_EXPORT std::uint32_t get_umask() const;
    VSOMEIP_EXPORT std::uint32_t get_permissions_shm() const;

    // Policy
    VSOMEIP_EXPORT bool is_security_enabled() const;
    VSOMEIP_EXPORT bool is_client_allowed(client_t _client, service_t _service,
            instance_t _instance) const;
    VSOMEIP_EXPORT bool is_offer_allowed(client_t _client, service_t _service,
            instance_t _instance) const;
    VSOMEIP_EXPORT bool check_credentials(client_t _client, uint32_t _uid, uint32_t _gid) const;

    VSOMEIP_EXPORT std::map<plugin_type_e, std::string> get_plugins(
            const std::string &_name) const;
    // E2E
    VSOMEIP_EXPORT std::map<e2exf::data_identifier, std::shared_ptr<cfg::e2e>> get_e2e_configuration() const;
    VSOMEIP_EXPORT bool is_e2e_enabled() const;

private:
    void read_data(const std::set<std::string> &_input,
            std::vector<element> &_elements,
            std::set<std::string> &_failed,
            bool _standard_only);

    bool load_data(const std::vector<element> &_elements,
            bool _load_mandatory, bool _load_optional);

    bool load_logging(const element &_element,
                std::set<std::string> &_warnings);
    bool load_routing(const element &_element);

    bool load_applications(const element &_element);
    void load_application_data(const boost::property_tree::ptree &_tree,
            const std::string &_name);

    void load_tracing(const element &_element);
    void load_trace_channels(const boost::property_tree::ptree &_tree);
    void load_trace_channel(const boost::property_tree::ptree &_tree);
    void load_trace_filters(const boost::property_tree::ptree &_tree);
    void load_trace_filter(const boost::property_tree::ptree &_tree);
    void load_trace_filter_expressions(
            const boost::property_tree::ptree &_tree,
            std::string &_criteria,
            std::shared_ptr<trace_filter_rule> &_filter_rule);

    void load_network(const element &_element);

    void load_unicast_address(const element &_element);
    void load_diagnosis_address(const element &_element);

    void load_service_discovery(const element &_element);
    void load_delays(const boost::property_tree::ptree &_tree);

    void load_services(const element &_element);
    void load_servicegroup(const boost::property_tree::ptree &_tree);
    void load_service(const boost::property_tree::ptree &_tree,
            const std::string &_unicast_address);
    void load_event(std::shared_ptr<service> &_service,
            const boost::property_tree::ptree &_tree);
    void load_eventgroup(std::shared_ptr<service> &_service,
            const boost::property_tree::ptree &_tree);

    void load_internal_services(const element &_element);

    void load_clients(const element &_element);
    void load_client(const boost::property_tree::ptree &_tree);
    std::set<uint16_t> load_client_ports(const boost::property_tree::ptree &_tree);

    void load_watchdog(const element &_element);

    void load_payload_sizes(const element &_element);
    void load_permissions(const element &_element);
    void load_selective_broadcasts_support(const element &_element);
    void load_policies(const element &_element);
    void load_policy(const boost::property_tree::ptree &_tree);

    servicegroup *find_servicegroup(const std::string &_name) const;
    std::shared_ptr<client> find_client(service_t _service, instance_t _instance) const;
    std::shared_ptr<service> find_service(service_t _service, instance_t _instance) const;
    std::shared_ptr<eventgroup> find_eventgroup(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) const;

    void set_magic_cookies_unicast_address();

    bool is_mandatory(const std::string &_name) const;
    bool is_remote(std::shared_ptr<service> _service) const;
    bool is_internal_service(service_t _service, instance_t _instance) const;

    void set_mandatory(const std::string &_input);
    void trim(std::string &_s);

    void load_e2e(const element &_element);
    void load_e2e_protected(const boost::property_tree::ptree &_tree);

private:
    std::mutex mutex_;

    bool is_loaded_;
    bool is_logging_loaded_;

    std::set<std::string> mandatory_;

protected:
    // Configuration data
    boost::asio::ip::address unicast_;
    unsigned short diagnosis_;

    bool has_console_log_;
    bool has_file_log_;
    bool has_dlt_log_;
    std::string logfile_;
    boost::log::trivial::severity_level loglevel_;

    std::map<std::string, std::tuple<client_t, std::size_t, std::size_t,
                size_t, size_t, std::map<plugin_type_e, std::string>>> applications_;
    std::set<client_t> client_identifiers_;

    std::map<service_t,
        std::map<instance_t,
            std::shared_ptr<service> > > services_;

    std::map<service_t,
        std::map<instance_t,
            std::shared_ptr<client> > > clients_;

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
    std::uint32_t sd_offer_debounce_time_;

    std::map<std::string, std::set<uint16_t> > magic_cookies_;

    std::map<std::string, std::map<std::uint16_t, std::uint32_t>> message_sizes_;
    std::uint32_t max_configured_message_size_;
    std::uint32_t max_local_message_size_;
    std::uint32_t max_reliable_message_size_;
    std::uint32_t buffer_shrink_threshold_;

    std::shared_ptr<trace> trace_;

    std::unordered_set<std::string> supported_selective_addresses;

    std::shared_ptr<watchdog> watchdog_;

    std::vector<service_instance_range> internal_service_ranges_;

    bool log_version_;
    uint32_t log_version_interval_;

    enum element_type_e {
        ET_NETWORK,
        ET_UNICAST,
        ET_DIAGNOSIS,
        ET_LOGGING_CONSOLE,
        ET_LOGGING_FILE,
        ET_LOGGING_DLT,
        ET_LOGGING_LEVEL,
        ET_ROUTING,
        ET_SERVICE_DISCOVERY_ENABLE,
        ET_SERVICE_DISCOVERY_PROTOCOL,
        ET_SERVICE_DISCOVERY_MULTICAST,
        ET_SERVICE_DISCOVERY_PORT,
        ET_SERVICE_DISCOVERY_INITIAL_DELAY_MIN,
        ET_SERVICE_DISCOVERY_INITIAL_DELAY_MAX,
        ET_SERVICE_DISCOVERY_REPETITION_BASE_DELAY,
        ET_SERVICE_DISCOVERY_REPETITION_MAX,
        ET_SERVICE_DISCOVERY_TTL,
        ET_SERVICE_DISCOVERY_CYCLIC_OFFER_DELAY,
        ET_SERVICE_DISCOVERY_REQUEST_RESPONSE_DELAY,
        ET_WATCHDOG_ENABLE,
        ET_WATCHDOG_TIMEOUT,
        ET_WATCHDOG_ALLOWED_MISSING_PONGS,
        ET_TRACING_ENABLE,
        ET_TRACING_SD_ENABLE,
        ET_SERVICE_DISCOVERY_OFFER_DEBOUNCE_TIME,
        ET_MAX = 25
    };

    bool is_configured_[ET_MAX];
    std::uint32_t permissions_shm_;
    std::uint32_t umask_;

    std::map<client_t, std::shared_ptr<policy>> policies_;
    bool policy_enabled_;
    bool check_credentials_;

    std::string network_;
    std::string configuration_path_;

    bool e2e_enabled_;
    std::map<e2exf::data_identifier, std::shared_ptr<cfg::e2e>> e2e_configuration_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

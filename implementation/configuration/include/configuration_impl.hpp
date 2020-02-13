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
#include <list>

#include <boost/property_tree/ptree.hpp>

#include "trace.hpp"
#include "configuration.hpp"
#include "watchdog.hpp"
#include "service_instance_range.hpp"
#include "policy.hpp"
#include "../../e2e_protection/include/e2exf/config.hpp"
#include "e2e.hpp"
#include "debounce.hpp"

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
    VSOMEIP_EXPORT configuration_impl(const configuration_impl &_other);
    VSOMEIP_EXPORT virtual ~configuration_impl();

    VSOMEIP_EXPORT bool load(const std::string &_name);
    VSOMEIP_EXPORT bool remote_offer_info_add(service_t _service,
                                              instance_t _instance,
                                              std::uint16_t _port,
                                              bool _reliable,
                                              bool _magic_cookies_enabled);
    VSOMEIP_EXPORT bool remote_offer_info_remove(service_t _service,
                                                 instance_t _instance,
                                                 std::uint16_t _port,
                                                 bool _reliable,
                                                 bool _magic_cookies_enabled,
                                                 bool* _still_offered_remote);

    VSOMEIP_EXPORT const std::string &get_network() const;

    VSOMEIP_EXPORT void set_configuration_path(const std::string &_path);

    VSOMEIP_EXPORT const boost::asio::ip::address & get_unicast_address() const;
    VSOMEIP_EXPORT const boost::asio::ip::address& get_netmask() const;
    VSOMEIP_EXPORT unsigned short get_diagnosis_address() const;
    VSOMEIP_EXPORT std::uint16_t get_diagnosis_mask() const;
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
    VSOMEIP_EXPORT major_version_t get_major_version(service_t _service,
            instance_t _instance) const;
    VSOMEIP_EXPORT minor_version_t get_minor_version(service_t _service,
            instance_t _instance) const;    
    VSOMEIP_EXPORT ttl_t get_ttl(service_t _service,
            instance_t _instance) const;

    VSOMEIP_EXPORT bool is_someip(service_t _service, instance_t _instance) const;

    VSOMEIP_EXPORT bool get_client_port(service_t _service, instance_t _instance,
            uint16_t _remote_port, bool _reliable,
            std::map<bool, std::set<uint16_t> > &_used_client_ports, uint16_t &_client_port) const;

    VSOMEIP_EXPORT const std::string & get_routing_host() const;

    VSOMEIP_EXPORT client_t get_id(const std::string &_name) const;
    VSOMEIP_EXPORT bool is_configured_client_id(client_t _id) const;

    VSOMEIP_EXPORT std::size_t get_max_dispatchers(const std::string &_name) const;
    VSOMEIP_EXPORT std::size_t get_max_dispatch_time(const std::string &_name) const;
    VSOMEIP_EXPORT std::size_t get_io_thread_count(const std::string &_name) const;
    VSOMEIP_EXPORT int get_io_thread_nice_level(const std::string &_name) const;
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

    VSOMEIP_EXPORT bool is_event_reliable(service_t _service, instance_t _instance, event_t _event) const;

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

    VSOMEIP_EXPORT std::uint32_t get_permissions_uds() const;
    VSOMEIP_EXPORT std::uint32_t get_permissions_shm() const;

    // Policy
    VSOMEIP_EXPORT bool is_security_enabled() const;
    VSOMEIP_EXPORT bool is_client_allowed(client_t _client, service_t _service,
            instance_t _instance, method_t _method, bool _is_request_service = false) const;
    VSOMEIP_EXPORT bool is_offer_allowed(client_t _client, service_t _service,
            instance_t _instance) const;
    VSOMEIP_EXPORT bool check_credentials(client_t _client,
            uint32_t _uid, uint32_t _gid);

    VSOMEIP_EXPORT bool store_client_to_uid_gid_mapping(client_t _client, uint32_t _uid, uint32_t _gid);
    VSOMEIP_EXPORT void store_uid_gid_to_client_mapping(uint32_t _uid, uint32_t _gid, client_t _client);
    VSOMEIP_EXPORT bool get_client_to_uid_gid_mapping(client_t _client,
            std::pair<uint32_t, uint32_t> &_uid_gid);
    VSOMEIP_EXPORT bool remove_client_to_uid_gid_mapping(client_t _client);
    VSOMEIP_EXPORT bool get_uid_gid_to_client_mapping(std::pair<uint32_t, uint32_t> _uid_gid,
            std::set<client_t> &_clients);

    VSOMEIP_EXPORT std::map<plugin_type_e, std::set<std::string>> get_plugins(
            const std::string &_name) const;
    // E2E
    VSOMEIP_EXPORT std::map<e2exf::data_identifier_t, std::shared_ptr<cfg::e2e>> get_e2e_configuration() const;
    VSOMEIP_EXPORT bool is_e2e_enabled() const;

    VSOMEIP_EXPORT bool log_memory() const;
    VSOMEIP_EXPORT uint32_t get_log_memory_interval() const;

    VSOMEIP_EXPORT bool log_status() const;
    VSOMEIP_EXPORT uint32_t get_log_status_interval() const;

    VSOMEIP_EXPORT ttl_map_t get_ttl_factor_offers() const;
    VSOMEIP_EXPORT ttl_map_t get_ttl_factor_subscribes() const;

    VSOMEIP_EXPORT std::shared_ptr<debounce> get_debounce(
            service_t _service, instance_t _instance, event_t _event) const;

    VSOMEIP_EXPORT endpoint_queue_limit_t get_endpoint_queue_limit(
            const std::string& _address, std::uint16_t _port) const;
    VSOMEIP_EXPORT endpoint_queue_limit_t get_endpoint_queue_limit_local() const;

    VSOMEIP_EXPORT std::uint32_t get_max_tcp_restart_aborts() const;
    VSOMEIP_EXPORT std::uint32_t get_max_tcp_connect_time() const;

    VSOMEIP_EXPORT bool offer_acceptance_required(
            const boost::asio::ip::address& _address) const;

    VSOMEIP_EXPORT void set_offer_acceptance_required(
            const boost::asio::ip::address& _address, const std::string& _path,
            bool _enable);
    VSOMEIP_EXPORT std::map<boost::asio::ip::address, std::string> get_offer_acceptance_required();

    VSOMEIP_EXPORT void update_security_policy(uint32_t _uid, uint32_t _gid, ::std::shared_ptr<policy> _policy);
    VSOMEIP_EXPORT bool remove_security_policy(uint32_t _uid, uint32_t _gid);

    VSOMEIP_EXPORT void add_security_credentials(uint32_t _uid, uint32_t _gid,
            ::std::shared_ptr<policy> _credentials_policy, client_t _client);

    VSOMEIP_EXPORT bool is_remote_client_allowed() const;

    VSOMEIP_EXPORT bool is_policy_update_allowed(uint32_t _uid, std::shared_ptr<policy> &_policy) const;

    VSOMEIP_EXPORT bool is_policy_removal_allowed(uint32_t _uid) const;

    VSOMEIP_EXPORT std::uint32_t get_udp_receive_buffer_size() const;

    VSOMEIP_EXPORT bool is_audit_mode_enabled() const;

    VSOMEIP_EXPORT bool check_routing_credentials(client_t _client, uint32_t _uid, uint32_t _gid) const;

    VSOMEIP_EXPORT std::uint32_t get_shutdown_timeout() const;
private:
    void read_data(const std::set<std::string> &_input,
            std::vector<element> &_elements,
            std::set<std::string> &_failed,
            bool _mandatory_only);

    bool load_data(const std::vector<element> &_elements,
            bool _load_mandatory, bool _load_optional);

    bool load_logging(const element &_element,
                std::set<std::string> &_warnings);
    bool load_routing(const element &_element);
    bool load_routing_credentials(const element &_element);

    bool load_applications(const element &_element);
    void load_application_data(const boost::property_tree::ptree &_tree,
            const std::string &_file_name);

    void load_tracing(const element &_element);
    void load_trace_channels(const boost::property_tree::ptree &_tree);
    void load_trace_channel(const boost::property_tree::ptree &_tree);
    void load_trace_filters(const boost::property_tree::ptree &_tree);
    void load_trace_filter(const boost::property_tree::ptree &_tree);
    void load_trace_filter_expressions(
            const boost::property_tree::ptree &_tree,
            std::string &_criteria,
            std::shared_ptr<trace_filter> &_filter);
    void load_trace_filter_match(
            const boost::property_tree::ptree &_data,
            std::tuple<service_t, instance_t, method_t> &_match);

    void load_network(const element &_element);

    void load_unicast_address(const element &_element);
    void load_netmask(const element &_element);
    void load_diagnosis_address(const element &_element);
    void load_shutdown_timeout(const element &_element);

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
    std::pair<uint16_t, uint16_t> load_client_port_range(const boost::property_tree::ptree &_tree);

    void load_watchdog(const element &_element);

    void load_payload_sizes(const element &_element);
    void load_permissions(const element &_element);
    void load_selective_broadcasts_support(const element &_element);
    void load_policies(const element &_element);
    void load_policy(const boost::property_tree::ptree &_tree);
    void load_credential(const boost::property_tree::ptree &_tree, ids_t &_ids);
    void load_ranges(const boost::property_tree::ptree &_tree, ranges_t &_range);
    void load_instance_ranges(const boost::property_tree::ptree &_tree, ranges_t &_range);

    void load_security_update_whitelist(const element &_element);
    void load_service_ranges(const boost::property_tree::ptree &_tree,
            std::set<std::pair<service_t, service_t>> &_ranges);

    void load_debounce(const element &_element);
    void load_service_debounce(const boost::property_tree::ptree &_tree);
    void load_events_debounce(const boost::property_tree::ptree &_tree,
            std::map<event_t, std::shared_ptr<debounce>> &_debounces);
    void load_event_debounce(const boost::property_tree::ptree &_tree,
                std::map<event_t, std::shared_ptr<debounce>> &_debounces);
    void load_event_debounce_ignore(const boost::property_tree::ptree &_tree,
            std::map<std::size_t, byte_t> &_ignore);
    void load_offer_acceptance_required(const element &_element);
    void load_udp_receive_buffer_size(const element &_element);


    servicegroup *find_servicegroup(const std::string &_name) const;
    std::shared_ptr<client> find_client(service_t _service,
            instance_t _instance) const;
    std::shared_ptr<service> find_service(service_t _service, instance_t _instance) const;
    std::shared_ptr<service> find_service_unlocked(service_t _service, instance_t _instance) const;
    std::shared_ptr<eventgroup> find_eventgroup(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) const;
    bool find_port(uint16_t &_port, uint16_t _remote, bool _reliable,
            std::map<bool, std::set<uint16_t> > &_used_client_ports) const;

    void set_magic_cookies_unicast_address();

    bool is_mandatory(const std::string &_name) const;
    bool is_remote(std::shared_ptr<service> _service) const;
    bool is_internal_service(service_t _service, instance_t _instance) const;
    bool is_in_port_range(uint16_t _port, std::pair<uint16_t, uint16_t> _port_range) const;

    void set_mandatory(const std::string &_input);
    void trim(std::string &_s);

    void load_e2e(const element &_element);
    void load_e2e_protected(const boost::property_tree::ptree &_tree);

    void load_ttl_factors(const boost::property_tree::ptree &_tree,
                          ttl_map_t* _target);

    void load_endpoint_queue_sizes(const element &_element);

    void load_tcp_restart_settings(const element &_element);

    std::shared_ptr<policy> find_client_id_policy(client_t _client) const;

private:
    std::mutex mutex_;
    mutable std::mutex ids_mutex_;
    mutable std::mutex uid_to_clients_mutex_;

    bool is_loaded_;
    bool is_logging_loaded_;

    std::set<std::string> mandatory_;

protected:
    // Configuration data
    boost::asio::ip::address unicast_;
    boost::asio::ip::address netmask_;
    unsigned short diagnosis_;
    std::uint16_t diagnosis_mask_;

    bool has_console_log_;
    bool has_file_log_;
    bool has_dlt_log_;
    std::string logfile_;
    boost::log::trivial::severity_level loglevel_;

    std::map<std::string, std::tuple<client_t, std::size_t, std::size_t,
                size_t, size_t, std::map<plugin_type_e, std::set<std::string>>, int>> applications_;
    std::set<client_t> client_identifiers_;

    mutable std::mutex services_mutex_;
    std::map<service_t,
        std::map<instance_t,
            std::shared_ptr<service> > > services_;

    std::list< std::shared_ptr<client> > clients_;

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
        ET_DIAGNOSIS_MASK,
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
        ET_SERVICE_DISCOVERY_TTL_FACTOR_OFFERS,
        ET_SERVICE_DISCOVERY_TTL_FACTOR_SUBSCRIPTIONS,
        ET_ENDPOINT_QUEUE_LIMITS,
        ET_ENDPOINT_QUEUE_LIMIT_EXTERNAL,
        ET_ENDPOINT_QUEUE_LIMIT_LOCAL,
        ET_TCP_RESTART_ABORTS_MAX,
        ET_TCP_CONNECT_TIME_MAX,
        ET_OFFER_ACCEPTANCE_REQUIRED,
        ET_NETMASK,
        ET_UDP_RECEIVE_BUFFER_SIZE,
        ET_ROUTING_CREDENTIALS,
        ET_SHUTDOWN_TIMEOUT,
        ET_MAX = 38
    };

    bool is_configured_[ET_MAX];
    std::uint32_t permissions_shm_;
    std::uint32_t permissions_uds_;

    std::map<std::pair<uint16_t, uint16_t>, std::shared_ptr<policy>> policies_;
    std::vector<std::shared_ptr<policy> > any_client_policies_;

    mutable std::mutex  policies_mutex_;
    mutable std::mutex  any_client_policies_mutex_;
    std::map<client_t, std::pair<uint32_t, uint32_t> > ids_;
    std::map<std::pair<uint32_t, uint32_t>, std::set<client_t> > uid_to_clients_;

    bool policy_enabled_;
    bool check_credentials_;
    bool check_routing_credentials_;
    bool allow_remote_clients_;
    bool check_whitelist_;

    std::string network_;
    std::string configuration_path_;

    bool e2e_enabled_;
    std::map<e2exf::data_identifier_t, std::shared_ptr<cfg::e2e>> e2e_configuration_;

    bool log_memory_;
    uint32_t log_memory_interval_;

    bool log_status_;
    uint32_t log_status_interval_;

    ttl_map_t ttl_factors_offers_;
    ttl_map_t ttl_factors_subscriptions_;

    std::map<service_t, std::map<instance_t, std::map<event_t, std::shared_ptr<debounce>>>> debounces_;

    std::map<std::string, std::map<std::uint16_t, endpoint_queue_limit_t>> endpoint_queue_limits_;
    endpoint_queue_limit_t endpoint_queue_limit_external_;
    endpoint_queue_limit_t endpoint_queue_limit_local_;

    uint32_t tcp_restart_aborts_max_;
    uint32_t tcp_connect_time_max_;

    mutable std::mutex offer_acceptance_required_ips_mutex_;
    std::map<boost::asio::ip::address, std::string> offer_acceptance_required_ips_;

    bool has_issued_methods_warning_;
    bool has_issued_clients_warning_;

    std::uint32_t udp_receive_buffer_size_;

    mutable std::mutex service_interface_whitelist_mutex_;
    std::set<std::pair<service_t, service_t>> service_interface_whitelist_;

    mutable std::mutex uid_whitelist_mutex_;
    ranges_t uid_whitelist_;

    mutable std::mutex routing_credentials_mutex_;
    std::pair<uint32_t, uint32_t> routing_credentials_;

    std::uint32_t shutdown_timeout_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_CONFIGURATION_IMPL_HPP

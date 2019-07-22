// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cctype>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <limits>

#define WIN32_LEAN_AND_MEAN

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/plugins/application_plugin.hpp>
#include <vsomeip/plugins/pre_configuration_plugin.hpp>

#include "../include/client.hpp"
#include "../include/configuration_impl.hpp"
#include "../include/event.hpp"
#include "../include/eventgroup.hpp"
#include "../include/service.hpp"
#include "../include/internal.hpp"
#include "../../logging/include/logger_impl.hpp"
#include "../../routing/include/event.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../utility/include/utility.hpp"
#include "../../plugin/include/plugin_manager.hpp"

VSOMEIP_PLUGIN(vsomeip::cfg::configuration_impl)

namespace vsomeip {
namespace cfg {

configuration_impl::configuration_impl()
    : plugin_impl("vsomeip cfg plugin", 1, plugin_type_e::CONFIGURATION_PLUGIN),
      is_loaded_(false),
      is_logging_loaded_(false),
      diagnosis_(VSOMEIP_DIAGNOSIS_ADDRESS),
      diagnosis_mask_(0xFF00),
      has_console_log_(true),
      has_file_log_(false),
      has_dlt_log_(false),
      logfile_("/tmp/vsomeip.log"),
      loglevel_(boost::log::trivial::severity_level::info),
      is_sd_enabled_(VSOMEIP_SD_DEFAULT_ENABLED),
      sd_protocol_(VSOMEIP_SD_DEFAULT_PROTOCOL),
      sd_multicast_(VSOMEIP_SD_DEFAULT_MULTICAST),
      sd_port_(VSOMEIP_SD_DEFAULT_PORT),
      sd_initial_delay_min_(VSOMEIP_SD_DEFAULT_INITIAL_DELAY_MIN),
      sd_initial_delay_max_(VSOMEIP_SD_DEFAULT_INITIAL_DELAY_MAX),
      sd_repetitions_base_delay_(VSOMEIP_SD_DEFAULT_REPETITIONS_BASE_DELAY),
      sd_repetitions_max_(VSOMEIP_SD_DEFAULT_REPETITIONS_MAX),
      sd_ttl_(VSOMEIP_SD_DEFAULT_TTL),
      sd_cyclic_offer_delay_(VSOMEIP_SD_DEFAULT_CYCLIC_OFFER_DELAY),
      sd_request_response_delay_(VSOMEIP_SD_DEFAULT_REQUEST_RESPONSE_DELAY),
      sd_offer_debounce_time_(VSOMEIP_SD_DEFAULT_OFFER_DEBOUNCE_TIME),
      max_configured_message_size_(0),
      max_local_message_size_(0),
      max_reliable_message_size_(0),
      buffer_shrink_threshold_(VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD),
      trace_(std::make_shared<trace>()),
      watchdog_(std::make_shared<watchdog>()),
      log_version_(true),
      log_version_interval_(10),
      permissions_shm_(VSOMEIP_DEFAULT_SHM_PERMISSION),
      permissions_uds_(VSOMEIP_DEFAULT_UDS_PERMISSIONS),
      policy_enabled_(false),
      check_credentials_(false),
      check_routing_credentials_(false),
      allow_remote_clients_(true),
      check_whitelist_(false),
      network_("vsomeip"),
      e2e_enabled_(false),
      log_memory_(false),
      log_memory_interval_(0),
      log_status_(false),
      log_status_interval_(0),
      endpoint_queue_limit_external_(QUEUE_SIZE_UNLIMITED),
      endpoint_queue_limit_local_(QUEUE_SIZE_UNLIMITED),
      tcp_restart_aborts_max_(VSOMEIP_MAX_TCP_RESTART_ABORTS),
      tcp_connect_time_max_(VSOMEIP_MAX_TCP_CONNECT_TIME),
      has_issued_methods_warning_(false),
      has_issued_clients_warning_(false),
      udp_receive_buffer_size_(VSOMEIP_DEFAULT_UDP_RCV_BUFFER_SIZE),
      shutdown_timeout_(VSOMEIP_DEFAULT_SHUTDOWN_TIMEOUT) {
    unicast_ = unicast_.from_string(VSOMEIP_UNICAST_ADDRESS);
    netmask_ = netmask_.from_string(VSOMEIP_NETMASK);
    for (auto i = 0; i < ET_MAX; i++)
        is_configured_[i] = false;
}

configuration_impl::configuration_impl(const configuration_impl &_other)
    : plugin_impl("vsomeip cfg plugin", 1, plugin_type_e::CONFIGURATION_PLUGIN),
      std::enable_shared_from_this<configuration_impl>(_other),
      is_loaded_(_other.is_loaded_),
      is_logging_loaded_(_other.is_logging_loaded_),
      mandatory_(_other.mandatory_),
      max_configured_message_size_(_other.max_configured_message_size_),
      max_local_message_size_(_other.max_local_message_size_),
      max_reliable_message_size_(_other.max_reliable_message_size_),
      buffer_shrink_threshold_(_other.buffer_shrink_threshold_),
      permissions_shm_(VSOMEIP_DEFAULT_SHM_PERMISSION),
      permissions_uds_(VSOMEIP_DEFAULT_UDS_PERMISSIONS),
      endpoint_queue_limit_external_(_other.endpoint_queue_limit_external_),
      endpoint_queue_limit_local_(_other.endpoint_queue_limit_local_),
      tcp_restart_aborts_max_(_other.tcp_restart_aborts_max_),
      tcp_connect_time_max_(_other.tcp_connect_time_max_),
      udp_receive_buffer_size_(_other.udp_receive_buffer_size_),
      shutdown_timeout_(_other.shutdown_timeout_) {

    applications_.insert(_other.applications_.begin(), _other.applications_.end());
    client_identifiers_ = _other.client_identifiers_;
    services_.insert(_other.services_.begin(), _other.services_.end());
    clients_ = _other.clients_;

    unicast_ = _other.unicast_;
    netmask_ = _other.netmask_;
    diagnosis_ = _other.diagnosis_;
    diagnosis_mask_ = _other.diagnosis_mask_;

    has_console_log_ = _other.has_console_log_;
    has_file_log_ = _other.has_file_log_;
    has_dlt_log_ = _other.has_dlt_log_;
    logfile_ = _other.logfile_;

    loglevel_ = _other.loglevel_;

    routing_host_ = _other.routing_host_;

    is_sd_enabled_ = _other.is_sd_enabled_;
    sd_multicast_ = _other.sd_multicast_;
    sd_port_ = _other.sd_port_;
    sd_protocol_ = _other.sd_protocol_;

    sd_initial_delay_min_ = _other.sd_initial_delay_min_;
    sd_initial_delay_max_ = _other.sd_initial_delay_max_;
    sd_repetitions_base_delay_= _other.sd_repetitions_base_delay_;
    sd_repetitions_max_ = _other.sd_repetitions_max_;
    sd_ttl_ = _other.sd_ttl_;
    sd_cyclic_offer_delay_= _other.sd_cyclic_offer_delay_;
    sd_request_response_delay_= _other.sd_request_response_delay_;
    sd_offer_debounce_time_ = _other.sd_offer_debounce_time_;

    trace_ = std::make_shared<trace>(*_other.trace_.get());
    supported_selective_addresses = _other.supported_selective_addresses;
    watchdog_ = std::make_shared<watchdog>(*_other.watchdog_.get());
    internal_service_ranges_ = _other.internal_service_ranges_;
    log_version_ = _other.log_version_;
    log_version_interval_ = _other.log_version_interval_;

    magic_cookies_.insert(_other.magic_cookies_.begin(), _other.magic_cookies_.end());
    message_sizes_ = _other.message_sizes_;

    for (auto i = 0; i < ET_MAX; i++)
        is_configured_[i] = _other.is_configured_[i];

    policies_ = _other.policies_;
    any_client_policies_ = _other.any_client_policies_;
    ids_ = _other.ids_;
    policy_enabled_ = _other.policy_enabled_;
    check_credentials_ = _other.check_credentials_;
    check_routing_credentials_ = _other.check_routing_credentials_;
    allow_remote_clients_ = _other.allow_remote_clients_;
    check_whitelist_ = _other.check_whitelist_;

    network_ = _other.network_;
    configuration_path_ = _other.configuration_path_;

    e2e_enabled_ = _other.e2e_enabled_;
    e2e_configuration_ = _other.e2e_configuration_;

    log_memory_ = _other.log_memory_;
    log_memory_interval_ = _other.log_memory_interval_;
    log_status_ = _other.log_status_;
    log_status_interval_ = _other.log_status_interval_;

    ttl_factors_offers_ = _other.ttl_factors_offers_;
    ttl_factors_subscriptions_ = _other.ttl_factors_subscriptions_;

    debounces_ = _other.debounces_;
    endpoint_queue_limits_ = _other.endpoint_queue_limits_;

    offer_acceptance_required_ips_ = _other.offer_acceptance_required_ips_;

    has_issued_methods_warning_ = _other.has_issued_methods_warning_;
    has_issued_clients_warning_ = _other.has_issued_clients_warning_;
}

configuration_impl::~configuration_impl() {
}

bool configuration_impl::load(const std::string &_name) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (is_loaded_)
        return true;

    // Predefine file / folder
    std::string its_file(VSOMEIP_DEFAULT_CONFIGURATION_FILE); // configuration file
    std::string its_folder(VSOMEIP_DEFAULT_CONFIGURATION_FOLDER); // configuration folder

    // Override with local file / folder (if existing)
    std::string its_local_file(VSOMEIP_LOCAL_CONFIGURATION_FILE);
    if (utility::is_file(its_local_file)) {
        its_file = its_local_file;
    }

    std::string its_local_folder(VSOMEIP_LOCAL_CONFIGURATION_FOLDER);
    if (utility::is_folder(its_local_folder)) {
        its_folder = its_local_folder;
    }

    // Override with path from environment (if existing)
    const char *its_env = getenv(VSOMEIP_ENV_CONFIGURATION);
    if (nullptr != its_env) {
        if (utility::is_file(its_env)) {
            its_file = its_env;
            its_folder = "";
        } else if (utility::is_folder(its_env)) {
            its_folder = its_env;
            its_file = "";
        }
    }

    std::set<std::string> its_input;
    if (its_file != "") {
        its_input.insert(its_file);
    }
    if (its_folder != "") {
        its_input.insert(its_folder);
#ifndef _WIN32
        // load security configuration files from UID_GID sub folder if existing
        std::stringstream its_security_config_folder;
        its_security_config_folder << its_folder << "/" << getuid() << "_" << getgid();
        if (utility::is_folder(its_security_config_folder.str())) {
            its_input.insert(its_security_config_folder.str());
        }
#endif
    }

    // Determine standard configuration file
    its_env = getenv(VSOMEIP_ENV_MANDATORY_CONFIGURATION_FILES);
    if (nullptr != its_env) {
        std::string its_temp(its_env);
        set_mandatory(its_temp);
    } else {
        set_mandatory(VSOMEIP_MANDATORY_CONFIGURATION_FILES);
    }

    // Start reading
    std::set<std::string> its_failed;

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    std::vector<element> its_mandatory_elements;
    std::vector<element> its_optional_elements;

    // Dummy initialization; maybe we'll find no logging configuration
    logger_impl::init(shared_from_this());

    // Look for the standard configuration file
    read_data(its_input, its_mandatory_elements, its_failed, true);
    load_data(its_mandatory_elements, true, false);

    // If the configuration is incomplete, this is the routing manager configuration or
    // the routing is yet unknown, read the full set of configuration files
    if (its_mandatory_elements.empty() ||
            _name == get_routing_host() ||
            "" == get_routing_host()) {
        read_data(its_input, its_optional_elements, its_failed, false);
        load_data(its_mandatory_elements, false, true);
        load_data(its_optional_elements, true, true);
    }

    // Tell, if reading of configuration file(s) failed.
    // (This may file if the logger configuration is incomplete/missing).
    for (auto f : its_failed)
        VSOMEIP_WARNING << "Reading of configuration file \""
            << f << "\" failed. Configuration may be incomplete.";

    // set global unicast address for all services with magic cookies enabled
    set_magic_cookies_unicast_address();

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    VSOMEIP_INFO << "Parsed vsomeip configuration in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
            << "ms";

    for (auto i : its_input) {
        if (utility::is_file(i))
            VSOMEIP_INFO << "Using configuration file: \"" << i << "\".";

        if (utility::is_folder(i))
            VSOMEIP_INFO << "Using configuration folder: \"" << i << "\".";
    }

    if (policy_enabled_ && check_credentials_)
        VSOMEIP_INFO << "Security configuration is active.";

    if (policy_enabled_ && !check_credentials_)
        VSOMEIP_INFO << "Security configuration is active but in audit mode (allow all)";

    is_loaded_ = true;

    return is_loaded_;
}

bool configuration_impl::remote_offer_info_add(service_t _service,
                                               instance_t _instance,
                                               std::uint16_t _port,
                                               bool _reliable,
                                               bool _magic_cookies_enabled) {
    bool ret = false;
    if (!is_loaded_) {
        VSOMEIP_ERROR << __func__ << " shall only be called after normal"
                "configuration has been parsed";
    } else {
        std::shared_ptr<service> its_service(std::make_shared<service>());
        its_service->service_ = _service;
        its_service->instance_ = _instance;
        its_service->reliable_ = its_service->unreliable_ = ILLEGAL_PORT;
        _reliable ?
                its_service->reliable_ = _port :
                its_service->unreliable_ = _port;
        its_service->unicast_address_ = "local";
        its_service->multicast_address_ = "";
        its_service->multicast_port_ = ILLEGAL_PORT;
        its_service->protocol_ = "someip";

        {
            std::lock_guard<std::mutex> its_lock(services_mutex_);
            bool updated(false);
            auto found_service = services_.find(its_service->service_);
            if (found_service != services_.end()) {
                auto found_instance = found_service->second.find(its_service->instance_);
                if (found_instance != found_service->second.end()) {
                    VSOMEIP_INFO << "Updating remote configuration for service ["
                            << std::hex << std::setw(4) << std::setfill('0')
                            << its_service->service_ << "." << its_service->instance_ << "]";
                    if (_reliable) {
                        found_instance->second->reliable_ = its_service->reliable_;
                    } else {
                        found_instance->second->unreliable_ = its_service->unreliable_;
                    }
                    updated = true;
                }
            }
            if (!updated) {
                services_[_service][_instance] = its_service;
                VSOMEIP_INFO << "Added new remote configuration for service ["
                        << std::hex << std::setw(4) << std::setfill('0')
                        << its_service->service_ << "."
                        << std::hex << std::setw(4) << std::setfill('0')
                        << its_service->instance_ << "]";
            }
            if (_magic_cookies_enabled) {
                magic_cookies_[its_service->unicast_address_].insert(its_service->reliable_);
            }
        }
        ret = true;
    }
    return ret;
}

bool configuration_impl::remote_offer_info_remove(service_t _service,
                                                  instance_t _instance,
                                                  std::uint16_t _port,
                                                  bool _reliable,
                                                  bool _magic_cookies_enabled,
                                                  bool* _still_offered_remote) {
    (void)_port;
    (void)_magic_cookies_enabled;
    bool ret = false;
    if (!is_loaded_) {
        VSOMEIP_ERROR << __func__ << " shall only be called after normal"
                "configuration has been parsed";
    } else {
        std::lock_guard<std::mutex> its_lock(services_mutex_);
        auto found_service = services_.find(_service);
        if (found_service != services_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                VSOMEIP_INFO << "Removing remote configuration for service ["
                        << std::hex << std::setw(4) << std::setfill('0')
                        << _service << "." << _instance << "]";
                if (_reliable) {
                    found_instance->second->reliable_ = ILLEGAL_PORT;
                    // TODO delete from magic_cookies_map without overwriting
                    // configurations from other services offered on the same port
                } else {
                    found_instance->second->unreliable_ = ILLEGAL_PORT;
                }
                *_still_offered_remote = (
                        found_instance->second->unreliable_ != ILLEGAL_PORT ||
                        found_instance->second->reliable_ != ILLEGAL_PORT);
                ret = true;
            }
        }
    }
    return ret;
}

void configuration_impl::read_data(const std::set<std::string> &_input,
        std::vector<element> &_elements, std::set<std::string> &_failed,
        bool _mandatory_only) {
    for (auto i : _input) {
        if (utility::is_file(i)) {
            if (is_mandatory(i) != _mandatory_only) {
                boost::property_tree::ptree its_tree;
                try {
                    boost::property_tree::json_parser::read_json(i, its_tree);
                    _elements.push_back({ i, its_tree });
                }
                catch (boost::property_tree::json_parser_error &e) {
    #ifdef _WIN32
                    e; // silence MSVC warning C4101
    #endif
                    _failed.insert(i);
                }
            }
        } else if (utility::is_folder(i)) {
            boost::filesystem::path its_path(i);
            for (auto j = boost::filesystem::directory_iterator(its_path);
                    j != boost::filesystem::directory_iterator();
                    j++) {
                auto its_file_path = j->path();
                if (!boost::filesystem::is_directory(its_file_path)) {
                    std::string its_name = its_file_path.string();
                    if (is_mandatory(its_name) == _mandatory_only) {
                        boost::property_tree::ptree its_tree;
                        try {
                            boost::property_tree::json_parser::read_json(its_name, its_tree);
                            _elements.push_back({its_name, its_tree});
                        }
                        catch (...) {
                            _failed.insert(its_name);
                        }
                    }
                }
            }
        }
    }
}


bool configuration_impl::load_data(const std::vector<element> &_elements,
        bool _load_mandatory, bool _load_optional) {
    // Load logging configuration data
    std::set<std::string> its_warnings;
    if (!is_logging_loaded_) {
        for (auto e : _elements)
            is_logging_loaded_
                = load_logging(e, its_warnings) || is_logging_loaded_;

        if (is_logging_loaded_) {
            logger_impl::init(shared_from_this());
            for (auto w : its_warnings)
                VSOMEIP_WARNING << w;
        }
    }

    bool has_routing(false);
    bool has_applications(false);
    if (_load_mandatory) {
        // Load mandatory configuration data
        for (auto e : _elements) {
            has_routing = load_routing(e) || has_routing;
            has_applications = load_applications(e) || has_applications;
            load_network(e);
            load_diagnosis_address(e);
            load_shutdown_timeout(e);
            load_payload_sizes(e);
            load_endpoint_queue_sizes(e);
            load_tcp_restart_settings(e);
            load_permissions(e);
            load_policies(e);
            load_tracing(e);
            load_udp_receive_buffer_size(e);
            load_security_update_whitelist(e);
            load_routing_credentials(e);
        }
    }

    if (_load_optional) {
        for (auto e : _elements) {
            load_unicast_address(e);
            load_netmask(e);
            load_service_discovery(e);
            load_services(e);
            load_internal_services(e);
            load_clients(e);
            load_watchdog(e);
            load_selective_broadcasts_support(e);
            load_e2e(e);
            load_debounce(e);
            load_offer_acceptance_required(e);
        }
    }

    return is_logging_loaded_ && has_routing && has_applications;
}

bool configuration_impl::load_logging(
        const element &_element, std::set<std::string> &_warnings) {
    try {
        auto its_logging = _element.tree_.get_child("logging");
        for (auto i = its_logging.begin(); i != its_logging.end(); ++i) {
            std::string its_key(i->first);
            if (its_key == "console") {
                if (is_configured_[ET_LOGGING_CONSOLE]) {
                    _warnings.insert("Multiple definitions for logging.console."
                            " Ignoring definition from " + _element.name_);
                } else {
                    std::string its_value(i->second.data());
                    has_console_log_ = (its_value == "true");
                    is_configured_[ET_LOGGING_CONSOLE] = true;
                }
            } else if (its_key == "file") {
                if (is_configured_[ET_LOGGING_FILE]) {
                    _warnings.insert("Multiple definitions for logging.file."
                            " Ignoring definition from " + _element.name_);
                } else {
                    for (auto j : i->second) {
                        std::string its_sub_key(j.first);
                        std::string its_sub_value(j.second.data());
                        if (its_sub_key == "enable") {
                            has_file_log_ = (its_sub_value == "true");
                        } else if (its_sub_key == "path") {
                            logfile_ = its_sub_value;
                        }
                    }
                    is_configured_[ET_LOGGING_FILE] = true;
                }
            } else if (its_key == "dlt") {
                if (is_configured_[ET_LOGGING_DLT]) {
                    _warnings.insert("Multiple definitions for logging.dlt."
                            " Ignoring definition from " + _element.name_);
                } else {
                    std::string its_value(i->second.data());
                    has_dlt_log_ = (its_value == "true");
                    is_configured_[ET_LOGGING_DLT] = true;
                }
            } else if (its_key == "level") {
                if (is_configured_[ET_LOGGING_LEVEL]) {
                    _warnings.insert("Multiple definitions for logging.level."
                            " Ignoring definition from " + _element.name_);
                } else {
                    std::string its_value(i->second.data());
                    loglevel_
                        = (its_value == "trace" ?
                                boost::log::trivial::severity_level::trace :
                          (its_value == "debug" ?
                                boost::log::trivial::severity_level::debug :
                          (its_value == "info" ?
                                boost::log::trivial::severity_level::info :
                          (its_value == "warning" ?
                                boost::log::trivial::severity_level::warning :
                          (its_value == "error" ?
                                boost::log::trivial::severity_level::error :
                          (its_value == "fatal" ?
                                boost::log::trivial::severity_level::fatal :
                          boost::log::trivial::severity_level::info))))));
                    is_configured_[ET_LOGGING_LEVEL] = true;
                }
            } else if (its_key == "version") {
                std::stringstream its_converter;
                for (auto j : i->second) {
                    std::string its_sub_key(j.first);
                    std::string its_sub_value(j.second.data());
                    if (its_sub_key == "enable") {
                        log_version_ = (its_sub_value == "true");
                    } else if (its_sub_key == "interval") {
                        its_converter << std::dec << its_sub_value;
                        its_converter >> log_version_interval_;
                    }
                }
            } else if (its_key == "memory_log_interval") {
                std::stringstream its_converter;
                its_converter << std::dec << i->second.data();
                its_converter >> log_memory_interval_;
                if (log_memory_interval_ > 0) {
                    log_memory_ = true;
                }
            } else if (its_key == "status_log_interval") {
                std::stringstream its_converter;
                its_converter << std::dec << i->second.data();
                its_converter >> log_status_interval_;
                if (log_status_interval_ > 0) {
                    log_status_ = true;
                }
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool configuration_impl::load_routing(const element &_element) {
    try {
        auto its_routing = _element.tree_.get_child("routing");
        if (is_configured_[ET_ROUTING]) {
            VSOMEIP_WARNING << "Multiple definitions of routing."
                    << " Ignoring definition from " << _element.name_;
        } else {
            routing_host_ = its_routing.data();
            is_configured_[ET_ROUTING] = true;
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool configuration_impl::load_routing_credentials(const element &_element) {
    try {
        auto its_routing_cred = _element.tree_.get_child("routing-credentials");
        if (is_configured_[ET_ROUTING_CREDENTIALS]) {
            VSOMEIP_WARNING << "vSomeIP Security: Multiple definitions of routing-credentials."
                    << " Ignoring definition from " << _element.name_;
        } else {
            for (auto i = its_routing_cred.begin();
                    i != its_routing_cred.end();
                    ++i) {
                std::string its_key(i->first);
                std::string its_value(i->second.data());
                std::stringstream its_converter;
                if (its_key == "uid") {
                    uint32_t its_uid(0);
                    if (its_value.find("0x") == 0) {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_uid;
                    std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
                    std::get<0>(routing_credentials_) = its_uid;
                } else if (its_key == "gid") {
                    uint32_t its_gid(0);
                    if (its_value.find("0x") == 0) {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_gid;
                    std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
                    std::get<1>(routing_credentials_) = its_gid;
                }
            }
            check_routing_credentials_ = true;
            is_configured_[ET_ROUTING_CREDENTIALS] = true;
        }
    } catch (...) {
        return false;
    }
    return true;
}


bool configuration_impl::load_applications(const element &_element) {
    try {
        std::stringstream its_converter;
        auto its_applications = _element.tree_.get_child("applications");
        for (auto i = its_applications.begin();
                i != its_applications.end();
                ++i) {
            load_application_data(i->second, _element.name_);
        }
    } catch (...) {
        return false;
    }
    return true;
}

void configuration_impl::load_application_data(
        const boost::property_tree::ptree &_tree, const std::string &_file_name) {
    std::string its_name("");
    client_t its_id(0);
    std::size_t its_max_dispatchers(VSOMEIP_MAX_DISPATCHERS);
    std::size_t its_max_dispatch_time(VSOMEIP_MAX_DISPATCH_TIME);
    std::size_t its_io_thread_count(VSOMEIP_IO_THREAD_COUNT);
    std::size_t its_request_debounce_time(VSOMEIP_REQUEST_DEBOUNCE_TIME);
    std::map<plugin_type_e, std::set<std::string>> plugins;
    int its_io_thread_nice_level(VSOMEIP_IO_THREAD_NICE_LEVEL);
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_key(i->first);
        std::string its_value(i->second.data());
        std::stringstream its_converter;
        if (its_key == "name") {
            its_name = its_value;
        } else if (its_key == "id") {
            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_id;
        } else if (its_key == "max_dispatchers") {
            its_converter << std::dec << its_value;
            its_converter >> its_max_dispatchers;
        } else if (its_key == "max_dispatch_time") {
            its_converter << std::dec << its_value;
            its_converter >> its_max_dispatch_time;
        } else if (its_key == "threads") {
            its_converter << std::dec << its_value;
            its_converter >> its_io_thread_count;
            if (its_io_thread_count == 0) {
                VSOMEIP_WARNING << "Min. number of threads per application is 1";
                its_io_thread_count = 1;
            } else if (its_io_thread_count > 255) {
                VSOMEIP_WARNING << "Max. number of threads per application is 255";
                its_io_thread_count = 255;
            }
        } else if (its_key == "io_thread_nice") {
            its_converter << std::dec << its_value;
            its_converter >> its_io_thread_nice_level;
        } else if (its_key == "request_debounce_time") {
            its_converter << std::dec << its_value;
            its_converter >> its_request_debounce_time;
            if (its_request_debounce_time > 10000) {
                VSOMEIP_WARNING << "Max. request debounce time is 10.000ms";
                its_request_debounce_time = 10000;
            }
        } else if (its_key == "plugins") {
            for (auto l = i->second.begin(); l != i->second.end(); ++l) {
                for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                    std::string its_inner_key(n->first);
                    std::string its_inner_value(n->second.data());
    #ifdef _WIN32
                    std::string library(its_inner_value);
                    library += ".dll";
    #else
                    std::string library("lib");
                    library += its_inner_value;
                    library += ".so";
    #endif
                    if (its_inner_key == "application_plugin") {
    #ifndef _WIN32
                        library += ".";
                        library += std::to_string(std::uint32_t(VSOMEIP_APPLICATION_PLUGIN_VERSION));
    #endif
                        plugins[plugin_type_e::APPLICATION_PLUGIN].insert(library);
                    } else if (its_inner_key == "configuration_plugin") {
    #ifndef _WIN32
                        library += ".";
                        library += std::to_string(std::uint32_t(VSOMEIP_PRE_CONFIGURATION_PLUGIN_VERSION));
    #endif
                        plugins[plugin_type_e::PRE_CONFIGURATION_PLUGIN].insert(library);
                    } else {
                        VSOMEIP_WARNING << "Unknown plug-in type ("
                                << its_inner_key << ") configured for client: "
                                << its_name;
                    }
                }
            }
        }
    }
    if (its_name != "") {
        if (applications_.find(its_name) == applications_.end()) {
            if (its_id > 0) {
                if (!is_configured_client_id(its_id)) {
                    client_identifiers_.insert(its_id);
                } else {
                    VSOMEIP_ERROR << "Multiple applications are configured to use"
                            << " client identifier " << std::hex << its_id
                            << ". Ignoring the configuration for application "
                            << its_name;
                    its_id = 0;
                }
            }
            applications_[its_name]
                = std::make_tuple(its_id, its_max_dispatchers,
                        its_max_dispatch_time, its_io_thread_count,
                        its_request_debounce_time, plugins, its_io_thread_nice_level);
        } else {
            VSOMEIP_WARNING << "Multiple configurations for application "
                    << its_name << ". Ignoring a configuration from "
                    << _file_name;
        }
    }
}

void configuration_impl::load_tracing(const element &_element) {
    try {
        std::stringstream its_converter;
        auto its_trace_configuration = _element.tree_.get_child("tracing");
        for(auto i = its_trace_configuration.begin();
                i != its_trace_configuration.end();
                ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            if(its_key == "enable") {
                if (is_configured_[ET_TRACING_ENABLE]) {
                    VSOMEIP_WARNING << "Multiple definitions of tracing.enable."
                            << " Ignoring definition from " << _element.name_;
                } else {
                    trace_->is_enabled_ = (its_value == "true");
                    is_configured_[ET_TRACING_ENABLE] = true;
                }
            } else if (its_key == "sd_enable") {
                if (is_configured_[ET_TRACING_SD_ENABLE]) {
                    VSOMEIP_WARNING << "Multiple definitions of tracing.sd_enable."
                            << " Ignoring definition from " << _element.name_;
                } else {
                    trace_->is_sd_enabled_ = (its_value == "true");
                    is_configured_[ET_TRACING_SD_ENABLE] = true;
                }
            } else if(its_key == "channels") {
                load_trace_channels(i->second);
            } else if(its_key == "filters") {
                load_trace_filters(i->second);
            }
        }
    } catch (...) {
        // intentionally left empty
    }
}

void configuration_impl::load_trace_channels(
        const boost::property_tree::ptree &_tree) {
    try {
        for(auto i = _tree.begin(); i != _tree.end(); ++i) {
            if(i == _tree.begin())
                trace_->channels_.clear();
            load_trace_channel(i->second);
        }
    } catch (...) {
        // intentionally left empty
    }
}

void configuration_impl::load_trace_channel(
        const boost::property_tree::ptree &_tree) {
    std::shared_ptr<trace_channel> its_channel = std::make_shared<trace_channel>();
    for(auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_key = i->first;
        std::string its_value = i->second.data();
        if(its_key == "name") {
            its_channel->name_ = its_value;
        } else if(its_key == "id") {
            its_channel->id_ = its_value;
        }
    }
    trace_->channels_.push_back(its_channel);
}

void configuration_impl::load_trace_filters(
        const boost::property_tree::ptree &_tree) {
    try {
        for(auto i = _tree.begin(); i != _tree.end(); ++i) {
            load_trace_filter(i->second);
        }
    } catch (...) {
        // intentionally left empty
    }
}

void configuration_impl::load_trace_filter(
        const boost::property_tree::ptree &_tree) {
    std::shared_ptr<trace_filter> its_filter = std::make_shared<trace_filter>();
    bool has_channel(false);
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_key = i->first;
        if (its_key == "channel") {
            std::string its_value;
            if (i->second.size() == 0) {
                its_value = i->second.data();
                its_filter->channels_.push_back(its_value);
            } else {
                for (auto j = i->second.begin(); j != i->second.end(); ++j) {
                    its_filter->channels_.push_back(j->second.data());
                }
            }
            has_channel = true;
        } else if(its_key == "type") {
            std::string its_value = i->second.data();
            its_filter->is_positive_ = (its_value == "positive");
        } else {
            load_trace_filter_expressions(i->second, its_key, its_filter);
        }
    }

    if (!has_channel) {
        its_filter->channels_.push_back("TC"); // default
    }

    if (!its_filter->is_range_ || its_filter->matches_.size() == 2) {
        trace_->filters_.push_back(its_filter);
    }
}

void configuration_impl::load_trace_filter_expressions(
        const boost::property_tree::ptree &_tree,
        std::string &_criteria,
        std::shared_ptr<trace_filter> &_filter) {
    if (_criteria == "services") {
        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            vsomeip::trace::match_t its_match;
            load_trace_filter_match(i->second, its_match);
            _filter->matches_.push_back(its_match);
        }
    } else if (_criteria == "methods") {
        if (!has_issued_methods_warning_) {
            VSOMEIP_WARNING << "\"method\" entry in filter configuration has no effect!";
            has_issued_methods_warning_ = true;
        }
    } else if (_criteria == "clients") {
        if (!has_issued_clients_warning_) {
            VSOMEIP_WARNING << "\"clients\" entry in filter configuration has no effect!";
            has_issued_clients_warning_ = true;
        }
    } else if (_criteria == "matches") {
        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            vsomeip::trace::match_t its_match;
            load_trace_filter_match(i->second, its_match);
            if (i->first == "from") {
                _filter->is_range_ = true;
                _filter->matches_.insert(_filter->matches_.begin(), its_match);
            } else {
                if (i->first == "to") _filter->is_range_ = true;
                _filter->matches_.push_back(its_match);
            }
        }
    }
}

void configuration_impl::load_trace_filter_match(
        const boost::property_tree::ptree &_data,
        vsomeip::trace::match_t &_match) {
    std::stringstream its_converter;

    if (_data.size() == 0) {
        std::string its_value(_data.data());
        service_t its_service(ANY_SERVICE);
        if (its_value.find("0x") == 0) {
            its_converter << std::hex << its_value;
        } else {
            its_converter << std::dec << its_value;
        }
        its_converter >> its_service;

        std::get<0>(_match) = its_service;
        std::get<1>(_match) = ANY_INSTANCE;
        std::get<2>(_match) = ANY_METHOD;
    } else {
        std::get<0>(_match) = ANY_SERVICE;
        std::get<1>(_match) = ANY_INSTANCE;
        std::get<2>(_match) = ANY_METHOD;

        for (auto i = _data.begin(); i != _data.end(); ++i) {
            std::string its_value;

            its_converter.str("");
            its_converter.clear();

            try {
                its_value = i->second.data();
                if (its_value == "any") its_value = "0xffff";

                if (i->first == "service") {
                    service_t its_service(ANY_SERVICE);
                    if (its_value.find("0x") == 0) {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_service;
                    std::get<0>(_match) = its_service;
                } else if (i->first == "instance") {
                    instance_t its_instance(ANY_INSTANCE);
                    if (its_value.find("0x") == 0) {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_instance;
                    std::get<1>(_match) = its_instance;
                } else if (i->first == "method") {
                    method_t its_method(ANY_METHOD);
                    if (its_value.find("0x") == 0) {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_method;
                    std::get<2>(_match) = its_method;
                }
            } catch (...) {
                // Intentionally left empty
            }
        }
    }
}

void configuration_impl::load_unicast_address(const element &_element) {
    try {
        std::string its_value = _element.tree_.get<std::string>("unicast");
        if (is_configured_[ET_UNICAST]) {
            VSOMEIP_WARNING << "Multiple definitions for unicast."
                    "Ignoring definition from " << _element.name_;
        } else {
            unicast_ = unicast_.from_string(its_value);
            is_configured_[ET_UNICAST] = true;
        }
    } catch (...) {
        // intentionally left empty!
    }
}

void configuration_impl::load_netmask(const element &_element) {
    try {
        std::string its_value = _element.tree_.get<std::string>("netmask");
        if (is_configured_[ET_NETMASK]) {
            VSOMEIP_WARNING << "Multiple definitions for netmask."
                    "Ignoring definition from " << _element.name_;
        } else {
            netmask_ = netmask_.from_string(its_value);
            is_configured_[ET_NETMASK] = true;
        }
    } catch (...) {
        // intentionally left empty!
    }
}

void configuration_impl::load_network(const element &_element) {
    try {
        std::string its_value(_element.tree_.get<std::string>("network"));
        if (is_configured_[ET_NETWORK]) {
            VSOMEIP_WARNING << "Multiple definitions for network."
                    "Ignoring definition from " << _element.name_;
        } else {
            network_ = its_value;
            is_configured_[ET_NETWORK] = true;
        }
    } catch (...) {
        // intentionally left empty
    }
}

void configuration_impl::load_diagnosis_address(const element &_element) {
    try {
        std::string its_value = _element.tree_.get<std::string>("diagnosis");
        if (is_configured_[ET_DIAGNOSIS]) {
            VSOMEIP_WARNING << "Multiple definitions for diagnosis."
                    "Ignoring definition from " << _element.name_;
        } else {
            std::stringstream its_converter;

            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> diagnosis_;
            is_configured_[ET_DIAGNOSIS] = true;
        }
        std::string its_mask = _element.tree_.get<std::string>("diagnosis_mask");
        if (is_configured_[ET_DIAGNOSIS_MASK]) {
            VSOMEIP_WARNING << "Multiple definitions for diagnosis_mask."
                    "Ignoring definition from " << _element.name_;
        } else {
            std::stringstream its_converter;

            if (its_mask.size() > 1 && its_mask[0] == '0' && its_mask[1] == 'x') {
                its_converter << std::hex << its_mask;
            } else {
                its_converter << std::dec << its_mask;
            }
            its_converter >> diagnosis_mask_;
            is_configured_[ET_DIAGNOSIS_MASK] = true;
        }
    } catch (...) {
        // intentionally left empty
    }
}

void configuration_impl::load_shutdown_timeout(const element &_element) {
    const std::string shutdown_timeout("shutdown_timeout");
    try {
        if (_element.tree_.get_child_optional(shutdown_timeout)) {
            std::string its_value = _element.tree_.get<std::string>("shutdown_timeout");
            if (is_configured_[ET_SHUTDOWN_TIMEOUT]) {
                VSOMEIP_WARNING << "Multiple definitions for shutdown_timeout."
                        "Ignoring definition from " << _element.name_;
            } else {
                std::stringstream its_converter;

                if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                    its_converter << std::hex << its_value;
                } else {
                    its_converter << std::dec << its_value;
                }
                its_converter >> shutdown_timeout_;
                is_configured_[ET_SHUTDOWN_TIMEOUT] = true;
            }
        }
    } catch (...) {
        // intentionally left empty
    }
}

void configuration_impl::load_service_discovery(
        const element &_element) {
    try {
        auto its_service_discovery = _element.tree_.get_child("service-discovery");
        for (auto i = its_service_discovery.begin();
                i != its_service_discovery.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;
            if (its_key == "enable") {
                if (is_configured_[ET_SERVICE_DISCOVERY_ENABLE]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.enabled."
                            " Ignoring definition from " << _element.name_;
                } else {
                    is_sd_enabled_ = (its_value == "true");
                    is_configured_[ET_SERVICE_DISCOVERY_ENABLE] = true;
                }
            } else if (its_key == "multicast") {
                if (is_configured_[ET_SERVICE_DISCOVERY_MULTICAST]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.multicast."
                            " Ignoring definition from " << _element.name_;
                } else {
                    sd_multicast_ = its_value;
                    is_configured_[ET_SERVICE_DISCOVERY_MULTICAST] = true;
                }
            } else if (its_key == "port") {
                if (is_configured_[ET_SERVICE_DISCOVERY_PORT]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.port."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_port_;
                    if (!sd_port_) {
                        sd_port_ = VSOMEIP_SD_DEFAULT_PORT;
                    } else {
                        is_configured_[ET_SERVICE_DISCOVERY_PORT] = true;
                    }
                }
            } else if (its_key == "protocol") {
                if (is_configured_[ET_SERVICE_DISCOVERY_PROTOCOL]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.protocol."
                            " Ignoring definition from " << _element.name_;
                } else {
                    sd_protocol_ = its_value;
                    is_configured_[ET_SERVICE_DISCOVERY_PROTOCOL] = true;
                }
            } else if (its_key == "initial_delay_min") {
                if (is_configured_[ET_SERVICE_DISCOVERY_INITIAL_DELAY_MIN]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.initial_delay_min."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_initial_delay_min_;
                    is_configured_[ET_SERVICE_DISCOVERY_INITIAL_DELAY_MIN] = true;
                }
            } else if (its_key == "initial_delay_max") {
                if (is_configured_[ET_SERVICE_DISCOVERY_INITIAL_DELAY_MAX]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.initial_delay_max."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_initial_delay_max_;
                    is_configured_[ET_SERVICE_DISCOVERY_INITIAL_DELAY_MAX] = true;
                }
            } else if (its_key == "repetitions_base_delay") {
                if (is_configured_[ET_SERVICE_DISCOVERY_REPETITION_BASE_DELAY]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.repetition_base_delay."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_repetitions_base_delay_;
                    is_configured_[ET_SERVICE_DISCOVERY_REPETITION_BASE_DELAY] = true;
                }
            } else if (its_key == "repetitions_max") {
                if (is_configured_[ET_SERVICE_DISCOVERY_REPETITION_MAX]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.repetition_max."
                            " Ignoring definition from " << _element.name_;
                } else {
                    int tmp;
                    its_converter << its_value;
                    its_converter >> tmp;
                    sd_repetitions_max_ = (tmp > (std::numeric_limits<std::uint8_t>::max)()) ?
                                    (std::numeric_limits<std::uint8_t>::max)() :
                                    static_cast<std::uint8_t>(tmp);
                    is_configured_[ET_SERVICE_DISCOVERY_REPETITION_MAX] = true;
                }
            } else if (its_key == "ttl") {
                if (is_configured_[ET_SERVICE_DISCOVERY_TTL]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.ttl."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_ttl_;
                    // We do _not_ accept 0 as this would mean "STOP OFFER"
                    if (sd_ttl_ == 0) sd_ttl_ = VSOMEIP_SD_DEFAULT_TTL;
                    else is_configured_[ET_SERVICE_DISCOVERY_TTL] = true;
                }
            } else if (its_key == "cyclic_offer_delay") {
                if (is_configured_[ET_SERVICE_DISCOVERY_CYCLIC_OFFER_DELAY]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.cyclic_offer_delay."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_cyclic_offer_delay_;
                    is_configured_[ET_SERVICE_DISCOVERY_CYCLIC_OFFER_DELAY] = true;
                }
            } else if (its_key == "request_response_delay") {
                if (is_configured_[ET_SERVICE_DISCOVERY_REQUEST_RESPONSE_DELAY]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.request_response_delay."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_request_response_delay_;
                    is_configured_[ET_SERVICE_DISCOVERY_REQUEST_RESPONSE_DELAY] = true;
                }
            } else if (its_key == "offer_debounce_time") {
                if (is_configured_[ET_SERVICE_DISCOVERY_OFFER_DEBOUNCE_TIME]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.offer_debounce."
                    " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << its_value;
                    its_converter >> sd_offer_debounce_time_;
                    is_configured_[ET_SERVICE_DISCOVERY_OFFER_DEBOUNCE_TIME] = true;
                }
            } else if (its_key == "ttl_factor_offers") {
                if (is_configured_[ET_SERVICE_DISCOVERY_TTL_FACTOR_OFFERS]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.ttl_factor_offers."
                    " Ignoring definition from " << _element.name_;
                } else {
                    load_ttl_factors(i->second, &ttl_factors_offers_);
                    is_configured_[ET_SERVICE_DISCOVERY_TTL_FACTOR_OFFERS] = true;
                }
            } else if (its_key == "ttl_factor_subscriptions") {
                if (is_configured_[ET_SERVICE_DISCOVERY_TTL_FACTOR_SUBSCRIPTIONS]) {
                    VSOMEIP_WARNING << "Multiple definitions for service_discovery.ttl_factor_subscriptions."
                    " Ignoring definition from " << _element.name_;
                } else {
                    load_ttl_factors(i->second, &ttl_factors_subscriptions_);
                    is_configured_[ET_SERVICE_DISCOVERY_TTL_FACTOR_SUBSCRIPTIONS] = true;
                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_delays(
        const boost::property_tree::ptree &_tree) {
    try {
        std::stringstream its_converter;
        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            if (its_key == "initial") {
                sd_initial_delay_min_ = i->second.get<uint32_t>("minimum");
                sd_initial_delay_max_ = i->second.get<uint32_t>("maximum");
            } else if (its_key == "repetition-base") {
                its_converter << std::dec << i->second.data();
                its_converter >> sd_repetitions_base_delay_;
            } else if (its_key == "repetition-max") {
                int tmp_repetition_max;
                its_converter << std::dec << i->second.data();
                its_converter >> tmp_repetition_max;
                sd_repetitions_max_ =
                        (tmp_repetition_max
                                > (std::numeric_limits<std::uint8_t>::max)()) ?
                                        (std::numeric_limits<std::uint8_t>::max)() :
                                        static_cast<std::uint8_t>(tmp_repetition_max);
            } else if (its_key == "cyclic-offer") {
                its_converter << std::dec << i->second.data();
                its_converter >> sd_cyclic_offer_delay_;
            } else if (its_key == "cyclic-request") {
                its_converter << std::dec << i->second.data();
                its_converter >> sd_request_response_delay_;
            } else if (its_key == "ttl") {
                its_converter << std::dec << i->second.data();
                its_converter >> sd_ttl_;
            }
            its_converter.str("");
            its_converter.clear();
        }
    } catch (...) {
    }
}

void configuration_impl::load_services(const element &_element) {
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    try {
        auto its_services = _element.tree_.get_child("services");
        for (auto i = its_services.begin(); i != its_services.end(); ++i)
            load_service(i->second, "local");
    } catch (...) {
        try {
            auto its_servicegroups = _element.tree_.get_child("servicegroups");
            for (auto i = its_servicegroups.begin(); i != its_servicegroups.end(); ++i)
                load_servicegroup(i->second);
        } catch (...) {
            // intentionally left empty
        }
    }
}

void configuration_impl::load_servicegroup(
        const boost::property_tree::ptree &_tree) {
    try {
        std::string its_unicast_address("local");

        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            if (its_key == "unicast") {
                its_unicast_address = i->second.data();
                break;
            }
        }

        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            if (its_key == "delays") {
                load_delays(i->second);
            } else if (its_key == "services") {
                for (auto j = i->second.begin(); j != i->second.end(); ++j)
                    load_service(j->second, its_unicast_address);
            }
        }
    } catch (...) {
        // Intentionally left empty
    }
}

void configuration_impl::load_service(
        const boost::property_tree::ptree &_tree,
        const std::string &_unicast_address) {
    try {
        bool is_loaded(true);
        bool use_magic_cookies(false);

        std::shared_ptr<service> its_service(std::make_shared<service>());
        its_service->reliable_ = its_service->unreliable_ = ILLEGAL_PORT;
        its_service->unicast_address_ = _unicast_address;
        its_service->multicast_address_ = "";
        its_service->multicast_port_ = ILLEGAL_PORT;
        its_service->protocol_ = "someip";

        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;

            if (its_key == "unicast") {
                its_service->unicast_address_ = its_value;
            } else if (its_key == "reliable") {
                try {
                    its_value = i->second.get_child("port").data();
                    its_converter << its_value;
                    its_converter >> its_service->reliable_;
                } catch (...) {
                    its_converter << its_value;
                    its_converter >> its_service->reliable_;
                }
                if(!its_service->reliable_) {
                    its_service->reliable_ = ILLEGAL_PORT;
                }
                try {
                    its_value
                        = i->second.get_child("enable-magic-cookies").data();
                    use_magic_cookies = ("true" == its_value);
                } catch (...) {

                }
            } else if (its_key == "unreliable") {
                its_converter << its_value;
                its_converter >> its_service->unreliable_;
                if(!its_service->unreliable_) {
                    its_service->unreliable_ = ILLEGAL_PORT;
                }
            } else if (its_key == "multicast") {
                try {
                    its_value = i->second.get_child("address").data();
                    its_service->multicast_address_ = its_value;
                    its_value = i->second.get_child("port").data();
                    its_converter << its_value;
                    its_converter >> its_service->multicast_port_;
                } catch (...) {
                }
            } else if (its_key == "protocol") {
                its_service->protocol_ = its_value;
            } else if (its_key == "events") {
                load_event(its_service, i->second);
            } else if (its_key == "eventgroups") {
                load_eventgroup(its_service, i->second);
            } else {
                // Trim "its_value"
                if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                    its_converter << std::hex << its_value;
                } else {
                    its_converter << std::dec << its_value;
                }

                if (its_key == "service") {
                    its_converter >> its_service->service_;
                } else if (its_key == "instance") {
                    its_converter >> its_service->instance_;
                }
            }
        }

        auto found_service = services_.find(its_service->service_);
        if (found_service != services_.end()) {
            auto found_instance = found_service->second.find(
                    its_service->instance_);
            if (found_instance != found_service->second.end()) {
                VSOMEIP_WARNING << "Multiple configurations for service ["
                        << std::hex << its_service->service_ << "."
                        << its_service->instance_ << "]";
                is_loaded = false;
            }
        }

        if (is_loaded) {
            services_[its_service->service_][its_service->instance_] =
                    its_service;
            if (use_magic_cookies) {
                magic_cookies_[its_service->unicast_address_].insert(its_service->reliable_);
            }
        }
    } catch (...) {
        // Intentionally left empty
    }
}

void configuration_impl::load_event(
        std::shared_ptr<service> &_service,
        const boost::property_tree::ptree &_tree) {
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        event_t its_event_id(0);
        bool its_is_field(false);
        bool its_is_reliable(false);

        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            std::string its_key(j->first);
            std::string its_value(j->second.data());
            if (its_key == "event") {
                std::stringstream its_converter;
                if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                    its_converter << std::hex << its_value;
                } else {
                    its_converter << std::dec << its_value;
                }
                its_converter >> its_event_id;
            } else if (its_key == "is_field") {
                its_is_field = (its_value == "true");
            } else if (its_key == "is_reliable") {
                its_is_reliable = (its_value == "true");
            }
        }

        if (its_event_id > 0) {
            auto found_event = _service->events_.find(its_event_id);
            if (found_event != _service->events_.end()) {
                VSOMEIP_ERROR << "Multiple configurations for event ["
                        << std::hex << _service->service_ << "."
                        << _service->instance_ << "."
                        << its_event_id << "]";
            } else {
                std::shared_ptr<event> its_event = std::make_shared<event>(
                        its_event_id, its_is_field, its_is_reliable);
                _service->events_[its_event_id] = its_event;
            }
        }
    }
}

void configuration_impl::load_eventgroup(
        std::shared_ptr<service> &_service,
        const boost::property_tree::ptree &_tree) {
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::shared_ptr<eventgroup> its_eventgroup =
                std::make_shared<eventgroup>();
        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            std::stringstream its_converter;
            std::string its_key(j->first);
            std::string its_value(j->second.data());
            if (its_key == "eventgroup") {
                if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                    its_converter << std::hex << its_value;
                } else {
                    its_converter << std::dec << its_value;
                }
                its_converter >> its_eventgroup->id_;
            } else if (its_key == "is_multicast") {
                std::string its_value(j->second.data());
                if (its_value == "true") {
                    its_eventgroup->multicast_address_ = _service->multicast_address_;
                    its_eventgroup->multicast_port_ = _service->multicast_port_;
                }
            } else if (its_key == "multicast") {
                try {
                    std::string its_value = j->second.get_child("address").data();
                    its_eventgroup->multicast_address_ = its_value;
                    its_value = j->second.get_child("port").data();
                    its_converter << its_value;
                    its_converter >> its_eventgroup->multicast_port_;
                } catch (...) {
                }
            } else if (its_key == "threshold") {
                int its_threshold(0);
                std::stringstream its_converter;
                its_converter << std::dec << its_value;
                its_converter >> std::dec >> its_threshold;
                its_eventgroup->threshold_ =
                        (its_threshold > (std::numeric_limits<std::uint8_t>::max)()) ?
                                (std::numeric_limits<std::uint8_t>::max)() :
                                static_cast<uint8_t>(its_threshold);
            } else if (its_key == "events") {
                for (auto k = j->second.begin(); k != j->second.end(); ++k) {
                    std::stringstream its_converter;
                    std::string its_value(k->second.data());
                    event_t its_event_id(0);
                    if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_event_id;
                    if (0 < its_event_id) {
                        std::shared_ptr<event> its_event(nullptr);
                        auto find_event = _service->events_.find(its_event_id);
                        if (find_event != _service->events_.end()) {
                            its_event = find_event->second;
                        } else {
                            its_event = std::make_shared<event>(its_event_id,
                            false, false);
                        }
                        if (its_event) {
                            its_event->groups_.push_back(its_eventgroup);
                            its_eventgroup->events_.insert(its_event);
                            _service->events_[its_event_id] = its_event;
                        }
                    }
                }
            }
        }

        if (its_eventgroup->id_ > 0) {
            _service->eventgroups_[its_eventgroup->id_] = its_eventgroup;
        }
    }
}

void configuration_impl::load_internal_services(const element &_element) {
    try {
        auto optional = _element.tree_.get_child_optional("internal_services");
        if (!optional) {
            return;
        }
        auto its_internal_services = _element.tree_.get_child("internal_services");
        for (auto found_range = its_internal_services.begin();
                found_range != its_internal_services.end(); ++found_range) {
            service_instance_range range;
            range.first_service_ = 0x0;
            range.last_service_ = 0x0;
            range.first_instance_ = 0x0;
            range.last_instance_ = 0xffff;
            for (auto i = found_range->second.begin();
                    i != found_range->second.end(); ++i) {
                if (i->first == "first") {
                    if (i->second.size() == 0) {
                        std::stringstream its_converter;
                        std::string value = i->second.data();
                        its_converter << std::hex << value;
                        its_converter >> range.first_service_;
                    }
                    for (auto n = i->second.begin();
                            n != i->second.end(); ++n) {
                        if (n->first == "service") {
                            std::stringstream its_converter;
                            std::string value = n->second.data();
                            its_converter << std::hex << value;
                            its_converter >> range.first_service_;
                        } else if (n->first == "instance") {
                            std::stringstream its_converter;
                            std::string value = n->second.data();
                            its_converter << std::hex << value;
                            its_converter >> range.first_instance_;
                        }
                    }
                } else if (i->first == "last") {
                    if (i->second.size() == 0) {
                        std::stringstream its_converter;
                        std::string value = i->second.data();
                        its_converter << std::hex << value;
                        its_converter >> range.last_service_;
                    }
                    for (auto n = i->second.begin();
                            n != i->second.end(); ++n) {
                        if (n->first == "service") {
                            std::stringstream its_converter;
                            std::string value = n->second.data();
                            its_converter << std::hex << value;
                            its_converter >> range.last_service_;
                        } else if (n->first == "instance") {
                            std::stringstream its_converter;
                            std::string value = n->second.data();
                            its_converter << std::hex << value;
                            its_converter >> range.last_instance_;
                        }
                    }
                }
            }
            if (range.last_service_ >= range.first_service_) {
                if (range.last_instance_ >= range.first_instance_) {
                    internal_service_ranges_.push_back(range);
                }
            }
        }
    } catch (...) {
        VSOMEIP_ERROR << "Error parsing internal service range configuration!";
    }
}

void configuration_impl::load_clients(const element &_element) {
    try {
        auto its_clients = _element.tree_.get_child("clients");
        for (auto i = its_clients.begin(); i != its_clients.end(); ++i)
            load_client(i->second);
    } catch (...) {
        // intentionally left empty!
    }
}

void configuration_impl::load_client(const boost::property_tree::ptree &_tree) {
    try {
        std::shared_ptr<client> its_client(std::make_shared<client>());
        its_client->remote_ports_[true]  = std::make_pair(ILLEGAL_PORT, ILLEGAL_PORT);
        its_client->remote_ports_[false] = std::make_pair(ILLEGAL_PORT, ILLEGAL_PORT);
        its_client->client_ports_[true]  = std::make_pair(ILLEGAL_PORT, ILLEGAL_PORT);
        its_client->client_ports_[false] = std::make_pair(ILLEGAL_PORT, ILLEGAL_PORT);

        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;

            if (its_key == "reliable_remote_ports") {
                its_client->remote_ports_[true] = load_client_port_range(i->second);
            } else if (its_key == "unreliable_remote_ports") {
                its_client->remote_ports_[false] = load_client_port_range(i->second);
            } else if (its_key == "reliable_client_ports") {
                its_client->client_ports_[true] = load_client_port_range(i->second);
            } else if (its_key == "unreliable_client_ports") {
                its_client->client_ports_[false] = load_client_port_range(i->second);
            } else if (its_key == "reliable") {
                its_client->ports_[true] = load_client_ports(i->second);
            } else if (its_key == "unreliable") {
                its_client->ports_[false] = load_client_ports(i->second);
            } else {
                // Trim "its_value"
                if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                    its_converter << std::hex << its_value;
                } else {
                    its_converter << std::dec << its_value;
                }

                if (its_key == "service") {
                    its_converter >> its_client->service_;
                } else if (its_key == "instance") {
                    its_converter >> its_client->instance_;
                }
            }
        }
        clients_.push_back(its_client);
    } catch (...) {
    }
}

std::set<uint16_t> configuration_impl::load_client_ports(
        const boost::property_tree::ptree &_tree) {
    std::set<uint16_t> its_ports;
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_value(i->second.data());
        uint16_t its_port_value;

        std::stringstream its_converter;
        if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
            its_converter << std::hex << its_value;
        } else {
            its_converter << std::dec << its_value;
        }
        its_converter >> its_port_value;
        its_ports.insert(its_port_value);
    }
    return its_ports;
}

std::pair<uint16_t,uint16_t> configuration_impl::load_client_port_range(
        const boost::property_tree::ptree &_tree) {
    std::pair<uint16_t,uint16_t> its_port_range;
    uint16_t its_first_port = ILLEGAL_PORT;
    uint16_t its_last_port = ILLEGAL_PORT;

    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_key(i->first);
        std::string its_value(i->second.data());
        std::stringstream its_converter;

        if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
            its_converter << std::hex << its_value;
        } else {
            its_converter << std::dec << its_value;
        }

        if (its_key == "first") {
            its_converter >> its_first_port;
        } else if (its_key == "last") {
            its_converter >> its_last_port;
        }
    }

    if (its_last_port < its_first_port) {
        VSOMEIP_WARNING << "Port range invalid: first: " << std::dec << its_first_port << " last: " << its_last_port;
        its_port_range = std::make_pair(ILLEGAL_PORT, ILLEGAL_PORT);
    } else {
        its_port_range = std::make_pair(its_first_port, its_last_port);
    }

    return its_port_range;
}

void configuration_impl::load_watchdog(const element &_element) {
    try {
        auto its_service_discovery = _element.tree_.get_child("watchdog");
        for (auto i = its_service_discovery.begin();
                i != its_service_discovery.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;
            if (its_key == "enable") {
                if (is_configured_[ET_WATCHDOG_ENABLE]) {
                    VSOMEIP_WARNING << "Multiple definitions of watchdog.enable."
                            " Ignoring definition from " << _element.name_;
                } else {
                    watchdog_->is_enabeled_ = (its_value == "true");
                    is_configured_[ET_WATCHDOG_ENABLE] = true;
                }
            } else if (its_key == "timeout") {
                if (is_configured_[ET_WATCHDOG_TIMEOUT]) {
                    VSOMEIP_WARNING << "Multiple definitions of watchdog.timeout."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << std::dec << its_value;
                    its_converter >> watchdog_->timeout_in_ms_;
                    is_configured_[ET_WATCHDOG_TIMEOUT] = true;
                }
            } else if (its_key == "allowed_missing_pongs") {
                if (is_configured_[ET_WATCHDOG_ALLOWED_MISSING_PONGS]) {
                    VSOMEIP_WARNING << "Multiple definitions of watchdog.allowed_missing_pongs."
                            " Ignoring definition from " << _element.name_;
                } else {
                    its_converter << std::dec << its_value;
                    its_converter >> watchdog_->missing_pongs_allowed_;
                    is_configured_[ET_WATCHDOG_ALLOWED_MISSING_PONGS] = true;
                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_payload_sizes(const element &_element) {
    const std::string payload_sizes("payload-sizes");
    const std::string max_local_payload_size("max-payload-size-local");
    const std::string buffer_shrink_threshold("buffer-shrink-threshold");
    const std::string max_reliable_payload_size("max-payload-size-reliable");
    try {
        if (_element.tree_.get_child_optional(max_local_payload_size)) {
            auto mpsl = _element.tree_.get_child(max_local_payload_size);
            std::string s(mpsl.data());
            try {
                // add 16 Byte for the SOME/IP header
                max_local_message_size_ = static_cast<std::uint32_t>(std::stoul(
                        s.c_str(), NULL, 10) + 16);
            } catch (const std::exception &e) {
                VSOMEIP_ERROR<< __func__ << ": " << max_local_payload_size
                        << " " << e.what();
            }
        }
        if (_element.tree_.get_child_optional(max_reliable_payload_size)) {
            auto mpsl = _element.tree_.get_child(max_reliable_payload_size);
            std::string s(mpsl.data());
            try {
                // add 16 Byte for the SOME/IP header
                max_reliable_message_size_ = static_cast<std::uint32_t>(std::stoul(
                        s.c_str(), NULL, 10) + 16);
            } catch (const std::exception &e) {
                VSOMEIP_ERROR<< __func__ << ": " << max_reliable_payload_size
                        << " " << e.what();
            }
        }
        if (_element.tree_.get_child_optional(buffer_shrink_threshold)) {
            auto bst = _element.tree_.get_child(buffer_shrink_threshold);
            std::string s(bst.data());
            try {
                buffer_shrink_threshold_ = static_cast<std::uint32_t>(
                        std::stoul(s.c_str(), NULL, 10));
            } catch (const std::exception &e) {
                VSOMEIP_ERROR<< __func__ << ": " << buffer_shrink_threshold
                << " " << e.what();
            }
        }
        if (_element.tree_.get_child_optional(payload_sizes)) {
            const std::string unicast("unicast");
            const std::string ports("ports");
            const std::string port("port");
            const std::string max_payload_size("max-payload-size");
            auto its_ps = _element.tree_.get_child(payload_sizes);
            for (auto i = its_ps.begin(); i != its_ps.end(); ++i) {
                if (!i->second.get_child_optional(unicast)
                        || !i->second.get_child_optional(ports)) {
                    continue;
                }
                std::string its_unicast(i->second.get_child(unicast).data());
                for (auto j = i->second.get_child(ports).begin();
                        j != i->second.get_child(ports).end(); ++j) {

                    if (!j->second.get_child_optional(port)
                            || !j->second.get_child_optional(max_payload_size)) {
                        continue;
                    }

                    std::uint16_t its_port = ILLEGAL_PORT;
                    std::uint32_t its_message_size = 0;

                    try {
                        std::string p(j->second.get_child(port).data());
                        its_port = static_cast<std::uint16_t>(std::stoul(p.c_str(),
                                NULL, 10));
                        std::string s(j->second.get_child(max_payload_size).data());
                        // add 16 Byte for the SOME/IP header
                        its_message_size = static_cast<std::uint32_t>(std::stoul(
                                s.c_str(),
                                NULL, 10) + 16);
                    } catch (const std::exception &e) {
                        VSOMEIP_ERROR << __func__ << ":" << e.what();
                    }

                    if (its_port == ILLEGAL_PORT || its_message_size == 0) {
                        continue;
                    }
                    if (max_configured_message_size_ < its_message_size) {
                        max_configured_message_size_ = its_message_size;
                    }

                    message_sizes_[its_unicast][its_port] = its_message_size;
                }
            }
            if (max_local_message_size_ != 0
                    && max_configured_message_size_ != 0
                    && max_configured_message_size_ > max_local_message_size_) {
                VSOMEIP_WARNING << max_local_payload_size
                        << " is configured smaller than the biggest payloadsize"
                        << " for external communication. "
                        << max_local_payload_size << " will be increased to "
                        << max_configured_message_size_ - 16 << " to ensure "
                        << "local message distribution.";
                max_local_message_size_ = max_configured_message_size_;
            }
            if (max_local_message_size_ != 0
                    && max_reliable_message_size_ != 0
                    && max_reliable_message_size_ > max_local_message_size_) {
                VSOMEIP_WARNING << max_local_payload_size << " ("
                        << max_local_message_size_ - 16 << ") is configured"
                        << " smaller than " << max_reliable_payload_size << " ("
                        << max_reliable_message_size_ - 16 << "). "
                        << max_local_payload_size << " will be increased to "
                        << max_reliable_message_size_ - 16 << " to ensure "
                        << "local message distribution.";
                max_local_message_size_ = max_reliable_message_size_;
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_permissions(const element &_element) {
    const std::string file_permissions("file-permissions");
    try {
        if (_element.tree_.get_child_optional(file_permissions)) {
            auto its_permissions = _element.tree_.get_child(file_permissions);
            for (auto i = its_permissions.begin(); i != its_permissions.end();
                    ++i) {
                std::string its_key(i->first);
                std::stringstream its_converter;
                if (its_key == "permissions-shm") {
                    std::string its_value(i->second.data());
                    its_converter << std::oct << its_value;
                    its_converter >> permissions_shm_;
                } else if (its_key == "permissions-uds") {
                    std::string its_value(i->second.data());
                    its_converter << std::oct << its_value;
                    its_converter >> permissions_uds_;

                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_selective_broadcasts_support(const element &_element) {
    try {
        auto its_service_discovery = _element.tree_.get_child("supports_selective_broadcasts");
        for (auto i = its_service_discovery.begin();
                i != its_service_discovery.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;
            if (its_key == "address") {
                supported_selective_addresses.insert(its_value);
            }
        }
    } catch (...) {
    }
}



void configuration_impl::load_policies(const element &_element) {
#ifdef _WIN32
        return;
#endif
    try {
        auto optional = _element.tree_.get_child_optional("security");
        if (!optional) {
            return;
        }
        policy_enabled_ = true;
        auto found_policy = _element.tree_.get_child("security");
        for (auto its_security = found_policy.begin();
                its_security != found_policy.end(); ++its_security) {
            if (its_security->first == "check_credentials") {
                if (its_security->second.data() == "true") {
                    check_credentials_ = true;
                } else {
                    check_credentials_ = false;
                }
            } else if (its_security->first == "allow_remote_clients")  {
                if (its_security->second.data() == "true") {
                    allow_remote_clients_ = true;
                } else {
                    allow_remote_clients_ = false;
                }
            } else if (its_security->first == "policies") {
                for (auto its_policy = its_security->second.begin();
                        its_policy != its_security->second.end(); ++its_policy) {
                    load_policy(its_policy->second);
                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_policy(const boost::property_tree::ptree &_tree) {
    client_t client = 0x0;
    std::shared_ptr<policy> policy(std::make_shared<policy>());
    bool has_been_inserted(false);
    bool allow_deny_set(false);
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        if (i->first == "client") {
            std::string value = i->second.data();
            if (value == "") {
                client_t firstClient(ILLEGAL_CLIENT);
                client_t lastClient(ILLEGAL_CLIENT);
                for (auto n = i->second.begin();
                        n != i->second.end(); ++n) {
                    if (n->first == "first") {
                        std::stringstream its_converter;
                        std::string value = n->second.data();
                        its_converter << std::hex << value;
                        its_converter >> firstClient;
                    } else if (n->first == "last") {
                        std::stringstream its_converter;
                        std::string value = n->second.data();
                        its_converter << std::hex << value;
                        its_converter >> lastClient;
                    }
                }
                if (firstClient < lastClient && lastClient != ANY_CLIENT) {
                    uint32_t overrides(0);
                    for (client_t c = firstClient; c <= lastClient; ++c) {
                        if (find_client_id_policy(c)) {
                            overrides++;
                        }
                    }
                    if (overrides) {
                        VSOMEIP_WARNING << std::hex << "Security configuration: "
                                << "for client range 0x" << firstClient
                                << " - 0x" << lastClient
                                << " will be ignored as it would override an already existing policy of "
                                << std::dec << overrides << " clients!";
                    } else {
                        std::lock_guard<std::mutex> its_lock(policies_mutex_);
                        policies_[std::make_pair(firstClient, lastClient)] = policy;
                    }
                    has_been_inserted = true;
                } else {
                    VSOMEIP_WARNING << std::hex << "Security configuration: "
                            << "Client range have to be ascending, \"first\"=0x"
                            << firstClient << " : \"last\"=0x" << lastClient
                            << " ~> Skip policy.";
                }
            } else {
                std::stringstream its_converter;
                its_converter << std::hex << value;
                its_converter >> client;
                if (client != 0x0) {
                    if (find_client_id_policy(client)) {
                        VSOMEIP_WARNING << std::hex << "Security configuration for client "
                                << client
                                << " will be ignored as it would overwrite an already existing policy!";
                    } else {
                        std::lock_guard<std::mutex> its_lock(policies_mutex_);
                        policies_[std::make_pair(client, client)] = policy;
                    }
                    has_been_inserted= true;
                }
            }
        } else if (i->first == "credentials") {
            std::pair<uint32_t, uint32_t> its_uid_range, its_gid_range;
            ranges_t its_uid_ranges, its_gid_ranges;

            bool has_uid(false), has_gid(false);
            bool has_uid_range(false), has_gid_range(false);
            for (auto n = i->second.begin();
                    n != i->second.end(); ++n) {
                std::string its_key(n->first);
                std::string its_value(n->second.data());
                if (its_key == "uid") {
                    if(n->second.data().empty()) {
                        load_ranges(n->second, its_uid_ranges);
                        has_uid_range = true;
                    } else {
                        if (its_value != "any") {
                            uint32_t its_uid;
                            std::stringstream its_converter;
                            its_converter << std::dec << its_value;
                            its_converter >> its_uid;
                            std::get<0>(its_uid_range) = its_uid;
                            std::get<1>(its_uid_range) = its_uid;
                        } else {
                            std::get<0>(its_uid_range) = 0;
                            std::get<1>(its_uid_range) = 0xFFFFFFFF;
                        }
                        has_uid = true;
                    }
                } else if (its_key == "gid") {
                    if(n->second.data().empty()) {
                        load_ranges(n->second, its_gid_ranges);
                        has_gid_range = true;
                    } else {
                        if (its_value != "any") {
                            uint32_t its_gid;
                            std::stringstream its_converter;
                            its_converter << std::dec << its_value;
                            its_converter >> its_gid;
                            std::get<0>(its_gid_range) = its_gid;
                            std::get<1>(its_gid_range) = its_gid;
                        } else {
                            std::get<0>(its_gid_range) = 0;
                            std::get<1>(its_gid_range) = 0xFFFFFFFF;
                        }
                        has_gid = true;
                    }
                } else if (its_key == "allow" || its_key == "deny") {
                    policy->allow_who_ = (its_key == "allow");
                    load_credential(n->second, policy->ids_);
                }
            }

            if (has_uid && has_gid) {
                std::set<std::pair<uint32_t, uint32_t>> its_uids, its_gids;

                its_uids.insert(its_uid_range);
                its_gids.insert(its_gid_range);

                policy->allow_who_ = true;
                policy->ids_.insert(std::make_pair(its_uids, its_gids));
            }
            if (has_uid_range && has_gid_range) {
                policy->allow_who_ = true;
                policy->ids_.insert(std::make_pair(its_uid_ranges, its_gid_ranges));
            }
        } else if (i->first == "allow") {
            if (allow_deny_set) {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: \"allow\" tag overrides "
                        << "already set \"deny\" tag. "
                        << "Either \"deny\" or \"allow\" is allowed.";
            }
            allow_deny_set = true;
            policy->allow_what_ = true;
            for (auto l = i->second.begin(); l != i->second.end(); ++l) {
                if (l->first == "requests") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ids_t its_instance_method_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                ranges_t its_instance_ranges;
                                ranges_t its_method_ranges;
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                        its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                                its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                            } else if (k->first == "instances") { // new instances definition
                                for (auto p = k->second.begin(); p != k->second.end(); ++p) {
                                    ranges_t its_instance_ranges;
                                    ranges_t its_method_ranges;
                                    for (auto m = p->second.begin(); m != p->second.end(); ++m) {
                                        if (m->first == "ids") {
                                            load_instance_ranges(m->second, its_instance_ranges);
                                        } else if (m->first == "methods") {
                                            load_instance_ranges(m->second, its_method_ranges);
                                        }
                                        if (!its_instance_ranges.empty() && !its_method_ranges.empty()) {
                                            its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                                        }
                                    }
                                }
                                if (its_instance_method_ranges.empty()) {
                                    ranges_t its_legacy_instance_ranges;
                                    ranges_t its_legacy_method_ranges;
                                    its_legacy_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    // try to only load instance ranges with any method to be allowed
                                    load_instance_ranges(k->second, its_legacy_instance_ranges);
                                    if (!its_legacy_instance_ranges.empty() && !its_legacy_method_ranges.empty()) {
                                        its_instance_method_ranges.insert(std::make_pair(its_legacy_instance_ranges,
                                            its_legacy_method_ranges));
                                    }
                                }
                            }
                        }
                        if (service != 0x0 && !its_instance_method_ranges.empty()) {
                            policy->services_.insert(
                                    std::make_pair(service, its_instance_method_ranges));
                        }
                    }
                } else if (l->first == "offers") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ranges_t its_instance_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                            } else if (k->first == "instances") { // new instances definition
                                load_instance_ranges(k->second, its_instance_ranges);
                            }
                        }
                        if (service != 0x0 && !its_instance_ranges.empty()) {
                            policy->offers_.insert(
                                    std::make_pair(service, its_instance_ranges));
                        }
                    }
                }
            }
        } else if (i->first == "deny") {
            if (allow_deny_set) {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: \"deny\" tag overrides "
                        << "already set \"allow\" tag. "
                        << "Either \"deny\" or \"allow\" is allowed.";
            }
            allow_deny_set = true;
            policy->allow_what_ = false;
            for (auto l = i->second.begin(); l != i->second.end(); ++l) {
                if (l->first == "requests") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ids_t its_instance_method_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                ranges_t its_instance_ranges;
                                ranges_t its_method_ranges;
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                        its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                                its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                            } else if (k->first == "instances") { // new instances definition
                                for (auto p = k->second.begin(); p != k->second.end(); ++p) {
                                    ranges_t its_instance_ranges;
                                    ranges_t its_method_ranges;
                                    for (auto m = p->second.begin(); m != p->second.end(); ++m) {
                                        if (m->first == "ids") {
                                            load_instance_ranges(m->second, its_instance_ranges);
                                        } else if (m->first == "methods") {
                                            load_instance_ranges(m->second, its_method_ranges);
                                        }
                                        if (!its_instance_ranges.empty() && !its_method_ranges.empty()) {
                                            its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                                        }
                                    }
                                }
                                if (its_instance_method_ranges.empty()) {
                                    ranges_t its_legacy_instance_ranges;
                                    ranges_t its_legacy_method_ranges;
                                    its_legacy_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    // try to only load instance ranges with any method to be allowed
                                    load_instance_ranges(k->second, its_legacy_instance_ranges);
                                    if (!its_legacy_instance_ranges.empty() && !its_legacy_method_ranges.empty()) {
                                        its_instance_method_ranges.insert(std::make_pair(its_legacy_instance_ranges,
                                            its_legacy_method_ranges));
                                    }
                                }
                            }
                        }
                        if (service != 0x0 && !its_instance_method_ranges.empty()) {
                            policy->services_.insert(
                                    std::make_pair(service, its_instance_method_ranges));
                        }
                    }
                }
                if (l->first == "offers") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ranges_t its_instance_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                            } else if (k->first == "instances") { // new instances definition
                                load_instance_ranges(k->second, its_instance_ranges);
                            }
                        }
                        if (service != 0x0 && !its_instance_ranges.empty()) {
                            policy->offers_.insert(
                                    std::make_pair(service, its_instance_ranges));
                        }
                    }
                }
            }
        }
    }

    if (!has_been_inserted) {
        std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
        any_client_policies_.push_back(policy);
    }
}

void configuration_impl::load_credential(
        const boost::property_tree::ptree &_tree, ids_t &_ids) {
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        ranges_t its_uid_ranges, its_gid_ranges;
        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            std::string its_key(j->first);
            if (its_key == "uid") {
                load_ranges(j->second, its_uid_ranges);
            } else if (its_key == "gid") {
                load_ranges(j->second, its_gid_ranges);
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                        << "Malformed credential (contains illegal key \""
                        << its_key << "\"";
            }
        }

        _ids.insert(std::make_pair(its_uid_ranges, its_gid_ranges));
    }
}

void configuration_impl::load_security_update_whitelist(const element &_element) {
#ifdef _WIN32
        return;
#endif
    try {
        auto optional = _element.tree_.get_child_optional("security-update-whitelist");
        if (!optional) {
            return;
        }
        auto found_whitelist = _element.tree_.get_child("security-update-whitelist");
        for (auto its_whitelist = found_whitelist.begin();
                its_whitelist != found_whitelist.end(); ++its_whitelist) {

            if (its_whitelist->first == "uids") {
                {
                    std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
                    load_ranges(its_whitelist->second, uid_whitelist_);
                }
            } else if (its_whitelist->first == "services") {
                {
                    std::lock_guard<std::mutex> its_lock(service_interface_whitelist_mutex_);
                    load_service_ranges(its_whitelist->second, service_interface_whitelist_);
                }
            } else if (its_whitelist->first == "check-whitelist") {
                if (its_whitelist->second.data() == "true") {
                    check_whitelist_ = true;
                } else {
                    check_whitelist_ = false;
                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_ranges(
        const boost::property_tree::ptree &_tree, ranges_t &_range) {
    ranges_t its_ranges;
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        auto its_data = i->second;
        if (!its_data.data().empty()) {
            uint32_t its_id;
            std::stringstream its_converter;
            its_converter << std::dec << its_data.data();
            its_converter >> its_id;
            its_ranges.insert(std::make_pair(its_id, its_id));
        } else {
            uint32_t its_first, its_last;
            bool has_first(false), has_last(false);
            for (auto j = its_data.begin(); j != its_data.end(); ++j) {
                std::string its_key(j->first);
                std::string its_value(j->second.data());
                if (its_key == "first") {
                    if (its_value == "max") {
                        its_first = 0xFFFFFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::dec << j->second.data();
                        its_converter >> its_first;
                    }
                    has_first = true;
                } else if (its_key == "last") {
                    if (its_value == "max") {
                        its_last = 0xFFFFFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::dec << j->second.data();
                        its_converter >> its_last;
                    }
                    has_last = true;
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                            << " Malformed range. Contains illegal key ("
                            << its_key << ")";
                }
            }

            if (has_first && has_last) {
                its_ranges.insert(std::make_pair(its_first, its_last));
            }
        }
    }

    _range = its_ranges;
}

void configuration_impl::load_instance_ranges(
        const boost::property_tree::ptree &_tree, ranges_t &_range) {
    ranges_t its_ranges;
    std::string key(_tree.data());
    if (key == "any") {
        its_ranges.insert(std::make_pair(0x01, 0xFFFF));
        _range = its_ranges;
        return;
    }
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        auto its_data = i->second;
        if (!its_data.data().empty()) {
            uint32_t its_id = 0x0;
            std::stringstream its_converter;
            its_converter << std::hex << its_data.data();
            its_converter >> its_id;
            if (its_id != 0x0) {
                its_ranges.insert(std::make_pair(its_id, its_id));
            }
        } else {
            uint32_t its_first, its_last;
            bool has_first(false), has_last(false);
            for (auto j = its_data.begin(); j != its_data.end(); ++j) {
                std::string its_key(j->first);
                std::string its_value(j->second.data());
                if (its_key == "first") {
                    if (its_value == "max") {
                        its_first = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_first;
                    }
                    has_first = true;
                } else if (its_key == "last") {
                    if (its_value == "max") {
                        its_last = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_last;
                    }
                    has_last = true;
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                            << " Malformed range. Contains illegal key ("
                            << its_key << ")";
                }
            }

            if (has_first && has_last) {
                if( its_last > its_first) {
                    its_ranges.insert(std::make_pair(its_first, its_last));
                }
            }
        }
    }

    _range = its_ranges;
}

void configuration_impl::load_service_ranges(
        const boost::property_tree::ptree &_tree, std::set<std::pair<service_t, service_t>> &_ranges) {
    std::set<std::pair<service_t, service_t>> its_ranges;
    std::string key(_tree.data());
    if (key == "any") {
        its_ranges.insert(std::make_pair(0x01, 0xFFFF));
        _ranges = its_ranges;
        return;
    }
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        auto its_data = i->second;
        if (!its_data.data().empty()) {
            service_t its_id = 0x0;
            std::stringstream its_converter;
            its_converter << std::hex << its_data.data();
            its_converter >> its_id;
            if (its_id != 0x0) {
                its_ranges.insert(std::make_pair(its_id, its_id));
            }
        } else {
            service_t its_first, its_last;
            bool has_first(false), has_last(false);
            for (auto j = its_data.begin(); j != its_data.end(); ++j) {
                std::string its_key(j->first);
                std::string its_value(j->second.data());
                if (its_key == "first") {
                    if (its_value == "max") {
                        its_first = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_first;
                    }
                    has_first = true;
                } else if (its_key == "last") {
                    if (its_value == "max") {
                        its_last = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_last;
                    }
                    has_last = true;
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Security interface whitelist configuration: "
                            << " Malformed range. Contains illegal key ("
                            << its_key << ")";
                }
            }

            if (has_first && has_last) {
                if( its_last >= its_first) {
                    its_ranges.insert(std::make_pair(its_first, its_last));
                }
            }
        }
    }

    _ranges = its_ranges;
}

///////////////////////////////////////////////////////////////////////////////
// Internal helper
///////////////////////////////////////////////////////////////////////////////
void configuration_impl::set_magic_cookies_unicast_address() {
    // get services with static routing that have magic cookies enabled
    std::map<std::string, std::set<uint16_t> > its_magic_cookies_ = magic_cookies_;
    its_magic_cookies_.erase("local");

    //set unicast address of host for all services without static routing
    its_magic_cookies_[get_unicast_address().to_string()].insert(magic_cookies_["local"].begin(),
            magic_cookies_["local"].end());
    magic_cookies_.clear();
    magic_cookies_ = its_magic_cookies_;
}

bool configuration_impl::is_internal_service(service_t _service,
        instance_t _instance) const {

    for (auto its_range : internal_service_ranges_) {
        if (_service >= its_range.first_service_ &&
                _service <= its_range.last_service_ &&
                _instance >= its_range.first_instance_ &&
                _instance <= its_range.last_instance_) {
            return true;
        }
    }
    return false;
}

bool configuration_impl::is_in_port_range(uint16_t _port,
      std::pair<uint16_t, uint16_t> _port_range) const {

    if (_port >= _port_range.first &&
            _port <= _port_range.second ) {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Public interface
///////////////////////////////////////////////////////////////////////////////
const std::string &configuration_impl::get_network() const {
    return network_;
}

const boost::asio::ip::address & configuration_impl::get_unicast_address() const {
    return unicast_;
}

const boost::asio::ip::address& configuration_impl::get_netmask() const {
    return netmask_;
}

unsigned short configuration_impl::get_diagnosis_address() const {
    return diagnosis_;
}

std::uint16_t configuration_impl::get_diagnosis_mask() const {
    return diagnosis_mask_;
}

bool configuration_impl::is_v4() const {
    return unicast_.is_v4();
}

bool configuration_impl::is_v6() const {
    return unicast_.is_v6();
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

std::string configuration_impl::get_unicast_address(service_t _service,
        instance_t _instance) const {
    std::string its_unicast_address("");
    auto its_service = find_service(_service, _instance);
    if (its_service) {
        its_unicast_address = its_service->unicast_address_;
    }

    if (its_unicast_address == "local" || its_unicast_address == "") {
            its_unicast_address = get_unicast_address().to_string();
    }
    return its_unicast_address;
}

uint16_t configuration_impl::get_reliable_port(service_t _service,
        instance_t _instance) const {
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    uint16_t its_reliable(ILLEGAL_PORT);
    auto its_service = find_service_unlocked(_service, _instance);
    if (its_service)
        its_reliable = its_service->reliable_;

    return its_reliable;
}

uint16_t configuration_impl::get_unreliable_port(service_t _service,
        instance_t _instance) const {
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    uint16_t its_unreliable = ILLEGAL_PORT;
     auto its_service = find_service_unlocked(_service, _instance);
    if (its_service)
        its_unreliable = its_service->unreliable_;

    return its_unreliable;
}

bool configuration_impl::is_someip(service_t _service,
        instance_t _instance) const {
    auto its_service = find_service(_service, _instance);
    if (its_service)
        return (its_service->protocol_ == "someip");
    return true; // we need to explicitely configure a service to
                 // be something else than SOME/IP
}

bool configuration_impl::get_client_port(
        service_t _service, instance_t _instance,
        uint16_t _remote_port, bool _reliable,
        std::map<bool, std::set<uint16_t> > &_used_client_ports,
        uint16_t &_client_port) const {
    bool is_configured(false);

    _client_port = ILLEGAL_PORT;
    auto its_client = find_client(_service, _instance);

    // Check for service, instance specific port configuration
    if (its_client  && !its_client->ports_[_reliable].empty()) {
        is_configured = true;
        for (auto its_port : its_client->ports_[_reliable]) {
            // Found free configured port
            if (_used_client_ports[_reliable].find(its_port) == _used_client_ports[_reliable].end()) {
                _client_port = its_port;
                return true;
            }
        }
    }

    // No specific port configuration found, use generic configuration
    uint16_t its_port(ILLEGAL_PORT);
    if (find_port(its_port, _remote_port, _reliable, _used_client_ports)) {
        is_configured = true;
        if (its_port != ILLEGAL_PORT) {
            _client_port = its_port;
            return true;
        }
    }

    if (!is_configured) {
        // Neither specific not generic configurarion available,
        // use dynamic port configuration!
        return true;
    }

    // Configured ports do exist, but they are all in use
    VSOMEIP_ERROR << "Cannot find free client port for communication to service: "
            << _service << " instance: "
            << _instance << " remote_port: "
            << _remote_port << " reliable: "
            << _reliable;
    return false;
}

bool configuration_impl::has_enabled_magic_cookies(std::string _address,
        uint16_t _port) const {
    bool has_enabled(false);
    auto find_address = magic_cookies_.find(_address);
    if (find_address != magic_cookies_.end()) {
        auto find_port = find_address->second.find(_port);
        if (find_port != find_address->second.end()) {
            has_enabled = true;
        }
    }
    return has_enabled;
}


const std::string & configuration_impl::get_routing_host() const {
    return routing_host_;
}

client_t configuration_impl::get_id(const std::string &_name) const {
    client_t its_client = 0;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_client = std::get<0>(found_application->second);
    }

    return its_client;
}

bool configuration_impl::is_configured_client_id(client_t _id) const {
    return (client_identifiers_.find(_id) != client_identifiers_.end());
}

std::size_t configuration_impl::get_request_debouncing(const std::string &_name) const {
    size_t debounce_time = VSOMEIP_REQUEST_DEBOUNCE_TIME;
    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        debounce_time = std::get<4>(found_application->second);
    }

    return debounce_time;
}

std::size_t configuration_impl::get_io_thread_count(const std::string &_name) const {
    std::size_t its_io_thread_count = VSOMEIP_IO_THREAD_COUNT;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_io_thread_count = std::get<3>(found_application->second);
    }

    return its_io_thread_count;
}

int configuration_impl::get_io_thread_nice_level(const std::string &_name) const {
    int its_io_thread_nice_level = VSOMEIP_IO_THREAD_NICE_LEVEL;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_io_thread_nice_level = std::get<6>(found_application->second);
    }

    return its_io_thread_nice_level;
}

std::size_t configuration_impl::get_max_dispatchers(
        const std::string &_name) const {
    std::size_t its_max_dispatchers = VSOMEIP_MAX_DISPATCHERS;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_max_dispatchers = std::get<1>(found_application->second);
    }

    return its_max_dispatchers;
}

std::size_t configuration_impl::get_max_dispatch_time(
        const std::string &_name) const {
    std::size_t its_max_dispatch_time = VSOMEIP_MAX_DISPATCH_TIME;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_max_dispatch_time = std::get<2>(found_application->second);
    }

    return its_max_dispatch_time;
}

std::set<std::pair<service_t, instance_t> >
configuration_impl::get_remote_services() const {
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    std::set<std::pair<service_t, instance_t> > its_remote_services;
    for (auto i : services_) {
        for (auto j : i.second) {
            if (is_remote(j.second)) {
                its_remote_services.insert(std::make_pair(i.first, j.first));
            }
        }
    }
    return its_remote_services;
}

bool configuration_impl::is_mandatory(const std::string &_name) const {
    std::set<std::string> its_candidates;
    for (auto m : mandatory_) {
        if (m.size() <= _name.size()) {
            its_candidates.insert(m);
        }
    }

    if (its_candidates.empty())
        return false;

    for (auto c : its_candidates) {
        if (std::equal(c.rbegin(), c.rend(), _name.rbegin())) {
            return true;
        }
    }

    return false;
}

void configuration_impl::set_mandatory(const std::string &_input) {
    if (_input.length() > 0) {
        auto found_separator = _input.find(',');
        std::string its_mandatory_file = _input.substr(0, found_separator);
        trim(its_mandatory_file);
        mandatory_.insert(its_mandatory_file);
        while (found_separator != std::string::npos) {
            auto last_separator = found_separator+1;
            found_separator = _input.find(',', last_separator);
            its_mandatory_file
                = _input.substr(last_separator, found_separator - last_separator);
            trim(its_mandatory_file);
            mandatory_.insert(its_mandatory_file);
        }
    }
}

void configuration_impl::trim(std::string &_s) {
    _s.erase(
        _s.begin(),
        std::find_if(
            _s.begin(),
            _s.end(),
            std::not1(std::ptr_fun(isspace))
        )
    );

    _s.erase(
        std::find_if(
            _s.rbegin(),
            _s.rend(),
            std::not1(std::ptr_fun(isspace))).base(),
            _s.end()
    );
}

bool configuration_impl::is_remote(std::shared_ptr<service> _service) const {
    return  (_service->unicast_address_ != "local" &&
            _service->unicast_address_ != "" &&
            _service->unicast_address_ != unicast_.to_string() &&
            _service->unicast_address_ != VSOMEIP_UNICAST_ADDRESS);
}

bool configuration_impl::get_multicast(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            std::string &_address, uint16_t &_port) const
{
    std::shared_ptr<eventgroup> its_eventgroup
        = find_eventgroup(_service, _instance, _eventgroup);
    if (!its_eventgroup)
        return false;

    if (its_eventgroup->multicast_address_.empty())
        return false;

    _address = its_eventgroup->multicast_address_;
    _port = its_eventgroup->multicast_port_;
    return true;
}

uint8_t configuration_impl::get_threshold(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) const {
    std::shared_ptr<eventgroup> its_eventgroup
        = find_eventgroup(_service, _instance, _eventgroup);
    return (its_eventgroup ? its_eventgroup->threshold_ : 0);
}

std::shared_ptr<client> configuration_impl::find_client(service_t _service,
        instance_t _instance) const {
    std::list<std::shared_ptr<client>>::const_iterator it;

    for (it = clients_.begin(); it != clients_.end(); ++it){
        // client was configured for specific service / instance
        if ((*it)->service_ == _service
                && (*it)->instance_ == _instance) {
            return *it;
        }
    }
    return nullptr;
}

bool configuration_impl::find_port(uint16_t &_port, uint16_t _remote, bool _reliable,
        std::map<bool, std::set<uint16_t> > &_used_client_ports) const {
    bool is_configured(false);
    std::list<std::shared_ptr<client>>::const_iterator it;

    for (it = clients_.begin(); it != clients_.end(); ++it) {
        if (is_in_port_range(_remote, (*it)->remote_ports_[_reliable])) {
            is_configured = true;
            for (uint16_t its_port = (*it)->client_ports_[_reliable].first;
                    its_port <= (*it)->client_ports_[_reliable].second;  its_port++ ) {
                if (_used_client_ports[_reliable].find(its_port) == _used_client_ports[_reliable].end()) {
                    _port = its_port;
                    return true;
                }
            }
        }
    }

    return is_configured;
}

bool configuration_impl::is_event_reliable(service_t _service,
        instance_t _instance, event_t _event) const {
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    bool is_reliable(false);
    auto its_service = find_service_unlocked(_service, _instance);
    if (its_service) {
        auto its_event = its_service->events_.find(_event);
        if (its_event != its_service->events_.end()) {
            return its_event->second->is_reliable_;
        } else {
            if (its_service->reliable_ != ILLEGAL_PORT &&
                    its_service->unreliable_ == ILLEGAL_PORT) {
                is_reliable = true;
            }
        }
    }
    return is_reliable;
}

std::shared_ptr<service> configuration_impl::find_service(service_t _service,
        instance_t _instance) const {
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    return find_service_unlocked(_service, _instance);
}

std::shared_ptr<service> configuration_impl::find_service_unlocked(service_t _service,
        instance_t _instance) const {
    std::shared_ptr<service> its_service;
    auto find_service = services_.find(_service);
    if (find_service != services_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
            its_service = find_instance->second;
        }
    }
    return its_service;
}

std::shared_ptr<eventgroup> configuration_impl::find_eventgroup(
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) const {
    std::shared_ptr<eventgroup> its_eventgroup;
    auto its_service = find_service(_service, _instance);
    if (its_service) {
        auto find_eventgroup = its_service->eventgroups_.find(_eventgroup);
        if (find_eventgroup != its_service->eventgroups_.end()) {
            its_eventgroup = find_eventgroup->second;
        }
    }
    return its_eventgroup;
}

std::uint32_t configuration_impl::get_max_message_size_local() const {
    if (max_local_message_size_ == 0
            && (VSOMEIP_MAX_LOCAL_MESSAGE_SIZE == 0
                    || VSOMEIP_MAX_TCP_MESSAGE_SIZE == 0)) {
        // no limit specified in configuration file and
        // defines are set to unlimited
        return MESSAGE_SIZE_UNLIMITED;
    }

    uint32_t its_max_message_size = max_local_message_size_;
    if (VSOMEIP_MAX_TCP_MESSAGE_SIZE >= its_max_message_size) {
        its_max_message_size = VSOMEIP_MAX_TCP_MESSAGE_SIZE;
    }
    if (VSOMEIP_MAX_UDP_MESSAGE_SIZE > its_max_message_size) {
        its_max_message_size = VSOMEIP_MAX_UDP_MESSAGE_SIZE;
    }
    if (its_max_message_size < max_configured_message_size_) {
        its_max_message_size = max_configured_message_size_;
    }

    // add sizes of the the routing_manager_proxy's messages
    // to the routing_manager stub
    return std::uint32_t(its_max_message_size + VSOMEIP_SEND_COMMAND_SIZE);
}

std::uint32_t configuration_impl::get_max_message_size_reliable(
        const std::string& _address, std::uint16_t _port) const {
    const auto its_address = message_sizes_.find(_address);
    if(its_address != message_sizes_.end()) {
        const auto its_port = its_address->second.find(_port);
        if(its_port != its_address->second.end()) {
            return its_port->second;
        }
    }
    return (max_reliable_message_size_ == 0) ?
            ((VSOMEIP_MAX_TCP_MESSAGE_SIZE == 0) ? MESSAGE_SIZE_UNLIMITED :
                    VSOMEIP_MAX_TCP_MESSAGE_SIZE) : max_reliable_message_size_;
}

std::uint32_t configuration_impl::get_buffer_shrink_threshold() const {
    return buffer_shrink_threshold_;
}

bool configuration_impl::supports_selective_broadcasts(boost::asio::ip::address _address) const {
    return supported_selective_addresses.find(_address.to_string()) != supported_selective_addresses.end();
}

bool configuration_impl::log_version() const {
    return log_version_;
}

uint32_t configuration_impl::get_log_version_interval() const {
    return log_version_interval_;
}

bool configuration_impl::is_offered_remote(service_t _service, instance_t _instance) const {
    uint16_t reliable_port = get_reliable_port(_service, _instance);
    uint16_t unreliable_port = get_unreliable_port(_service, _instance);
    return (reliable_port != ILLEGAL_PORT || unreliable_port != ILLEGAL_PORT);
}

bool configuration_impl::is_local_service(service_t _service, instance_t _instance) const {
    std::shared_ptr<service> s = find_service(_service, _instance);
    if (s && !is_remote(s)) {
        return true;
    }
    if (is_internal_service(_service, _instance)) {
        return true;
    }

    return false;
}

// Service Discovery configuration
bool configuration_impl::is_sd_enabled() const {
    return is_sd_enabled_;
}

const std::string & configuration_impl::get_sd_multicast() const {
    return sd_multicast_;
}

uint16_t configuration_impl::get_sd_port() const {
    return sd_port_;
}

const std::string & configuration_impl::get_sd_protocol() const {
    return sd_protocol_;
}

int32_t configuration_impl::get_sd_initial_delay_min() const {
    return sd_initial_delay_min_;
}

int32_t configuration_impl::get_sd_initial_delay_max() const {
    return sd_initial_delay_max_;
}

int32_t configuration_impl::get_sd_repetitions_base_delay() const {
    return sd_repetitions_base_delay_;
}

uint8_t configuration_impl::get_sd_repetitions_max() const {
    return sd_repetitions_max_;
}

ttl_t configuration_impl::get_sd_ttl() const {
    return sd_ttl_;
}

int32_t configuration_impl::get_sd_cyclic_offer_delay() const {
    return sd_cyclic_offer_delay_;
}

int32_t configuration_impl::get_sd_request_response_delay() const {
    return sd_request_response_delay_;
}

std::uint32_t configuration_impl::get_sd_offer_debounce_time() const {
    return sd_offer_debounce_time_;
}

// Trace configuration
std::shared_ptr<cfg::trace> configuration_impl::get_trace() const {
    return trace_;
}

// Watchdog config
bool configuration_impl::is_watchdog_enabled() const {
    return watchdog_->is_enabeled_;
}

uint32_t configuration_impl::get_watchdog_timeout() const {
    return watchdog_->timeout_in_ms_;
}

uint32_t configuration_impl::get_allowed_missing_pongs() const {
    return watchdog_->missing_pongs_allowed_;
}
std::uint32_t configuration_impl::get_permissions_uds() const {
    return permissions_uds_;
}

std::uint32_t configuration_impl::get_permissions_shm() const {
    return permissions_shm_;
}

bool configuration_impl::is_security_enabled() const {
    return policy_enabled_;
}

bool configuration_impl::is_audit_mode_enabled() const {
    return check_credentials_;
}

bool configuration_impl::check_credentials(client_t _client, uint32_t _uid,
        uint32_t _gid) {
    if (!policy_enabled_) {
        return true;
    }

    std::vector<std::shared_ptr<policy> > its_policies;
    bool has_id(false);
    auto found_policy = find_client_id_policy(_client);
    if (found_policy) {
        its_policies.push_back(found_policy);
    } else {
        std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
        its_policies = any_client_policies_;
    }

    for (const auto &p : its_policies) {
        for (auto its_credential : p->ids_) {
            bool has_uid(false), has_gid(false);
            for (auto its_range : std::get<0>(its_credential)) {
                if (std::get<0>(its_range) <= _uid && _uid <= std::get<1>(its_range)) {
                    has_uid = true;
                    break;
                }
            }
            for (auto its_range : std::get<1>(its_credential)) {
                if (std::get<0>(its_range) <= _gid && _gid <= std::get<1>(its_range)) {
                    has_gid = true;
                    break;
                }
            }

            if (has_uid && has_gid) {
                has_id = true;
                break;
            }
        }

        if ((has_id && p->allow_who_) || (!has_id && !p->allow_who_)) {
            if (!store_client_to_uid_gid_mapping(_client,_uid, _gid)) {
                std::string security_mode_text = "!";
                if (!check_credentials_) {
                    security_mode_text = " but will be allowed due to audit mode is active!";
                }

                VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
                        << " with UID/GID=" << std::dec << _uid << "/" << _gid
                        << " : Check credentials failed as existing credentials would be overwritten"
                        << security_mode_text;

                return !check_credentials_;
            }
            store_uid_gid_to_client_mapping(_uid, _gid, _client);
            return true;
        }
    }

    std::string security_mode_text = " ~> Skip!";
    if (!check_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }
    VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
                 << " with UID/GID=" << std::dec << _uid << "/" << _gid
                 << " : Check credentials failed" << security_mode_text;


    return !check_credentials_;
}

bool configuration_impl::is_client_allowed(client_t _client, service_t _service,
        instance_t _instance, method_t _method, bool _is_request_service) const {
    if (!policy_enabled_) {
        return true;
    }

    uint32_t its_uid(0xffffffff), its_gid(0xffffffff);
    bool must_apply(true);
    std::vector<std::shared_ptr<policy> > its_policies;
    auto found_policy = find_client_id_policy(_client);
    if (found_policy)
        its_policies.push_back(found_policy);
    else {
        must_apply = false;
        {
            std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
            its_policies = any_client_policies_;
        }

        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        auto found_id = ids_.find(_client);
        if (found_id != ids_.end()) {
            its_uid = found_id->second.first;
            its_gid = found_id->second.second;
        } else {
            std::string security_mode_text = " ~> Skip!";
            if (!check_credentials_) {
                security_mode_text = " but will be allowed due to audit mode is active!";
            }
            VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
                    << " : Cannot determine uid/gid. Therefore it isn't allowed to communicate to service/instance "
                    << _service << "/" << _instance
                    << security_mode_text;

            return !check_credentials_;
        }
    }

    for (const auto &p : its_policies) {
        bool has_uid(false), has_gid(false), has_service(false), has_instance_id(false), has_method_id(false);
        if (must_apply) {
            has_uid = has_gid = p->allow_who_;
        } else {
            for (auto its_credential : p->ids_) {
                has_uid = has_gid = false;
                for (auto its_range : std::get<0>(its_credential)) {
                    if (std::get<0>(its_range) <= its_uid && its_uid <= std::get<1>(its_range)) {
                        has_uid = true;
                        break;
                    }
                }
                for (auto its_range : std::get<1>(its_credential)) {
                    if (std::get<0>(its_range) <= its_gid && its_gid <= std::get<1>(its_range)) {
                        has_gid = true;
                        break;
                    }
                }

                if (has_uid && has_gid)
                    break;
            }
        }

        for (auto its_offer : p->services_) {
            if (std::get<0>(its_offer) == _service) {
                for (auto its_ids : std::get<1>(its_offer)) {
                    has_service = has_instance_id = has_method_id = false;
                    for (auto its_instance_range : std::get<0>(its_ids)) {
                        if (std::get<0>(its_instance_range) <= _instance && _instance <= std::get<1>(its_instance_range)) {
                            has_instance_id = true;
                            break;
                        }
                    }
                    if (!_is_request_service) {
                        for (auto its_method_range : std::get<1>(its_ids)) {
                            if (std::get<0>(its_method_range) <= _method && _method <= std::get<1>(its_method_range)) {
                                has_method_id = true;
                                break;
                            }
                        }
                    } else {
                        // handle VSOMEIP_REQUEST_SERVICE
                        has_method_id = true;
                    }

                    if (has_instance_id && has_method_id) {
                        has_service = true;
                        break;
                    }
                }
                if (has_service)
                    break;
            }
        }

        if ((has_uid && has_gid && p->allow_who_) || ((!has_uid || !has_gid) && !p->allow_who_)) {
            if (p->allow_what_) {
                // allow policy
                if (has_service) {
                    return true;
                }
            } else {
                // deny policy
                // allow client if the service / instance / !ANY_METHOD was not found
                if ((!has_service && (_method != ANY_METHOD))
                        // allow client if the service / instance / ANY_METHOD was not found
                        // and it is a "deny nothing" policy
                        || (!has_service && (_method == ANY_METHOD) && p->services_.empty())) {
                    return true;
                }
            }
        }
    }

    std::string security_mode_text = " ~> Skip!";
    if (!check_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }

    VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
            << " with UID/GID=" << std::dec << its_uid << "/" << its_gid
            << " : Isn't allowed to communicate with service/instance/(method / event) " << std::hex
            << _service << "/" << _instance << "/" << _method
            << security_mode_text;

    return !check_credentials_;
}

bool configuration_impl::is_offer_allowed(client_t _client, service_t _service,
        instance_t _instance) const {
    if (!policy_enabled_) {
        return true;
    }

    uint32_t its_uid(0xffffffff), its_gid(0xffffffff);
    bool must_apply(true);
    std::vector<std::shared_ptr<policy> > its_policies;
    auto found_policy = find_client_id_policy(_client);
    if (found_policy)
        its_policies.push_back(found_policy);
    else {
        must_apply = false;
        {
            std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
            its_policies = any_client_policies_;
        }

        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        auto found_id = ids_.find(_client);
        if (found_id != ids_.end()) {
            its_uid = found_id->second.first;
            its_gid = found_id->second.second;
        } else {
            std::string audit_mode_text = " ~> Skip offer!";
            if (!check_credentials_) {
                audit_mode_text = " but will be allowed due to audit mode is active!";
            }

            VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
                    << " : Cannot determine uid/gid. Therefore it isn't allowed to offer service/instance "
                    << _service << "/" << _instance << audit_mode_text;

            return !check_credentials_;
        }
    }

    for (const auto &p : its_policies) {
        bool has_uid(false), has_gid(false), has_offer(false);
        if (must_apply) {
            has_uid = has_gid = p->allow_who_;
        } else {
            for (auto its_credential : p->ids_) {
                has_uid = has_gid = false;
                for (auto its_range : std::get<0>(its_credential)) {
                    if (std::get<0>(its_range) <= its_uid && its_uid <= std::get<1>(its_range)) {
                        has_uid = true;
                        break;
                    }
                }
                for (auto its_range : std::get<1>(its_credential)) {
                    if (std::get<0>(its_range) <= its_gid && its_gid <= std::get<1>(its_range)) {
                        has_gid = true;
                        break;
                    }
                }

                if (has_uid && has_gid)
                    break;
            }
        }

        for (auto its_offer : p->offers_) {
            has_offer = false;
            if (std::get<0>(its_offer) == _service) {
                for (auto its_instance_range : std::get<1>(its_offer)) {
                    if (std::get<0>(its_instance_range) <= _instance && _instance <= std::get<1>(its_instance_range)) {
                        has_offer = true;
                        break;
                    }
                }
                if (has_offer)
                    break;
            }
        }

        if ((has_uid && has_gid && p->allow_who_) || ((!has_uid || !has_gid) && !p->allow_who_)) {
            if (p->allow_what_ == has_offer) {
                return true;
            }
        }
    }

    std::string security_mode_text = " ~> Skip offer!";
    if (!check_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }

    VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
            << " with UID/GID=" << std::dec << its_uid << "/" << its_gid
            << " isn't allowed to offer service/instance " << std::hex
            << _service << "/" << _instance
            << security_mode_text;


    return !check_credentials_;
}

bool configuration_impl::store_client_to_uid_gid_mapping(client_t _client,
                                                         uint32_t _uid, uint32_t _gid) {
    {
        // store the client -> (uid, gid) mapping
        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        auto found_client = ids_.find(_client);
        if (found_client != ids_.end()) {
            if (found_client->second != std::make_pair(_uid, _gid)) {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x"
                        << std::hex << _client << " with UID/GID="
                        << std::dec << _uid << "/" << _gid << " : Overwriting existing credentials UID/GID="
                        << std::dec << std::get<0>(found_client->second) << "/"
                        << std::get<1>(found_client->second);
                found_client->second = std::make_pair(_uid, _gid);
                return true;
            }
        } else {
            ids_[_client] = std::make_pair(_uid, _gid);
        }
        return true;
    }
}

bool configuration_impl::get_client_to_uid_gid_mapping(client_t _client, std::pair<uint32_t, uint32_t> &_uid_gid) {
    {
        // get the UID / GID of the client
        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        if (ids_.find(_client) != ids_.end()) {
            _uid_gid = ids_[_client];
            return true;
        }
        return false;
    }
}

bool configuration_impl::remove_client_to_uid_gid_mapping(client_t _client) {
    std::pair<uint32_t, uint32_t> its_uid_gid;
    bool client_removed(false);
    bool uid_gid_removed(false);
    {
        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        auto found_client = ids_.find(_client);
        if (found_client != ids_.end()) {
            its_uid_gid = found_client->second;
            ids_.erase(found_client);
            client_removed = true;
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(uid_to_clients_mutex_);
        if (client_removed) {
            auto found_uid_gid = uid_to_clients_.find(its_uid_gid);
            if (found_uid_gid != uid_to_clients_.end()) {
               auto its_client = found_uid_gid->second.find(_client);
               if (its_client != found_uid_gid->second.end()) {
                   found_uid_gid->second.erase(its_client);
                   if (found_uid_gid->second.empty()) {
                       uid_to_clients_.erase(found_uid_gid);
                   }
                   uid_gid_removed = true;
               }
            }
        } else {
            for (auto its_uid_gid = uid_to_clients_.begin();
                    its_uid_gid != uid_to_clients_.end(); ++its_uid_gid) {
                auto its_client = its_uid_gid->second.find(_client);
                if (its_client != its_uid_gid->second.end()) {
                    its_uid_gid->second.erase(its_client);
                    if (its_uid_gid->second.empty()) {
                        uid_to_clients_.erase(its_uid_gid);
                    }
                    uid_gid_removed = true;
                    break;
                }
            }
        }
    }
    return (client_removed && uid_gid_removed);
}

void configuration_impl::store_uid_gid_to_client_mapping(uint32_t _uid, uint32_t _gid,
        client_t _client) {
    {
        // store the uid gid to clients mapping
        std::lock_guard<std::mutex> its_lock(uid_to_clients_mutex_);
        std::set<client_t> mapped_clients;
        if (uid_to_clients_.find(std::make_pair(_uid, _gid)) != uid_to_clients_.end()) {
            mapped_clients = uid_to_clients_[std::make_pair(_uid, _gid)];
            mapped_clients.insert(_client);
            uid_to_clients_[std::make_pair(_uid, _gid)] = mapped_clients;
        } else {
            mapped_clients.insert(_client);
            uid_to_clients_[std::make_pair(_uid, _gid)] = mapped_clients;
        }
    }
}

bool configuration_impl::get_uid_gid_to_client_mapping(std::pair<uint32_t, uint32_t> _uid_gid,
        std::set<client_t> &_clients) {
    {
        // get the clients corresponding to uid, gid
        std::lock_guard<std::mutex> its_lock(uid_to_clients_mutex_);
        if (uid_to_clients_.find(_uid_gid) != uid_to_clients_.end()) {
            _clients = uid_to_clients_[_uid_gid];
            return true;
        }
        return false;
    }
}

std::map<plugin_type_e, std::set<std::string>> configuration_impl::get_plugins(
            const std::string &_name) const {
    std::map<plugin_type_e, std::set<std::string>> result;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        result = std::get<5>(found_application->second);
    }

    return result;
}

void configuration_impl::set_configuration_path(const std::string &_path) {
    configuration_path_ = _path;
}

bool configuration_impl::is_e2e_enabled() const {
    return e2e_enabled_;
}

void configuration_impl::load_e2e(const element &_element) {
#ifdef _WIN32
        return;
#endif
    try {
        auto optional = _element.tree_.get_child_optional("e2e");
        if (!optional) {
            return;
        }
        auto found_e2e = _element.tree_.get_child("e2e");
        for (auto its_e2e = found_e2e.begin();
                its_e2e != found_e2e.end(); ++its_e2e) {
            if (its_e2e->first == "e2e_enabled") {
                if (its_e2e->second.data() == "true") {
                    e2e_enabled_ = true;
                }
            }
            if (its_e2e->first == "protected") {
                for (auto its_protected = its_e2e->second.begin();
                        its_protected != its_e2e->second.end(); ++its_protected) {
                    load_e2e_protected(its_protected->second);
                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_e2e_protected(const boost::property_tree::ptree &_tree) {

    uint16_t data_id(0);
    std::string variant("");
    std::string profile("");
    service_t service_id(0);
    event_t event_id(0);

    uint16_t crc_offset(0);
    uint8_t  data_id_mode(0);
    uint16_t data_length(0);
    uint16_t data_id_nibble_offset(12); // data id nibble behind 4 bit counter value
    uint16_t counter_offset(8); // counter field behind CRC8

    for (auto l = _tree.begin(); l != _tree.end(); ++l) {
        std::stringstream its_converter;
        uint16_t tmp;
        if (l->first == "data_id" && data_id == 0) {
            std::string value = l->second.data();
            if (value.size() > 1 && value[0] == '0' && value[1] == 'x') {
                its_converter << std::hex << value;
            } else {
                its_converter << std::dec << value;
            }
            its_converter >> data_id;
        } else if (l->first == "service_id") {
            std::string value = l->second.data();
            if (value.size() > 1 && value[0] == '0' && value[1] == 'x') {
                its_converter << std::hex << value;
            } else {
                its_converter << std::dec << value;
            }
            its_converter >> service_id;
        } else if (l->first == "event_id") {
            std::string value = l->second.data();
            if (value.size() > 1 && value[0] == '0' && value[1] == 'x') {
                its_converter << std::hex << value;
            } else {
                its_converter << std::dec << value;
            }
            its_converter >> event_id;
        } else if (l->first == "variant") {
            std::string value = l->second.data();
            its_converter << value;
            its_converter >> variant;
        } else if (l->first == "profile") {
            std::string value = l->second.data();
            its_converter << value;
            its_converter >> profile;
        } else if (l->first == "crc_offset") {
            std::string value = l->second.data();
            its_converter << value;
            its_converter >> crc_offset;
        }  else if (l->first == "counter_offset") {
            std::string value = l->second.data();
            its_converter << value;
            its_converter >> counter_offset;
        } else if (l->first == "data_id_mode") {
            std::string value = l->second.data();
            its_converter << value;
            its_converter >> tmp;
            data_id_mode = static_cast<uint8_t>(tmp);
        } else if (l->first == "data_id_nibble_offset") {
            std::string value = l->second.data();
            its_converter << value;
            its_converter >> data_id_nibble_offset;
        }
        else if (l->first == "data_length") {
            std::string value = l->second.data();
            its_converter << value;
            its_converter >> data_length;
        }
    }
    e2e_configuration_[std::make_pair(service_id, event_id)] = std::make_shared<cfg::e2e>(
        data_id,
        variant,
        profile,
        service_id,
        event_id,
        crc_offset,
        data_id_mode,
        data_length,
        data_id_nibble_offset,
        counter_offset
    );
}

std::map<e2exf::data_identifier_t, std::shared_ptr<cfg::e2e>> configuration_impl::get_e2e_configuration() const {
    return e2e_configuration_;
}

bool configuration_impl::log_memory() const {
    return log_memory_;
}

uint32_t configuration_impl::get_log_memory_interval() const {
    return log_memory_interval_;
}

bool configuration_impl::log_status() const {
    return log_status_;
}

uint32_t configuration_impl::get_log_status_interval() const {
    return log_status_interval_;
}

void configuration_impl::load_ttl_factors(
        const boost::property_tree::ptree &_tree, ttl_map_t* _target) {
    const service_t ILLEGAL_VALUE(0xffff);
    for (const auto& i : _tree) {
        service_t its_service(ILLEGAL_VALUE);
        instance_t its_instance(ILLEGAL_VALUE);
        configuration::ttl_factor_t its_ttl_factor(0);

        for (const auto& j : i.second) {
            std::string its_key(j.first);
            std::string its_value(j.second.data());
            std::stringstream its_converter;

            if (its_key == "ttl_factor") {
                its_converter << its_value;
                its_converter >> its_ttl_factor;
            } else {
                // Trim "its_value"
                if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                    its_converter << std::hex << its_value;
                } else {
                    its_converter << std::dec << its_value;
                }

                if (its_key == "service") {
                    its_converter >> its_service;
                } else if (its_key == "instance") {
                    its_converter >> its_instance;
                }
            }
        }
        if (its_service != ILLEGAL_VALUE
            && its_instance != ILLEGAL_VALUE
            && its_ttl_factor > 0) {
            (*_target)[its_service][its_instance] = its_ttl_factor;
        } else {
            VSOMEIP_ERROR << "Invalid ttl factor configuration";
        }
    }
}

configuration::ttl_map_t configuration_impl::get_ttl_factor_offers() const {
    return ttl_factors_offers_;
}

configuration::ttl_map_t configuration_impl::get_ttl_factor_subscribes() const {
    return ttl_factors_subscriptions_;
}

configuration::endpoint_queue_limit_t
configuration_impl::get_endpoint_queue_limit(
        const std::string& _address, std::uint16_t _port) const {
    auto found_address = endpoint_queue_limits_.find(_address);
    if (found_address != endpoint_queue_limits_.end()) {
        auto found_port = found_address->second.find(_port);
        if (found_port != found_address->second.end()) {
            return found_port->second;
        }
    }
    return endpoint_queue_limit_external_;
}

configuration::endpoint_queue_limit_t
configuration_impl::get_endpoint_queue_limit_local() const {
    return endpoint_queue_limit_local_;
}

void configuration_impl::load_endpoint_queue_sizes(const element &_element) {
    const std::string endpoint_queue_limits("endpoint-queue-limits");
    const std::string endpoint_queue_limit_external("endpoint-queue-limit-external");
    const std::string endpoint_queue_limit_local("endpoint-queue-limit-local");

    try {
        if (_element.tree_.get_child_optional(endpoint_queue_limit_external)) {
            if (is_configured_[ET_ENDPOINT_QUEUE_LIMIT_EXTERNAL]) {
                VSOMEIP_WARNING << "Multiple definitions for "
                        << endpoint_queue_limit_external
                        << " Ignoring definition from " << _element.name_;
            } else {
                is_configured_[ET_ENDPOINT_QUEUE_LIMIT_EXTERNAL] = true;
                auto mpsl = _element.tree_.get_child(
                        endpoint_queue_limit_external);
                std::string s(mpsl.data());
                try {
                    endpoint_queue_limit_external_ =
                            static_cast<configuration::endpoint_queue_limit_t>(std::stoul(
                                    s.c_str(), NULL, 10));
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR<<__func__ << ": " << endpoint_queue_limit_external
                    << " " << e.what();
                }
            }
        }
        if (_element.tree_.get_child_optional(endpoint_queue_limit_local)) {
            if (is_configured_[ET_ENDPOINT_QUEUE_LIMIT_LOCAL]) {
                VSOMEIP_WARNING << "Multiple definitions for "
                        << endpoint_queue_limit_local
                        << " Ignoring definition from " << _element.name_;
            } else {
                is_configured_[ET_ENDPOINT_QUEUE_LIMIT_LOCAL] = true;
                auto mpsl = _element.tree_.get_child(endpoint_queue_limit_local);
                std::string s(mpsl.data());
                try {
                    endpoint_queue_limit_local_=
                            static_cast<configuration::endpoint_queue_limit_t>(
                                    std::stoul(s.c_str(), NULL, 10));
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR<< __func__ << ": "<< endpoint_queue_limit_local
                            << " " << e.what();
                }
            }
        }

        if (_element.tree_.get_child_optional(endpoint_queue_limits)) {
            if (is_configured_[ET_ENDPOINT_QUEUE_LIMITS]) {
                VSOMEIP_WARNING << "Multiple definitions for "
                        << endpoint_queue_limits
                        << " Ignoring definition from " << _element.name_;
            } else {
                is_configured_[ET_ENDPOINT_QUEUE_LIMITS] = true;
                const std::string unicast("unicast");
                const std::string ports("ports");
                const std::string port("port");
                const std::string queue_size_limit("queue-size-limit");

                for (const auto i : _element.tree_.get_child(endpoint_queue_limits)) {
                    if (!i.second.get_child_optional(unicast)
                            || !i.second.get_child_optional(ports)) {
                        continue;
                    }
                    std::string its_unicast(i.second.get_child(unicast).data());
                    for (const auto j : i.second.get_child(ports)) {

                        if (!j.second.get_child_optional(port)
                                || !j.second.get_child_optional(queue_size_limit)) {
                            continue;
                        }

                        std::uint16_t its_port = ILLEGAL_PORT;
                        std::uint32_t its_queue_size_limit = 0;

                        try {
                            std::string p(j.second.get_child(port).data());
                            its_port = static_cast<std::uint16_t>(std::stoul(p.c_str(),
                                            NULL, 10));
                            std::string s(j.second.get_child(queue_size_limit).data());
                            its_queue_size_limit = static_cast<std::uint32_t>(std::stoul(
                                            s.c_str(), NULL, 10));
                        } catch (const std::exception &e) {
                            VSOMEIP_ERROR << __func__ << ":" << e.what();
                        }

                        if (its_port == ILLEGAL_PORT || its_queue_size_limit == 0) {
                            continue;
                        }

                        endpoint_queue_limits_[its_unicast][its_port] = its_queue_size_limit;
                    }
                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::load_debounce(const element &_element) {
    try {
        auto its_debounce = _element.tree_.get_child("debounce");
        for (auto i = its_debounce.begin(); i != its_debounce.end(); ++i) {
            load_service_debounce(i->second);
        }
    } catch (...) {
    }
}

void configuration_impl::load_service_debounce(
        const boost::property_tree::ptree &_tree) {
    service_t its_service(0);
    instance_t its_instance(0);
    std::map<event_t, std::shared_ptr<debounce>> its_debounces;

    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_key(i->first);
        std::string its_value(i->second.data());
        std::stringstream its_converter;

        if (its_key == "service") {
            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_service;
        } else if (its_key == "instance") {
            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_instance;
        } else if (its_key == "events") {
            load_events_debounce(i->second, its_debounces);
        }
    }

    // TODO: Improve error handling!
    if (its_service > 0 && its_instance > 0 && !its_debounces.empty()) {
        auto find_service = debounces_.find(its_service);
        if (find_service != debounces_.end()) {
            auto find_instance = find_service->second.find(its_instance);
            if (find_instance != find_service->second.end()) {
                VSOMEIP_ERROR << "Multiple debounce configurations for service "
                    << std::hex << std::setw(4) << std::setfill('0') << its_service
                    << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance;
                return;
            }
        }
        debounces_[its_service][its_instance] = its_debounces;
    }
}

void configuration_impl::load_events_debounce(
        const boost::property_tree::ptree &_tree,
        std::map<event_t, std::shared_ptr<debounce>> &_debounces) {
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        load_event_debounce(i->second, _debounces);
    }
}

void configuration_impl::load_event_debounce(
        const boost::property_tree::ptree &_tree,
        std::map<event_t, std::shared_ptr<debounce>> &_debounces) {
    event_t its_event(0);
    std::shared_ptr<debounce> its_debounce = std::make_shared<debounce>();

    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_key(i->first);
        std::string its_value(i->second.data());
        std::stringstream its_converter;

        if (its_key == "event") {
            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_event;
        } else if (its_key == "on_change") {
            its_debounce->on_change_ = (its_value == "true");
        } else if (its_key == "on_change_resets_interval") {
            its_debounce->on_change_resets_interval_ = (its_value == "true");
        } else if (its_key == "ignore") {
            load_event_debounce_ignore(i->second, its_debounce->ignore_);
        } else if (its_key == "interval") {
           if (its_value == "never") {
               its_debounce->interval_ = -1;
           } else {
               its_converter << std::dec << its_value;
               its_converter >> its_debounce->interval_;
           }
        }
    }

    // TODO: Improve error handling
    if (its_event > 0) {
        auto find_event = _debounces.find(its_event);
        if (find_event == _debounces.end()) {
            _debounces[its_event] = its_debounce;
        }
    }
}

void configuration_impl::load_event_debounce_ignore(
        const boost::property_tree::ptree &_tree,
        std::map<std::size_t, byte_t> &_ignore) {
    std::size_t its_ignored;
    byte_t its_mask;
    std::stringstream its_converter;

    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_value = i->second.data();

        its_mask = 0xff;

        if (!its_value.empty()
               && std::find_if(its_value.begin(), its_value.end(),
                      [](char _c) { return !std::isdigit(_c); })
                      == its_value.end()) {
            its_converter.str("");
            its_converter.clear();
            its_converter << std::dec << its_value;
            its_converter >> its_ignored;

        } else {
            for (auto j = i->second.begin(); j != i->second.end(); ++j) {
                std::string its_ignore_key(j->first);
                std::string its_ignore_value(j->second.data());

                if (its_ignore_key == "index") {
                    its_converter.str("");
                    its_converter.clear();
                    its_converter << std::dec << its_ignore_value;
                    its_converter >> its_ignored;
                } else if (its_ignore_key == "mask") {
                    its_converter.str("");
                    its_converter.clear();

                    int its_tmp_mask;
                    its_converter << std::hex << its_ignore_value;
                    its_converter >> its_tmp_mask;

                    its_mask = static_cast<byte_t>(its_tmp_mask);
                }
            }
        }

        _ignore[its_ignored] = its_mask;
    }
}

void configuration_impl::load_offer_acceptance_required(
        const element &_element) {
    const std::string oar("offer_acceptance_required");
    try {
        std::lock_guard<std::mutex> its_lock(offer_acceptance_required_ips_mutex_);
        if (_element.tree_.get_child_optional(oar)) {
            if (is_configured_[ET_OFFER_ACCEPTANCE_REQUIRED]) {
                VSOMEIP_WARNING << "Multiple definitions of " << oar
                        << " Ignoring definition from " << _element.name_;
            } else {
                for (const auto& ipe : _element.tree_.get_child(oar)) {
                    boost::system::error_code ec;
                    boost::asio::ip::address its_address =
                            boost::asio::ip::address::from_string(ipe.first.data(), ec);
                    if (!its_address.is_unspecified()) {
                        offer_acceptance_required_ips_[its_address] = ipe.second.data();
                    }
                }
                is_configured_[ET_OFFER_ACCEPTANCE_REQUIRED] = true;
            }
        }
    } catch (...) {
        // intentionally left empty
    }
}

void configuration_impl::load_udp_receive_buffer_size(const element &_element) {
    const std::string urbs("udp-receive-buffer-size");
    try {
        if (_element.tree_.get_child_optional(urbs)) {
            if (is_configured_[ET_UDP_RECEIVE_BUFFER_SIZE]) {
                VSOMEIP_WARNING << "Multiple definitions of " << urbs
                        << " Ignoring definition from " << _element.name_;
            } else {
                const std::string s(_element.tree_.get_child(urbs).data());
                try {
                    udp_receive_buffer_size_ = static_cast<std::uint32_t>(std::stoul(
                            s.c_str(), NULL, 10));
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR<< __func__ << ": " << urbs << " " << e.what();
                }
                is_configured_[ET_UDP_RECEIVE_BUFFER_SIZE] = true;
            }
        }
    } catch (...) {
        // intentionally left empty
    }
}

std::shared_ptr<debounce> configuration_impl::get_debounce(
        service_t _service, instance_t _instance, event_t _event) const {
    auto found_service = debounces_.find(_service);
    if (found_service != debounces_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_event = found_instance->second.find(_event);
            if (found_event != found_instance->second.end()) {
                return found_event->second;
            }
        }
    }
    return nullptr;
}

void configuration_impl::load_tcp_restart_settings(const element &_element) {
    const std::string tcp_restart_aborts_max("tcp-restart-aborts-max");
    const std::string tcp_connect_time_max("tcp-connect-time-max");

    try {
        if (_element.tree_.get_child_optional(tcp_restart_aborts_max)) {
            if (is_configured_[ET_TCP_RESTART_ABORTS_MAX]) {
                VSOMEIP_WARNING << "Multiple definitions for "
                        << tcp_restart_aborts_max
                        << " Ignoring definition from " << _element.name_;
            } else {
                is_configured_[ET_TCP_RESTART_ABORTS_MAX] = true;
                auto mpsl = _element.tree_.get_child(
                        tcp_restart_aborts_max);
                std::string s(mpsl.data());
                try {
                    tcp_restart_aborts_max_ =
                            static_cast<std::uint32_t>(std::stoul(
                                    s.c_str(), NULL, 10));
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR<<__func__ << ": " << tcp_restart_aborts_max
                    << " " << e.what();
                }
            }
        }
        if (_element.tree_.get_child_optional(tcp_connect_time_max)) {
            if (is_configured_[ET_TCP_CONNECT_TIME_MAX]) {
                VSOMEIP_WARNING << "Multiple definitions for "
                        << tcp_connect_time_max
                        << " Ignoring definition from " << _element.name_;
            } else {
                is_configured_[ET_TCP_CONNECT_TIME_MAX] = true;
                auto mpsl = _element.tree_.get_child(tcp_connect_time_max);
                std::string s(mpsl.data());
                try {
                    tcp_connect_time_max_=
                            static_cast<std::uint32_t>(
                                    std::stoul(s.c_str(), NULL, 10));
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR<< __func__ << ": "<< tcp_connect_time_max
                            << " " << e.what();
                }
            }
        }
    } catch (...) {
    }
}

std::uint32_t configuration_impl::get_max_tcp_restart_aborts() const {
    return tcp_restart_aborts_max_;
}

std::uint32_t configuration_impl::get_max_tcp_connect_time() const {
    return tcp_connect_time_max_;
}

bool configuration_impl::offer_acceptance_required(
        const boost::asio::ip::address& _address) const {
    std::lock_guard<std::mutex> its_lock(offer_acceptance_required_ips_mutex_);
    return offer_acceptance_required_ips_.find(_address)
            != offer_acceptance_required_ips_.end();
}

void configuration_impl::set_offer_acceptance_required(
        const boost::asio::ip::address& _address, const std::string& _path,
        bool _enable) {
    std::lock_guard<std::mutex> its_lock(offer_acceptance_required_ips_mutex_);
    if (_enable) {
        const auto found_address = offer_acceptance_required_ips_.find(_address);
        if (found_address != offer_acceptance_required_ips_.end()) {
            boost::system::error_code ec;
            VSOMEIP_WARNING << __func__ << " configuration for: "
                    << found_address->first.to_string(ec) << " -> "
                    << found_address->second << " already configured."
                    << " Won't update with: "<< _path;
        } else {
            offer_acceptance_required_ips_[_address] = _path;
        }
    } else {
        offer_acceptance_required_ips_.erase(_address);
    }
}

std::map<boost::asio::ip::address, std::string>
configuration_impl::get_offer_acceptance_required() {
    std::lock_guard<std::mutex> its_lock(offer_acceptance_required_ips_mutex_);
    return offer_acceptance_required_ips_;
}

std::uint32_t configuration_impl::get_udp_receive_buffer_size() const {
    return udp_receive_buffer_size_;
}

std::shared_ptr<policy> configuration_impl::find_client_id_policy(client_t _client) const {
    std::lock_guard<std::mutex> its_lock(policies_mutex_);
    for (auto client_id_pair : policies_) {
        if (std::get<0>(client_id_pair.first) <= _client
                && _client <= std::get<1>(client_id_pair.first)) {
            return client_id_pair.second;
        }
    }
    return nullptr;
}

bool configuration_impl::remove_security_policy(uint32_t _uid, uint32_t _gid) {
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    bool was_removed(false);
    if (!any_client_policies_.empty()) {
        std::vector<std::shared_ptr<policy>>::iterator p_it = any_client_policies_.begin();
        while (p_it != any_client_policies_.end()) {
            bool has_uid(false), has_gid(false);
            for (auto its_credential : p_it->get()->ids_) {
                has_uid = has_gid = false;
                for (auto its_range : std::get<0>(its_credential)) {
                    if (std::get<0>(its_range) <= _uid && _uid <= std::get<1>(its_range)) {
                        has_uid = true;
                        break;
                    }
                }
                for (auto its_range : std::get<1>(its_credential)) {
                    if (std::get<0>(its_range) <= _gid && _gid <= std::get<1>(its_range)) {
                        has_gid = true;
                        break;
                    }
                }
                // only remove "credentials allow" policies to prevent removal of
                // blacklist configured in file
                if (has_uid && has_gid && p_it->get()->allow_who_) {
                    was_removed = true;
                    break;
                }
            }
            if (was_removed) {
                p_it = any_client_policies_.erase(p_it);
                break;
            } else {
                ++p_it;
            }
        }
    }
    return was_removed;
}

void configuration_impl::update_security_policy(uint32_t _uid, uint32_t _gid, ::std::shared_ptr<policy> _policy) {
    remove_security_policy(_uid, _gid);
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    any_client_policies_.push_back(_policy);
}

void configuration_impl::add_security_credentials(uint32_t _uid, uint32_t _gid,
        ::std::shared_ptr<policy> _credentials_policy, client_t _client) {

    bool was_found(false);
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    for (auto its_policy : any_client_policies_) {
        bool has_uid(false), has_gid(false);
        for (auto its_credential : its_policy->ids_) {
            has_uid = has_gid = false;
            for (auto its_range : std::get<0>(its_credential)) {
                if (std::get<0>(its_range) <= _uid && _uid <= std::get<1>(its_range)) {
                    has_uid = true;
                    break;
                }
            }
            for (auto its_range : std::get<1>(its_credential)) {
                if (std::get<0>(its_range) <= _gid && _gid <= std::get<1>(its_range)) {
                    has_gid = true;
                    break;
                }
            }
            if (has_uid && has_gid && its_policy->allow_who_) {
                was_found = true;
                break;
            }
        }
        if (was_found) {
            break;
        }
    }
    // Do not add the new (credentials-only-policy) if a allow credentials policy with same credentials was found
    if (!was_found) {
        any_client_policies_.push_back(_credentials_policy);
        VSOMEIP_INFO << __func__ << " Added security credentials at client: 0x"
                << std::hex << _client << std::dec << " with UID: " << _uid << " GID: " << _gid;
    }
}

bool configuration_impl::is_remote_client_allowed() const {
    if (!check_credentials_) {
        return true;
    }
    return allow_remote_clients_;
}

bool configuration_impl::is_policy_update_allowed(uint32_t _uid, std::shared_ptr<policy> &_policy) const {
    bool uid_allowed(false);
    {
        std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
        for (auto its_uid_range : uid_whitelist_) {
            if (std::get<0>(its_uid_range) <= _uid && _uid <= std::get<1>(its_uid_range)) {
                uid_allowed = true;
                break;
            }
        }
    }

    if (uid_allowed) {
        std::lock_guard<std::mutex> its_lock(service_interface_whitelist_mutex_);
        for (auto its_request : _policy->services_) {
            auto its_requested_service = std::get<0>(its_request);
            bool has_service(false);
            for (auto its_service_range : service_interface_whitelist_) {
                if (std::get<0>(its_service_range) <= its_requested_service
                        && its_requested_service <= std::get<1>(its_service_range)) {
                    has_service = true;
                    break;
                }
            }
            if (!has_service) {
                if (!check_whitelist_) {
                    VSOMEIP_INFO << "vSomeIP Security: Policy update requesting service ID: "
                            << std::hex << its_requested_service
                            << " is not allowed, but will be allowed due to whitelist audit mode is active!";
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Policy update requesting service ID: "
                            << std::hex << its_requested_service
                            << " is not allowed! -> ignore update";
                }
                return !check_whitelist_;
            }
        }
        return true;
    } else {
        if (!check_whitelist_) {
            VSOMEIP_INFO << "vSomeIP Security: Policy update for UID: " << std::dec << _uid
                    << " is not allowed, but will be allowed due to whitelist audit mode is active!";
        } else {
            VSOMEIP_WARNING << "vSomeIP Security: Policy update for UID: " << std::dec << _uid
                    << " is not allowed! -> ignore update";
        }
        return !check_whitelist_;
    }
}

bool configuration_impl::is_policy_removal_allowed(uint32_t _uid) const {
    std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
    for (auto its_uid_range : uid_whitelist_) {
        if (std::get<0>(its_uid_range) <= _uid && _uid <= std::get<1>(its_uid_range)) {
            return true;
        }
    }

    if (!check_whitelist_) {
        VSOMEIP_INFO << "vSomeIP Security: Policy removal for UID: " << std::dec << _uid
                << " is not allowed, but will be allowed due to whitelist audit mode is active!";
    } else {
        VSOMEIP_WARNING << "vSomeIP Security: Policy removal for UID: " << std::dec << _uid
                << " is not allowed! -> ignore removal";
    }
    return !check_whitelist_;
}

bool configuration_impl::check_routing_credentials(client_t _client, uint32_t _uid, uint32_t _gid) const {
    if (_client != get_id(routing_host_)) {
        return true;
    } else {
        std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
        if ( std::get<0>(routing_credentials_) == _uid
                && std::get<1>(routing_credentials_) == _gid) {
            return true;
        }
    }

    std::string security_mode_text = "!";
    if (!check_routing_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }
    VSOMEIP_INFO << "vSomeIP Security: Client 0x"
            << std::hex << _client << " and UID/GID=" << std::dec << _uid
            << "/" << _gid << " : Check routing credentials failed as "
            << "configured routing manager credentials "
            << "do not match with routing manager credentials"
            << security_mode_text;

    return !check_routing_credentials_;
}

std::uint32_t configuration_impl::get_shutdown_timeout() const {
    return shutdown_timeout_;
}

}  // namespace config
}  // namespace vsomeip

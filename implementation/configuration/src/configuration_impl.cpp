// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <fstream>
#include <set>
#include <sstream>

#define WIN32_LEAN_AND_MEAN

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <vsomeip/constants.hpp>

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

namespace vsomeip {
namespace cfg {

std::shared_ptr<configuration_impl> configuration_impl::the_configuration;
std::mutex configuration_impl::mutex_;

std::shared_ptr<configuration> configuration_impl::get(
        const std::set<std::string> &_input) {
    std::shared_ptr<configuration> its_configuration;
    std::lock_guard<std::mutex> its_lock(mutex_);

    std::set<std::string> failed_files;
    static bool has_reading_failed(false);

    if (!the_configuration) {
        the_configuration = std::make_shared<configuration_impl>();
        std::vector<element> its_configuration_elements;

        // Load logger configuration first
        for (auto i : _input) {
            if (utility::is_file(i)) {
                boost::property_tree::ptree its_tree;
                try {
                    boost::property_tree::json_parser::read_json(i, its_tree);
                    its_configuration_elements.push_back({ i, its_tree });
                }
                catch (boost::property_tree::json_parser_error &e) {
#ifdef WIN32
                    e; // silence MSVC warining C4101
#endif
                    failed_files.insert(i);
                }
            } else if (utility::is_folder(i)) {
                boost::filesystem::path its_path(i);
                for (auto j = boost::filesystem::directory_iterator(its_path);
                        j != boost::filesystem::directory_iterator();
                        j++) {
                    auto its_file_path = j->path();
                    if (!boost::filesystem::is_directory(its_file_path)) {
                        std::string its_name = its_file_path.string();
                        boost::property_tree::ptree its_tree;
                        try {
                            boost::property_tree::json_parser::read_json(its_name, its_tree);
                            its_configuration_elements.push_back({its_name, its_tree});
                        }
                        catch (...) {
                            failed_files.insert(its_name);
                        }
                    }
                }
            }
        }

        // Load log configuration
        the_configuration->load_log(its_configuration_elements);

        // Check whether reading of configuration file(s) succeeded.
        if (!failed_files.empty()) {
            has_reading_failed = true;
            for (auto its_failed : failed_files)
                VSOMEIP_ERROR << "Reading of configuration file \""
                    << its_failed << "\" failed.";
        } else {
            // Load other configuration parts
            std::sort(its_configuration_elements.begin(),
                      its_configuration_elements.end());
            for (auto e : its_configuration_elements)
                the_configuration->load(e);
        }
    }

    // There is only one attempt to read the configuration file(s).
    // If it has failed, we must not return the configuration object.
    if (has_reading_failed)
        return nullptr;

    return the_configuration;
}

void configuration_impl::reset() {
    the_configuration.reset();
}

configuration_impl::configuration_impl() :
        diagnosis_(VSOMEIP_DIAGNOSIS_ADDRESS),
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
        max_configured_message_size_(0),
        trace_(std::make_shared<trace>()),
        watchdog_(std::make_shared<watchdog>()),
        permissions_shm_(VSOMEIP_DEFAULT_SHM_PERMISSION),
        umask_(VSOMEIP_DEFAULT_UMASK_LOCAL_ENDPOINTS) {
    unicast_ = unicast_.from_string(VSOMEIP_UNICAST_ADDRESS);
    for (auto i = 0; i < ET_MAX; i++)
        is_configured_[i] = false;
}

configuration_impl::configuration_impl(const configuration_impl &_other) :
    max_configured_message_size_(0),
    permissions_shm_(VSOMEIP_DEFAULT_SHM_PERMISSION),
    umask_(VSOMEIP_DEFAULT_UMASK_LOCAL_ENDPOINTS) {

    applications_.insert(_other.applications_.begin(), _other.applications_.end());
    services_.insert(_other.services_.begin(), _other.services_.end());

    unicast_ = _other.unicast_;
    diagnosis_ = _other.diagnosis_;

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

    trace_ = std::make_shared<trace>(*_other.trace_.get());
    watchdog_ = std::make_shared<watchdog>(*_other.watchdog_.get());

    magic_cookies_.insert(_other.magic_cookies_.begin(), _other.magic_cookies_.end());

    for (auto i = 0; i < ET_MAX; i++)
        is_configured_[i] = _other.is_configured_[i];
}

configuration_impl::~configuration_impl() {
}

void configuration_impl::load(const element &_element) {
    try {
        // Read the configuration data
        get_someip_configuration(_element);
        get_services_configuration(_element.tree_);
        get_clients_configuration(_element.tree_);
        get_payload_sizes_configuration(_element.tree_);
        get_routing_configuration(_element);
        get_permission_configuration(_element);
        get_service_discovery_configuration(_element);
        get_applications_configuration(_element);
        get_trace_configuration(_element);
        get_supports_selective_broadcasts(_element.tree_);
        get_watchdog_configuration(_element);
    } catch (std::exception &e) {
#ifdef WIN32
      e; // silence MSVC warning C4101
#endif
    }
}

void configuration_impl::load_log(const std::vector<element> &_elements) {
    std::set<std::string> its_warnings;

    // Read the logger configuration(s)
    for (auto e : _elements)
        get_logging_configuration(e, its_warnings);

    // Initialize logger
    logger_impl::init(the_configuration);

    // Print warnings after(!) logger initialization
    for (auto w : its_warnings)
        VSOMEIP_WARNING << w;
}

void configuration_impl::get_logging_configuration(
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
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_someip_configuration(
        const element &_element) {
    try {
        if (is_configured_[ET_UNICAST]) {
            VSOMEIP_WARNING << "Multiple definitions for unicast."
                    "Ignoring definition from " << _element.name_;
        } else {
            std::string its_value = _element.tree_.get<std::string>("unicast");
            unicast_ = unicast_.from_string(its_value);
            is_configured_[ET_UNICAST] = true;
        }
    } catch (...) {
    }
    try {
        if (is_configured_[ET_DIAGNOSIS]) {
            VSOMEIP_WARNING << "Multiple definitions for diagnosis."
                    "Ignoring definition from " << _element.name_;
        } else {
            std::string its_value = _element.tree_.get<std::string>("diagnosis");
            std::stringstream its_converter;

            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> diagnosis_;
            is_configured_[ET_DIAGNOSIS] = true;
        }
    } catch (...) {
    }
}

void configuration_impl::get_services_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        auto its_services = _tree.get_child("services");
        for (auto i = its_services.begin(); i != its_services.end(); ++i)
            get_service_configuration(i->second, "");
    } catch (...) {
        try {
            auto its_servicegroups = _tree.get_child("servicegroups");
            for (auto i = its_servicegroups.begin(); i != its_servicegroups.end(); ++i)
                get_servicegroup_configuration(i->second);
        } catch (...) {
            // intentionally left empty!
        }
    }
}

void configuration_impl::get_clients_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        auto its_clients = _tree.get_child("clients");
        for (auto i = its_clients.begin(); i != its_clients.end(); ++i)
            get_client_configuration(i->second);
    } catch (...) {
        // intentionally left empty!
    }
}

void configuration_impl::get_payload_sizes_configuration(
        const boost::property_tree::ptree &_tree) {
    const std::string payload_sizes("payload-sizes");
    try {
        if (_tree.get_child_optional(payload_sizes)) {
            const std::string unicast("unicast");
            const std::string ports("ports");
            const std::string port("port");
            const std::string max_payload_size("max-payload-size");
            auto its_ps = _tree.get_child(payload_sizes);
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
                    if(max_configured_message_size_ < its_message_size) {
                        max_configured_message_size_ = its_message_size;
                    }

                    message_sizes_[its_unicast][its_port] = its_message_size;
                }
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_servicegroup_configuration(
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
                get_delays_configuration(i->second);
            } else if (its_key == "services") {
                for (auto j = i->second.begin(); j != i->second.end(); ++j)
                    get_service_configuration(j->second, its_unicast_address);
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_delays_configuration(
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
                sd_repetitions_max_ = uint8_t(tmp_repetition_max);
            } else if (its_key == "cyclic-offer") {
                its_converter << std::dec << i->second.data();
                its_converter >> sd_cyclic_offer_delay_;
            } else if (its_key == "cyclic-request") {
                its_converter << std::dec << i->second.data();
                its_converter >> sd_request_response_delay_;
            }
            its_converter.str("");
            its_converter.clear();
        }
    } catch (...) {
    }
}

void configuration_impl::get_service_configuration(
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
                get_event_configuration(its_service, i->second);
            } else if (its_key == "eventgroups") {
                get_eventgroup_configuration(its_service, i->second);
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
                magic_cookies_[get_unicast_address(its_service->service_,
                        its_service->instance_)].insert(its_service->reliable_);
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_client_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        bool is_loaded(true);

        std::shared_ptr<client> its_client(std::make_shared<client>());

        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;

            if (its_key == "reliable") {
                its_client->ports_[true] = get_client_port_configuration(i->second);
            } else if (its_key == "unreliable") {
                its_client->ports_[false] = get_client_port_configuration(i->second);
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

        auto found_service = clients_.find(its_client->service_);
        if (found_service != clients_.end()) {
            auto found_instance = found_service->second.find(
                    its_client->instance_);
            if (found_instance != found_service->second.end()) {
                VSOMEIP_ERROR << "Multiple client configurations for service ["
                        << std::hex << its_client->service_ << "."
                        << its_client->instance_ << "]";
                is_loaded = false;
            }
        }

        if (is_loaded) {
            clients_[its_client->service_][its_client->instance_] = its_client;
        }
    } catch (...) {
    }
}

std::set<uint16_t> configuration_impl::get_client_port_configuration(
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

void configuration_impl::get_event_configuration(
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

void configuration_impl::get_eventgroup_configuration(
        std::shared_ptr<service> &_service,
        const boost::property_tree::ptree &_tree) {
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::shared_ptr<eventgroup> its_eventgroup =
                std::make_shared<eventgroup>();
        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            std::stringstream its_converter;
            std::string its_key(j->first);
            if (its_key == "eventgroup") {

                std::string its_value(j->second.data());
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

void configuration_impl::get_routing_configuration(const element &_element) {
    try {
        if (is_configured_[ET_ROUTING]) {
            VSOMEIP_WARNING << "Multiple definitions of routing."
                    << " Ignoring definition from " << _element.name_;
        } else {
            auto its_routing = _element.tree_.get_child("routing");
            routing_host_ = its_routing.data();
            is_configured_[ET_ROUTING] = true;
        }
    } catch (...) {
    }
}

void configuration_impl::get_permission_configuration(const element &_element) {
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
                } else if (its_key == "umask") {
                    std::string its_value(i->second.data());
                    its_converter << std::oct << its_value;
                    its_converter >> umask_;
                }
            }
        }
    } catch (...) {
    }
}

std::uint32_t configuration_impl::get_umask() const {
    return umask_;
}

std::uint32_t configuration_impl::get_permissions_shm() const {
    return permissions_shm_;
}

void configuration_impl::get_service_discovery_configuration(
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
                    sd_repetitions_max_ = (uint8_t)tmp;
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
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_applications_configuration(
        const element &_element) {
    try {
        std::stringstream its_converter;
        auto its_applications = _element.tree_.get_child("applications");
        for (auto i = its_applications.begin();
                i != its_applications.end();
                ++i) {
            get_application_configuration(i->second, _element.name_);
        }
    } catch (...) {
    }
}

void configuration_impl::get_application_configuration(
        const boost::property_tree::ptree &_tree, const std::string &_file_name) {
    std::string its_name("");
    client_t its_id(0);
    std::size_t its_max_dispatchers(VSOMEIP_MAX_DISPATCHERS);
    std::size_t its_max_dispatch_time(VSOMEIP_MAX_DISPATCH_TIME);
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
        }
    }
    if (its_name != "" && its_id != 0) {
        if (applications_.find(its_name) == applications_.end()) {
            if (!is_configured_client_id(its_id)) {
                applications_[its_name]
                    = std::make_tuple(its_id, its_max_dispatchers, its_max_dispatch_time);
                client_identifiers_.insert(its_id);
            } else {
                VSOMEIP_WARNING << "Multiple configurations for application "
                        << its_name << ". Ignoring a configuration from "
                        << _file_name;
            }
        } else {
            VSOMEIP_WARNING << "Multiple configurations for application "
                    << its_name << ". Ignoring a configuration from "
                    << _file_name;
        }
    }
}

void configuration_impl::get_trace_configuration(const element &_element) {
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
            } else if(its_key == "channels") {
                get_trace_channels_configuration(i->second);
            } else if(its_key == "filters") {
                get_trace_filters_configuration(i->second);
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_trace_channels_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        for(auto i = _tree.begin(); i != _tree.end(); ++i) {
            if(i == _tree.begin())
                trace_->channels_.clear();
            get_trace_channel_configuration(i->second);
        }
    } catch (...) {
    }
}

void configuration_impl::get_trace_channel_configuration(
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

void configuration_impl::get_trace_filters_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        for(auto i = _tree.begin(); i != _tree.end(); ++i) {
            get_trace_filter_configuration(i->second);
        }
    } catch (...) {
    }
}

void configuration_impl::get_trace_filter_configuration(
        const boost::property_tree::ptree &_tree) {
    std::shared_ptr<trace_filter_rule> its_filter_rule = std::make_shared<trace_filter_rule>();
    for(auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_key = i->first;
        std::string its_value = i->second.data();
        if(its_key == "channel") {
            its_filter_rule->channel_ = its_value;
        } else {
            get_trace_filter_expressions(i->second, its_key, its_filter_rule);
        }
    }
    trace_->filter_rules_.push_back(its_filter_rule);
}

void configuration_impl::get_trace_filter_expressions(
        const boost::property_tree::ptree &_tree,
        std::string &_criteria,
        std::shared_ptr<trace_filter_rule> &_filter_rule) {
    for(auto i = _tree.begin(); i != _tree.end(); ++i) {
        std::string its_value = i->second.data();
        std::stringstream its_converter;

        if(_criteria == "services") {
            service_t its_id = NO_TRACE_FILTER_EXPRESSION;
            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_id;
            _filter_rule->services_.push_back(its_id);
        } else if(_criteria == "methods") {
            method_t its_id = NO_TRACE_FILTER_EXPRESSION;
            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_id;
            _filter_rule->methods_.push_back(its_id);
        } else if(_criteria == "clients") {
            client_t its_id = NO_TRACE_FILTER_EXPRESSION;
            if (its_value.size() > 1 && its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_id;
            _filter_rule->clients_.push_back(its_id);
        }
    }
}

void configuration_impl::get_supports_selective_broadcasts(const boost::property_tree::ptree &_tree) {
    try {
        auto its_service_discovery = _tree.get_child("supports_selective_broadcasts");
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

void configuration_impl::get_watchdog_configuration(const element &_element) {
    watchdog_->is_enabeled_ = false;
    watchdog_->timeout_in_ms_ = VSOMEIP_DEFAULT_WATCHDOG_TIMEOUT;
    watchdog_->missing_pongs_allowed_ = VSOMEIP_DEFAULT_MAX_MISSING_PONGS;
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

// Public interface
const boost::asio::ip::address & configuration_impl::get_unicast_address() const {
    return unicast_;
}

unsigned short configuration_impl::get_diagnosis_address() const {
    return diagnosis_;
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
    uint16_t its_reliable(ILLEGAL_PORT);
    auto its_service = find_service(_service, _instance);
    if (its_service)
        its_reliable = its_service->reliable_;

    return its_reliable;
}

uint16_t configuration_impl::get_unreliable_port(service_t _service,
        instance_t _instance) const {
    uint16_t its_unreliable = ILLEGAL_PORT;
     auto its_service = find_service(_service, _instance);
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
        service_t _service, instance_t _instance, bool _reliable,
        std::map<bool, std::set<uint16_t> > &_used,
        uint16_t &_port) const {
    _port = ILLEGAL_PORT;
    auto its_client = find_client(_service, _instance);

    // If no client ports are configured, return true
    if (!its_client || its_client->ports_[_reliable].empty()) {
        return true;
    }

    for (auto its_port : its_client->ports_[_reliable]) {
        // Found free configured port
        if (_used[_reliable].find(its_port) == _used[_reliable].end()) {
            _port = its_port;
            return true;
        }
    }

    // Configured ports do exist, but they are all in use
    VSOMEIP_ERROR << "Cannot find free client port!";
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
    std::set<std::pair<service_t, instance_t> > its_remote_services;
    for (auto i : services_) {
        for (auto j : i.second) {
            if (j.second->unicast_address_ != "local" &&
                j.second->unicast_address_ != "" &&
                j.second->unicast_address_ != unicast_.to_string() &&
                j.second->unicast_address_ != VSOMEIP_UNICAST_ADDRESS)
                its_remote_services.insert(std::make_pair(i.first, j.first));
        }
    }
    return its_remote_services;
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

std::shared_ptr<client> configuration_impl::find_client(service_t _service,
        instance_t _instance) const {
    std::shared_ptr<client> its_client;
    auto find_service = clients_.find(_service);
    if (find_service != clients_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
            its_client = find_instance->second;
        }
    }
    return its_client;
}

std::shared_ptr<service> configuration_impl::find_service(service_t _service,
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
    uint32_t its_max_message_size = VSOMEIP_MAX_LOCAL_MESSAGE_SIZE;
    if (VSOMEIP_MAX_TCP_MESSAGE_SIZE > its_max_message_size) {
        its_max_message_size = VSOMEIP_MAX_TCP_MESSAGE_SIZE;
    }
    if (VSOMEIP_MAX_UDP_MESSAGE_SIZE > its_max_message_size) {
        its_max_message_size = VSOMEIP_MAX_UDP_MESSAGE_SIZE;
    }
    if(its_max_message_size < max_configured_message_size_) {
        its_max_message_size = max_configured_message_size_;
    }

    // add sizes of the the routing_manager_proxy's messages
    // to the routing_manager stub
    return std::uint32_t(its_max_message_size
            + VSOMEIP_COMMAND_HEADER_SIZE + sizeof(instance_t)
            + sizeof(bool) + sizeof(bool) + sizeof(bool));
}

std::uint32_t configuration_impl::get_message_size_reliable(
        const std::string& _address, std::uint16_t _port) const {
    auto its_address = message_sizes_.find(_address);
    if(its_address != message_sizes_.end()) {
        auto its_port = its_address->second.find(_port);
        if(its_port != its_address->second.end()) {
            return its_port->second;
        }
    }
    return VSOMEIP_MAX_TCP_MESSAGE_SIZE;
}

bool configuration_impl::supports_selective_broadcasts(boost::asio::ip::address _address) const {
    return supported_selective_addresses.find(_address.to_string()) != supported_selective_addresses.end();
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


}  // namespace config
}  // namespace vsomeip

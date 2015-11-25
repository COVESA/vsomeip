// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <fstream>
#include <set>
#include <sstream>

#define WIN32_LEAN_AND_MEAN

#if defined ( WIN32 )
#define __func__ __FUNCTION__
#endif

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <vsomeip/constants.hpp>

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

    if (!the_configuration) {
        the_configuration = std::make_shared<configuration_impl>();
        std::vector<boost::property_tree::ptree> its_tree_set;

        // Load logger configuration first
        for (auto i : _input) {
            if (utility::is_file(i)) {
                boost::property_tree::ptree its_tree;
                try {
                    boost::property_tree::json_parser::read_json(i, its_tree);
                    its_tree_set.push_back(its_tree);
                }
                catch (...) {
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
                            its_tree_set.push_back(its_tree);
                        }
                        catch (...) {
                        }
                    }
                }
            }
        }

        // Load log configuration
        the_configuration->load_log(its_tree_set);

        // Load other configuration parts
        for (auto t : its_tree_set)
            the_configuration->load(t);
    }

    return the_configuration;
}

void configuration_impl::reset() {
    the_configuration.reset();
}

configuration_impl::configuration_impl() :
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
        max_configured_message_size_(0) {

    unicast_ = unicast_.from_string(VSOMEIP_UNICAST_ADDRESS);
}

configuration_impl::configuration_impl(const configuration_impl &_other) :
    max_configured_message_size_(0) {

    applications_.insert(_other.applications_.begin(), _other.applications_.end());
    services_.insert(_other.services_.begin(), _other.services_.end());

    unicast_ = _other.unicast_;

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

    magic_cookies_.insert(_other.magic_cookies_.begin(), _other.magic_cookies_.end());
}

configuration_impl::~configuration_impl() {
}

void configuration_impl::load(const boost::property_tree::ptree &_tree) {
    try {
        // Read the configuration data
        get_someip_configuration(_tree);
        get_services_configuration(_tree);
        get_payload_sizes_configuration(_tree);
        get_routing_configuration(_tree);
        get_service_discovery_configuration(_tree);
        get_applications_configuration(_tree);
    } catch (std::exception &e) {
    }
}

void configuration_impl::load_log(const std::vector<boost::property_tree::ptree> &_trees) {
    // Read the logger configuration(s)
    for (auto t : _trees)
        get_logging_configuration(t);

    // Initialize logger
    logger_impl::init(the_configuration);
}

void configuration_impl::get_logging_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        auto its_logging = _tree.get_child("logging");
        for (auto i = its_logging.begin(); i != its_logging.end(); ++i) {
            std::string its_key(i->first);
            if (its_key == "console") {
               std::string its_value(i->second.data());
               has_console_log_ = (its_value == "true");
            } else if (its_key == "file") {
                for (auto j : i->second) {
                    std::string its_sub_key(j.first);
                    std::string its_sub_value(j.second.data());
                    if (its_sub_key == "enable") {
                        has_file_log_ = (its_sub_value == "true");
                    } else if (its_sub_key == "path") {
                        logfile_ = its_sub_value;
                    }
                }
            } else if (its_key == "dlt") {
                std::string its_value(i->second.data());
                has_dlt_log_ = (its_value == "true");
            } else if (its_key == "level") {
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
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_someip_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        std::string its_value = _tree.get<std::string>("unicast");
        unicast_ = unicast_.from_string(its_value);
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
        its_service->multicast_group_ = 0xFFFF;  // TODO: use symbolic constant
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
                try {
                    its_value
                        = i->second.get_child("enable-magic-cookies").data();
                    use_magic_cookies = ("true" == its_value);
                } catch (...) {

                }
            } else if (its_key == "unreliable") {
                its_converter << its_value;
                its_converter >> its_service->unreliable_;
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
                if (its_value[0] == '0' && its_value[1] == 'x') {
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
                is_loaded = false;
            }
        }

        if (is_loaded) {
            services_[its_service->service_][its_service->instance_] =
                    its_service;
            if (use_magic_cookies) {
                magic_cookies_[its_service->unicast_address_].insert(
                        its_service->reliable_);
            }
        }
    } catch (...) {
    }
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
                if (its_value[0] == '0' && its_value[1] == 'x') {
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
                found_event->second->is_field_ = its_is_field;
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
    bool is_multicast;
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        is_multicast = false;
        std::shared_ptr<eventgroup> its_eventgroup =
                std::make_shared<eventgroup>();
        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            std::string its_key(j->first);
            if (its_key == "eventgroup") {
                std::stringstream its_converter;
                std::string its_value(j->second.data());
                if (its_value[0] == '0' && its_value[1] == 'x') {
                    its_converter << std::hex << its_value;
                } else {
                    its_converter << std::dec << its_value;
                }
                its_converter >> its_eventgroup->id_;
            } else if (its_key == "is_multicast") {
                std::string its_value(j->second.data());
                is_multicast = (its_value == "true");
            } else if (its_key == "events") {
                for (auto k = j->second.begin(); k != j->second.end(); ++k) {
                    std::stringstream its_converter;
                    std::string its_value(k->second.data());
                    event_t its_event_id(0);
                    if (its_value[0] == '0' && its_value[1] == 'x') {
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
            if (is_multicast) {
                _service->multicast_group_ = its_eventgroup->id_;
            }
            _service->eventgroups_[its_eventgroup->id_] = its_eventgroup;
        }
    }
}

void configuration_impl::get_routing_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        auto its_routing = _tree.get_child("routing");
        routing_host_ = its_routing.data();
    } catch (...) {
    }
}

void configuration_impl::get_service_discovery_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        auto its_service_discovery = _tree.get_child("service-discovery");
        for (auto i = its_service_discovery.begin();
                i != its_service_discovery.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;
            if (its_key == "enable") {
                is_sd_enabled_ = (its_value == "true");
            } else if (its_key == "multicast") {
                sd_multicast_ = its_value;
            } else if (its_key == "port") {
                its_converter << its_value;
                its_converter >> sd_port_;
            } else if (its_key == "protocol") {
                sd_protocol_ = its_value;
            } else if (its_key == "initial_delay_min") {
                its_converter << its_value;
                its_converter >> sd_initial_delay_min_;
            } else if (its_key == "initial_delay_max") {
                its_converter << its_value;
                its_converter >> sd_initial_delay_max_;
            } else if (its_key == "repetitions_base_delay") {
                its_converter << its_value;
                its_converter >> sd_repetitions_base_delay_;
            } else if (its_key == "repetitions_max") {
                int tmp;
                its_converter << its_value;
                its_converter >> tmp;
                sd_repetitions_max_ = (uint8_t)tmp;
            } else if (its_key == "ttl") {
                its_converter << its_value;
                its_converter >> sd_ttl_;
            } else if (its_key == "cyclic_offer_delay") {
                its_converter << its_value;
                its_converter >> sd_cyclic_offer_delay_;
            } else if (its_key == "request_response_delay") {
                its_converter << its_value;
                its_converter >> sd_request_response_delay_;
            }
        }
    } catch (...) {
    }
}

void configuration_impl::get_applications_configuration(
        const boost::property_tree::ptree &_tree) {
    try {
        std::stringstream its_converter;
        auto its_applications = _tree.get_child("applications");
        for (auto i = its_applications.begin();
                i != its_applications.end();
                ++i) {
            get_application_configuration(i->second);
        }
    } catch (...) {
    }
}

void configuration_impl::get_application_configuration(
        const boost::property_tree::ptree &_tree) {
    std::string its_name("");
    client_t its_id;
    std::size_t its_num_dispatchers(0);
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
        } else if (its_key == "num_dispatchers") {
            if (its_value[0] == '0' && its_value[1] == 'x') {
                its_converter << std::hex << its_value;
            } else {
                its_converter << std::dec << its_value;
            }
            its_converter >> its_num_dispatchers;
        }
    }
    if (its_name != "" && its_id != 0) {
        applications_[its_name] = {its_id, its_num_dispatchers};
    }
}

// Public interface
const boost::asio::ip::address & configuration_impl::get_unicast_address() const {
    return unicast_;
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
    service *its_service = find_service(_service, _instance);
    if (its_service) {
        its_unicast_address = its_service->unicast_address_;
    }

    if (its_unicast_address == "local" || its_unicast_address == "") {
            its_unicast_address = get_unicast_address().to_string();
    }
    return its_unicast_address;
}

std::string configuration_impl::get_multicast_address(service_t _service,
        instance_t _instance) const {
    std::string its_multicast_address("");
    service *its_service = find_service(_service, _instance);
    if (its_service)
        its_multicast_address = its_service->multicast_address_;
    return its_multicast_address;
}

uint16_t configuration_impl::get_multicast_port(service_t _service,
        instance_t _instance) const {
    uint16_t its_multicast_port(ILLEGAL_PORT);
    service *its_service = find_service(_service, _instance);
    if (its_service)
        its_multicast_port = its_service->multicast_port_;
    return its_multicast_port;
}

uint16_t configuration_impl::get_multicast_group(service_t _service,
        instance_t _instance) const {
    uint16_t its_multicast_group(0xFFFF);
    service *its_service = find_service(_service, _instance);
    if (its_service)
        its_multicast_group = its_service->multicast_group_;
    return its_multicast_group;
}

uint16_t configuration_impl::get_reliable_port(service_t _service,
        instance_t _instance) const {
    uint16_t its_reliable(ILLEGAL_PORT);
    service *its_service = find_service(_service, _instance);
    if (its_service)
        its_reliable = its_service->reliable_;

    return its_reliable;
}

bool configuration_impl::is_someip(service_t _service,
        instance_t _instance) const {
    service *its_service = find_service(_service, _instance);
    if (its_service)
        return (its_service->protocol_ == "someip");
    return true; // we need to explicitely configure a service to
                 // be something else than SOME/IP
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

uint16_t configuration_impl::get_unreliable_port(service_t _service,
        instance_t _instance) const {
    uint16_t its_unreliable = ILLEGAL_PORT;

    service *its_service = find_service(_service, _instance);
    if (its_service)
        its_unreliable = its_service->unreliable_;

    return its_unreliable;
}

const std::string & configuration_impl::get_routing_host() const {
    return routing_host_;
}

client_t configuration_impl::get_id(const std::string &_name) const {
    client_t its_client = 0;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_client = found_application->second.first;
    }

    return its_client;
}

std::size_t configuration_impl::get_num_dispatchers(
        const std::string &_name) const {
    std::size_t its_num_dispatchers = 0;

    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_num_dispatchers = found_application->second.second;
    }

    return its_num_dispatchers;
}

std::set<std::pair<service_t, instance_t> >
configuration_impl::get_remote_services() const {
    std::set<std::pair<service_t, instance_t> > its_remote_services;
    for (auto i : services_) {
        for (auto j : i.second) {
            if (j.second->unicast_address_ != "local" && j.second->unicast_address_ != "")
                its_remote_services.insert(std::make_pair(i.first, j.first));
        }
    }
    return its_remote_services;
}

service *configuration_impl::find_service(service_t _service,
        instance_t _instance) const {
    service *its_service(0);
    auto find_service = services_.find(_service);
    if (find_service != services_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
            its_service = find_instance->second.get();
        }
    }
    return its_service;
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
            + sizeof(bool) + sizeof(bool));
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

}  // namespace config
}  // namespace vsomeip

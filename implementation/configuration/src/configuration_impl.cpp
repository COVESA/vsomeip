// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <fstream>
#include <map>
#include <sstream>

#define WIN32_LEAN_AND_MEAN

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <vsomeip/constants.hpp>

#include "../include/configuration_impl.hpp"
#include "../include/event.hpp"
#include "../include/eventgroup.hpp"
#include "../include/servicegroup.hpp"
#include "../include/service.hpp"
#include "../../logging/include/logger_impl.hpp"
#include "../../routing/include/event.hpp"
#include "../../service_discovery/include/defines.hpp"

namespace vsomeip {
namespace cfg {

std::map<std::string, configuration *> configuration_impl::the_configurations;
std::mutex configuration_impl::mutex_;

configuration * configuration_impl::get(const std::string &_path) {
    configuration *its_configuration(0);

    bool is_freshly_loaded(false);
    {
        std::lock_guard<std::mutex> its_lock(mutex_);

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

configuration_impl::configuration_impl() :
        has_console_log_(true), has_file_log_(false), has_dlt_log_(false), logfile_(
                "/tmp/vsomeip.log"), loglevel_(
                boost::log::trivial::severity_level::info), routing_host_(
                "vsomeipd"), is_service_discovery_enabled_(false) {

    unicast_ = unicast_.from_string("127.0.0.1");
}

configuration_impl::~configuration_impl() {
}

bool configuration_impl::load(const std::string &_path) {
    bool is_loaded(true);
    boost::property_tree::ptree its_tree;

    try {
        boost::property_tree::json_parser::read_json(_path, its_tree);

        // Read the configuration data
        is_loaded = get_someip_configuration(its_tree);
        is_loaded = get_logging_configuration(its_tree);
        is_loaded = is_loaded && get_services_configuration(its_tree);
        is_loaded = is_loaded && get_routing_configuration(its_tree);
        is_loaded = is_loaded && get_service_discovery_configuration(its_tree);
        is_loaded = is_loaded && get_applications_configuration(its_tree);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        is_loaded = false;
    }

    return is_loaded;
}

bool configuration_impl::get_someip_configuration(
        boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        std::string its_value = _tree.get<std::string>("unicast");
        unicast_ = unicast_.from_string(its_value);
    } catch (...) {
        is_loaded = false;
    }
    return is_loaded;
}

bool configuration_impl::get_logging_configuration(
        boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
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
                loglevel_ = (its_value == "trace" ?
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
        is_loaded = false;
    }
    return is_loaded;
}

bool configuration_impl::get_services_configuration(
        boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        auto its_services = _tree.get_child("servicegroups");
        for (auto i = its_services.begin(); i != its_services.end(); ++i)
            is_loaded = is_loaded && get_servicegroup_configuration(i->second);
    } catch (...) {
        // This section is optional!
    }
    return is_loaded;
}

bool configuration_impl::get_servicegroup_configuration(
        const boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        std::shared_ptr<servicegroup> its_servicegroup(
                std::make_shared<servicegroup>());
        its_servicegroup->unicast_ = "local";  // Default
        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            if (its_key == "name") {
                its_servicegroup->name_ = i->second.data();
            } else if (its_key == "unicast") {
                its_servicegroup->unicast_ = i->second.data();
            } else if (its_key == "delays") {
                is_loaded = is_loaded
                        && get_delays_configuration(its_servicegroup,
                                i->second);
            } else if (its_key == "services") {
                for (auto j = i->second.begin(); j != i->second.end(); ++j)
                    is_loaded = is_loaded
                            && get_service_configuration(its_servicegroup,
                                    j->second);
            }
        }
        servicegroups_[its_servicegroup->name_] = its_servicegroup;
    } catch (...) {
        is_loaded = false;
    }
    return is_loaded;
}

bool configuration_impl::get_delays_configuration(
        std::shared_ptr<servicegroup> &_group,
        const boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        std::stringstream its_converter;
        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            if (its_key == "initial") {
                _group->min_initial_delay_ = i->second.get<uint32_t>("minimum");
                _group->max_initial_delay_ = i->second.get<uint32_t>("maximum");
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
    } catch (...) {
        is_loaded = false;
    }
    return is_loaded;
}

bool configuration_impl::get_service_configuration(
        std::shared_ptr<servicegroup> &_group,
        const boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        bool use_magic_cookies(false);

        std::shared_ptr<service> its_service(std::make_shared<service>());
        its_service->reliable_ = its_service->unreliable_ = ILLEGAL_PORT;
        its_service->use_magic_cookies_ = false;
        its_service->group_ = _group;
        its_service->multicast_address_ = "";
        its_service->multicast_port_ = ILLEGAL_PORT;
        its_service->multicast_group_ = 0xFFFF;  // TODO: use symbolic constant

        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            std::stringstream its_converter;

            if (its_key == "reliable") {
                try {
                    its_value = i->second.get_child("port").data();
                    its_converter << its_value;
                    its_converter >> its_service->reliable_;
                } catch (...) {
                    its_converter << its_value;
                    its_converter >> its_service->reliable_;
                }
                try {
                    its_value =
                            i->second.get_child("enable-magic-cookies").data();
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
        }

        if (use_magic_cookies) {
            std::string its_unicast(_group->unicast_);
            if (its_unicast == "local")
                its_unicast = unicast_.to_string();
            magic_cookies_[its_unicast].insert(its_service->reliable_);
        }
    } catch (...) {
        is_loaded = false;
    }
    return is_loaded;
}

bool configuration_impl::get_event_configuration(
        std::shared_ptr<service> &_service,
        const boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
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
    return is_loaded;
}
bool configuration_impl::get_eventgroup_configuration(
        std::shared_ptr<service> &_service,
        const boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
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
    return is_loaded;
}

bool configuration_impl::get_routing_configuration(
        boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        auto its_routing = _tree.get_child("routing");
        routing_host_ = its_routing.data();
    } catch (...) {
        is_loaded = false;
    }
    return is_loaded;
}

bool configuration_impl::get_service_discovery_configuration(
        boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        auto its_service_discovery = _tree.get_child("service-discovery");
        for (auto i = its_service_discovery.begin();
                i != its_service_discovery.end(); ++i) {
            std::string its_key(i->first);
            std::string its_value(i->second.data());
            if (its_key == "enable") {
                is_service_discovery_enabled_ = (its_value == "true");
            } else if (its_key == "multicast") {
                service_discovery_multicast_ = its_value;
            } else if (its_key == "port") {
                std::stringstream its_converter;
                its_converter << its_value;
                its_converter >> service_discovery_port_;
            } else if (its_key == "protocol") {
                service_discovery_protocol_ = its_value;
            }
        }
    } catch (...) {
        is_loaded = false;
    }

    return is_loaded;
}

bool configuration_impl::get_applications_configuration(
        boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
    try {
        std::stringstream its_converter;
        auto its_applications = _tree.get_child("applications");
        for (auto i = its_applications.begin(); i != its_applications.end();
                ++i)
            is_loaded = is_loaded && get_application_configuration(i->second);
    } catch (...) {
        is_loaded = false;
    }

    return is_loaded;
}

bool configuration_impl::get_application_configuration(
        const boost::property_tree::ptree &_tree) {
    bool is_loaded(true);
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
    return is_loaded;
}

// Public interface
const boost::asio::ip::address & configuration_impl::get_unicast() const {
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

std::set<std::string> configuration_impl::get_servicegroups() const {
    std::set<std::string> its_keys;
    for (auto i : servicegroups_)
        its_keys.insert(i.first);
    return its_keys;
}

bool configuration_impl::is_local_servicegroup(const std::string &_name) const {
    bool is_local(false);

    servicegroup *its_servicegroup = find_servicegroup(_name);
    if (its_servicegroup) {
        is_local = (its_servicegroup->unicast_ == "local"
                || its_servicegroup->unicast_ == get_unicast().to_string());
    }

    return is_local;
}

int32_t configuration_impl::get_min_initial_delay(
        const std::string &_name) const {
    int32_t its_delay = 0;

    servicegroup *its_servicegroup = find_servicegroup(_name);
    if (its_servicegroup)
        its_delay = its_servicegroup->min_initial_delay_;

    return its_delay;
}

int32_t configuration_impl::get_max_initial_delay(
        const std::string &_name) const {
    int32_t its_delay = 0;

    servicegroup *its_servicegroup = find_servicegroup(_name);
    if (its_servicegroup)
        its_delay = its_servicegroup->max_initial_delay_;

    return its_delay;
}

int32_t configuration_impl::get_repetition_base_delay(
        const std::string &_name) const {
    int32_t its_delay = 0;

    servicegroup *its_servicegroup = find_servicegroup(_name);
    if (its_servicegroup)
        its_delay = its_servicegroup->repetition_base_delay_;

    return its_delay;
}

uint8_t configuration_impl::get_repetition_max(const std::string &_name) const {
    uint8_t its_max = 0;

    servicegroup *its_servicegroup = find_servicegroup(_name);
    if (its_servicegroup)
        its_max = its_servicegroup->repetition_max_;

    return its_max;
}

int32_t configuration_impl::get_cyclic_offer_delay(
        const std::string &_name) const {
    uint32_t its_delay = 0;

    servicegroup *its_servicegroup = find_servicegroup(_name);
    if (its_servicegroup)
        its_delay = its_servicegroup->cyclic_offer_delay_;

    return its_delay;
}

int32_t configuration_impl::get_cyclic_request_delay(
        const std::string &_name) const {
    uint32_t its_delay = 0;

    servicegroup *its_servicegroup = find_servicegroup(_name);
    if (its_servicegroup)
        its_delay = its_servicegroup->cyclic_request_delay_;

    return its_delay;
}

std::string configuration_impl::get_group(service_t _service,
        instance_t _instance) const {
    std::string its_group("default");
    service *its_service = find_service(_service, _instance);
    if (nullptr != its_service) {
        its_group = its_service->group_->name_;
    }
    return its_group;
}

std::string configuration_impl::get_unicast(service_t _service,
        instance_t _instance) const {
    std::string its_unicast("");
    service *its_service = find_service(_service, _instance);
    if (its_service)
        its_unicast = its_service->group_->unicast_;
    return its_unicast;
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
    uint16_t its_reliable = ILLEGAL_PORT;

    service *its_service = find_service(_service, _instance);
    if (its_service)
        its_reliable = its_service->reliable_;

    return its_reliable;
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

bool configuration_impl::is_service_discovery_enabled() const {
    return is_service_discovery_enabled_;
}

const std::string &
configuration_impl::get_service_discovery_protocol() const {
    return service_discovery_protocol_;
}

const std::string &
configuration_impl::get_service_discovery_multicast() const {
    return service_discovery_multicast_;
}

uint16_t configuration_impl::get_service_discovery_port() const {
    return service_discovery_port_;
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
            if (j.second->group_->unicast_ != "local")
                its_remote_services.insert(std::make_pair(i.first, j.first));
        }
    }
    return its_remote_services;
}

std::map<service_t,
        std::map<instance_t, std::map<eventgroup_t, std::set<event_t> > > >
configuration_impl::get_eventgroups() const {
    std::map<service_t,
        std::map<instance_t,
            std::map<eventgroup_t, std::set<event_t> > > > its_eventgroups;
    for (auto i : services_) {
        for (auto j : i.second) {
            if (j.second->group_->unicast_ == "local") {
                for (auto k : j.second->eventgroups_) {
                    for (auto l : k.second->events_) {
                        its_eventgroups[i.first][j.first]
                            [k.second->id_].insert(l->id_);
                    }
                }
            }
        }
    }
    return its_eventgroups;
}

std::map<service_t, std::map<instance_t, std::set<event_t> > >
configuration_impl::get_events() const {
    std::map<service_t, std::map<instance_t, std::set<event_t> > > its_events;
    for (auto i : services_) {
        for (auto j : i.second) {
            if (j.second->group_->unicast_ == "local") {
                for (auto k : j.second->events_) {
                    its_events[i.first][j.first].insert(k.first);
                }
            }
        }
    }
    return its_events;
}

void configuration_impl::set_event(
        std::shared_ptr<vsomeip::event> &_event) const {
    auto found_service = services_.find(_event->get_service());
    if (found_service != services_.end()) {
        auto found_instance = found_service->second.find(
                _event->get_instance());
        if (found_instance != found_service->second.end()) {
            auto found_event = found_instance->second->events_.find(
                    _event->get_event());
            if (found_event != found_instance->second->events_.end()) {
                _event->set_field(found_event->second->is_field_);
                _event->set_reliable(found_event->second->is_reliable_);
            }
        }
    }
}

servicegroup *configuration_impl::find_servicegroup(
        const std::string &_name) const {
    servicegroup *its_servicegroup(0);
    auto find_servicegroup = servicegroups_.find(_name);
    if (find_servicegroup != servicegroups_.end()) {
        its_servicegroup = find_servicegroup->second.get();
    }
    return its_servicegroup;
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

}  // namespace config
}  // namespace vsomeip

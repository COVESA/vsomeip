// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <future>
#include <thread>
#include <iomanip>

#ifndef WIN32
#include <dlfcn.h>
#endif
#include <iostream>
#include <vsomeip/defines.hpp>

#include "../include/application_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/serializer.hpp"
#include "../../routing/include/routing_manager_impl.hpp"
#include "../../routing/include/routing_manager_proxy.hpp"
#include "../../utility/include/utility.hpp"
#include "../../configuration/include/configuration_impl.hpp"

namespace vsomeip {

application_impl::application_impl(const std::string &_name)
        : is_initialized_(false), name_(_name),
          file_(VSOMEIP_DEFAULT_CONFIGURATION_FILE),
          folder_(VSOMEIP_DEFAULT_CONFIGURATION_FOLDER),
          routing_(0),
          signals_(io_, SIGINT, SIGTERM),
          num_dispatchers_(0), logger_(logger::get()),
          stopped_(false) {
}

application_impl::~application_impl() {
#ifdef WIN32
    exit(0); // TODO: clean solution...
#endif
    stop_thread_.join();
}

void application_impl::set_configuration(
        const std::shared_ptr<configuration> _configuration) {
    if(_configuration)
        configuration_ = std::make_shared<cfg::configuration_impl>(*(std::static_pointer_cast<cfg::configuration_impl, configuration>(_configuration)));
}

bool application_impl::init() {
    // Application name
    if (name_ == "") {
        const char *its_name = getenv(VSOMEIP_ENV_APPLICATION_NAME);
        if (nullptr != its_name) {
            name_ = its_name;
        }
    }

    // load configuration from module
    std::string config_module = "";
    const char *its_config_module = getenv(VSOMEIP_ENV_CONFIGURATION_MODULE);
    if(nullptr != its_config_module) {
        config_module = its_config_module;
        if (config_module.rfind(".so") != config_module.length() - 3) {
                config_module += ".so";
        }
        VSOMEIP_INFO << "Loading configuration from module \"" << config_module << "\".";
#ifdef WIN32
        HMODULE config = LoadLibrary(config_module.c_str());
        if (config != 0) {
            VSOMEIP_INFO << "\"" << config_module << "\" was loaded";
            if (!configuration_) {
                VSOMEIP_ERROR << "Configuration not set.";
                return false;
            }
        } else {
            VSOMEIP_ERROR << "\"" << config_module << "\" could not be loaded (" << GetLastError() << ")";
            return false;
        }
        FreeModule(config);
#else
        void *config = dlopen(config_module.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if(config != 0) {
            VSOMEIP_INFO << "\"" << config_module << "\" was loaded";
            if(!configuration_) {
                VSOMEIP_ERROR << "Configuration not set.";
                return false;
            }
        } else {
            VSOMEIP_ERROR << "\"" << config_module << "\" could not be loaded (" << dlerror() << ")";
            return false;
        }
        dlclose(config);
#endif
    } else {
        // Override with local file /folder
        std::string its_local_file(VSOMEIP_LOCAL_CONFIGURATION_FILE);
        if (utility::is_file(its_local_file)) {
            file_ = its_local_file;
        }

        std::string its_local_folder(VSOMEIP_LOCAL_CONFIGURATION_FOLDER);
        if (utility::is_folder(its_local_folder)) {
            folder_ = its_local_folder;
        }

        // Finally, override with path from environment
        const char *its_env = getenv(VSOMEIP_ENV_CONFIGURATION);
        if (nullptr != its_env) {
            if (utility::is_file(its_env)) {
                file_ = its_env;
                folder_ = "";
            } else if (utility::is_folder(its_env)) {
                folder_ = its_env;
                file_ = "";
            }
        }
    }

    std::shared_ptr<configuration> its_configuration = get_configuration();
    if (its_configuration) {
        VSOMEIP_INFO << "Initializing vsomeip application \"" << name_ << "\"";

        if (utility::is_file(file_))
            VSOMEIP_INFO << "Using configuration file: \"" << file_ << "\"";

        if (utility::is_folder(folder_))
            VSOMEIP_INFO << "Using configuration folder: \"" << folder_ << "\"";

        bool is_routing_manager_host(false);
        client_ = its_configuration->get_id(name_);
        std::string its_routing_host = its_configuration->get_routing_host();

        if (client_ == 0 || its_routing_host == "") {
#ifndef WIN32
            if (!utility::auto_configuration_init()) {
                VSOMEIP_ERROR << "Configuration incomplete and "
                                 "Auto-configuration failed!";
                return false;
            }
#else
            return false;
#endif
        }
        // Client ID
        if (client_ == 0) {
#ifndef WIN32
            client_ = utility::get_client_id();
            VSOMEIP_INFO << "No SOME/IP client identifier configured. "
                    << "Using auto-configured "
                    << std::hex << std::setfill('0') << std::setw(4)
                    << client_;
#else
            return false;
#endif
        }

        // Routing
        if (its_routing_host == "") {
#ifndef WIN32
            is_routing_manager_host = utility::is_routing_manager_host();
            VSOMEIP_INFO << "No routing manager configured. "
                    << "Using auto-configuration ("
                    << (is_routing_manager_host ?
                            "Host" : "Proxy") << ")";
#else
            return false;
#endif
        } else {
            is_routing_manager_host = (its_routing_host == name_);
        }

        if (is_routing_manager_host) {
            routing_ = std::make_shared<routing_manager_impl>(this);
        } else {
            routing_ = std::make_shared<routing_manager_proxy>(this);
        }

        routing_->init();

        num_dispatchers_ = its_configuration->get_num_dispatchers(name_);

        // Smallest allowed session identifier
        session_ = 0x0001;

        VSOMEIP_DEBUG<< "Application(" << (name_ != "" ? name_ : "unnamed")
                << ", " << std::hex << client_ << ") is initialized (uses "
                << std::dec << num_dispatchers_ << " dispatcher threads).";

        is_initialized_ = true;
    }

    if (is_initialized_) {
        signals_.add(SIGINT);
        signals_.add(SIGTERM);

        // Register signal handler
        std::function<void(boost::system::error_code const &, int)> its_signal_handler =
                [this] (boost::system::error_code const &_error, int _signal) {
                    if (!_error) {
                        switch (_signal) {
                            case SIGTERM:
                            case SIGINT:
                            stop();
                            exit(0);
                            break;
                            default:
                            break;
                        }
                    }
                };
        signals_.async_wait(its_signal_handler);
    }

    return is_initialized_;
}

void application_impl::start() {
    {
        std::lock_guard<std::mutex> its_lock(start_stop_mutex_);
        if(io_.stopped()) {
            io_.reset();
        } else if(stop_thread_.joinable()) {
            VSOMEIP_ERROR << "Trying to start an already started application.";
            return;
        }

        is_dispatching_ = true;

        for (size_t i = 0; i < num_dispatchers_; i++)
            dispatchers_.push_back(
                    std::thread(std::bind(&application_impl::dispatch, this)));

        if(stop_thread_.joinable()) {
            stop_thread_.join();
        }
        stop_thread_= std::thread(&application_impl::wait_for_stop, this);

        if (routing_)
            routing_->start();
    }
    VSOMEIP_INFO << "Starting vsomeip application \"" << name_ << "\"";
    io_.run();
}

void application_impl::stop() {
    VSOMEIP_INFO << "Stopping vsomeip application \"" << name_ << "\"";
    std::lock_guard<std::mutex> its_lock(start_stop_mutex_);
    is_dispatching_ = false;
    dispatch_condition_.notify_all();
    for (auto &t : dispatchers_) {
        if(t.get_id() == std::this_thread::get_id()) {
            continue;
        }
        if(t.joinable()) {
            t.join();
        }
    }

    if (routing_)
        routing_->stop();
#ifndef WIN32
    utility::auto_configuration_exit();
#endif
    stopped_ = true;
    stop_cv_.notify_one();
}

void application_impl::offer_service(service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {
    if (routing_)
        routing_->offer_service(client_, _service, _instance, _major, _minor);
}

void application_impl::stop_offer_service(service_t _service,
        instance_t _instance) {
    if (routing_)
        routing_->stop_offer_service(client_, _service, _instance);
}

void application_impl::request_service(service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor, bool _use_exclusive_proxy) {
    if (routing_)
        routing_->request_service(client_, _service, _instance, _major, _minor,
                _use_exclusive_proxy);
}

void application_impl::release_service(service_t _service,
        instance_t _instance) {
    if (routing_)
        routing_->release_service(client_, _service, _instance);
}

void application_impl::subscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, major_version_t _major) {
    if (routing_)
        routing_->subscribe(client_, _service, _instance, _eventgroup, _major);
}

void application_impl::unsubscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) {
    if (routing_)
        routing_->unsubscribe(client_, _service, _instance, _eventgroup);
}

bool application_impl::is_available(
        service_t _service, instance_t _instance) const {
    auto found_available = available_.find(_service);
    if (found_available == available_.end())
        return false;

    return (found_available->second.find(_instance)
            != found_available->second.end());
}

void application_impl::send(std::shared_ptr<message> _message, bool _flush) {
    std::lock_guard<std::mutex> its_lock(session_mutex_);
    if (routing_) {
        // in case of requests set the request-id (client-id|session-id)
        bool is_request = utility::is_request(_message);
        if (is_request) {
            _message->set_client(client_);
            _message->set_session(session_);
        }
        // in case of successful sending, increment the session-id
        if (routing_->send(client_, _message, _flush)) {
            if (is_request) {
                update_session();
            }
        }
    }
}

void application_impl::notify(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload) const {
    if (routing_)
        routing_->notify(_service, _instance, _event, _payload);
}

void application_impl::notify_one(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload,
        client_t _client) const {

    if (routing_) {
        routing_->notify_one(_service, _instance, _event, _payload, _client);
    }
}

void application_impl::register_state_handler(state_handler_t _handler) {
    handler_ = _handler;
}

void application_impl::unregister_state_handler() {
    handler_ = nullptr;
}

void application_impl::register_availability_handler(service_t _service,
        instance_t _instance, availability_handler_t _handler) {
    {
        std::unique_lock<std::mutex> its_lock(availability_mutex_);
        availability_[_service][_instance] = _handler;
    }
    std::async(std::launch::async, [this, _service, _instance, _handler]() {
        _handler(_service, _instance, is_available(_service, _instance));
    });
}

void application_impl::unregister_availability_handler(service_t _service,
        instance_t _instance) {
    std::unique_lock<std::mutex> its_lock(availability_mutex_);
    auto found_service = availability_.find(_service);
    if (found_service != availability_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            found_service->second.erase(_instance);
        }
    }
}

bool application_impl::on_subscription(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, client_t _client, bool _subscribed) {

    std::unique_lock<std::mutex> its_lock(subscription_mutex_);
    auto found_service = subscription_.find(_service);
    if (found_service != subscription_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                return found_eventgroup->second(_client, _subscribed);
            }
        }
    }
    return true;
}

void application_impl::register_subscription_handler(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        subscription_handler_t _handler) {

    std::unique_lock<std::mutex> its_lock(subscription_mutex_);
    subscription_[_service][_instance][_eventgroup] = _handler;
}

void application_impl::unregister_subscription_handler(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {

    std::unique_lock<std::mutex> its_lock(subscription_mutex_);
    auto found_service = subscription_.find(_service);
    if (found_service != subscription_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                found_instance->second.erase(_eventgroup);
            }
        }
    }
}

void application_impl::register_message_handler(service_t _service,
        instance_t _instance, method_t _method, message_handler_t _handler) {
    std::unique_lock<std::mutex> its_lock(members_mutex_);
    members_[_service][_instance][_method] = _handler;
}

void application_impl::unregister_message_handler(service_t _service,
        instance_t _instance, method_t _method) {
    std::unique_lock<std::mutex> its_lock(members_mutex_);
    auto found_service = members_.find(_service);
    if (found_service != members_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_method = found_instance->second.find(_method);
            if (found_method != found_instance->second.end()) {
                found_instance->second.erase(_method);
            }
        }
    }
}

void application_impl::offer_event(service_t _service, instance_t _instance,
           event_t _event, std::set<eventgroup_t> _eventgroups, bool _is_field) {
       if (routing_)
           routing_->register_event(client_, _service, _instance, _event,
                   _eventgroups, _is_field, true);
}

void application_impl::stop_offer_event(service_t _service, instance_t _instance,
       event_t _event) {
   if (routing_)
       routing_->unregister_event(client_, _service, _instance, _event, true);
}

void application_impl::request_event(service_t _service, instance_t _instance,
           event_t _event, std::set<eventgroup_t> _eventgroups, bool _is_field) {
       if (routing_)
           routing_->register_event(client_, _service, _instance, _event,
                   _eventgroups, _is_field, false);
  }

void application_impl::release_event(service_t _service, instance_t _instance,
       event_t _event) {
   if (routing_)
       routing_->unregister_event(client_, _service, _instance, _event, false);
}

// Interface "routing_manager_host"
const std::string & application_impl::get_name() const {
    return name_;
}

client_t application_impl::get_client() const {
    return client_;
}

std::shared_ptr<configuration> application_impl::get_configuration() const {
    if(configuration_) {
        return configuration_;
    } else {
        std::set<std::string> its_input;
        std::shared_ptr<configuration> its_configuration;
        if (file_ != "") {
            its_input.insert(file_);
        }
        if (folder_ != "") {
            its_input.insert(folder_);
        }
        its_configuration = configuration::get(its_input);
        return its_configuration;
    }
}

boost::asio::io_service & application_impl::get_io() {
    return io_;
}

void application_impl::on_state(state_type_e _state) {
    if (handler_) {
        if (num_dispatchers_ > 0) {
            std::unique_lock<std::mutex> its_lock(dispatch_mutex_);
            handlers_.push_back([this, _state]() {
                handler_(_state);
            });
            dispatch_condition_.notify_one();
        } else {
            handler_(_state);
        }
    }
}

void application_impl::on_availability(service_t _service, instance_t _instance,
        bool _is_available) const {

    std::map<instance_t, availability_handler_t>::const_iterator found_instance;
    availability_handler_t its_handler;
    std::map<instance_t, availability_handler_t>::const_iterator found_wildcard_instance;
    availability_handler_t its_wildcard_handler;
    bool has_handler(false);
    bool has_wildcard_handler(false);

    {
        std::unique_lock<std::mutex> its_lock(availability_mutex_);

        if (_is_available == is_available(_service, _instance))
            return;

        if (_is_available) {
            available_[_service].insert(_instance);
        } else {
            auto found_available_service = available_.find(_service);
            if (found_available_service != available_.end())
                found_available_service->second.erase(_instance);
        }

        auto found_service = availability_.find(_service);
        if (found_service != availability_.end()) {
            found_instance = found_service->second.find(_instance);
            has_handler = (found_instance != found_service->second.end());
            if (has_handler)
                its_handler = found_instance->second;
            found_wildcard_instance = found_service->second.find(ANY_INSTANCE);
            has_wildcard_handler = (found_wildcard_instance != found_service->second.end());
            if (has_wildcard_handler)
                its_wildcard_handler = found_wildcard_instance->second;
        }
    }

    if (num_dispatchers_ > 0) {
        if (has_handler) {
            std::unique_lock<std::mutex> its_lock(dispatch_mutex_);
            handlers_.push_back(
                    [its_handler, _service, _instance, _is_available]() {
                        its_handler(_service, _instance, _is_available);
                    });
            dispatch_condition_.notify_one();
        }
        if (has_wildcard_handler) {
            std::unique_lock < std::mutex > its_lock(dispatch_mutex_);
            handlers_.push_back(
                    [its_wildcard_handler, _service, _instance, _is_available]() {
                        its_wildcard_handler(_service, _instance, _is_available);
                    });
            dispatch_condition_.notify_one();
        }
    } else {
        if(has_handler) {
            its_handler(_service, _instance, _is_available);
        }
        if(has_wildcard_handler) {
            its_wildcard_handler(_service, _instance, _is_available);
        }
    }
}

void application_impl::on_message(std::shared_ptr<message> _message) {
    service_t its_service = _message->get_service();
    instance_t its_instance = _message->get_instance();
    method_t its_method = _message->get_method();

    std::map<method_t, message_handler_t>::iterator found_method;
    message_handler_t its_handler;
    bool has_handler(false);

    {
        std::unique_lock<std::mutex> its_lock(members_mutex_);

        auto found_service = members_.find(its_service);
        if (found_service == members_.end()) {
            found_service = members_.find(ANY_SERVICE);
        }
        if (found_service != members_.end()) {
            auto found_instance = found_service->second.find(its_instance);
            if (found_instance == found_service->second.end()) {
                found_instance = found_service->second.find(ANY_INSTANCE);
            }
            if (found_instance != found_service->second.end()) {
                auto found_method = found_instance->second.find(its_method);
                if (found_method == found_instance->second.end()) {
                    found_method = found_instance->second.find(ANY_METHOD);
                }

                if (found_method != found_instance->second.end()) {
                    its_handler = found_method->second;
                    has_handler = true;
                }
            }
        }
    }

    if (has_handler) {
        if (num_dispatchers_ > 0) {
            std::unique_lock<std::mutex> its_lock(dispatch_mutex_);
            handlers_.push_back([its_handler, _message]() {
                its_handler(_message);
            });
            dispatch_condition_.notify_one();
        } else {
            its_handler(_message);
        }
    }
}

void application_impl::on_error(error_code_e _error) {
    VSOMEIP_ERROR<< ERROR_INFO[static_cast<int>(_error)] << " ("
    << static_cast<int>(_error) << ")";
}

// Interface "service_discovery_host"
routing_manager * application_impl::get_routing_manager() const {
    return routing_.get();
}

// Internal
void application_impl::service() {
    io_.run();
}

void application_impl::dispatch() {
    std::function<void()> handler;
    while (is_dispatching_) {
        {
            std::unique_lock<std::mutex> its_lock(dispatch_mutex_);
            if (handlers_.empty()) {
                dispatch_condition_.wait(its_lock);
                continue;
            } else {
                handler = handlers_.front();
                handlers_.pop_front();
            }
        }
        handler();
    }
}

void application_impl::wait_for_stop() {
    std::unique_lock<std::mutex> its_lock(start_stop_mutex_);
    while(!stopped_) {
        stop_cv_.wait(its_lock);
    }
    stopped_ = false;

    for (auto &t : dispatchers_) {
        if(t.joinable()) {
            t.join();
        }
    }
    io_.stop();
}

} // namespace vsomeip

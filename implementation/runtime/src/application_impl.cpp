// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <thread>
#include <iostream>

#include <vsomeip/configuration.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/application_impl.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../message/include/serializer.hpp"
#include "../../routing/include/routing_manager_impl.hpp"
#include "../../routing/include/routing_manager_proxy.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

application_impl::application_impl(const std::string &_name)
        : name_(_name), is_initialized_(false), routing_(0), signals_(io_,
                SIGINT, SIGTERM), num_dispatchers_(0), logger_(logger::get()) {
}

application_impl::~application_impl() {
#ifdef WIN32
    // killemall
    exit(0);
#endif
}

bool application_impl::init() {
    bool is_initialized(false);

    // Application name
    if (name_ == "") {
        const char *its_name = getenv(VSOMEIP_ENV_APPLICATION_NAME);
        if (nullptr != its_name) {
            name_ = its_name;
        } else {
            VSOMEIP_ERROR<< "Missing application name. "
            "Please set environment variable VSOMEIP_APPLICATION_NAME.";
            return false;
        }
    }

    // Set default path
    std::string its_path(VSOMEIP_DEFAULT_CONFIGURATION_FILE_PATH);

    // Override with path from environment
    const char *its_env_path = getenv(VSOMEIP_ENV_CONFIGURATION_FILE_PATH);
    if (nullptr != its_env_path && utility::exists(its_env_path))
        its_path = its_env_path;

    // Override with local path
    std::string its_local_path(VSOMEIP_LOCAL_CONFIGURATION_FILE_PATH);
    if (utility::exists(its_local_path))
        its_path = its_local_path;

    configuration_.reset(configuration::get(its_path));
    VSOMEIP_INFO<< "Using configuration file: " << its_path;

    if (configuration_) {
        client_ = configuration_->get_id(name_);

        // Routing
        if (name_ == configuration_->get_routing_host()) {
            routing_ = std::make_shared<routing_manager_impl>(this);
        } else {
            routing_ = std::make_shared<routing_manager_proxy>(this);
        }

        routing_->init();

        num_dispatchers_ = configuration_->get_num_dispatchers(name_);

        // Smallest allowed session identifier
        session_ = 0x0001;

        VSOMEIP_DEBUG<< "Application(" << name_ << ", "
                << std::hex << client_ << ") is initialized (uses "
                << std::dec << num_dispatchers_ << " dispatcher threads).";

        is_initialized_ = true;
    }

    if (is_initialized_) {
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
    is_dispatching_ = true;

    for (size_t i = 0; i < num_dispatchers_; i++)
        dispatchers_.push_back(
                std::thread(std::bind(&application_impl::dispatch, this)));

    if (routing_)
        routing_->start();

    io_.run();
}

void application_impl::stop() {
    is_dispatching_ = false;
    dispatch_condition_.notify_all();
    for (auto &t : dispatchers_)
        t.join();

    if (routing_)
        routing_->stop();

    io_.stop();
}

void application_impl::offer_service(service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor, ttl_t _ttl) {
    if (routing_)
        routing_->offer_service(client_, _service, _instance, _major, _minor,
                _ttl);
}

void application_impl::stop_offer_service(service_t _service,
        instance_t _instance) {
    if (routing_)
        routing_->stop_offer_service(client_, _service, _instance);
}

void application_impl::request_service(service_t _service, instance_t _instance,
        bool _has_selective, major_version_t _major, minor_version_t _minor,
        ttl_t _ttl) {
    if (routing_)
        routing_->request_service(client_, _service, _instance, _major, _minor,
                _ttl, _has_selective);
}

void application_impl::release_service(service_t _service,
        instance_t _instance) {
    if (routing_)
        routing_->release_service(client_, _service, _instance);
}

void application_impl::subscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl) {
    if (routing_)
        routing_->subscribe(client_, _service, _instance, _eventgroup, _major,
                _ttl);
}

void application_impl::unsubscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) {
    if (routing_)
        routing_->unsubscribe(client_, _service, _instance, _eventgroup);
}

bool application_impl::is_available(service_t _service, instance_t _instance) {
    auto found_available = available_.find(_service);
    if (found_available == available_.end())
        return false;

    return (found_available->second.find(_instance)
            != found_available->second.end());
}

void application_impl::send(std::shared_ptr<message> _message, bool _flush) {
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

void application_impl::register_event_handler(event_handler_t _handler) {
    handler_ = _handler;
}

void application_impl::unregister_event_handler() {
    handler_ = nullptr;
}

void application_impl::register_availability_handler(service_t _service,
        instance_t _instance, availability_handler_t _handler) {
    std::unique_lock<std::mutex> its_lock(availability_mutex_);
    availability_[_service][_instance] = _handler;
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

// Interface "routing_manager_host"
const std::string & application_impl::get_name() const {
    return name_;
}

client_t application_impl::get_client() const {
    return client_;
}

std::shared_ptr<configuration> application_impl::get_configuration() const {
    return configuration_;
}

boost::asio::io_service & application_impl::get_io() {
    return io_;
}

void application_impl::on_event(event_type_e _event) {
    if (handler_) {
        if (num_dispatchers_ > 0) {
            std::unique_lock<std::mutex> its_lock(dispatch_mutex_);
            handlers_.push_back([this, _event]() {
                handler_(_event);
            });
            dispatch_condition_.notify_one();
        } else {
            handler_(_event);
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

} // namespace vsomeip

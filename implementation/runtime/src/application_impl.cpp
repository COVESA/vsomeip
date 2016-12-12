// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <future>
#include <thread>
#include <iomanip>

#ifndef WIN32
#include <dlfcn.h>
#endif

#include <vsomeip/defines.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/application_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/serializer.hpp"
#include "../../routing/include/routing_manager_impl.hpp"
#include "../../routing/include/routing_manager_proxy.hpp"
#include "../../utility/include/utility.hpp"
#include "../../tracing/include/trace_connector.hpp"
#include "../../tracing/include/enumeration_types.hpp"

namespace vsomeip {

uint32_t application_impl::app_counter__ = 0;
std::mutex application_impl::app_counter_mutex__;

application_impl::application_impl(const std::string &_name)
        : runtime_(runtime::get()),
          client_(ILLEGAL_CLIENT),
          session_(1),
          is_initialized_(false), name_(_name),
          work_(std::make_shared<boost::asio::io_service::work>(io_)),
          routing_(0),
          state_(state_type_e::ST_DEREGISTERED),
#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
          signals_(io_, SIGINT, SIGTERM),
          catched_signal_(false),
#endif
          is_dispatching_(false),
          max_dispatchers_(VSOMEIP_MAX_DISPATCHERS),
          max_dispatch_time_(VSOMEIP_MAX_DISPATCH_TIME),
          logger_(logger::get()),
          stopped_(false),
          block_stopping_(false),
          is_routing_manager_host_(false),
          stopped_called_(false) {
}

application_impl::~application_impl() {
    runtime_->remove_application(name_);
}

void application_impl::set_configuration(
        const std::shared_ptr<configuration> _configuration) {
    (void)_configuration;
    // Dummy.
}

bool application_impl::init() {
    if(is_initialized_) {
        VSOMEIP_WARNING << "Trying to initialize an already initialized application.";
        return true;
    }
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
    if (nullptr != its_config_module) {
        // TODO: Add loading of custom configuration module
    } else { // load default module
        std::shared_ptr<configuration> *its_configuration =
                static_cast<std::shared_ptr<configuration> *>(utility::load_library(
                        VSOMEIP_CFG_LIBRARY,
                        VSOMEIP_CFG_RUNTIME_SYMBOL_STRING));

        if (its_configuration && (*its_configuration)) {
            configuration_ = (*its_configuration);
            configuration_->load(name_);
            VSOMEIP_INFO << "Default configuration module loaded.";
        } else {
            exit(-1);
        }
    }

    std::shared_ptr<configuration> its_configuration = get_configuration();
    if (its_configuration) {
        VSOMEIP_INFO << "Initializing vsomeip application \"" << name_ << "\".";
        client_ = its_configuration->get_id(name_);

        // Max dispatchers is the configured maximum number of dispatchers and
        // the main dispatcher
        max_dispatchers_ = its_configuration->get_max_dispatchers(name_) + 1;
        max_dispatch_time_ = its_configuration->get_max_dispatch_time(name_);

        std::string its_routing_host = its_configuration->get_routing_host();
        if (!utility::auto_configuration_init(its_configuration)) {
            VSOMEIP_WARNING << "Could _not_ initialize auto-configuration:"
                    " Cannot guarantee unique application identifiers!";
        } else {
            // Client Identifier
            client_t its_old_client = client_;
            client_ = utility::request_client_id(its_configuration, name_, client_);
            if (client_ == ILLEGAL_CLIENT) {
                VSOMEIP_ERROR << "Couldn't acquire client identifier";
                return false;
            }
            VSOMEIP_INFO << "SOME/IP client identifier configured. "
                    << "Using "
                    << std::hex << std::setfill('0') << std::setw(4)
                    << client_
                    << " (was: "
                    << std::hex << std::setfill('0') << std::setw(4)
                    << its_old_client
                    << ")";

            // Routing
            if (its_routing_host == "") {
                VSOMEIP_INFO << "No routing manager configured. Using auto-configuration.";
                is_routing_manager_host_ = utility::is_routing_manager_host(client_);
            } 
        }

        if (its_routing_host != "") {
            is_routing_manager_host_ = (its_routing_host == name_);
        }

        if (is_routing_manager_host_) {
            VSOMEIP_INFO << "Instantiating routing manager [Host].";
            routing_ = std::make_shared<routing_manager_impl>(this);
        } else {
            VSOMEIP_INFO << "Instantiating routing manager [Proxy].";
            routing_ = std::make_shared<routing_manager_proxy>(this);
        }

        routing_->init();

        // Smallest allowed session identifier
        session_ = 0x0001;

#ifdef USE_DLT
        // Tracing
        std::shared_ptr<tc::trace_connector> its_trace_connector = tc::trace_connector::get();
        std::shared_ptr<cfg::trace> its_trace_cfg = its_configuration->get_trace();

        auto &its_channels_cfg = its_trace_cfg->channels_;
        for (auto it = its_channels_cfg.begin(); it != its_channels_cfg.end(); ++it) {
            its_trace_connector->add_channel(it->get()->id_, it->get()->name_);
        }

        auto &its_filter_rules_cfg = its_trace_cfg->filter_rules_;
        for (auto it = its_filter_rules_cfg.begin(); it != its_filter_rules_cfg.end(); ++it) {
            std::shared_ptr<cfg::trace_filter_rule> its_filter_rule_cfg = *it;
            tc::trace_connector::filter_rule_t its_filter_rule;

            its_filter_rule[tc::filter_criteria_e::SERVICES] = its_filter_rule_cfg->services_;
            its_filter_rule[tc::filter_criteria_e::METHODS] = its_filter_rule_cfg->methods_;
            its_filter_rule[tc::filter_criteria_e::CLIENTS] = its_filter_rule_cfg->clients_;

            its_trace_connector->add_filter_rule(it->get()->channel_, its_filter_rule);
        }

        bool enable_tracing = its_trace_cfg->is_enabled_;
        if (enable_tracing)
            its_trace_connector->init();
        its_trace_connector->set_enabled(enable_tracing);

        bool enable_sd_tracing = its_trace_cfg->is_sd_enabled_;
        its_trace_connector->set_sd_enabled(enable_sd_tracing);
#endif

        VSOMEIP_INFO << "Application(" << (name_ != "" ? name_ : "unnamed")
                << ", " << std::hex << client_ << ") is initialized ("
                << std::dec << max_dispatchers_ << ", "
                << std::dec << max_dispatch_time_ << ").";

        is_initialized_ = true;
    }

#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
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
                                catched_signal_ = true;
                                stop();
                                break;
                            default:
                                break;
                        }
                    }
                };
        signals_.async_wait(its_signal_handler);
    }
#endif
    return is_initialized_;
}

void application_impl::start() {
    const size_t io_thread_count = configuration_->get_io_thread_count(name_);
    {
        std::lock_guard<std::mutex> its_lock(start_stop_mutex_);
        if (io_.stopped()) {
            io_.reset();
        } else if(stop_thread_.joinable()) {
            VSOMEIP_ERROR << "Trying to start an already started application.";
            return;
        }
        if (stopped_) {
            utility::release_client_id(client_);
            utility::auto_configuration_exit(client_);

            {
                std::lock_guard<std::mutex> its_lock_start_stop(block_stop_mutex_);
                block_stopping_ = true;
            }
            block_stop_cv_.notify_all();

            stopped_ = false;
            return;
        }
        stopped_ = false;
        stopped_called_ = false;
        VSOMEIP_INFO << "Starting vsomeip application \"" << name_ << "\" using "
                << std::dec << io_thread_count << " threads";

        is_dispatching_ = true;

        start_caller_id_ = std::this_thread::get_id();

        auto its_main_dispatcher = std::make_shared<std::thread>(
                std::bind(&application_impl::main_dispatch, this));
        dispatchers_[its_main_dispatcher->get_id()] = its_main_dispatcher;

        if (stop_thread_.joinable()) {
            stop_thread_.join();
        }
        stop_thread_= std::thread(&application_impl::shutdown, this);

        if (routing_)
            routing_->start();

        for (size_t i = 0; i < io_thread_count - 1; i++) {
            std::shared_ptr<std::thread> its_thread
                = std::make_shared<std::thread>([this, i] {
                      io_.run();
                  });
            io_threads_.insert(its_thread);
        }
    }

    app_counter_mutex__.lock();
    app_counter__++;
    app_counter_mutex__.unlock();

    io_.run();

    if (stop_thread_.joinable()) {
        stop_thread_.join();
    }

    utility::release_client_id(client_);
    utility::auto_configuration_exit(client_);

    {
        std::lock_guard<std::mutex> its_lock_start_stop(block_stop_mutex_);
        block_stopping_ = true;
    }
    block_stop_cv_.notify_all();

    {
        std::lock_guard<std::mutex> its_lock(start_stop_mutex_);
        stopped_ = false;
    }

    app_counter_mutex__.lock();
    app_counter__--;

#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
    if (catched_signal_ && !app_counter__) {
        app_counter_mutex__.unlock();
        VSOMEIP_INFO << "Exiting vsomeip application...";
        exit(0);
    }
#endif
    app_counter_mutex__.unlock();
}

void application_impl::stop() {
#ifndef WIN32 // Gives serious problems under Windows.
    VSOMEIP_INFO << "Stopping vsomeip application \"" << name_ << "\".";
#endif
    bool block = true;
    {
        std::lock_guard<std::mutex> its_lock_start_stop(start_stop_mutex_);
        if (stopped_ || stopped_called_) {
            return;
        }
        stop_caller_id_ = std::this_thread::get_id();
        stopped_ = true;
        stopped_called_ = true;
        for (auto thread : io_threads_) {
            if (thread->get_id() == std::this_thread::get_id()) {
                block = false;
            }
        }
        if (start_caller_id_ == stop_caller_id_) {
            block = false;
        }
    }
    stop_cv_.notify_one();

    if (block) {
        std::unique_lock<std::mutex> block_stop_lock(block_stop_mutex_);
        while (!block_stopping_) {
            block_stop_cv_.wait(block_stop_lock);
        }
        block_stopping_ = false;
    }
}

void application_impl::offer_service(service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {
    if (routing_)
        routing_->offer_service(client_, _service, _instance, _major, _minor);
}

void application_impl::stop_offer_service(service_t _service, instance_t _instance,
    major_version_t _major, minor_version_t _minor) {
    if (routing_)
        routing_->stop_offer_service(client_, _service, _instance, _major, _minor);
}

void application_impl::request_service(service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor, bool _use_exclusive_proxy) {
    if (_use_exclusive_proxy) {
        message_handler_t handler([&](const std::shared_ptr<message>& response) {
            routing_->on_identify_response(get_client(), response->get_service(),
                    response->get_instance(), response->is_reliable());
        });
        register_message_handler(_service, _instance, ANY_METHOD - 1, handler);
    }

    if (routing_)
        routing_->request_service(client_, _service, _instance, _major, _minor,
                _use_exclusive_proxy);
}

void application_impl::release_service(service_t _service,
        instance_t _instance) {
    if (routing_) {
        routing_->release_service(client_, _service, _instance);
    }
}

void application_impl::subscribe(service_t _service, instance_t _instance,
                                 eventgroup_t _eventgroup,
                                 major_version_t _major,
                                 subscription_type_e _subscription_type,
                                 event_t _event) {
    if (routing_) {
        bool send_back_cached(false);
        bool send_back_cached_group(false);
        {
            std::lock_guard<std::mutex> its_lock(event_subscriptions_mutex_);
            auto found_service = event_subscriptions_.find(_service);
            if(found_service != event_subscriptions_.end()) {
                auto found_instance = found_service->second.find(_instance);
                if (found_instance != found_service->second.end()) {
                    auto its_event = found_instance->second.find(_event);
                    if (its_event == found_instance->second.end()) {
                        // first subscription to this event
                        event_subscriptions_[_service][_instance][_event] = false;
                    } else {
                        if(its_event->second) {
                            // initial values for this event have already been sent,
                            // send back cached value
                            if(_event == ANY_EVENT) {
                                send_back_cached_group = true;
                            } else {
                                send_back_cached = true;
                            }
                        }
                    }
                } else {
                    // first subscription to this service instance
                    event_subscriptions_[_service][_instance][_event] = false;
                }
            } else {
                // first subscription to this service
                event_subscriptions_[_service][_instance][_event] = false;
            }
        }
        if(send_back_cached) {
            send_back_cached_event(_service, _instance, _event);
        } else if(send_back_cached_group) {
            send_back_cached_eventgroup(_service, _instance, _event);
        }
        routing_->subscribe(client_, _service, _instance, _eventgroup, _major,
                _subscription_type);
    }
}

void application_impl::unsubscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) {
    if (routing_)
        routing_->unsubscribe(client_, _service, _instance, _eventgroup);
}

bool application_impl::is_available(
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) const {
    std::lock_guard<std::mutex> its_lock(availability_mutex_);
    return is_available_unlocked(_service, _instance, _major, _minor);
}

bool application_impl::is_available_unlocked(
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) const {

    bool is_available(false);

    auto found_available_service = available_.find(_service);
    if (found_available_service != available_.end()) {
        auto found_instance = found_available_service->second.find(_instance);
        if( found_instance != found_available_service->second.end()) {
            auto found_major = found_instance->second.find(_major);
            if( found_major != found_instance->second.end() ){
                if( _minor <= found_major->second) {
                    is_available = true;
                }
            } else if(_major == DEFAULT_MAJOR &&
                    _minor == DEFAULT_MINOR){
                is_available = true;
            }
        }
    }
    return is_available;
}

bool application_impl::are_available(
        available_t &_available,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) const {
    std::lock_guard<std::mutex> its_lock(availability_mutex_);
    return are_available_unlocked(_available, _service, _instance, _major, _minor);
}

bool application_impl::are_available_unlocked(available_t &_available,
                            service_t _service, instance_t _instance,
                            major_version_t _major, minor_version_t _minor) const {

    //find available services
    if(_service == ANY_SERVICE) {
        //add all available services
        for(auto its_available_services_it = available_.begin();
                its_available_services_it != available_.end();
                ++its_available_services_it)
            _available[its_available_services_it->first];
    } else {
        // check if specific service is available
        if(available_.find(_service) != available_.end())
            _available[_service];
    }

    //find available instances
    //iterate through found available services
    for(auto its_available_services_it = _available.begin();
            its_available_services_it != _available.end();
            ++its_available_services_it) {
        //get available service
        auto found_available_service = available_.find(its_available_services_it->first);
        if (found_available_service != available_.end()) {
            if(_instance == ANY_INSTANCE) {
                //add all available instances
                for(auto its_available_instances_it = found_available_service->second.begin();
                        its_available_instances_it != found_available_service->second.end();
                        ++its_available_instances_it)
                    _available[its_available_services_it->first][its_available_instances_it->first];
            } else {
                if(found_available_service->second.find(_instance) != found_available_service->second.end())
                    _available[its_available_services_it->first][_instance];
            }
        }
    }

    //find major versions
    //iterate through found available services
    for(auto its_available_services_it = _available.begin();
            its_available_services_it != _available.end();
            ++its_available_services_it) {
        //get available service
         auto found_available_service = available_.find(its_available_services_it->first);
         if (found_available_service != available_.end()) {
             //iterate through found available instances
             for(auto its_available_instances_it = found_available_service->second.begin();
                     its_available_instances_it != found_available_service->second.end();
                     ++its_available_instances_it) {
                 //get available instance
                 auto found_available_instance = found_available_service->second.find(its_available_instances_it->first);
                 if(found_available_instance != found_available_service->second.end()) {
                     if(_major == ANY_MAJOR || _major == DEFAULT_MAJOR) {
                         //add all major versions
                         for(auto its_available_major_it = found_available_instance->second.begin();
                                 its_available_major_it != found_available_instance->second.end();
                                 ++its_available_major_it)
                             _available[its_available_services_it->first][its_available_instances_it->first][its_available_major_it->first];
                     } else {
                         if(found_available_instance->second.find(_major) != found_available_instance->second.end())
                             _available[its_available_services_it->first][its_available_instances_it->first][_major];
                     }
                 }
             }
         }
    }

    //find minor
    //iterate through found available services
    auto its_available_services_it = _available.begin();
    while(its_available_services_it != _available.end()) {
        bool found_minor(false);
        //get available service
         auto found_available_service = available_.find(its_available_services_it->first);
         if (found_available_service != available_.end()) {
             //iterate through found available instances
             for(auto its_available_instances_it = found_available_service->second.begin();
                     its_available_instances_it != found_available_service->second.end();
                     ++its_available_instances_it) {
                 //get available instance
                 auto found_available_instance = found_available_service->second.find(its_available_instances_it->first);
                 if(found_available_instance != found_available_service->second.end()) {
                     //iterate through found available major version
                     for(auto its_available_major_it = found_available_instance->second.begin();
                             its_available_major_it != found_available_instance->second.end();
                             ++its_available_major_it) {
                         //get available major version
                         auto found_available_major = found_available_instance->second.find(its_available_major_it->first);
                         if(found_available_major != found_available_instance->second.end()) {
                             if(_minor == ANY_MINOR || _minor == DEFAULT_MINOR) {
                                 //add minor version
                                 _available[its_available_services_it->first][its_available_instances_it->first][its_available_major_it->first] = _minor;
                                 found_minor = true;
                             } else {
                                 if(_minor == found_available_major->second) {
                                     _available[its_available_services_it->first][its_available_instances_it->first][_major] = _minor;
                                     found_minor = true;
                                 }
                             }
                         }
                     }
                 }
             }
         }
         if(found_minor)
             ++its_available_services_it;
         else
             its_available_services_it = _available.erase(its_available_services_it);
    }

    if(_available.empty()) {
        _available[_service][_instance][_major] = _minor ;
        return false;
    }
    return true;
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
    return notify(_service, _instance, _event, _payload, false, true);
}

void application_impl::notify(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload, bool _force) const {
    if (routing_)
        routing_->notify(_service, _instance, _event, _payload, _force, true);
}

void application_impl::notify(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload, bool _force, bool _flush) const {
    if (routing_)
        routing_->notify(_service, _instance, _event, _payload, _force, _flush);
}

void application_impl::notify_one(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload,
        client_t _client) const {
    return notify_one(_service, _instance, _event, _payload, _client, false, true);
}

void application_impl::notify_one(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload,
        client_t _client, bool _force) const {
    if (routing_) {
        routing_->notify_one(_service, _instance, _event, _payload, _client,
                _force, true);
    }
}

void application_impl::notify_one(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload,
        client_t _client, bool _force, bool _flush) const {
    if (routing_) {
        routing_->notify_one(_service, _instance, _event, _payload, _client,
                _force, _flush);
    }
}

void application_impl::register_state_handler(state_handler_t _handler) {
    handler_ = _handler;
}

void application_impl::unregister_state_handler() {
    handler_ = nullptr;
}

void application_impl::register_availability_handler(service_t _service,
        instance_t _instance, availability_handler_t _handler,
        major_version_t _major, minor_version_t _minor) {
    std::lock_guard<std::mutex> availability_lock(availability_mutex_);
    if (state_ == state_type_e::ST_REGISTERED) {
        do_register_availability_handler(_service, _instance,
                _handler, _major, _minor);
    } else {
        availability_[_service][_instance]
            = std::make_tuple(_major, _minor, _handler, false);
    }
}

void application_impl::do_register_availability_handler(service_t _service,
        instance_t _instance, availability_handler_t _handler,
        major_version_t _major, minor_version_t _minor) {
        available_t available;
    bool are_available = are_available_unlocked(available, _service, _instance, _major, _minor);
    availability_[_service][_instance] = std::make_tuple(_major, _minor, _handler, true);

    std::lock_guard<std::mutex> handlers_lock(handlers_mutex_);

    std::shared_ptr<sync_handler> its_sync_handler
        = std::make_shared<sync_handler>([_handler, are_available, available]() {
                                             for(auto available_services_it : available)
                                                 for(auto available_instances_it : available_services_it.second)
                                                     _handler(available_services_it.first, available_instances_it.first, are_available);
                                         });
    handlers_.push_back(its_sync_handler);

    dispatcher_condition_.notify_one();
}

void application_impl::unregister_availability_handler(service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    std::lock_guard<std::mutex> its_lock(availability_mutex_);
    auto found_service = availability_.find(_service);
    if (found_service != availability_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_version = found_instance->second;
            auto found_major = std::get<0>(found_version);
            auto found_minor = std::get<1>(found_version);
            if(found_major == _major) {
                if(found_minor == _minor) {
                    found_service->second.erase(_instance);
                }
            }
        }
    }
}

bool application_impl::on_subscription(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, client_t _client, bool _subscribed) {

    std::lock_guard<std::mutex> its_lock(subscription_mutex_);
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

    std::lock_guard<std::mutex> its_lock(subscription_mutex_);
    subscription_[_service][_instance][_eventgroup] = _handler;

    message_handler_t handler([&](const std::shared_ptr<message>& request) {
        send(runtime_->create_response(request), true);
    });
    register_message_handler(_service, _instance, ANY_METHOD - 1, handler);
}

void application_impl::unregister_subscription_handler(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    std::lock_guard<std::mutex> its_lock(subscription_mutex_);
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
    unregister_message_handler(_service, _instance, ANY_METHOD - 1);
}

void application_impl::on_subscription_error(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, uint16_t _error) {
    error_handler_t handler = nullptr;
    std::lock_guard<std::mutex> its_lock(subscription_error_mutex_);
    auto found_service = eventgroup_error_handlers_.find(_service);
    if (found_service != eventgroup_error_handlers_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                auto found_client = found_eventgroup->second.find(get_client());
                if (found_client != found_eventgroup->second.end()) {
                    handler = found_client->second;

                }
            }
        }
    }
    if (handler) {
        {
            std::unique_lock<std::mutex> handlers_lock(handlers_mutex_);
            std::shared_ptr<sync_handler> its_sync_handler
                = std::make_shared<sync_handler>([handler, _error]() {
                                                    handler(_error);
                                                 });
            handlers_.push_back(its_sync_handler);
        }
        dispatcher_condition_.notify_all();
    }
}

void application_impl::register_subscription_error_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            error_handler_t _handler) {
    std::lock_guard<std::mutex> its_lock(subscription_error_mutex_);
    eventgroup_error_handlers_[_service][_instance][_eventgroup][get_client()] = _handler;
}

void application_impl::unregister_subscription_error_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup) {
    std::lock_guard<std::mutex> its_lock(subscription_error_mutex_);
    auto found_service = eventgroup_error_handlers_.find(_service);
    if (found_service != eventgroup_error_handlers_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                found_eventgroup->second.erase(get_client());
            }
        }
    }
}

void application_impl::register_message_handler(service_t _service,
        instance_t _instance, method_t _method, message_handler_t _handler) {
    std::lock_guard<std::mutex> its_lock(members_mutex_);
    members_[_service][_instance][_method] = _handler;
}

void application_impl::unregister_message_handler(service_t _service,
        instance_t _instance, method_t _method) {
    std::lock_guard<std::mutex> its_lock(members_mutex_);
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
           event_t _event, const std::set<eventgroup_t> &_eventgroups,
           bool _is_field) {
    return offer_event(_service, _instance, _event, _eventgroups, _is_field,
            std::chrono::milliseconds::zero(), false, nullptr);
}

void application_impl::offer_event(service_t _service, instance_t _instance,
           event_t _event, const std::set<eventgroup_t> &_eventgroups,
           bool _is_field,
           std::chrono::milliseconds _cycle, bool _change_resets_cycle,
           const epsilon_change_func_t &_epsilon_change_func) {
       if (routing_)
           routing_->register_event(client_, _service, _instance, _event,
                   _eventgroups, _is_field, _cycle, _change_resets_cycle,
                   _epsilon_change_func, true);
}

void application_impl::stop_offer_event(service_t _service, instance_t _instance,
       event_t _event) {
   if (routing_)
       routing_->unregister_event(client_, _service, _instance, _event, true);
}

void application_impl::request_event(service_t _service, instance_t _instance,
           event_t _event, const std::set<eventgroup_t> &_eventgroups,
           bool _is_field) {
       if (routing_)
           routing_->register_event(client_, _service, _instance, _event,
                   _eventgroups, _is_field,
                   std::chrono::milliseconds::zero(), false,
                   nullptr,
                   false);
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
    return configuration_;
}

boost::asio::io_service & application_impl::get_io() {
    return io_;
}

void application_impl::on_state(state_type_e _state) {
    {
        std::lock_guard<std::mutex> availability_lock(availability_mutex_);
        if (state_ != _state) {
            state_ = _state;
            if (state_ == state_type_e::ST_REGISTERED) {
                for (auto &its_service : availability_) {
                    for (auto &its_instance : its_service.second) {
                        if (!std::get<3>(its_instance.second)) {
                            do_register_availability_handler(
                                    its_service.first, its_instance.first,
                                    std::get<2>(its_instance.second),
                                    std::get<0>(its_instance.second),
                                    std::get<1>(its_instance.second));
                        }
                    }
                }
            }
        }
    }
    if (handler_) {
        {
            std::lock_guard<std::mutex> its_lock(handlers_mutex_);

            state_handler_t handler = handler_;
            std::shared_ptr<sync_handler> its_sync_handler
                = std::make_shared<sync_handler>([handler, _state]() {
                                                    handler(_state);
                                                 });

            handlers_.push_back(its_sync_handler);
        }
        dispatcher_condition_.notify_one();
    }
}

void application_impl::on_availability(service_t _service, instance_t _instance,
        bool _is_available, major_version_t _major, minor_version_t _minor) {

    std::map<minor_version_t, availability_handler_t>::const_iterator found_minor;
    availability_handler_t its_handler;
    std::map<minor_version_t, availability_handler_t>::const_iterator found_wildcard_minor;
    availability_handler_t its_wildcard_handler;
    bool has_handler(false);
    bool has_wildcard_handler(false);

    {
        std::lock_guard<std::mutex> availability_lock(availability_mutex_);
        if (_is_available == is_available_unlocked(_service, _instance, _major, _minor)) {
            return;
        }

        if (_is_available) {
            available_[_service][_instance][_major] = _minor;
        } else {
            auto found_available_service = available_.find(_service);
            if (found_available_service != available_.end()) {
                auto found_instance = found_available_service->second.find(_instance);
                if( found_instance != found_available_service->second.end()) {
                    auto found_major = found_instance->second.find(_major);
                    if( found_major != found_instance->second.end() ){
                        if( _minor == found_major->second)
                            found_available_service->second.erase(_instance);
                    }
                }
            }
        }

        auto found_service = availability_.find(_service);
        if (found_service != availability_.end()) {
            //find specific instance
            auto found_instance = found_service->second.find(_instance);
            if( found_instance != found_service->second.end()) {
                auto found_version = found_instance->second;
                auto requested_major = std::get<0>(found_version);
                if(requested_major == _major) {
                   has_handler = true;
                   its_handler = std::get<2>(found_version);
                }
            }

            //find wildcard instance major minor
            found_instance = found_service->second.find(ANY_INSTANCE);
            if( found_instance != found_service->second.end()) {
                auto found_version = found_instance->second;
                auto requested_major = std::get<0>(found_version);
                if(requested_major == ANY_MAJOR) {
                   has_wildcard_handler = true;
                   its_wildcard_handler = std::get<2>(found_version);
                }
            }
        }

        std::lock_guard<std::mutex> handlers_lock(handlers_mutex_);
        if (has_handler) {

            std::shared_ptr<sync_handler> its_sync_handler
                = std::make_shared<sync_handler>(
                        [its_handler, _service, _instance, _is_available]() {
                            its_handler(_service, _instance, _is_available);
                        }
                  );

            handlers_.push_back(its_sync_handler);
        }
        if (has_wildcard_handler) {

            std::shared_ptr<sync_handler> its_sync_handler
                = std::make_shared<sync_handler>(
                        [its_wildcard_handler, _service, _instance, _is_available]() {
                            its_wildcard_handler(_service, _instance, _is_available);
                        }
                  );

            handlers_.push_back(its_sync_handler);
        }
    }
    if (!_is_available) {
        std::lock_guard<std::mutex> its_lock(event_subscriptions_mutex_);
        auto found_service = event_subscriptions_.find(_service);
        if (found_service != event_subscriptions_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                for (auto &e : found_instance->second) {
                    e.second = false;
                }
            }
        }
    }

    if (has_handler || has_wildcard_handler) {
        dispatcher_condition_.notify_one();
    }
}

void application_impl::on_message(const std::shared_ptr<message> &&_message) {
    service_t its_service = _message->get_service();
    instance_t its_instance = _message->get_instance();
    method_t its_method = _message->get_method();

    if (_message->get_message_type() == message_type_e::MT_NOTIFICATION) {
        std::lock_guard<std::mutex> its_lock(event_subscriptions_mutex_);
        auto found_service = event_subscriptions_.find(its_service);
        if(found_service != event_subscriptions_.end()) {
            auto found_instance = found_service->second.find(its_instance);
            if (found_instance != found_service->second.end()) {
                auto its_event = found_instance->second.find(its_method);
                if (its_event != found_instance->second.end()) {
                    its_event->second = true;
                } else {
                    // received a event which nobody yet subscribed to
                    event_subscriptions_[its_service][its_instance][its_method] = true;
                    // check if someone subscribed to ANY_EVENT
                    auto its_any_event = found_instance->second.find(ANY_EVENT);
                    if(its_any_event == found_instance->second.end()) {
                        return;
                    }
                }
            } else {
                // received a event from a service instance which nobody yet subscribed to
                event_subscriptions_[its_service][its_instance][its_method] = true;
            }
        } else {
            // received a event from a service which nobody yet subscribed to
            event_subscriptions_[its_service][its_instance][its_method] = true;
        }
    }

    {
        std::lock_guard<std::mutex> its_lock(members_mutex_);
        std::set<message_handler> its_handlers;
        auto found_service = members_.find(its_service);
        if (found_service != members_.end()) {
            auto found_instance = found_service->second.find(its_instance);
            if (found_instance != found_service->second.end()) {
                auto found_method = found_instance->second.find(its_method);
                if (found_method != found_instance->second.end()) {
                    its_handlers.insert(found_method->second);
                }
                auto found_any_method = found_instance->second.find(ANY_METHOD);
                if (found_any_method != found_instance->second.end()) {
                    its_handlers.insert(found_any_method->second);
                }
            }
            auto found_any_instance = found_service->second.find(ANY_INSTANCE);
            if (found_any_instance != found_service->second.end()) {
                auto found_method = found_any_instance->second.find(its_method);
                if (found_method != found_any_instance->second.end()) {
                    its_handlers.insert(found_method->second);
                }
                auto found_any_method = found_any_instance->second.find(ANY_METHOD);
                if (found_any_method != found_any_instance->second.end()) {
                    its_handlers.insert(found_any_method->second);
                }
            }
        }
        auto found_any_service = members_.find(ANY_SERVICE);
        if (found_any_service != members_.end()) {
            auto found_instance = found_any_service->second.find(its_instance);
            if (found_instance != found_any_service->second.end()) {
                auto found_method = found_instance->second.find(its_method);
                if (found_method != found_instance->second.end()) {
                    its_handlers.insert(found_method->second);
                }
                auto found_any_method = found_instance->second.find(ANY_METHOD);
                if (found_any_method != found_instance->second.end()) {
                    its_handlers.insert(found_any_method->second);
                }
            }
            auto found_any_instance = found_any_service->second.find(ANY_INSTANCE);
            if (found_any_instance != found_any_service->second.end()) {
                auto found_method = found_any_instance->second.find(its_method);
                if (found_method != found_any_instance->second.end()) {
                    its_handlers.insert(found_method->second);
                }
                auto found_any_method = found_any_instance->second.find(ANY_METHOD);
                if (found_any_method != found_any_instance->second.end()) {
                    its_handlers.insert(found_any_method->second);
                }
            }
        }

        if (its_handlers.size()) {
            std::lock_guard<std::mutex> its_lock(handlers_mutex_);
            for (const auto &its_handler : its_handlers) {
                auto handler = its_handler.handler_;
                std::shared_ptr<sync_handler> its_sync_handler =
                        std::make_shared<sync_handler>([handler, _message]() {
                            handler(std::move(_message));
                        });
                handlers_.push_back(its_sync_handler);
            }
        }
        dispatcher_condition_.notify_one();
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

void application_impl::main_dispatch() {
    std::unique_lock<std::mutex> its_lock(handlers_mutex_);
    while (is_dispatching_) {
        if (handlers_.empty()) {
            // Cancel other waiting dispatcher
            dispatcher_condition_.notify_all();
            // Wait for new handlers to execute
            while (handlers_.empty() && is_dispatching_) {
                dispatcher_condition_.wait(its_lock);
            }
        } else {
            while (is_dispatching_ && !handlers_.empty()) {
                std::shared_ptr<sync_handler> its_handler = handlers_.front();
                handlers_.pop_front();
                its_lock.unlock();
                invoke_handler(its_handler);
                its_lock.lock();

                remove_elapsed_dispatchers();

                if(!is_dispatching_) {
                    its_lock.unlock();
#ifdef WIN32
                    return;
#endif
                }
            }
        }
    }
}

void application_impl::dispatch() {
    std::thread::id its_id = std::this_thread::get_id();
    while (is_active_dispatcher(its_id)) {
        std::unique_lock<std::mutex> its_lock(handlers_mutex_);
        if (handlers_.empty()) {
             dispatcher_condition_.wait(its_lock);
             if (handlers_.empty()) { // Maybe woken up from main dispatcher
                 std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
                 elapsed_dispatchers_.insert(its_id);
                 return;
             }
        } else {
            while (!handlers_.empty() && is_active_dispatcher(its_id)) {
                std::shared_ptr<sync_handler> its_handler = handlers_.front();
                handlers_.pop_front();
                its_lock.unlock();
                invoke_handler(its_handler);
                its_lock.lock();

                remove_elapsed_dispatchers();
            }
        }
    }

    std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
    elapsed_dispatchers_.insert(std::this_thread::get_id());
}

void application_impl::invoke_handler(std::shared_ptr<sync_handler> &_handler) {
    std::thread::id its_id = std::this_thread::get_id();

    boost::asio::steady_timer its_dispatcher_timer(io_);
    its_dispatcher_timer.expires_from_now(std::chrono::milliseconds(max_dispatch_time_));
    its_dispatcher_timer.async_wait([this, its_id](const boost::system::error_code &_error) {
        if (!_error) {
            VSOMEIP_INFO << "Blocking call detected. Client=" << std::hex << get_client();
            std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
            blocked_dispatchers_.insert(its_id);

            // If possible, create a new dispatcher thread to unblock.
            // If this is _not_ possible, dispatching is blocked until at least
            // one of the active handler calls returns.
            if (dispatchers_.size() < max_dispatchers_) {
                auto its_dispatcher = std::make_shared<std::thread>(
                        std::bind(&application_impl::dispatch, this));
                dispatchers_[its_dispatcher->get_id()] = its_dispatcher;
            } else {
                VSOMEIP_ERROR << "Maximum number of dispatchers exceeded.";
            }
        }
    });

    _handler->handler_();
    its_dispatcher_timer.cancel();
    {
        std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
        blocked_dispatchers_.erase(its_id);
    }
}

bool application_impl::is_active_dispatcher(std::thread::id &_id) {
    std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
    for (auto d : dispatchers_) {
        if (d.first != _id &&
            blocked_dispatchers_.find(d.first) == blocked_dispatchers_.end()) {
            return false;
        }
    }
    return true;
}

void application_impl::remove_elapsed_dispatchers() {
    std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
    for (auto id : elapsed_dispatchers_) {
        auto its_dispatcher = dispatchers_.find(id);
        if (its_dispatcher->second->joinable())
            its_dispatcher->second->join();
        dispatchers_.erase(id);
    }
    elapsed_dispatchers_.clear();
}

void application_impl::clear_all_handler() {
    unregister_state_handler();

    {
        std::lock_guard<std::mutex> availability_lock(availability_mutex_);
        availability_.clear();
    }

    {
        std::lock_guard<std::mutex> its_lock(subscription_mutex_);
        subscription_.clear();
    }

    {
        std::lock_guard<std::mutex> its_lock(subscription_error_mutex_);
        eventgroup_error_handlers_.clear();
    }

    {
        std::lock_guard<std::mutex> its_lock(members_mutex_);
        members_.clear();
    }
    {
        std::unique_lock<std::mutex> its_lock(handlers_mutex_);
        handlers_.clear();
    }
}

void application_impl::shutdown() {
#ifndef WIN32
    boost::asio::detail::posix_signal_blocker blocker;
#endif

    {
        std::unique_lock<std::mutex> its_lock(start_stop_mutex_);
        while(!stopped_) {
            stop_cv_.wait(its_lock);
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(handlers_mutex_);
        is_dispatching_ = false;
    }
    dispatcher_condition_.notify_all();
    for (auto its_dispatcher : dispatchers_) {
        if (its_dispatcher.second->get_id() != stop_caller_id_) {
            if (its_dispatcher.second->joinable()) {
                its_dispatcher.second->join();
            }
        } else {
            // If the caller of stop() is one of our dispatchers
            // it can happen the shutdown mechanism will block
            // as that thread probably can't be joined. The reason
            // is the caller of stop() probably wants to join the
            // thread once call start (which got to the IO-Thread)
            // and which is expected to return after stop() has been
            // called.
            // Therefore detach this thread instead of joining because
            // after it will return to "main_dispatch" it will be
            // properly shutdown anyways because "is_dispatching_"
            // was set to "false" here.
            its_dispatcher.second->detach();
        }
    }

    if (routing_)
        routing_->stop();

    work_.reset();
    io_.stop();

    {
        std::lock_guard<std::mutex> its_lock_start_stop(start_stop_mutex_);
        for (auto t : io_threads_) {
            t->join();
        }
        io_threads_.clear();
    }
}

bool application_impl::is_routing() const {
    return is_routing_manager_host_;
}

void application_impl::send_back_cached_event(service_t _service,
                                              instance_t _instance,
                                              event_t _event) {
    std::shared_ptr<event> its_event = routing_->get_event(_service,
            _instance, _event);
    if (its_event && its_event->is_field() && its_event->is_set()) {
        std::shared_ptr<message> its_message = runtime_->create_notification();
        its_message->set_service(_service);
        its_message->set_method(_event);
        its_message->set_instance(_instance);
        its_message->set_payload(its_event->get_payload());
        its_message->set_initial(true);
        on_message(std::move(its_message));
    }
}

void application_impl::send_back_cached_eventgroup(service_t _service,
                                                   instance_t _instance,
                                                   eventgroup_t _eventgroup) {
    std::set<std::shared_ptr<event>> its_events = routing_->find_events(_service, _instance,
            _eventgroup);
    for(const auto &its_event : its_events) {
        if (its_event && its_event->is_field() && its_event->is_set()) {
            std::shared_ptr<message> its_message = runtime_->create_notification();
            its_message->set_service(_service);
            its_message->set_method(its_event->get_event());
            its_message->set_instance(_instance);
            its_message->set_payload(its_event->get_payload());
            its_message->set_initial(true);
            on_message(std::move(its_message));
        }
    }
}

void application_impl::set_routing_state(routing_state_e _routing_state) {
    if (routing_)
        routing_->set_routing_state(_routing_state);
}

} // namespace vsomeip

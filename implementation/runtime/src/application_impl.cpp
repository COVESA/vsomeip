// Copyright (C) 2014-2018 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <future>
#include <thread>
#include <iomanip>
#include <iostream>

#include <boost/exception/diagnostic_information.hpp>

#ifndef _WIN32
#include <dlfcn.h>
#include <sys/syscall.h>
#endif

#include <vsomeip/defines.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/plugins/application_plugin.hpp>
#include <vsomeip/plugins/pre_configuration_plugin.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/application_impl.hpp"
#ifdef VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS
#include "../../configuration/include/configuration_impl.hpp"
#else
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/configuration_plugin.hpp"
#endif // VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS
#include "../../message/include/serializer.hpp"
#include "../../routing/include/routing_manager_impl.hpp"
#include "../../routing/include/routing_manager_proxy.hpp"
#include "../../utility/include/utility.hpp"
#include "../../tracing/include/connector_impl.hpp"
#include "../../plugin/include/plugin_manager_impl.hpp"
#include "../../endpoints/include/endpoint.hpp"
#include "../../security/include/security.hpp"

namespace vsomeip_v3 {

#ifdef ANDROID
configuration::~configuration() {}
#endif

uint32_t application_impl::app_counter__ = 0;
std::mutex application_impl::app_counter_mutex__;

application_impl::application_impl(const std::string &_name)
        : runtime_(runtime::get()),
          client_(VSOMEIP_CLIENT_UNSET),
          session_(0),
          is_initialized_(false), name_(_name),
          work_(std::make_shared<boost::asio::io_service::work>(io_)),
          routing_(0),
          state_(state_type_e::ST_DEREGISTERED),
          security_mode_(security_mode_e::SM_OFF),
#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
          signals_(io_, SIGINT, SIGTERM),
          catched_signal_(false),
#endif
          is_dispatching_(false),
          max_dispatchers_(VSOMEIP_MAX_DISPATCHERS),
          max_dispatch_time_(VSOMEIP_MAX_DISPATCH_TIME),
          stopped_(false),
          block_stopping_(false),
          is_routing_manager_host_(false),
          stopped_called_(false),
          watchdog_timer_(io_),
          client_side_logging_(false)
#ifdef VSOMEIP_HAS_SESSION_HANDLING_CONFIG
          , has_session_handling_(true)
#endif // VSOMEIP_HAS_SESSION_HANDLING_CONFIG
{
    own_uid_ = ANY_UID;
    own_gid_ = ANY_GID;
#ifndef _WIN32
    own_uid_ = getuid();
    own_gid_ = getgid();
#endif
}

application_impl::~application_impl() {
    runtime_->remove_application(name_);
    try {
        if (stop_thread_.joinable()) {
            stop_thread_.detach();
        }
    } catch (const std::exception& e) {
        std::cerr << __func__ << " catched exception (shutdown): " << e.what() << std::endl;
    }

    try {
        std::lock_guard<std::mutex> its_lock_start_stop(start_stop_mutex_);
        for (const auto& t : io_threads_) {
            if (t->joinable()) {
                t->detach();
            }
        }
        io_threads_.clear();
    } catch (const std::exception& e) {
        std::cerr << __func__ << " catched exception (io threads): " << e.what() << std::endl;
    }

    try {
        std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
        for (const auto& its_dispatcher : dispatchers_) {
            if (its_dispatcher.second->joinable()) {
                its_dispatcher.second->detach();
            }
        }
        dispatchers_.clear();
    } catch (const std::exception& e) {
        std::cerr << __func__ << " catched exception (dispatchers): " << e.what() << std::endl;
    }
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

    std::string configuration_path;

    // load configuration from module
    std::string config_module = "";
    const char *its_config_module = getenv(VSOMEIP_ENV_CONFIGURATION_MODULE);
    if (nullptr != its_config_module) {
        // TODO: Add loading of custom configuration module
    } else { // load default module
#ifndef VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS
        auto its_plugin = plugin_manager::get()->get_plugin(
                plugin_type_e::CONFIGURATION_PLUGIN, VSOMEIP_CFG_LIBRARY);
        if (its_plugin) {
            auto its_configuration_plugin
                = std::dynamic_pointer_cast<configuration_plugin>(its_plugin);
            if (its_configuration_plugin) {
                configuration_ = its_configuration_plugin->get_configuration(name_);
                VSOMEIP_INFO << "Configuration module loaded.";
            } else {
                std::cerr << "Invalid configuration module!" << std::endl;
                std::exit(EXIT_FAILURE);
            }
        } else {
            std::cerr << "Configuration module could not be loaded!" << std::endl;
            std::exit(EXIT_FAILURE);
        }
#else
        configuration_ = std::dynamic_pointer_cast<configuration>(
                std::make_shared<vsomeip_v3::cfg::configuration_impl>());
        if (configuration_path.length()) {
            configuration_->set_configuration_path(configuration_path);
        }
        configuration_->load(name_);
#endif // VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS
    }

    // Set security mode
    auto its_security = security::get();
    if (its_security->is_enabled()) {
        if (its_security->is_audit()) {
            security_mode_ = security_mode_e::SM_AUDIT;
        } else {
            security_mode_ = security_mode_e::SM_ON;
        }
    } else {
        security_mode_ = security_mode_e::SM_OFF;
    }

    const char *client_side_logging = getenv(VSOMEIP_ENV_CLIENTSIDELOGGING);
    if (client_side_logging != nullptr) {
        client_side_logging_ = true;
        VSOMEIP_INFO << "Client side logging for application: " << name_
                << " is enabled";

        if ('\0' != *client_side_logging) {
            std::stringstream its_converter(client_side_logging);
            if ('"' == its_converter.peek()) {
                its_converter.get(); // skip quote
            }
            uint16_t val(0xffffu);
            bool stop_parsing(false);
            do {
                const uint16_t prev_val(val);
                its_converter >> std::hex >> std::setw(4) >> val;
                if (its_converter.good()) {
                    const std::stringstream::int_type c = its_converter.eof()?'\0':its_converter.get();
                    switch (c) {
                    case '"':
                    case '.':
                    case ':':
                    case ' ':
                    case '\0': {
                            if ('.' != c) {
                                if (0xffffu == prev_val) {
                                    VSOMEIP_INFO << "+filter "
                                    << std::hex << std::setw(4) << std::setfill('0') << val;
                                    client_side_logging_filter_.insert(std::make_tuple(val, ANY_INSTANCE));
                                } else {
                                    VSOMEIP_INFO << "+filter "
                                    << std::hex << std::setw(4) << std::setfill('0') << prev_val << "."
                                    << std::hex << std::setw(4) << std::setfill('0') << val;
                                    client_side_logging_filter_.insert(std::make_tuple(prev_val, val));
                                }
                                val = 0xffffu;
                            }
                        }
                        break;
                    default:
                        stop_parsing = true;
                        break;
                    }
                }
            }
            while (!stop_parsing && its_converter.good());
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

#ifdef VSOMEIP_HAS_SESSION_HANDLING_CONFIG
        has_session_handling_ = its_configuration->has_session_handling(name_);
        if (!has_session_handling_)
            VSOMEIP_INFO << "application: " << name_
                << " has session handling switched off!";
#endif // VSOMEIP_HAS_SESSION_HANDLING_CONFIG

        std::string its_routing_host = its_configuration->get_routing_host();
        if (its_routing_host != "") {
            is_routing_manager_host_ = (its_routing_host == name_);
            if (is_routing_manager_host_ &&
                    !utility::is_routing_manager(configuration_)) {
#ifndef VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS
                VSOMEIP_ERROR << "application: " << name_ << " configured as "
                        "routing but other routing manager present. Won't "
                        "instantiate routing";
                is_routing_manager_host_ = false;
                return false;
#else
            is_routing_manager_host_ = true;
#endif // VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS
            }
        } else {
            is_routing_manager_host_ = utility::is_routing_manager(configuration_);
        }

        if (is_routing_manager_host_) {
            VSOMEIP_INFO << "Instantiating routing manager [Host].";
            if (client_ == VSOMEIP_CLIENT_UNSET) {
                client_ = static_cast<client_t>(
                          (configuration_->get_diagnosis_address() << 8)
                        & configuration_->get_diagnosis_mask());
                utility::request_client_id(configuration_, name_, client_);
            }
            routing_ = std::make_shared<routing_manager_impl>(this);
        } else {
            VSOMEIP_INFO << "Instantiating routing manager [Proxy].";
            routing_ = std::make_shared<routing_manager_proxy>(this, client_side_logging_, client_side_logging_filter_);
        }

        routing_->set_client(client_);
        routing_->init();

#ifdef USE_DLT
        // Tracing
        std::shared_ptr<trace::connector_impl> its_connector
            = trace::connector_impl::get();
        std::shared_ptr<cfg::trace> its_trace_configuration
            = its_configuration->get_trace();
        its_connector->configure(its_trace_configuration);
#endif

        VSOMEIP_INFO << "Application(" << (name_ != "" ? name_ : "unnamed")
                << ", " << std::hex << std::setw(4) << std::setfill('0') << client_
                << ") is initialized ("
                << std::dec << max_dispatchers_ << ", "
                << std::dec << max_dispatch_time_ << ").";

        is_initialized_ = true;
    }

#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
    if (is_initialized_) {
        signals_.add(SIGINT);
        signals_.add(SIGTERM);

        // Register signal handler
        auto its_signal_handler =
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

    if (configuration_) {
        auto its_plugins = configuration_->get_plugins(name_);
        auto its_app_plugin_info = its_plugins.find(plugin_type_e::APPLICATION_PLUGIN);
        if (its_app_plugin_info != its_plugins.end()) {
            for (auto its_library : its_app_plugin_info->second) {
                auto its_application_plugin = plugin_manager::get()->get_plugin(
                        plugin_type_e::APPLICATION_PLUGIN, its_library);
                if (its_application_plugin) {
                    VSOMEIP_INFO << "Client 0x" << std::hex << get_client()
                            << " Loading plug-in library: " << its_library << " succeeded!";
                    std::dynamic_pointer_cast<application_plugin>(its_application_plugin)->
                            on_application_state_change(name_, application_plugin_state_e::STATE_INITIALIZED);
                }
            }
        }
    } else {
        std::cerr << "Configuration module could not be loaded!" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return is_initialized_;
}

void application_impl::start() {
#ifndef _WIN32
    if (getpid() != static_cast<pid_t>(syscall(SYS_gettid))) {
        // only set threadname if calling thread isn't the main thread
        std::stringstream s;
        s << std::hex << std::setw(4) << std::setfill('0') << client_
                << "_io" << std::setw(2) << std::setfill('0') << 0;
        pthread_setname_np(pthread_self(),s.str().c_str());
    }
#endif
    if (!is_initialized_) {
        VSOMEIP_ERROR << "Trying to start an unintialized application.";
        return;
    }

    const size_t io_thread_count = configuration_->get_io_thread_count(name_);
    const int io_thread_nice_level = configuration_->get_io_thread_nice_level(name_);
    {
        std::lock_guard<std::mutex> its_lock(start_stop_mutex_);
        if (io_.stopped()) {
            io_.reset();
        } else if(stop_thread_.joinable()) {
            VSOMEIP_ERROR << "Trying to start an already started application.";
            return;
        }
        if (stopped_) {
            {
                std::lock_guard<std::mutex> its_lock_start_stop(block_stop_mutex_);
                block_stopping_ = true;
                block_stop_cv_.notify_all();
            }

            stopped_ = false;
            return;
        }
        stopped_ = false;
        stopped_called_ = false;
        VSOMEIP_INFO << "Starting vsomeip application \"" << name_ << "\" ("
                << std::hex << std::setw(4) << std::setfill('0') << client_
                << ") using "  << std::dec << io_thread_count << " threads"
#ifndef _WIN32
                << " I/O nice " << io_thread_nice_level
#endif
        ;

        start_caller_id_ = std::this_thread::get_id();
        {
            std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
            is_dispatching_ = true;
            auto its_main_dispatcher = std::make_shared<std::thread>(
                    std::bind(&application_impl::main_dispatch, shared_from_this()));
            dispatchers_[its_main_dispatcher->get_id()] = its_main_dispatcher;
        }

        if (stop_thread_.joinable()) {
            stop_thread_.join();
        }
        stop_thread_= std::thread(&application_impl::shutdown, shared_from_this());

        if (routing_)
            routing_->start();

        for (size_t i = 0; i < io_thread_count - 1; i++) {
            std::shared_ptr<std::thread> its_thread
                = std::make_shared<std::thread>([this, i, io_thread_nice_level] {
                    VSOMEIP_INFO << "io thread id from application: "
                            << std::hex << std::setw(4) << std::setfill('0')
                            << client_ << " (" << name_ << ") is: " << std::hex
                            << std::this_thread::get_id()
                    #ifndef _WIN32
                            << " TID: " << std::dec << static_cast<int>(syscall(SYS_gettid))
                    #endif
                            ;
                    #ifndef _WIN32
                        {
                            std::stringstream s;
                            s << std::hex << std::setw(4) << std::setfill('0')
                                << client_ << "_io" << std::setw(2)
                                << std::setfill('0') << i+1;
                            pthread_setname_np(pthread_self(),s.str().c_str());
                        }
                        if ((VSOMEIP_IO_THREAD_NICE_LEVEL != io_thread_nice_level) && (io_thread_nice_level != nice(io_thread_nice_level))) {
                            VSOMEIP_WARNING << "nice(" << io_thread_nice_level << ") failed " << errno << " for " << std::this_thread::get_id();
                        }
                    #endif
                    try {
                      io_.run();
                    } catch (const std::exception &e) {
                        VSOMEIP_ERROR << "application_impl::start() "
                                "catched exception: " << e.what();
                    }
                  });
            io_threads_.insert(its_thread);
        }
    }

    auto its_plugins = configuration_->get_plugins(name_);
    auto its_app_plugin_info = its_plugins.find(plugin_type_e::APPLICATION_PLUGIN);
    if (its_app_plugin_info != its_plugins.end()) {
        for (const auto& its_library : its_app_plugin_info->second) {
            auto its_application_plugin = plugin_manager::get()->get_plugin(
                    plugin_type_e::APPLICATION_PLUGIN, its_library);
            if (its_application_plugin) {
                std::dynamic_pointer_cast<application_plugin>(its_application_plugin)->
                        on_application_state_change(name_, application_plugin_state_e::STATE_STARTED);
            }
        }

    }

    app_counter_mutex__.lock();
    app_counter__++;
    app_counter_mutex__.unlock();
    VSOMEIP_INFO << "io thread id from application: "
            << std::hex << std::setw(4) << std::setfill('0') << client_ << " ("
            << name_ << ") is: " << std::hex << std::this_thread::get_id()
#ifndef _WIN32
            << " TID: " << std::dec << static_cast<int>(syscall(SYS_gettid))
#endif
    ;
#ifndef _WIN32
    if ((VSOMEIP_IO_THREAD_NICE_LEVEL != io_thread_nice_level) && (io_thread_nice_level != nice(io_thread_nice_level))) {
        VSOMEIP_WARNING << "nice(" << io_thread_nice_level << ") failed " << errno << " for " << std::this_thread::get_id();
    }
#endif
    try {
        io_.run();

        if (stop_thread_.joinable()) {
            stop_thread_.join();
        }

    } catch (const std::exception &e) {
        VSOMEIP_ERROR << "application_impl::start() catched exception: " << e.what();
    }

    {
        std::lock_guard<std::mutex> its_lock_start_stop(block_stop_mutex_);
        block_stopping_ = true;
        block_stop_cv_.notify_all();
    }

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

    VSOMEIP_INFO << "Stopping vsomeip application \"" << name_ << "\" ("
                << std::hex << std::setw(4) << std::setfill('0') << client_ << ").";

    bool block = true;
    {
        std::lock_guard<std::mutex> its_lock_start_stop(start_stop_mutex_);
        if (stopped_ || stopped_called_) {
            return;
        }
        stop_caller_id_ = std::this_thread::get_id();
        stopped_ = true;
        stopped_called_ = true;
        for (const auto& thread : io_threads_) {
            if (thread->get_id() == std::this_thread::get_id()) {
                block = false;
            }
        }
        if (start_caller_id_ == stop_caller_id_) {
            block = false;
        }
    }
    auto its_plugins = configuration_->get_plugins(name_);
    auto its_app_plugin_info = its_plugins.find(plugin_type_e::APPLICATION_PLUGIN);
    if (its_app_plugin_info != its_plugins.end()) {
        for (const auto& its_library : its_app_plugin_info->second) {
            auto its_application_plugin = plugin_manager::get()->get_plugin(
                    plugin_type_e::APPLICATION_PLUGIN, its_library);
            if (its_application_plugin) {
                std::dynamic_pointer_cast<application_plugin>(its_application_plugin)->
                        on_application_state_change(name_, application_plugin_state_e::STATE_STOPPED);
            }
        }

    }

    {
        std::lock_guard<std::mutex> its_lock_start_stop(start_stop_mutex_);
        stop_cv_.notify_one();
    }

    if (block) {
        std::unique_lock<std::mutex> block_stop_lock(block_stop_mutex_);
        while (!block_stopping_) {
            block_stop_cv_.wait(block_stop_lock);
        }
        block_stopping_ = false;
    }
}

void application_impl::process(int _number) {
    (void)_number;
    VSOMEIP_ERROR << "application::process is not (yet) implemented.";
}

security_mode_e application_impl::get_security_mode() const {
    return security_mode_;
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
        major_version_t _major, minor_version_t _minor) {
    if (routing_)
        routing_->request_service(client_, _service, _instance, _major, _minor);
}

void application_impl::release_service(service_t _service,
        instance_t _instance) {
    if (routing_)
        routing_->release_service(client_, _service, _instance);
}

void application_impl::subscribe(service_t _service, instance_t _instance,
                                 eventgroup_t _eventgroup,
                                 major_version_t _major,
                                 event_t _event) {
    if (routing_) {
        bool send_back_cached(false);
        bool send_back_cached_group(false);
        check_send_back_cached_event(_service, _instance, _event, _eventgroup,
                &send_back_cached, &send_back_cached_group);

        if (send_back_cached) {
            send_back_cached_event(_service, _instance, _event);
        } else if(send_back_cached_group) {
            send_back_cached_eventgroup(_service, _instance, _eventgroup);
        }

        if (check_subscription_state(_service, _instance, _eventgroup, _event)) {
            routing_->subscribe(client_, own_uid_, own_gid_, _service, _instance, _eventgroup, _major,
                    _event);
        }
    }
}

void application_impl::unsubscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) {
    remove_subscription(_service, _instance, _eventgroup, ANY_EVENT);
    if (routing_)
        routing_->unsubscribe(client_, own_uid_, own_gid_, _service, _instance, _eventgroup, ANY_EVENT);
}

void application_impl::unsubscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, event_t _event) {
    remove_subscription(_service, _instance, _eventgroup, _event);
    if (routing_)
        routing_->unsubscribe(client_, own_uid_, own_gid_, _service, _instance, _eventgroup, _event);
}

bool application_impl::is_available(
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) const {
    std::lock_guard<std::recursive_mutex> its_lock(availability_mutex_);
    return is_available_unlocked(_service, _instance, _major, _minor);
}

bool application_impl::is_available_unlocked(
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) const {

    bool is_available(false);

    auto check_major_minor = [&](const std::map<instance_t,
                std::map<major_version_t,
                    minor_version_t >>::const_iterator &_found_instance) {
        auto found_major = _found_instance->second.find(_major);
        if (found_major != _found_instance->second.end()) {
            if (_minor <= found_major->second || _minor == ANY_MINOR
                    || _minor == DEFAULT_MINOR) {
                is_available = true;
            }
        } else if ((_major == DEFAULT_MAJOR || _major == ANY_MAJOR)) {
            for (const auto &found_major : _found_instance->second) {
                if (_minor == DEFAULT_MINOR || _minor == ANY_MINOR) {
                    is_available = true;
                    break;
                } else if (_minor <= found_major.second) {
                    is_available = true;
                    break;
                }
            }
        }
    };
    auto found_service = available_.find(_service);
    if (found_service != available_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            check_major_minor(found_instance);
        } else if (_instance == ANY_INSTANCE) {
            for (auto it = found_service->second.cbegin();
                    it != found_service->second.cend(); it++) {
                check_major_minor(it);
                if (is_available) {
                    break;
                }
            }
        }
    } else if (_service == ANY_SERVICE) {
        for (const auto &found_service : available_) {
            auto found_instance = found_service.second.find(_instance);
            if (found_instance != found_service.second.end()) {
                check_major_minor(found_instance);
                if (is_available) {
                    break;
                }
            } else if (_instance == ANY_INSTANCE) {
                for (auto it = found_service.second.cbegin();
                        it != found_service.second.cend(); it++) {
                    check_major_minor(it);
                    if (is_available) {
                        break;
                    }
                }
            }
            if (is_available) {
                break;
            }
        }
    }
    return is_available;
}

bool application_impl::are_available(
        available_t &_available,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) const {
    std::lock_guard<std::recursive_mutex> its_lock(availability_mutex_);
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
                             if(_minor == ANY_MINOR || _minor == DEFAULT_MINOR
                                     || _minor <= found_available_major->second) {
                                 //add minor version
                                 _available[its_available_services_it->first][its_available_instances_it->first][its_available_major_it->first] = found_available_major->second;
                                 found_minor = true;
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

void application_impl::send(std::shared_ptr<message> _message) {
    bool is_request = utility::is_request(_message);
    if (client_side_logging_
        && (client_side_logging_filter_.empty()
            || (1 == client_side_logging_filter_.count(std::make_tuple(_message->get_service(), ANY_INSTANCE)))
            || (1 == client_side_logging_filter_.count(std::make_tuple(_message->get_service(), _message->get_instance()))))) {
        VSOMEIP_INFO << "application_impl::send: ("
            << std::hex << std::setw(4) << std::setfill('0') << client_ <<"): ["
            << std::hex << std::setw(4) << std::setfill('0') << _message->get_service() << "."
            << std::hex << std::setw(4) << std::setfill('0') << _message->get_instance() << "."
            << std::hex << std::setw(4) << std::setfill('0') << _message->get_method() << ":"
            << std::hex << std::setw(4) << std::setfill('0')
            << ((is_request) ? session_ : _message->get_session()) << ":"
            << std::hex << std::setw(4) << std::setfill('0')
                                << ((is_request) ? client_.load() : _message->get_client()) << "] "
            << "type=" << std::hex << static_cast<std::uint32_t>(_message->get_message_type())
            << " thread=" << std::hex << std::this_thread::get_id();
    }
    if (routing_) {
        // in case of requests set the request-id (client-id|session-id)
        if (is_request) {
            _message->set_client(client_);
            _message->set_session(get_session());
        }
        // Always increment the session-id
        (void)routing_->send(client_, _message);
    }
}

void application_impl::notify(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload, bool _force) const {
    if (routing_)
        routing_->notify(_service, _instance, _event, _payload, _force);
}

void application_impl::notify_one(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload,
        client_t _client, bool _force) const {
    if (routing_) {
        routing_->notify_one(_service, _instance, _event, _payload, _client,
                _force
#ifdef VSOMEIP_ENABLE_COMPAT
                , false
#endif
                );
    }
}

void application_impl::register_state_handler(state_handler_t _handler) {
    std::lock_guard<std::mutex> its_lock(state_handler_mutex_);
    handler_ = _handler;
}

void application_impl::unregister_state_handler() {
    std::lock_guard<std::mutex> its_lock(state_handler_mutex_);
    handler_ = nullptr;
}

void application_impl::register_availability_handler(service_t _service,
        instance_t _instance, availability_handler_t _handler,
        major_version_t _major, minor_version_t _minor) {
    std::lock_guard<std::recursive_mutex> availability_lock(availability_mutex_);
    if (state_ == state_type_e::ST_REGISTERED) {
        do_register_availability_handler(_service, _instance,
                _handler, _major, _minor);
    } else {
        availability_[_service][_instance][_major][_minor] = std::make_pair(
                _handler, false);
    }
}

void application_impl::do_register_availability_handler(service_t _service,
        instance_t _instance, availability_handler_t _handler,
        major_version_t _major, minor_version_t _minor) {
        available_t available;
    bool are_available = are_available_unlocked(available, _service, _instance, _major, _minor);
    availability_[_service][_instance][_major][_minor] = std::make_pair(
            _handler, true);

    std::lock_guard<std::mutex> handlers_lock(handlers_mutex_);

    std::shared_ptr<sync_handler> its_sync_handler
        = std::make_shared<sync_handler>([_handler, are_available, available]() {
                 for(const auto& available_services_it : available)
                     for(const auto& available_instances_it : available_services_it.second)
                         _handler(available_services_it.first, available_instances_it.first, are_available);
             });
    its_sync_handler->handler_type_ = handler_type_e::AVAILABILITY;
    its_sync_handler->service_id_ = _service;
    its_sync_handler->instance_id_ = _instance;
    handlers_.push_back(its_sync_handler);

    dispatcher_condition_.notify_one();
}

void application_impl::unregister_availability_handler(service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    std::lock_guard<std::recursive_mutex> its_lock(availability_mutex_);
    auto found_service = availability_.find(_service);
    if (found_service != availability_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_major = found_instance->second.find(_major);
            if (found_major != found_instance->second.end()) {
                auto found_minor = found_major->second.find(_minor);
                if (found_minor != found_major->second.end()) {
                    found_major->second.erase(_minor);

                    if (!found_major->second.size()) {
                        found_instance->second.erase(_major);
                        if (!found_instance->second.size()) {
                            found_service->second.erase(_instance);
                            if (!found_service->second.size()) {
                                availability_.erase(_service);
                            }
                        }
                    }
                }
            }
        }
    }
}

void application_impl::on_subscription(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, client_t _client, uid_t _uid, gid_t _gid,
        bool _subscribed, std::function<void(bool)> _accepted_cb) {
    bool handler_found = false;
    std::pair<subscription_handler_t, async_subscription_handler_t> its_handlers;
    {
        std::lock_guard<std::mutex> its_lock(subscription_mutex_);
        auto found_service = subscription_.find(_service);
        if (found_service != subscription_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    its_handlers = found_eventgroup->second;
                    handler_found = true;
                }
            }
        }
    }

    if(handler_found) {
        if(auto its_handler = its_handlers.first) {
            // "normal" subscription handler exists
            _accepted_cb(its_handler(_client, _uid, _gid, _subscribed));
        } else if(auto its_handler = its_handlers.second) {
            // async subscription handler exists
            its_handler(_client, _uid, _gid, _subscribed, _accepted_cb);
        }
    } else {
        _accepted_cb(true);
    }
}

void application_impl::register_subscription_handler(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        subscription_handler_t _handler) {

    std::lock_guard<std::mutex> its_lock(subscription_mutex_);
    subscription_[_service][_instance][_eventgroup] = std::make_pair(_handler, nullptr);
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
}

void application_impl::on_subscription_status(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, event_t _event,
        uint16_t _error) {
    bool entry_found(false);
    {
        auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, _event);
        std::lock_guard<std::mutex> its_lock(subscriptions_state_mutex_);
        auto its_subscription_state = subscription_state_.find(its_tuple);
        if (its_subscription_state == subscription_state_.end()) {
            its_tuple = std::make_tuple(_service, _instance, _eventgroup, ANY_EVENT);
            auto its_any_subscription_state = subscription_state_.find(its_tuple);
            if (its_any_subscription_state == subscription_state_.end()) {
                VSOMEIP_TRACE << std::hex << get_client( )
                        << " application_impl::on_subscription_status: "
                        << "Received a subscription status without subscribe for "
                        << std::hex << _service << "/" << _instance << "/"
                        << _eventgroup << "/" << _event << "/error=" << _error;
            } else {
                entry_found = true;
            }
        } else {
            entry_found = true;
        }
        if (entry_found) {
            if (_error) {
                subscription_state_[its_tuple] =
                        subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED;
            } else {
                subscription_state_[its_tuple] =
                        subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED;
            }
        }
    }

    if (entry_found) {
        deliver_subscription_state(_service, _instance, _eventgroup, _event, _error);
    }
}

void application_impl::deliver_subscription_state(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, event_t _event, uint16_t _error) {
    std::vector<subscription_status_handler_t> handlers;
    {
        std::lock_guard<std::mutex> its_lock(subscription_status_handlers_mutex_);
        auto found_service = subscription_status_handlers_.find(_service);
        if (found_service != subscription_status_handlers_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    auto found_event = found_eventgroup->second.find(_event);
                    if (found_event != found_eventgroup->second.end()) {
                        if (!_error || (_error && found_event->second.second)) {
                            handlers.push_back(found_event->second.first);
                        }
                    } else {
                        auto its_any_event = found_eventgroup->second.find(ANY_EVENT);
                        if (its_any_event != found_eventgroup->second.end()) {
                            if (!_error || (_error && its_any_event->second.second)) {
                                handlers.push_back(its_any_event->second.first);
                            }
                        }
                    }
                }
            }
            found_instance = found_service->second.find(ANY_INSTANCE);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    auto found_event = found_eventgroup->second.find(_event);
                    if (found_event != found_eventgroup->second.end()) {
                        if (!_error || (_error && found_event->second.second)) {
                            handlers.push_back(found_event->second.first);
                        }
                    } else {
                        auto its_any_event = found_eventgroup->second.find(ANY_EVENT);
                        if (its_any_event != found_eventgroup->second.end()) {
                            if (!_error || (_error && its_any_event->second.second)) {
                                handlers.push_back(its_any_event->second.first);
                            }
                        }
                    }
                }
            }
        }
        found_service = subscription_status_handlers_.find(ANY_SERVICE);
        if (found_service != subscription_status_handlers_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    auto found_event = found_eventgroup->second.find(_event);
                    if (found_event != found_eventgroup->second.end()) {
                        if (!_error || (_error && found_event->second.second)) {
                            handlers.push_back(found_event->second.first);
                        }
                    } else {
                        auto its_any_event = found_eventgroup->second.find(ANY_EVENT);
                        if (its_any_event != found_eventgroup->second.end()) {
                            if (!_error || (_error && its_any_event->second.second)) {
                                handlers.push_back(its_any_event->second.first);
                            }
                        }
                    }
                }
            }
            found_instance = found_service->second.find(ANY_INSTANCE);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    auto found_event = found_eventgroup->second.find(_event);
                    if (found_event != found_eventgroup->second.end()) {
                        if (!_error || (_error && found_event->second.second)) {
                            handlers.push_back(found_event->second.first);
                        }
                    } else {
                        auto its_any_event = found_eventgroup->second.find(ANY_EVENT);
                        if (its_any_event != found_eventgroup->second.end()) {
                            if (!_error || (_error && its_any_event->second.second)) {
                                handlers.push_back(its_any_event->second.first);
                            }
                        }
                    }
                }
            }
        }
    }
    {
        std::unique_lock<std::mutex> handlers_lock(handlers_mutex_);
        for (auto &handler : handlers) {
            std::shared_ptr<sync_handler> its_sync_handler
                = std::make_shared<sync_handler>([handler, _service,
                                                  _instance, _eventgroup,
                                                  _event, _error]() {
                                handler(_service, _instance,
                                        _eventgroup, _event, _error);
                                                 });
            its_sync_handler->handler_type_ = handler_type_e::SUBSCRIPTION;
            its_sync_handler->service_id_ = _service;
            its_sync_handler->instance_id_ = _instance;
            its_sync_handler->method_id_ = _event;
            its_sync_handler->eventgroup_id_ = _eventgroup;
            handlers_.push_back(its_sync_handler);
        }
        if (handlers.size()) {
            dispatcher_condition_.notify_one();
        }
    }
}

void application_impl::register_subscription_status_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event,
            subscription_status_handler_t _handler, bool _is_selective) {
    std::lock_guard<std::mutex> its_lock(subscription_status_handlers_mutex_);
    if (_handler) {
        subscription_status_handlers_[_service][_instance][_eventgroup][_event] =
                std::make_pair(_handler, _is_selective);
    } else {
        VSOMEIP_WARNING <<
                "application_impl::register_subscription_status_handler: "
                "_handler is null, for unregistration please use "
                "application_impl::unregister_subscription_status_handler ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "."
                << std::hex << std::setw(4) << std::setfill('0') << _event << "]";
    }
}

void application_impl::unregister_subscription_status_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event) {
    std::lock_guard<std::mutex> its_lock(subscription_status_handlers_mutex_);
    auto its_service = subscription_status_handlers_.find(_service);
    if (its_service != subscription_status_handlers_.end()) {
        auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            auto its_eventgroup = its_instance->second.find(_eventgroup);
            if (its_eventgroup != its_instance->second.end()) {
                its_eventgroup->second.erase(_event);
                if (its_eventgroup->second.empty()) {
                    its_instance->second.erase(_eventgroup);
                    if (its_instance->second.empty()) {
                        its_service->second.erase(_instance);
                        if (its_service->second.empty()) {
                            subscription_status_handlers_.erase(_service);
                        }
                    }
                }
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
           event_t _notifier, const std::set<eventgroup_t> &_eventgroups,
           event_type_e _type,
           std::chrono::milliseconds _cycle, bool _change_resets_cycle,
           bool _update_on_change,
           const epsilon_change_func_t &_epsilon_change_func,
           reliability_type_e _reliability) {
       if (routing_)
           routing_->register_event(client_,
                   _service, _instance,
                   _notifier, _eventgroups, _type, _reliability,
                   _cycle, _change_resets_cycle, _update_on_change,
                   _epsilon_change_func, true);
}

void application_impl::stop_offer_event(service_t _service, instance_t _instance,
       event_t _event) {
   if (routing_)
       routing_->unregister_event(client_, _service, _instance, _event, true);
}

void application_impl::request_event(service_t _service, instance_t _instance,
           event_t _event, const std::set<eventgroup_t> &_eventgroups,
           event_type_e _type, reliability_type_e _reliability) {
       if (routing_)
           routing_->register_event(client_,
                   _service, _instance,
                   _event, _eventgroups, _type, _reliability,
                   std::chrono::milliseconds::zero(), false, true,
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

void application_impl::set_client(const client_t &_client) {
    client_ = _client;
}

session_t application_impl::get_session() {

#ifdef VSOMEIP_HAS_SESSION_HANDLING_CONFIG
    if (!has_session_handling_)
        return (0);
#endif // VSOMEIP_HAS_SESSION_HANDLING_CONFIG

    std::lock_guard<std::mutex> its_lock(session_mutex_);
    if (0 == ++session_) {
        // Smallest allowed session identifier
        session_ = 1;
    }

    return session_;
}

std::shared_ptr<configuration> application_impl::get_configuration() const {
    return configuration_;
}

diagnosis_t application_impl::get_diagnosis() const {
    return configuration_->get_diagnosis_address();
}

boost::asio::io_service & application_impl::get_io() {
    return io_;
}

void application_impl::on_state(state_type_e _state) {
    {
        std::lock_guard<std::recursive_mutex> availability_lock(availability_mutex_);
        if (state_ != _state) {
            state_ = _state;
            if (state_ == state_type_e::ST_REGISTERED) {
                for (const auto &its_service : availability_) {
                    for (const auto &its_instance : its_service.second) {
                        for (const auto &its_major : its_instance.second) {
                            for (const auto &its_minor : its_major.second) {
                                if (!its_minor.second.second) {
                                    do_register_availability_handler(
                                            its_service.first,
                                            its_instance.first,
                                            its_minor.second.first,
                                            its_major.first, its_minor.first);
                                }
                            }
                        }
                    }
                }
            } else {
                // Call on_availability callback on each service
                for (const auto &its_service : availability_) {
                    for (const auto &its_instance : its_service.second) {
                        for (const auto &its_major : its_instance.second) {
                            for (const auto &its_minor : its_major.second) {
                                on_availability(its_service.first, its_instance.first, false, its_major.first, its_minor.first);
                            }
                        }
                    }
                }
            }
        }
    }
    bool has_state_handler(false);
    state_handler_t handler = nullptr;
    {
        std::lock_guard<std::mutex> its_lock(state_handler_mutex_);
        if (handler_) {
            has_state_handler = true;
            handler = handler_;
        }
    }
    if (has_state_handler) {
        std::lock_guard<std::mutex> its_lock(handlers_mutex_);
        std::shared_ptr<sync_handler> its_sync_handler
            = std::make_shared<sync_handler>([handler, _state]() {
                                                handler(_state);
                                             });
        its_sync_handler->handler_type_ = handler_type_e::STATE;
        handlers_.push_back(its_sync_handler);
        dispatcher_condition_.notify_one();
    }
}

void application_impl::on_availability(service_t _service, instance_t _instance,
        bool _is_available, major_version_t _major, minor_version_t _minor) {
    std::vector<availability_handler_t> its_handlers;
    {
        std::lock_guard<std::recursive_mutex> availability_lock(availability_mutex_);
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

        auto find_matching_handler =
                [&](const availability_major_minor_t& _av_ma_mi_it) {
            auto found_major = _av_ma_mi_it.find(_major);
            if (found_major != _av_ma_mi_it.end()) {
                for (std::int32_t mi = static_cast<std::int32_t>(_minor); mi >= 0; mi--) {
                    const auto found_minor = found_major->second.find(static_cast<minor_version_t>(mi));
                    if (found_minor != found_major->second.end()) {
                        its_handlers.push_back(found_minor->second.first);
                    }
                }
                const auto found_any_minor = found_major->second.find(ANY_MINOR);
                if (found_any_minor != found_major->second.end()) {
                    its_handlers.push_back(found_any_minor->second.first);
                }
            }
            found_major = _av_ma_mi_it.find(ANY_MAJOR);
            if (found_major != _av_ma_mi_it.end()) {
                for (std::int32_t mi = static_cast<std::int32_t>(_minor); mi >= 0; mi--) {
                    const auto found_minor = found_major->second.find(static_cast<minor_version_t>(mi));
                    if (found_minor != found_major->second.end()) {
                        its_handlers.push_back(found_minor->second.first);
                    }
                }
                const auto found_any_minor = found_major->second.find(ANY_MINOR);
                if (found_any_minor != found_major->second.end()) {
                    its_handlers.push_back(found_any_minor->second.first);
                }
            }
        };

        auto found_service = availability_.find(_service);
        if (found_service != availability_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                find_matching_handler(found_instance->second);
            }
            found_instance = found_service->second.find(ANY_INSTANCE);
            if (found_instance != found_service->second.end()) {
                find_matching_handler(found_instance->second);
            }
        }
        found_service = availability_.find(ANY_SERVICE);
        if (found_service != availability_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if( found_instance != found_service->second.end()) {
                find_matching_handler(found_instance->second);
            }
            found_instance = found_service->second.find(ANY_INSTANCE);
            if( found_instance != found_service->second.end()) {
                find_matching_handler(found_instance->second);
            }
        }
        {
            std::lock_guard<std::mutex> handlers_lock(handlers_mutex_);
            for (const auto &handler : its_handlers) {
                std::shared_ptr<sync_handler> its_sync_handler =
                        std::make_shared<sync_handler>(
                                [handler, _service, _instance, _is_available]()
                                {
                                    handler(_service, _instance, _is_available);
                                });
                its_sync_handler->handler_type_ = handler_type_e::AVAILABILITY;
                its_sync_handler->service_id_ = _service;
                its_sync_handler->instance_id_ = _instance;
                handlers_.push_back(its_sync_handler);
            }
        }
    }
    if (!_is_available) {
        {
            std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
            auto found_service = subscriptions_.find(_service);
            if (found_service != subscriptions_.end()) {
                auto found_instance = found_service->second.find(_instance);
                if (found_instance != found_service->second.end()) {
                    for (auto &event : found_instance->second) {
                        for (auto &eventgroup : event.second) {
                            eventgroup.second = false;
                        }
                    }
                }
            }
        }
        {
            std::lock_guard<std::mutex> its_lock(subscriptions_state_mutex_);
            for (auto &its_subscription_state : subscription_state_) {
                if (std::get<0>(its_subscription_state.first) == _service &&
                        std::get<1>(its_subscription_state.first) == _instance) {
                    its_subscription_state.second =
                            subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED;
                }
            }
        }
    }

    if (its_handlers.size()) {
        std::lock_guard<std::mutex> handlers_lock(handlers_mutex_);
        dispatcher_condition_.notify_one();
    }
}

void application_impl::on_message(std::shared_ptr<message> &&_message) {
    const service_t its_service = _message->get_service();
    const instance_t its_instance = _message->get_instance();
    const method_t its_method = _message->get_method();

    if (_message->get_message_type() == message_type_e::MT_NOTIFICATION) {
        if (!check_for_active_subscription(its_service, its_instance,
                static_cast<event_t>(its_method))) {
            VSOMEIP_ERROR << "application_impl::on_message ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_method << "]";
            return;
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
                            handler(_message);
                        });
                its_sync_handler->handler_type_ = handler_type_e::MESSAGE;
                its_sync_handler->service_id_ = _message->get_service();
                its_sync_handler->instance_id_ = _message->get_instance();
                its_sync_handler->method_id_ = _message->get_method();
                its_sync_handler->session_id_ = _message->get_session();
                handlers_.push_back(its_sync_handler);
            }
            dispatcher_condition_.notify_one();
        }
    }
}

// Interface "service_discovery_host"
routing_manager * application_impl::get_routing_manager() const {
    return routing_.get();
}

void application_impl::main_dispatch() {
#ifndef _WIN32
    {
        std::stringstream s;
        s << std::hex << std::setw(4) << std::setfill('0')
            << client_ << "_m_dispatch";
        pthread_setname_np(pthread_self(),s.str().c_str());
    }
#endif
    const std::thread::id its_id = std::this_thread::get_id();
    VSOMEIP_INFO << "main dispatch thread id from application: "
            << std::hex << std::setw(4) << std::setfill('0') << client_ << " ("
            << name_ << ") is: " << std::hex << its_id
#ifndef _WIN32
            << " TID: " << std::dec << static_cast<int>(syscall(SYS_gettid))
#endif
            ;
    std::unique_lock<std::mutex> its_lock(handlers_mutex_);
    while (is_dispatching_) {
        if (handlers_.empty() || !is_active_dispatcher(its_id)) {
            // Cancel other waiting dispatcher
            dispatcher_condition_.notify_all();
            // Wait for new handlers to execute
            while (is_dispatching_ && (handlers_.empty() || !is_active_dispatcher(its_id))) {
                dispatcher_condition_.wait(its_lock);
            }
        } else {
            std::shared_ptr<sync_handler> its_handler;
            while (is_dispatching_  && is_active_dispatcher(its_id)
                   && (its_handler = get_next_handler())) {
                its_lock.unlock();
                invoke_handler(its_handler);

                if (!is_dispatching_)
                    return;

                its_lock.lock();

                reschedule_availability_handler(its_handler);
                remove_elapsed_dispatchers();

#ifdef _WIN32
                if(!is_dispatching_) {
                    its_lock.unlock();
                    return;
                }
#endif
            }
        }
    }
    its_lock.unlock();
}

void application_impl::dispatch() {
#ifndef _WIN32
    {
        std::stringstream s;
        s << std::hex << std::setw(4) << std::setfill('0')
            << client_ << "_dispatch";
        pthread_setname_np(pthread_self(),s.str().c_str());
    }
#endif
    const std::thread::id its_id = std::this_thread::get_id();
    VSOMEIP_INFO << "dispatch thread id from application: "
            << std::hex << std::setw(4) << std::setfill('0') << client_ << " ("
            << name_ << ") is: " << std::hex << its_id
#ifndef _WIN32
            << " TID: " << std::dec << static_cast<int>(syscall(SYS_gettid))
#endif
            ;
    std::unique_lock<std::mutex> its_lock(handlers_mutex_);
    while (is_active_dispatcher(its_id)) {
        if (is_dispatching_ && handlers_.empty()) {
             dispatcher_condition_.wait(its_lock);
             // Maybe woken up from main dispatcher
             if (handlers_.empty() && !is_active_dispatcher(its_id)) {
                 if (!is_dispatching_) {
                     return;
                 }
                 std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
                 elapsed_dispatchers_.insert(its_id);
                 return;
             }
        } else {
            std::shared_ptr<sync_handler> its_handler;
            while (is_dispatching_ && is_active_dispatcher(its_id)
                   && (its_handler = get_next_handler())) {
                its_lock.unlock();
                invoke_handler(its_handler);

                if (!is_dispatching_)
                    return;

                its_lock.lock();

                reschedule_availability_handler(its_handler);
                remove_elapsed_dispatchers();
            }
        }
    }
    if (is_dispatching_) {
        std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
        elapsed_dispatchers_.insert(its_id);
    }
    dispatcher_condition_.notify_all();
}

std::shared_ptr<application_impl::sync_handler> application_impl::get_next_handler() {
    std::shared_ptr<sync_handler> its_next_handler;
    while (!handlers_.empty() && !its_next_handler) {
        its_next_handler = handlers_.front();
        handlers_.pop_front();

        // Check handler
        if (its_next_handler->handler_type_ == handler_type_e::AVAILABILITY) {
            const std::pair<service_t, instance_t> its_si_pair = std::make_pair(
                    its_next_handler->service_id_,
                    its_next_handler->instance_id_);
            auto found_si = availability_handlers_.find(its_si_pair);
            if (found_si != availability_handlers_.end()
                    && !found_si->second.empty()
                    && found_si->second.front() != its_next_handler) {
                found_si->second.push_back(its_next_handler);
                // There is a running availability handler for this service.
                // Therefore, this one must wait...
                its_next_handler = nullptr;
            } else {
                availability_handlers_[its_si_pair].push_back(its_next_handler);
            }
        } else if (its_next_handler->handler_type_ == handler_type_e::MESSAGE) {
            const std::pair<service_t, instance_t> its_si_pair = std::make_pair(
                    its_next_handler->service_id_,
                    its_next_handler->instance_id_);
            auto found_si = availability_handlers_.find(its_si_pair);
            if (found_si != availability_handlers_.end()
                    && found_si->second.size() > 1) {
                // The message comes after the next availability handler
                // Therefore, queue it to the last one
                found_si->second.push_back(its_next_handler);
                its_next_handler = nullptr;
            }
        }
    }

    return its_next_handler;
}

void application_impl::reschedule_availability_handler(
        const std::shared_ptr<sync_handler> &_handler) {
    if (_handler->handler_type_ == handler_type_e::AVAILABILITY) {
        const std::pair<service_t, instance_t> its_si_pair = std::make_pair(
                _handler->service_id_, _handler->instance_id_);
        auto found_si = availability_handlers_.find(its_si_pair);
        if (found_si != availability_handlers_.end()) {
            if (!found_si->second.empty()
                    && found_si->second.front() == _handler) {
                found_si->second.pop_front();

                // If there are other availability handlers pending, schedule
                //  them and all handlers that were queued because of them
                for (auto it = found_si->second.rbegin();
                        it != found_si->second.rend(); it++) {
                    handlers_.push_front(*it);
                }
                availability_handlers_.erase(found_si);
            }
            return;
        }
        VSOMEIP_WARNING << __func__
                << ": An unknown availability handler returned!";
    }
}

void application_impl::invoke_handler(std::shared_ptr<sync_handler> &_handler) {
    const std::thread::id its_id = std::this_thread::get_id();

    std::shared_ptr<sync_handler> its_sync_handler
        = std::make_shared<sync_handler>(_handler->service_id_,
            _handler->instance_id_, _handler->method_id_,
            _handler->session_id_, _handler->eventgroup_id_,
            _handler->handler_type_);

    boost::asio::steady_timer its_dispatcher_timer(io_);
    its_dispatcher_timer.expires_from_now(std::chrono::milliseconds(max_dispatch_time_));
    its_dispatcher_timer.async_wait([this, its_sync_handler](const boost::system::error_code &_error) {
        if (!_error) {
            print_blocking_call(its_sync_handler);
            if (has_active_dispatcher()) {
                std::lock_guard<std::mutex> its_lock(handlers_mutex_);
                dispatcher_condition_.notify_all();
            } else {
                // If possible, create a new dispatcher thread to unblock.
                // If this is _not_ possible, dispatching is blocked until
                // at least one of the active handler calls returns.
                while (is_dispatching_) {
                    if (dispatcher_mutex_.try_lock()) {
                        if (dispatchers_.size() < max_dispatchers_) {
                            if (is_dispatching_) {
                                auto its_dispatcher = std::make_shared<std::thread>(
                                    std::bind(&application_impl::dispatch, shared_from_this()));
                                dispatchers_[its_dispatcher->get_id()] = its_dispatcher;
                            } else {
                                VSOMEIP_INFO << "Won't start new dispatcher "
                                        "thread as Client=" << std::hex
                                        << get_client() << " is shutting down";
                            }
                        } else {
                            VSOMEIP_ERROR << "Maximum number of dispatchers exceeded.";
                        }
                        dispatcher_mutex_.unlock();
                        break;
                    } else {
                        std::this_thread::yield();
                    }
                }
            }
        }
    });
    if (client_side_logging_
        && (client_side_logging_filter_.empty()
            || (1 == client_side_logging_filter_.count(std::make_tuple(its_sync_handler->service_id_, ANY_INSTANCE)))
            || (1 == client_side_logging_filter_.count(std::make_tuple(its_sync_handler->service_id_, its_sync_handler->instance_id_))))) {
        VSOMEIP_INFO << "Invoking handler: ("
            << std::hex << std::setw(4) << std::setfill('0') << client_ <<"): ["
            << std::hex << std::setw(4) << std::setfill('0') << its_sync_handler->service_id_ << "."
            << std::hex << std::setw(4) << std::setfill('0') << its_sync_handler->instance_id_ << "."
            << std::hex << std::setw(4) << std::setfill('0') << its_sync_handler->method_id_ << ":"
            << std::hex << std::setw(4) << std::setfill('0') << its_sync_handler->session_id_ << "] "
            << "type=" << static_cast<std::uint32_t>(its_sync_handler->handler_type_)
            << " thread=" << std::hex << its_id;
    }

    while (is_dispatching_ ) {
        if (dispatcher_mutex_.try_lock()) {
            running_dispatchers_.insert(its_id);
            dispatcher_mutex_.unlock();
            break;
        }
        std::this_thread::yield();
    }

    if (is_dispatching_) {
        try {
            _handler->handler_();
        } catch (const std::exception &e) {
            VSOMEIP_ERROR << "application_impl::invoke_handler caught exception: "
                    << e.what();
            print_blocking_call(its_sync_handler);
        }
    }
    boost::system::error_code ec;
    its_dispatcher_timer.cancel(ec);

    while (is_dispatching_ ) {
        if (dispatcher_mutex_.try_lock()) {
            running_dispatchers_.erase(its_id);
            dispatcher_mutex_.unlock();
            return;
        }
        std::this_thread::yield();
    }
}

bool application_impl::has_active_dispatcher() {
    while (is_dispatching_) {
        if (dispatcher_mutex_.try_lock()) {
            for (const auto &d : dispatchers_) {
                if (running_dispatchers_.find(d.first) == running_dispatchers_.end() &&
                    elapsed_dispatchers_.find(d.first) == elapsed_dispatchers_.end()) {
                    dispatcher_mutex_.unlock();
                    return true;
                }
            }
            dispatcher_mutex_.unlock();
            return false;
        }
        std::this_thread::yield();
    }
    return false;
}

bool application_impl::is_active_dispatcher(const std::thread::id &_id) {
    while (is_dispatching_) {
        if (dispatcher_mutex_.try_lock()) {
            for (const auto &d : dispatchers_) {
                if (d.first != _id &&
                    running_dispatchers_.find(d.first) == running_dispatchers_.end() &&
                    elapsed_dispatchers_.find(d.first) == elapsed_dispatchers_.end()) {
                    dispatcher_mutex_.unlock();
                    return false;
                }
            }
            dispatcher_mutex_.unlock();
            return true;
        }
        std::this_thread::yield();
    }
    return false;
}

void application_impl::remove_elapsed_dispatchers() {
    if (is_dispatching_) {
        std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
        for (auto id : elapsed_dispatchers_) {
            auto its_dispatcher = dispatchers_.find(id);
            if (its_dispatcher->second->joinable())
                its_dispatcher->second->join();
            dispatchers_.erase(id);
        }
        elapsed_dispatchers_.clear();
    }
}

void application_impl::clear_all_handler() {
    unregister_state_handler();
    {
        std::lock_guard<std::mutex> its_lock(offered_services_handler_mutex_);
        offered_services_handler_ = nullptr;
    }

    {
        std::lock_guard<std::recursive_mutex> availability_lock(availability_mutex_);
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
        std::lock_guard<std::mutex> its_lock(handlers_mutex_);
        handlers_.clear();
    }
}

void application_impl::shutdown() {
    VSOMEIP_INFO << "shutdown thread id from application: "
            << std::hex << std::setw(4) << std::setfill('0') << client_ << " ("
            << name_ << ") is: " << std::hex << std::this_thread::get_id()
#ifndef _WIN32
            << " TID: " << std::dec << static_cast<int>(syscall(SYS_gettid))
#endif
    ;
#ifndef _WIN32
    boost::asio::detail::posix_signal_blocker blocker;
    {
        std::stringstream s;
        s << std::hex << std::setw(4) << std::setfill('0')
            << client_ << "_shutdown";
        pthread_setname_np(pthread_self(),s.str().c_str());
    }
#endif

    {
        std::unique_lock<std::mutex> its_lock(start_stop_mutex_);
        while(!stopped_) {
            stop_cv_.wait(its_lock);
        }
    }
    {
        std::lock_guard<std::mutex> its_handler_lock(handlers_mutex_);
        is_dispatching_ = false;
        dispatcher_condition_.notify_all();
    }

    try {
        std::lock_guard<std::mutex> its_lock(dispatcher_mutex_);
        for (const auto& its_dispatcher : dispatchers_) {
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
        availability_handlers_.clear();
        running_dispatchers_.clear();
        elapsed_dispatchers_.clear();
        dispatchers_.clear();
    } catch (const std::exception &e) {
        VSOMEIP_ERROR << "application_impl::" << __func__ << ": stopping dispatchers, "
                << " catched exception: " << e.what();
    }

    try {
        if (routing_)
            routing_->stop();
    } catch (const std::exception &e) {
        VSOMEIP_ERROR << "application_impl::" << __func__ << ": stopping routing, "
                << " catched exception: " << e.what();
    }

    try {
        work_.reset();
        io_.stop();
    } catch (const std::exception &e) {
        VSOMEIP_ERROR << "application_impl::" << __func__ << ": stopping io, "
                << " catched exception: " << e.what();
    }

    try {
        std::lock_guard<std::mutex> its_lock_start_stop(start_stop_mutex_);
        for (const auto& t : io_threads_) {
            if (t->joinable()) {
                t->join();
            }
        }
        io_threads_.clear();
    } catch (const std::exception &e) {
        VSOMEIP_ERROR << "application_impl::" << __func__ << ": joining threads, "
                << " catched exception: " << e.what();
    }
}

bool application_impl::is_routing() const {
    return is_routing_manager_host_;
}

void application_impl::send_back_cached_event(service_t _service,
                                              instance_t _instance,
                                              event_t _event) {
    std::shared_ptr<event> its_event = routing_->find_event(_service,
            _instance, _event);
    if (its_event && its_event->is_field() && its_event->is_set()) {
        std::shared_ptr<message> its_message = runtime_->create_notification();
        its_message->set_service(_service);
        its_message->set_method(_event);
        its_message->set_instance(_instance);
        its_message->set_payload(its_event->get_payload());
        its_message->set_initial(true);
        on_message(std::move(its_message));
        VSOMEIP_INFO << "Sending back cached event ("
                << std::hex << std::setw(4) << std::setfill('0') << client_ <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _event << "]";
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
            const event_t its_event_id(its_event->get_event());
            its_message->set_service(_service);
            its_message->set_method(its_event_id);
            its_message->set_instance(_instance);
            its_message->set_payload(its_event->get_payload());
            its_message->set_initial(true);
            on_message(std::move(its_message));
            VSOMEIP_INFO << "Sending back cached event ("
                    << std::hex << std::setw(4) << std::setfill('0') << client_ <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_event_id
                    << "] from eventgroup "
                    << std::hex << std::setw(4) << std::setfill('0') << _eventgroup;
        }
    }
}

void application_impl::set_routing_state(routing_state_e _routing_state) {
    if (routing_)
        routing_->set_routing_state(_routing_state);
}

void application_impl::check_send_back_cached_event(
        service_t _service, instance_t _instance, event_t _event,
        eventgroup_t _eventgroup, bool *_send_back_cached_event,
        bool *_send_back_cached_eventgroup) {
    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    *_send_back_cached_event = false;
    *_send_back_cached_eventgroup = false;
    bool already_subscribed(false);
    auto found_service = subscriptions_.find(_service);
    if(found_service != subscriptions_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_event = found_instance->second.find(_event);
            if (found_event != found_instance->second.end()) {
                auto found_eventgroup = found_event->second.find(_eventgroup);
                if (found_eventgroup != found_event->second.end()) {
                    already_subscribed = true;
                    if (found_eventgroup->second) {
                        // initial values for this event have already been
                        // received, send back cached value
                        if(_event == ANY_EVENT) {
                            *_send_back_cached_eventgroup = true;
                        } else {
                            *_send_back_cached_event = true;
                        }
                    }
                }
            }
        }
    }

    if (!already_subscribed) {
        subscriptions_[_service][_instance][_event][_eventgroup] = false;
    }
}

void application_impl::remove_subscription(service_t _service,
                                           instance_t _instance,
                                           eventgroup_t _eventgroup,
                                           event_t _event) {

    {
        auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, _event);
        std::lock_guard<std::mutex> its_lock(subscriptions_state_mutex_);
        subscription_state_.erase(its_tuple);
    }

    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);

    auto found_service = subscriptions_.find(_service);
    if(found_service != subscriptions_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_event = found_instance->second.find(_event);
            if (found_event != found_instance->second.end()) {
                if (found_event->second.erase(_eventgroup)) {
                    if (!found_event->second.size()) {
                        found_instance->second.erase(_event);
                        if (!found_instance->second.size()) {
                            found_service->second.erase(_instance);
                            if (!found_service->second.size()) {
                                subscriptions_.erase(_service);
                            }
                        }
                    }
                }
            }
        }
    }
}

bool application_impl::check_for_active_subscription(service_t _service,
                                                     instance_t _instance,
                                                     event_t _event) {
    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    auto found_service = subscriptions_.find(_service);
    if(found_service != subscriptions_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_event = found_instance->second.find(_event);
            if (found_event != found_instance->second.end()) {
                if (found_event->second.size()) {
                    for (auto &eventgroup : found_event->second) {
                        eventgroup.second = true;
                    }
                    return true;
                }
            } else {
                // Received a event which nobody yet explicitly subscribed to.
                // Check if someone subscribed to ANY_EVENT for one of
                // the received event's eventgroups
                auto found_any_event = found_instance->second.find(ANY_EVENT);
                if (found_any_event != found_instance->second.end()) {
                    if (routing_) {
                        std::shared_ptr<event> its_event = routing_->find_event(
                                _service, _instance, _event);
                        if (its_event) {
                            for (const auto& eg : its_event->get_eventgroups()) {
                                auto found_eventgroup = found_any_event->second.find(eg);
                                if (found_eventgroup != found_any_event->second.end()) {
                                    // set the flag for initial event received to true
                                    // even if we might not already received all of the
                                    // eventgroups events.
                                    found_eventgroup->second = true;
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Return false if an event was received from:
    // - a service which nobody yet subscribed to
    // - a service instance which nobody yet subscribed to
    // - a service instance and nobody yet subscribed to one of the event's
    //   eventgroups
    return false;
}

bool application_impl::check_subscription_state(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, event_t _event) {
    bool is_acknowledged(false);
    bool should_subscribe(true);
    {
        auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, _event);
        std::lock_guard<std::mutex> its_lock(subscriptions_state_mutex_);
        auto its_subscription_state = subscription_state_.find(its_tuple);
        if (its_subscription_state != subscription_state_.end()) {
            if (its_subscription_state->second !=
                    subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED) {
                // only return true if subscription is NACK
                // as only then we need to subscribe!
                should_subscribe = false;
                if (its_subscription_state->second ==
                        subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED) {
                    is_acknowledged = true;
                }
            }
        } else {
            subscription_state_[its_tuple] = subscription_state_e::IS_SUBSCRIBING;
        }
    }

    if (!should_subscribe && is_acknowledged) {
        // Deliver subscription state only if ACK has already received
        deliver_subscription_state(_service, _instance, _eventgroup, _event, 0 /* OK */);
    }

    return should_subscribe;
}

void application_impl::print_blocking_call(const std::shared_ptr<sync_handler>& _handler) {
    switch (_handler->handler_type_) {
        case handler_type_e::AVAILABILITY:
            VSOMEIP_WARNING << "BLOCKING CALL AVAILABILITY("
                << std::hex << std::setw(4) << std::setfill('0') << get_client() <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << _handler->service_id_ << "."
                << std::hex << std::setw(4) << std::setfill('0') << _handler->instance_id_ << "]";
            break;
        case handler_type_e::MESSAGE:
            VSOMEIP_WARNING << "BLOCKING CALL MESSAGE("
                << std::hex << std::setw(4) << std::setfill('0') << get_client() <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << _handler->service_id_ << "."
                << std::hex << std::setw(4) << std::setfill('0') << _handler->instance_id_ << "."
                << std::hex << std::setw(4) << std::setfill('0') << _handler->method_id_ << ":"
                << std::hex << std::setw(4) << std::setfill('0') << _handler->session_id_ << "]";
            break;
        case handler_type_e::STATE:
            VSOMEIP_WARNING << "BLOCKING CALL STATE("
                << std::hex << std::setw(4) << std::setfill('0') << get_client() << ")";
            break;
        case handler_type_e::SUBSCRIPTION:
            VSOMEIP_WARNING << "BLOCKING CALL SUBSCRIPTION("
                << std::hex << std::setw(4) << std::setfill('0') << get_client() <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << _handler->service_id_ << "."
                << std::hex << std::setw(4) << std::setfill('0') << _handler->instance_id_ << "."
                << std::hex << std::setw(4) << std::setfill('0') << _handler->eventgroup_id_ << ":"
                << std::hex << std::setw(4) << std::setfill('0') << _handler->method_id_ << "]";
            break;
        case handler_type_e::OFFERED_SERVICES_INFO:
            VSOMEIP_WARNING << "BLOCKING CALL OFFERED_SERVICES_INFO("
                << std::hex << std::setw(4) << std::setfill('0') << get_client() <<")";
            break;
        case handler_type_e::WATCHDOG:
            VSOMEIP_WARNING << "BLOCKING CALL WATCHDOG("
                << std::hex << std::setw(4) << std::setfill('0') << get_client() <<")";
            break;
        case handler_type_e::UNKNOWN:
            VSOMEIP_WARNING << "BLOCKING CALL UNKNOWN("
                << std::hex << std::setw(4) << std::setfill('0') << get_client() << ")";
            break;
    }
}


void application_impl::get_offered_services_async(offer_type_e _offer_type, offered_services_handler_t _handler) {
    {
        std::lock_guard<std::mutex> its_lock(offered_services_handler_mutex_);
        offered_services_handler_ = _handler;
    }

    if (!is_routing_manager_host_) {
        routing_->send_get_offered_services_info(get_client(), _offer_type);
    } else {
        std::vector<std::pair<service_t, instance_t>> its_services;
        auto its_routing_manager_host = std::dynamic_pointer_cast<routing_manager_impl>(routing_);

        for (const auto& s : its_routing_manager_host->get_offered_services()) {
            for (const auto& i : s.second) {
                auto its_unreliable_endpoint = i.second->get_endpoint(false);
                auto its_reliable_endpoint = i.second->get_endpoint(true);

                if (_offer_type == offer_type_e::OT_LOCAL) {
                    if ( ((its_unreliable_endpoint && (its_unreliable_endpoint->get_local_port() == ILLEGAL_PORT))
                                && (its_reliable_endpoint && (its_reliable_endpoint->get_local_port() == ILLEGAL_PORT)))
                                || (!its_reliable_endpoint && !its_unreliable_endpoint)) {
                        its_services.push_back(std::make_pair(s.first, i.first));
                    }
                } else if (_offer_type == offer_type_e::OT_REMOTE) {
                    if ((its_unreliable_endpoint && its_unreliable_endpoint->get_local_port() != ILLEGAL_PORT)
                                 || (its_reliable_endpoint && its_reliable_endpoint->get_local_port() != ILLEGAL_PORT)) {
                        its_services.push_back(std::make_pair(s.first, i.first));
                     }
                } else if (_offer_type == offer_type_e::OT_ALL) {
                    its_services.push_back(std::make_pair(s.first, i.first));
                }
            }
        }
        on_offered_services_info(its_services);
    }
    return;
}


void application_impl::on_offered_services_info(std::vector<std::pair<service_t, instance_t>> &_services) {
    bool has_offered_services_handler(false);
    offered_services_handler_t handler = nullptr;
    {
        std::lock_guard<std::mutex> its_lock(offered_services_handler_mutex_);
        if (offered_services_handler_) {
            has_offered_services_handler = true;
            handler = offered_services_handler_;
        }
    }
    if (has_offered_services_handler) {
        std::lock_guard<std::mutex> its_lock(handlers_mutex_);
        std::shared_ptr<sync_handler> its_sync_handler
            = std::make_shared<sync_handler>([handler, _services]() {
                                                handler(_services);
                                             });
        its_sync_handler->handler_type_ = handler_type_e::OFFERED_SERVICES_INFO;
        handlers_.push_back(its_sync_handler);
        dispatcher_condition_.notify_one();
    }
}

void application_impl::watchdog_cbk(boost::system::error_code const &_error) {
    if (!_error) {

        watchdog_handler_t handler = nullptr;
        {
            std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
            handler = watchdog_handler_;
            if (handler && std::chrono::seconds::zero() != watchdog_interval_) {
                watchdog_timer_.expires_from_now(watchdog_interval_);
                watchdog_timer_.async_wait(std::bind(&application_impl::watchdog_cbk,
                        this, std::placeholders::_1));
            }
        }

        if (handler) {
            std::lock_guard<std::mutex> its_lock(handlers_mutex_);
            std::shared_ptr<sync_handler> its_sync_handler
                = std::make_shared<sync_handler>([handler]() { handler(); });
            its_sync_handler->handler_type_ = handler_type_e::WATCHDOG;
            handlers_.push_back(its_sync_handler);
            dispatcher_condition_.notify_one();
        }
    }
}

void application_impl::set_watchdog_handler(watchdog_handler_t _handler,
            std::chrono::seconds _interval) {
    if (_handler && std::chrono::seconds::zero() != _interval) {
        std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
        watchdog_handler_ = _handler;
        watchdog_interval_ = _interval;
        watchdog_timer_.expires_from_now(_interval);
        watchdog_timer_.async_wait(std::bind(&application_impl::watchdog_cbk,
                this, std::placeholders::_1));
    } else {
        std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
        watchdog_timer_.cancel();
        watchdog_handler_ = nullptr;
        watchdog_interval_ = std::chrono::seconds::zero();
    }
}

void application_impl::register_async_subscription_handler(service_t _service,
    instance_t _instance, eventgroup_t _eventgroup,
    async_subscription_handler_t _handler) {

    std::lock_guard<std::mutex> its_lock(subscription_mutex_);
    subscription_[_service][_instance][_eventgroup] = std::make_pair(nullptr, _handler);;
}

void application_impl::register_sd_acceptance_handler(
        sd_acceptance_handler_t _handler) {
    if (is_routing() && routing_) {
        const auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        rm_impl->register_sd_acceptance_handler(_handler);
    }
}

void application_impl::register_reboot_notification_handler(
        reboot_notification_handler_t _handler) {
    if (is_routing() && routing_) {
        const auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        rm_impl->register_reboot_notification_handler(_handler);
    }
}

void application_impl::set_sd_acceptance_required(
        const remote_info_t &_remote, const std::string &_path, bool _enable) {

    if (!is_routing()) {
        return;
    }

    const boost::asio::ip::address its_address(_remote.ip_.is_v4_ ?
            static_cast<boost::asio::ip::address>(boost::asio::ip::address_v4(
                    _remote.ip_.address_.v4_)) :
            static_cast<boost::asio::ip::address>(boost::asio::ip::address_v6(
                    _remote.ip_.address_.v6_)));

    if (_remote.first_ == std::numeric_limits<std::uint16_t>::max()
            && _remote.last_ == 0) {
        // special case to (de)activate rules per IP
        configuration_->set_sd_acceptance_rules_active(its_address, _enable);
        return;
    }

    configuration::port_range_t its_range { _remote.first_, _remote.last_ };
    configuration_->set_sd_acceptance_rule(its_address,
            its_range, port_type_e::PT_UNKNOWN,
            _path, _remote.is_reliable_, _enable, true);

    if (_enable && routing_) {
        const auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        rm_impl->sd_acceptance_enabled(its_address, its_range,
                _remote.is_reliable_);
    }
}

void application_impl::set_sd_acceptance_required(
        const sd_acceptance_map_type_t& _remotes, bool _enable) {

    (void)_remotes;
    (void)_enable;

#if 0
    if (!is_routing()) {
        return;
    }

    configuration::sd_acceptance_rules_t its_rules;
    for (const auto& remote_info : _remotes) {
        const boost::asio::ip::address its_address(remote_info.first.ip_.is_v4_ ?
                static_cast<boost::asio::ip::address>(boost::asio::ip::address_v4(
                        remote_info.first.ip_.address_.v4_)) :
                static_cast<boost::asio::ip::address>(boost::asio::ip::address_v6(
                        remote_info.first.ip_.address_.v6_)));
        const boost::icl::interval<std::uint16_t>::interval_type its_interval =
                remote_info.first.is_range_ ?
                    boost::icl::interval<std::uint16_t>::closed(
                            remote_info.first.first_,
                            ((remote_info.first.last_ == ANY_PORT) ?
                                    std::numeric_limits<std::uint16_t>::max() :
                                    remote_info.first.last_)) :
                    boost::icl::interval<std::uint16_t>::closed(
                            remote_info.first.first_, remote_info.first.first_);

        const bool its_reliability = remote_info.first.is_reliable_;

        const auto found_address = its_rules.find(its_address);
        if (found_address != its_rules.end()) {
            const auto found_reliability = found_address->second.second.find(
                    its_reliability);
            if (found_reliability != found_address->second.second.end()) {
                found_reliability->second.insert(its_interval);
            } else {
                found_address->second.second.emplace(std::make_pair(
                        its_reliability,
                        boost::icl::interval_set<std::uint16_t>(its_interval)));
            }
        } else {
            its_rules.insert(std::make_pair(its_address,
                   std::make_pair(remote_info.second,
                           std::map<bool, boost::icl::interval_set<std::uint16_t>>(
                                  {{ its_reliability,
                                      boost::icl::interval_set<std::uint16_t>(
                                              its_interval) } }))));
        }
    }

    configuration_->set_sd_acceptance_rules(its_rules, _enable);
#endif
}

application::sd_acceptance_map_type_t
application_impl::get_sd_acceptance_required() {

    sd_acceptance_map_type_t its_ret;

    if (is_routing()) {
        for (const auto& e : configuration_->get_sd_acceptance_rules()) {
            remote_info_t its_remote_info;
            its_remote_info.ip_.is_v4_ = e.first.is_v4();
            if (its_remote_info.ip_.is_v4_) {
                its_remote_info.ip_.address_.v4_ = e.first.to_v4().to_bytes();
            } else {
                its_remote_info.ip_.address_.v6_ = e.first.to_v6().to_bytes();
            }
            for (const auto& reliability : e.second.second) {
                its_remote_info.is_reliable_ = reliability.first;
                for (const auto& port_range : reliability.second.first) {
                    if (port_range.lower() == port_range.upper()) {
                        its_remote_info.first_ = port_range.lower();
                        its_remote_info.last_ = port_range.lower();
                        its_remote_info.is_range_ = false;
                    } else {
                        its_remote_info.first_ = port_range.lower();
                        its_remote_info.last_ = port_range.upper();
                        its_remote_info.is_range_ = true;
                    }
                    its_ret[its_remote_info] = e.second.first;
                }
                for (const auto& port_range : reliability.second.second) {
                    if (port_range.lower() == port_range.upper()) {
                        its_remote_info.first_ = port_range.lower();
                        its_remote_info.last_ = port_range.lower();
                        its_remote_info.is_range_ = false;
                    } else {
                        its_remote_info.first_ = port_range.lower();
                        its_remote_info.last_ = port_range.upper();
                        its_remote_info.is_range_ = true;
                    }
                    its_ret[its_remote_info] = e.second.first;
                }
            }
        }
    }

    return its_ret;
}

void application_impl::register_routing_ready_handler(
        routing_ready_handler_t _handler) {
    if (is_routing() && routing_) {
        const auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        rm_impl->register_routing_ready_handler(_handler);
    }
}

void application_impl::register_routing_state_handler(
        routing_state_handler_t _handler) {
    if (is_routing() && routing_) {
        const auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        rm_impl->register_routing_state_handler(_handler);
    }
}

bool application_impl::update_service_configuration(service_t _service,
                                                    instance_t _instance,
                                                    std::uint16_t _port,
                                                    bool _reliable,
                                                    bool _magic_cookies_enabled,
                                                    bool _offer) {
    bool ret = false;
    if (!is_routing_manager_host_) {
        VSOMEIP_ERROR << __func__ << " is only intended to be called by "
                "application acting as routing manager host";
    } else if (!routing_) {
        VSOMEIP_ERROR << __func__ << " routing is zero";
    } else {
        auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        if (rm_impl) {
            if (_offer) {
                ret = rm_impl->offer_service_remotely(_service, _instance,
                        _port, _reliable, _magic_cookies_enabled);
            } else {
                ret = rm_impl->stop_offer_service_remotely(_service, _instance,
                        _port, _reliable, _magic_cookies_enabled);
            }
        }
    }
    return ret;
}

void application_impl::update_security_policy_configuration(uint32_t _uid,
                                                  uint32_t _gid,
                                                  ::std::shared_ptr<policy> _policy,
                                                  std::shared_ptr<payload> _payload,
                                                  security_update_handler_t _handler) {
    if (!is_routing()) {
        VSOMEIP_ERROR << __func__ << " is only intended to be called by "
                "application acting as routing manager host";
    } else if (!routing_) {
        VSOMEIP_ERROR << __func__ << " routing is zero";
    } else {
        auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        if (rm_impl) {
            rm_impl->update_security_policy_configuration(_uid, _gid, _policy, _payload, _handler);
        }
    }
}

void application_impl::remove_security_policy_configuration(uint32_t _uid,
                                                  uint32_t _gid,
                                                  security_update_handler_t _handler) {
    if (!is_routing()) {
        VSOMEIP_ERROR << __func__ << " is only intended to be called by "
                "application acting as routing manager host";
    } else if (!routing_) {
        VSOMEIP_ERROR << __func__ << " routing is zero";
    } else {
        auto rm_impl = std::dynamic_pointer_cast<routing_manager_impl>(routing_);
        if (rm_impl) {
            rm_impl->remove_security_policy_configuration(_uid, _gid, _handler);
        }
    }
}

} // namespace vsomeip_v3

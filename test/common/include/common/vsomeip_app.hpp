// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once

#include <condition_variable>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include <vsomeip/internal/logger.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/vsomeip.hpp>

#ifdef USE_DLT
#ifndef ANDROID
#include <dlt/dlt.h>
#endif // ifndef ANDROID
#endif // ifded USE_DLT

namespace common {

std::shared_ptr<vsomeip_v3::message> create_standard_vsip_request(vsomeip::service_t _service, vsomeip::instance_t _instance,
                                                                  vsomeip_v3::method_t _method, vsomeip_v3::interface_version_t _interface,
                                                                  vsomeip_v3::message_type_e _message_type);

/**
 * @brief Base logger for DLT integration (if enabled).
 * @note Used as a base for vsomeip app classes.
 */
class base_logger {
public:
    const char* dlt_application_id_ = nullptr;
    const char* dlt_application_name_ = nullptr;

    /**
     * @brief Construct a base logger with DLT application id and name.
     * @param dlt_application_id_ DLT application identifier.
     * @param dlt_application_name_ DLT application name.
     */
    base_logger(const char* dlt_application_id_, const char* dlt_application_name_);
    /**
     * @brief Destructor.
     */
    ~base_logger();
};

/**
 * @brief Event parameters for vsomeip event configuration.
 */
struct event_params_t {
    vsomeip_v3::event_type_e type = vsomeip_v3::event_type_e::ET_EVENT;
    std::chrono::milliseconds cycle = std::chrono::milliseconds::zero();
    bool change_resets_cycle = false;
    bool update_on_change = true;
    vsomeip_v3::epsilon_change_func_t epsilon_change_func = nullptr;
    vsomeip_v3::reliability_type_e reliability = vsomeip_v3::reliability_type_e::RT_UNKNOWN;
};

/**
 * @brief Eventgroup information: group id and associated event ids.
 */
struct eventgroup_info_t {
    vsomeip_v3::eventgroup_t group_id;
    std::vector<vsomeip_v3::event_t> event_ids;
};

/**
 * @brief Service information: ids, eventgroups, and event parameters.
 * @invariant service_id/instance_id/method_id must be valid for vsomeip.
 */
struct service_info_t {
    vsomeip::service_t service_id = 0xbad;
    vsomeip::instance_t instance_id = 0x1;
    vsomeip::method_t method_id = 0x1;
    std::vector<eventgroup_info_t> eventgroups;
    std::map<vsomeip_v3::event_t, event_params_t> events;

    ~service_info_t() = default;

    /**
     * @brief Get all eventgroups containing a given event in the service.
     * @param service_info Service info to search in.
     * @param event_id Event id to look for.
     * @return Set of eventgroup ids containing the event.
     */
    static std::set<vsomeip_v3::eventgroup_t> get_eventgroups_for_event(const service_info_t& service_info, vsomeip_v3::event_t event_id);
};

// Type aliases
using service_info_list_t = std::vector<service_info_t>;
using service_instance_pair_t = std::pair<vsomeip::service_t, vsomeip::instance_t>;

/**
 * @brief Set of service/instance pairs for fast lookup and management.
 * @invariant Only unique pairs are stored. Not thread-safe.
 */
class service_instance_set_t {
private:
    std::set<service_instance_pair_t> service_instances_;

public:
    /**
     * @brief Construct from a list of service infos.
     * @param service_list List of service_info_t.
     */
    explicit service_instance_set_t(const service_info_list_t& service_list) {
        for (const auto& service : service_list) {
            service_instances_.emplace(service.service_id, service.instance_id);
        }
    }
    /**
     * @brief Add a service/instance pair.
     * @param service_id Service id.
     * @param instance_id Instance id.
     */
    void add(vsomeip::service_t service_id, vsomeip::instance_t instance_id) { service_instances_.emplace(service_id, instance_id); }
    /**
     * @brief Remove a service/instance pair.
     * @param service_id Service id.
     * @param instance_id Instance id.
     */
    void remove(vsomeip::service_t service_id, vsomeip::instance_t instance_id) { service_instances_.erase({service_id, instance_id}); }
    /**
     * @brief Check if set is empty.
     * @return True if empty.
     */
    bool is_empty() const { return service_instances_.empty(); }
    /**
     * @brief Check if a service/instance pair exists.
     * @param service_id Service id.
     * @param instance_id Instance id.
     * @return True if found.
     */
    bool contains(vsomeip::service_t service_id, vsomeip::instance_t instance_id) const {
        return service_instances_.find({service_id, instance_id}) != service_instances_.end();
    }
    /**
     * @brief Get the number of pairs in the set.
     * @return Number of pairs.
     */
    size_t size() const { return service_instances_.size(); }
};

/**
 * @brief Base class for vsomeip test/service applications with request/offer/command API.
 * @invariant Not copyable. Callbacks must be set before start().
 * @note Use set_request/offer/provide, set callbacks, then start/stop/send commands.
 */
class base_vsip_app : public base_logger {
public:
    using state_callback_fn = std::function<void(vsomeip::state_type_e)>;
    using availability_callback_fn = std::function<void(vsomeip::service_t, vsomeip::instance_t, bool)>;
    /**
     * @brief Construct a vsomeip app with name and id.
     * @param app_name_ Application name.
     * @param app_id_ Application id.
     */
    base_vsip_app(const char* app_name_, const char* app_id_);
    /**
     * @brief Destructor.
     */
    ~base_vsip_app() = default;
    // SETUP
    /**
     * @brief Set a single request service.
     * @param request Service info to request.
     */
    void set_request(service_info_t& request);
    /**
     * @brief Set multiple request services.
     * @param requests List of service infos to request.
     */
    void set_request(service_info_list_t& requests);
    /**
     * @brief Set a single offer service.
     * @param offer Service info to offer.
     */
    void set_offer(service_info_t& offer);
    /**
     * @brief Set multiple offer services.
     * @param offers List of service infos to offer.
     */
    void set_offer(service_info_list_t& offers);
    /**
     * @brief Set a single provide service.
     * @param provide Service info to provide.
     */
    void set_provide(service_info_t& provide);
    /**
     * @brief Set the state change callback.
     * @param cb Callback function.
     */
    void set_state_callback(state_callback_fn& cb);
    /**
     * @brief Set the service availability callback.
     * @param cb Callback function.
     */
    void set_availability_callback(availability_callback_fn& cb);
    // COMMANDS
    /**
     * @brief Only call app->start().
     */
    void only_start();
    /**
     * @brief Start the application by enabling the greenlight.
     */
    void start();
    /**
     * @brief Stop the application.
     */
    void stop();
    /**
     * @brief Only call app->stop().
     */
    void only_stop();
    /**
     * @brief Join the run thread.
     */
    void join();
    /**
     * @brief Send a request for all configured services.
     */
    void send_request();
    /**
     * @brief Send a request for a specific service.
     * @param request Service info to request.
     */
    void send_request(const service_info_t& request);
    /**
     * @brief Release all requested services.
     */
    void send_release();
    /**
     * @brief Release a specific requested service.
     * @param request Service info to release.
     */
    void send_release(const service_info_t& request);
    /**
     * @brief Offer all configured services.
     */
    void send_offer();
    /**
     * @brief Offer a specific service.
     * @param offer Service info to offer.
     */
    void send_offer(service_info_t& offer);
    /**
     * @brief Stop offering all services.
     */
    void send_stop_offer();

    void greenlight();
    /**
     * @brief Stop offering a specific service.
     * @param offer Service info to stop offering.
     */
    void send_stop_offer(const service_info_t& offer);

protected:
    std::shared_ptr<vsomeip::application> _app;
    std::thread _run_thread;

    std::condition_variable greenlight_;
    std::mutex greenlight_mutex_;

    bool greenlight_ready_ = false;

    service_info_list_t _requests;
    service_info_list_t _offers;

    // Custom callbacks
    std::optional<state_callback_fn> _register_state_callback = std::nullopt;
    std::optional<availability_callback_fn> _register_availability_handler = std::nullopt;

    void run();
    void default_on_state(vsomeip::state_type_e state_);
};

/**
 * @brief Builder for base_vsip_app, allowing fluent configuration of requests, offers, and
 * callbacks.
 * @note Use with_* methods to configure, then build() to create the app.
 */
class base_vsip_app_builder {
private:
    std::string app_name;
    std::string app_id;
    service_info_list_t requests;
    service_info_list_t offers;
    std::optional<base_vsip_app::state_callback_fn> state_callback;
    std::optional<base_vsip_app::availability_callback_fn> availability_callback;
    bool auto_start_ = true;

public:
    /**
     * @brief Construct a builder for base_vsip_app.
     * @param name Application name.
     * @param id Application id.
     */
    base_vsip_app_builder(const char* name, const char* id);

    /**
     * @brief Add a single request service to the builder.
     * @param info Service info to request.
     * @return Reference to builder.
     */
    base_vsip_app_builder& with_request(const service_info_t& info);
    /**
     * @brief Add multiple request services to the builder.
     * @param list List of service infos to request.
     * @return Reference to builder.
     */
    base_vsip_app_builder& with_request(service_info_list_t list);
    /**
     * @brief Add a single offer service to the builder.
     * @param info Service info to offer.
     * @return Reference to builder.
     */
    base_vsip_app_builder& with_offer(const service_info_t& info);
    /**
     * @brief Add multiple offer services to the builder.
     * @param list List of service infos to offer.
     * @return Reference to builder.
     */
    base_vsip_app_builder& with_offer(service_info_list_t list);
    /**
     * @brief Set the state callback for the builder.
     * @param fn Callback function.
     * @return Reference to builder.
     */
    base_vsip_app_builder& with_state_callback(const base_vsip_app::state_callback_fn& fn);
    /**
     * @brief Set the availability callback for the builder.
     * @param fn Callback function.
     * @return Reference to builder.
     */
    base_vsip_app_builder& with_availability_callback(const base_vsip_app::availability_callback_fn& fn);
    /**
     * @brief Defines whether the application should start automatically.
     * @param value True to start automatically, false otherwise.
     * @return Reference to the constructor.
     */
    base_vsip_app_builder& auto_start(bool value) {
        auto_start_ = value;
        return *this;
    }
    /**
     * @brief Build and return the configured base_vsip_app.
     * @return Unique pointer to base_vsip_app.
     */
    std::unique_ptr<base_vsip_app> build();
};

} // namespace common

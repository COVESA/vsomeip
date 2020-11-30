// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_APPLICATION_IMPL_HPP_
#define VSOMEIP_V3_APPLICATION_IMPL_HPP_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>

#include <vsomeip/export.hpp>
#include <vsomeip/application.hpp>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID
#include "../../routing/include/routing_manager_host.hpp"

namespace vsomeip_v3 {

class runtime;
class configuration;
class routing_manager;
class routing_manager_stub;

class application_impl: public application,
        public routing_manager_host,
        public std::enable_shared_from_this<application_impl> {
public:
    VSOMEIP_EXPORT application_impl(const std::string &_name);
    VSOMEIP_EXPORT  ~application_impl();

    VSOMEIP_EXPORT bool init();
    VSOMEIP_EXPORT void start();
    VSOMEIP_EXPORT void stop();
    VSOMEIP_EXPORT void process(int _number);

    VSOMEIP_EXPORT security_mode_e get_security_mode() const;

    // Provide services / events
    VSOMEIP_EXPORT void offer_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor);

    VSOMEIP_EXPORT void stop_offer_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor);

    VSOMEIP_EXPORT void offer_event(service_t _service, instance_t _instance,
            event_t _notifier,
            const std::set<eventgroup_t> &_eventgroups, event_type_e _type,
            std::chrono::milliseconds _cycle, bool _change_resets_cycle,
            bool _update_on_change,
            const epsilon_change_func_t &_epsilon_change_func,
            reliability_type_e _reliability);

    VSOMEIP_EXPORT void stop_offer_event(service_t _service,
            instance_t _instance, event_t _event);

    // Consume services / events
    VSOMEIP_EXPORT void request_service(
            service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor);
    VSOMEIP_EXPORT void release_service(
            service_t _service, instance_t _instance);

    VSOMEIP_EXPORT void request_event(service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups,
            event_type_e _type, reliability_type_e _reliability);
    VSOMEIP_EXPORT void release_event(service_t _service,
            instance_t _instance, event_t _event);

    VSOMEIP_EXPORT void subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major, event_t _event);

    VSOMEIP_EXPORT void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);
    VSOMEIP_EXPORT void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event);

    VSOMEIP_EXPORT bool is_available(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor) const;

    VSOMEIP_EXPORT void send(std::shared_ptr<message> _message);

    VSOMEIP_EXPORT void notify(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload,
            bool _force) const;

    VSOMEIP_EXPORT void notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload, client_t _client,
            bool _force) const;

    VSOMEIP_EXPORT void register_state_handler(state_handler_t _handler);
    VSOMEIP_EXPORT void unregister_state_handler();

    VSOMEIP_EXPORT void register_message_handler(service_t _service,
            instance_t _instance, method_t _method, message_handler_t _handler);
    VSOMEIP_EXPORT void unregister_message_handler(service_t _service,
            instance_t _instance, method_t _method);

    VSOMEIP_EXPORT void register_availability_handler(service_t _service,
            instance_t _instance, availability_handler_t _handler,
            major_version_t _major, minor_version_t _minor);
    VSOMEIP_EXPORT void unregister_availability_handler(service_t _service,
            instance_t _instance,
            major_version_t _major, minor_version_t _minor);

    VSOMEIP_EXPORT void register_subscription_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, subscription_handler_t _handler);
    VSOMEIP_EXPORT void unregister_subscription_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup);

    VSOMEIP_EXPORT bool is_routing() const;

    // routing_manager_host
    VSOMEIP_EXPORT const std::string & get_name() const;
    VSOMEIP_EXPORT client_t get_client() const;
    VSOMEIP_EXPORT void set_client(const client_t &_client);
    VSOMEIP_EXPORT session_t get_session();
    VSOMEIP_EXPORT diagnosis_t get_diagnosis() const;
    VSOMEIP_EXPORT std::shared_ptr<configuration> get_configuration() const;
    VSOMEIP_EXPORT std::shared_ptr<configuration_public> get_public_configuration() const;
    VSOMEIP_EXPORT boost::asio::io_service & get_io();

    VSOMEIP_EXPORT void on_state(state_type_e _state);
    VSOMEIP_EXPORT void on_availability(service_t _service, instance_t _instance,
            bool _is_available, major_version_t _major, minor_version_t _minor);
    VSOMEIP_EXPORT void on_message(std::shared_ptr<message> &&_message);
    VSOMEIP_EXPORT void on_subscription(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, client_t _client, uid_t _uid, gid_t _gid, bool _subscribed,
            std::function<void(bool)> _accepted_cb);
    VSOMEIP_EXPORT void on_subscription_status(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event, uint16_t _error);
    VSOMEIP_EXPORT void register_subscription_status_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event,
            subscription_status_handler_t _handler, bool _is_selective);
    VSOMEIP_EXPORT void unregister_subscription_status_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    // service_discovery_host
    VSOMEIP_EXPORT routing_manager * get_routing_manager() const;

    VSOMEIP_EXPORT bool are_available(available_t &_available,
                       service_t _service, instance_t _instance,
                       major_version_t _major, minor_version_t _minor) const;
    VSOMEIP_EXPORT void set_routing_state(routing_state_e _routing_state);

    VSOMEIP_EXPORT void clear_all_handler();


    VSOMEIP_EXPORT void get_offered_services_async(offer_type_e _offer_type, offered_services_handler_t _handler);

    VSOMEIP_EXPORT void on_offered_services_info(std::vector<std::pair<service_t, instance_t>> &_services);

    VSOMEIP_EXPORT void set_watchdog_handler(watchdog_handler_t _handler, std::chrono::seconds _interval);

    VSOMEIP_EXPORT void register_async_subscription_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, async_subscription_handler_t _handler);

    VSOMEIP_EXPORT void set_sd_acceptance_required(const remote_info_t& _remote,
                                                   const std::string& _path, bool _enable);
    VSOMEIP_EXPORT void set_sd_acceptance_required(
            const sd_acceptance_map_type_t& _remotes, bool _enable);

    VSOMEIP_EXPORT sd_acceptance_map_type_t get_sd_acceptance_required();

    VSOMEIP_EXPORT void register_sd_acceptance_handler(sd_acceptance_handler_t _handler);

    VSOMEIP_EXPORT void register_reboot_notification_handler(reboot_notification_handler_t _handler);

    VSOMEIP_EXPORT void register_routing_ready_handler(routing_ready_handler_t _handler);
    VSOMEIP_EXPORT void register_routing_state_handler(routing_state_handler_t _handler);

    VSOMEIP_EXPORT bool update_service_configuration(service_t _service,
                                                     instance_t _instance,
                                                     std::uint16_t _port,
                                                     bool _reliable,
                                                     bool _magic_cookies_enabled,
                                                     bool _offer);

    VSOMEIP_EXPORT void update_security_policy_configuration(uint32_t _uid,
                                                             uint32_t _gid,
                                                             std::shared_ptr<policy> _policy,
                                                             std::shared_ptr<payload> _payload,
                                                             security_update_handler_t _handler);
    VSOMEIP_EXPORT void remove_security_policy_configuration(uint32_t _uid,
                                                             uint32_t _gid,
                                                             security_update_handler_t _handler);

private:
    //
    // Types
    //

    enum class handler_type_e : uint8_t {
        MESSAGE,
        AVAILABILITY,
        STATE,
        SUBSCRIPTION,
        OFFERED_SERVICES_INFO,
        WATCHDOG,
        UNKNOWN
    };

    struct sync_handler {

        sync_handler(std::function<void()> _handler) :
                    handler_(_handler),
                    service_id_(ANY_SERVICE),
                    instance_id_(ANY_INSTANCE),
                    method_id_(ANY_METHOD),
                    session_id_(0),
                    eventgroup_id_(0),
                    handler_type_(handler_type_e::UNKNOWN) { }

        sync_handler(service_t _service_id, instance_t _instance_id,
                     method_t _method_id, session_t _session_id,
                     eventgroup_t _eventgroup_id, handler_type_e _handler_type) :
                    handler_(nullptr),
                    service_id_(_service_id),
                    instance_id_(_instance_id),
                    method_id_(_method_id),
                    session_id_(_session_id),
                    eventgroup_id_(_eventgroup_id),
                    handler_type_(_handler_type) { }

        std::function<void()> handler_;
        service_t service_id_;
        instance_t instance_id_;
        method_t method_id_;
        session_t session_id_;
        eventgroup_t eventgroup_id_;
        handler_type_e handler_type_;
    };

    struct message_handler {
        message_handler(message_handler_t _handler) :
            handler_(_handler) {}

        bool operator<(const message_handler& _other) const {
            return handler_.target<void (*)(const std::shared_ptr<message> &)>()
                    < _other.handler_.target<void (*)(const std::shared_ptr<message> &)>();
        }
        message_handler_t handler_;
    };

    //
    // Methods
    //
    bool is_available_unlocked(service_t _service, instance_t _instance,
                               major_version_t _major, minor_version_t _minor) const;

    bool are_available_unlocked(available_t &_available,
                                service_t _service, instance_t _instance,
                                major_version_t _major, minor_version_t _minor) const;
    void do_register_availability_handler(service_t _service,
            instance_t _instance, availability_handler_t _handler,
            major_version_t _major, minor_version_t _minor);


    void main_dispatch();
    void dispatch();
    void invoke_handler(std::shared_ptr<sync_handler> &_handler);
    std::shared_ptr<sync_handler> get_next_handler();
    void reschedule_availability_handler(const std::shared_ptr<sync_handler> &_handler);
    bool has_active_dispatcher();
    bool is_active_dispatcher(const std::thread::id &_id);
    void remove_elapsed_dispatchers();

    void shutdown();

    void send_back_cached_event(service_t _service, instance_t _instance, event_t _event);
    void send_back_cached_eventgroup(service_t _service, instance_t _instance, eventgroup_t _eventgroup);
    void check_send_back_cached_event(service_t _service, instance_t _instance,
                                      event_t _event, eventgroup_t _eventgroup,
                                      bool *_send_back_cached_event,
                                      bool *_send_back_cached_eventgroup);
    void remove_subscription(service_t _service, instance_t _instance,
                             eventgroup_t _eventgroup, event_t _event);
    bool check_for_active_subscription(service_t _service, instance_t _instance,
                                       event_t _event);

    void deliver_subscription_state(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event, uint16_t _error);

    bool check_subscription_state(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event);

    void print_blocking_call(const std::shared_ptr<sync_handler>& _handler);

    void watchdog_cbk(boost::system::error_code const &_error);

    //
    // Attributes
    //
    std::shared_ptr<runtime> runtime_;
    std::atomic<client_t> client_; // unique application identifier
    session_t session_;
    std::mutex session_mutex_;

    std::mutex initialize_mutex_;
    bool is_initialized_;

    std::string name_;
    std::shared_ptr<configuration> configuration_;

    boost::asio::io_service io_;
    std::set<std::shared_ptr<std::thread> > io_threads_;
    std::shared_ptr<boost::asio::io_service::work> work_;

    // Proxy to or the Routing Manager itself
    std::shared_ptr<routing_manager> routing_;

    // vsomeip state (registered / deregistered)
    state_type_e state_;

    // vsomeip state handler
    std::mutex state_handler_mutex_;
    state_handler_t handler_;

    // vsomeip security mode
    security_mode_e security_mode_;

    // vsomeip offered services handler
    std::mutex offered_services_handler_mutex_;
    offered_services_handler_t offered_services_handler_;

    // Method/Event (=Member) handlers
    std::map<service_t,
            std::map<instance_t, std::map<method_t, message_handler_t> > > members_;
    mutable std::mutex members_mutex_;

    // Availability handlers
    typedef std::map<major_version_t, std::map<minor_version_t, std::pair<availability_handler_t,
            bool>>> availability_major_minor_t;
    std::map<service_t, std::map<instance_t, availability_major_minor_t>> availability_;
    mutable std::recursive_mutex availability_mutex_;

    // Availability
    mutable available_t available_;

    // Subscription handlers
    std::map<service_t,
            std::map<instance_t,
                    std::map<eventgroup_t,
                            std::pair<subscription_handler_t, async_subscription_handler_t>>>> subscription_;
    mutable std::mutex subscription_mutex_;
    std::map<service_t,
        std::map<instance_t, std::map<eventgroup_t,
        std::map<client_t, error_handler_t > > > > eventgroup_error_handlers_;
    mutable std::mutex subscription_error_mutex_;

#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
    // Signals
    boost::asio::signal_set signals_;
    bool catched_signal_;
#endif

    // Handlers
    mutable std::deque<std::shared_ptr<sync_handler>> handlers_;
    mutable std::mutex handlers_mutex_;

    // Dispatching
    std::atomic<bool> is_dispatching_;
    // Dispatcher threads
    std::map<std::thread::id, std::shared_ptr<std::thread>> dispatchers_;
    // Dispatcher threads that elapsed and can be removed
    std::set<std::thread::id> elapsed_dispatchers_;
    // Dispatcher threads that are running
    std::set<std::thread::id> running_dispatchers_;
    // Mutex to protect access to dispatchers_ & elapsed_dispatchers_
    std::mutex dispatcher_mutex_;

    // Condition to wakeup the dispatcher thread
    mutable std::condition_variable dispatcher_condition_;
    std::size_t max_dispatchers_;
    std::size_t max_dispatch_time_;

    std::condition_variable stop_cv_;
    std::mutex start_stop_mutex_;
    bool stopped_;
    std::thread stop_thread_;

    std::condition_variable block_stop_cv_;
    std::mutex block_stop_mutex_;
    bool block_stopping_;

    static uint32_t app_counter__;
    static std::mutex app_counter_mutex__;

    bool is_routing_manager_host_;

    // Event subscriptions
    std::mutex subscriptions_mutex_;
    std::map<service_t, std::map<instance_t,
            std::map<event_t, std::map<eventgroup_t, bool>>>> subscriptions_;

    std::thread::id stop_caller_id_;
    std::thread::id start_caller_id_;

    bool stopped_called_;

    std::map<service_t, std::map<instance_t, std::map<eventgroup_t,
            std::map<event_t, std::pair<subscription_status_handler_t, bool> > > > > subscription_status_handlers_;
    std::mutex subscription_status_handlers_mutex_;

    std::mutex subscriptions_state_mutex_;
    std::map<std::tuple<service_t, instance_t, eventgroup_t, event_t>,
        subscription_state_e> subscription_state_;

    std::mutex watchdog_timer_mutex_;
    boost::asio::steady_timer watchdog_timer_;
    watchdog_handler_t watchdog_handler_;
    std::chrono::seconds watchdog_interval_;

    bool client_side_logging_;
    std::set<std::tuple<service_t, instance_t> > client_side_logging_filter_;

    std::map<std::pair<service_t, instance_t>,
            std::deque<std::shared_ptr<sync_handler> > > availability_handlers_;

    uid_t own_uid_;
    gid_t own_gid_;

#ifdef VSOMEIP_HAS_SESSION_HANDLING_CONFIG
    bool has_session_handling_;
#endif // VSOMEIP_HAS_SESSION_HANDLING_CONFIG
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_APPLICATION_IMPL_HPP_

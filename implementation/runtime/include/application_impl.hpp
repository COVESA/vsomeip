// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_APPLICATION_IMPL_HPP
#define VSOMEIP_APPLICATION_IMPL_HPP

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

#include <vsomeip/export.hpp>
#include <vsomeip/application.hpp>

#include "../../routing/include/routing_manager_host.hpp"

namespace vsomeip {

class configuration;
class logger;
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

    VSOMEIP_EXPORT void offer_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl);

    VSOMEIP_EXPORT void stop_offer_service(service_t _service,
            instance_t _instance);

    // Consume services
    VSOMEIP_EXPORT void request_service(service_t _service,
            instance_t _instance, bool _has_selective, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl);
    VSOMEIP_EXPORT void release_service(service_t _service,
            instance_t _instance);

    VSOMEIP_EXPORT void subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl);

    VSOMEIP_EXPORT void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);

    VSOMEIP_EXPORT bool is_available(service_t _service, instance_t _instance);

    VSOMEIP_EXPORT void send(std::shared_ptr<message> _message, bool _flush);

    VSOMEIP_EXPORT void notify(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload) const;

    VSOMEIP_EXPORT void notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload, client_t _client) const;

    VSOMEIP_EXPORT void register_event_handler(event_handler_t _handler);
    VSOMEIP_EXPORT void unregister_event_handler();

    VSOMEIP_EXPORT void register_message_handler(service_t _service,
            instance_t _instance, method_t _method, message_handler_t _handler);
    VSOMEIP_EXPORT void unregister_message_handler(service_t _service,
            instance_t _instance, method_t _method);

    VSOMEIP_EXPORT void register_availability_handler(service_t _service,
            instance_t _instance, availability_handler_t _handler);
    VSOMEIP_EXPORT void unregister_availability_handler(service_t _service,
            instance_t _instance);

    VSOMEIP_EXPORT void register_subscription_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, subscription_handler_t _handler);
    VSOMEIP_EXPORT void unregister_subscription_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup);

    // routing_manager_host
    VSOMEIP_EXPORT const std::string & get_name() const;
    VSOMEIP_EXPORT client_t get_client() const;
    VSOMEIP_EXPORT std::shared_ptr<configuration> get_configuration() const;
    VSOMEIP_EXPORT boost::asio::io_service & get_io();

    VSOMEIP_EXPORT void on_event(event_type_e _event);
    VSOMEIP_EXPORT void on_availability(service_t _service,
            instance_t _instance,
    bool _is_available) const;
    VSOMEIP_EXPORT void on_message(std::shared_ptr<message> _message);
    VSOMEIP_EXPORT void on_error(error_code_e _error);
    VSOMEIP_EXPORT bool on_subscription(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, client_t _client, bool _subscribed);

    // service_discovery_host
    VSOMEIP_EXPORT routing_manager * get_routing_manager() const;

private:
    void service();
    inline void update_session() {
        session_++;
        if (0 == session_) {
            session_++;
        }
    }

    void dispatch();

private:
    client_t client_; // unique application identifier
    session_t session_;

    std::mutex initialize_mutex_;
    bool is_initialized_;

    std::string name_;
    std::shared_ptr<configuration> configuration_;

    boost::asio::io_service io_;

    // Proxy to or the Routing Manager itself
    std::shared_ptr<routing_manager> routing_;

    // (Non-SOME/IP) Event handler
    event_handler_t handler_;

    // Method/Event (=Member) handlers
    std::map<service_t,
            std::map<instance_t, std::map<method_t, message_handler_t> > > members_;
    mutable std::mutex members_mutex_;

    // Availability handlers
    std::map<service_t, std::map<instance_t, availability_handler_t> > availability_;
    mutable std::mutex availability_mutex_;

    // Availability
    mutable std::map<service_t, std::set<instance_t>> available_;

    // Subscriptopn handlers
    std::map<service_t, std::map<instance_t, std::map<eventgroup_t, subscription_handler_t>>>
        subscription_;
    mutable std::mutex subscription_mutex_;

    // Signals
    boost::asio::signal_set signals_;

    // Thread pool for dispatch handlers
    std::size_t num_dispatchers_;
    std::vector<std::thread> dispatchers_;
    std::atomic_bool is_dispatching_;

    // Handlers
    mutable std::deque<std::function<void()>> handlers_;

    // Condition to wake up
    mutable std::mutex dispatch_mutex_;
    mutable std::condition_variable dispatch_condition_;

    // Workaround for destruction problem
    std::shared_ptr<logger> logger_;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_IMPL_HPP

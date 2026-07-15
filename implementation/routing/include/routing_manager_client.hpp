// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <mutex>
#include <atomic>
#include <span>
#include <tuple>
#include <vector>
#include <condition_variable>
#include <functional>
#include <queue>
#include <unordered_set>

#include <vsomeip/constants.hpp>
#include <vsomeip/vsomeip_sec.h>

#include "event.hpp"
#include "serviceinfo.hpp"
#include "routing_host.hpp"
#include "eventgroupinfo.hpp"
#include "routing_manager_host.hpp"
#include "local_service_table.hpp"

#include <boost/asio/steady_timer.hpp>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/handler.hpp>
#include <vsomeip/primitive_types.hpp>

#include "local_service_table.hpp"
#include "local_offering_table.hpp"
#include "event_dispatcher.hpp"
#include "types.hpp"
#include "../../protocol/include/protocol.hpp"
#include "../../protocol/include/command_types.hpp"
#include "../../endpoints/include/local_endpoint_manager_host.hpp"
#include "../../utility/include/service_instance_map.hpp"
#include "../../endpoints/include/endpoint_manager_base.hpp"
#include "../../tracing/include/connector_impl.hpp"

namespace vsomeip_v3 {

namespace trace {
class connector_impl;
} // namespace trace

class configuration;
class event;
class timer;
class local_server;
class routing_manager_host;
class routing_client_state_machine;

namespace protocol {
class offered_services_response_command;
}

class routing_manager_client : public local_endpoint_manager_host,
                               public event_dispatcher,
                               public routing_host,
                               public std::enable_shared_from_this<routing_manager_client> {
    struct subscription_data_t;

public:
    using available_t = std::map<service_t, std::map<instance_t, std::map<major_version_t, minor_version_t>>>;

    routing_manager_client(routing_manager_host* _host, bool _client_side_logging,
                           const std::set<std::tuple<service_t, instance_t>>& _client_side_logging_filter);
    virtual ~routing_manager_client();

    void init();
    void start();
    async::hook stop();

    std::shared_ptr<configuration> get_configuration() const;

    void ping_host();
    void on_pong(client_t _client);

    bool offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    void stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    void request_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    void release_service(client_t _client, service_t _service, instance_t _instance);

    void subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
                   event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter);

    void unsubscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);
    void unsubscribe_base(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                          std::scoped_lock<std::mutex> const& _lock);

    bool send_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target, std::shared_ptr<message> _message);

    bool send_to(const std::shared_ptr<endpoint_definition>& _target, const byte_t* _data, uint32_t _size, instance_t _instance);

    void register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                        const std::set<eventgroup_t>& _eventgroups, const event_type_e _type, reliability_type_e _reliability,
                        std::chrono::milliseconds _cycle, bool _change_resets_cycle, bool _update_on_change,
                        epsilon_change_func_t _epsilon_change_func, bool _is_provided);

    void unregister_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier, bool _is_provided);

    void on_routing_info(const byte_t* _data, uint32_t _size);

    void register_client_error_handler(client_t _client, const std::shared_ptr<local_endpoint>& _endpoint, connection_role_e _role);
    void cleanup_client(client_t _client, bool _due_to_error, connection_role_e _role);

    // local_endpoint_manager_host
    client_t get_client_id() override;
    void set_port(port_t _port) override;
    void register_error_handler(client_t _client, std::shared_ptr<local_endpoint> _ep, connection_role_e _role) override;

    void on_offered_services_info(std::vector<protocol::service_data> const& _services);

    void send_get_offered_services_info(client_t _client, offer_type_e _offer_type);
    bool send(client_t _client, std::shared_ptr<message> _message, bool _force);
    bool is_available(service_t _service, instance_t _instance, major_version_t _major) const;

    std::set<std::shared_ptr<event>> find_consumed_events(service_t _service, instance_t _instance, eventgroup_t _eventgroup) const;
    void notify_one(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload, client_t _client,
                    bool _force);
    void notify(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload, bool _force);
    /**
     * @brief Notify current value for event/eventgroup
     *
     * Caller *MUST* hold `provider_mutex_` through not only call, but also during the subscription insertion + subscription ack/nack
     */
    void notify_one_current_value(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                  std::scoped_lock<std::mutex> const& _lock);
    std::shared_ptr<event> find_provided_event(service_t _service, instance_t _instance, event_t _event) const;
    std::shared_ptr<event> find_consumed_event(service_t _service, instance_t _instance, event_t _event) const;

    std::string const& get_name() const;
    std::string get_client_host() const;
    vsomeip_sec_client_t get_sec_client() const;
    void set_sec_client_port(port_t _port);

    bool is_available(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor) const;
    bool are_available(available_t& _available, service_t _service, instance_t _instance, major_version_t _major,
                       minor_version_t _minor) const;

    // Registers the handler against the current availability under consumer_mutex_, avoiding the TOCTOU race of a
    // separate get_availability_state(); for wildcard registrations it also replays the currently available instances.
    void register_availability_handler(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor,
                                       const std::function<void(bool _is_available)>& _register_handler);

private:
    void unregister_event_base(client_t _client, service_t _service, instance_t _instance, event_t _event, bool _is_provided);

    std::shared_ptr<event> find_provided_event(service_t _service, instance_t _instance, event_t _event,
                                               std::scoped_lock<std::mutex> const& _lock) const;
    std::shared_ptr<event> find_consumed_event(service_t _service, instance_t _instance, event_t _event,
                                               std::scoped_lock<std::mutex> const& _lock) const;
    void remove_pending_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                     std::scoped_lock<std::mutex> const&);

    client_t get_client_by_address(const boost::asio::ip::address& _address, port_t _port) const;

    void register_application(client_t _client, std::unique_lock<std::mutex>& receiver_lock_);

    void reconnect();

    void send_pong() const;

    bool send_offer_service(protocol::service_data const& _data);

    bool send_event_registrations(client_t _client, std::span<protocol::register_event_data const> _registrations);

    void send_subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
                        event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter);

    void send_subscribe_nack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                             remote_subscription_id_t _id);

    void send_subscribe_ack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                            remote_subscription_id_t _id);

    void update_subscription_state_and_notify(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                              uint16_t _error);

    void on_subscribe_nack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    void on_subscribe_ack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    [[nodiscard]] bool cache_event_payload(const std::shared_ptr<message>& _message);

    void send_back_cached_event_unlocked(service_t _service, instance_t _instance, event_t _event,
                                         std::scoped_lock<std::mutex> const& _consumer_lock);

    void send_back_cached_eventgroup_unlocked(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              std::scoped_lock<std::mutex> const& _consumer_lock);

    void on_stop_offer_service(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor,
                               bool _was_available, std::scoped_lock<std::mutex> const& _consumer_lock);

    [[nodiscard]] bool send_pending_commands(std::scoped_lock<std::mutex, std::mutex> const& _consumer_provider_lock);

    void init_receiver_side([[maybe_unused]] std::unique_lock<std::mutex> const& _receive_lock);

    void notify_remote_initially(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                 std::scoped_lock<std::mutex> const& _lock);

    uint32_t get_remote_subscriber_count(service_t _service, instance_t _instance, eventgroup_t _eventgroup, bool _increment,
                                         std::scoped_lock<std::mutex> const& _lock);
    void clear_remote_subscriber_count(service_t _service, instance_t _instance, std::scoped_lock<std::mutex> const& _lock);

    bool create_placeholder_event_and_subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _notifier,
                                                const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client,
                                                std::scoped_lock<std::mutex> const& _lock);

    void request_debounce_timeout_cbk(boost::system::error_code const& _error);

    bool send_request_services(std::span<protocol::service_data const> _requests);

    void resend_provided_event_registrations();
    void log_status();
    void log_version();
#ifndef VSOMEIP_DISABLE_SECURITY
    void on_update_security_credentials(std::vector<std::pair<uid_t, gid_t>> const& _credentials);
#endif
    void on_client_assign_ack(const client_t& _client, bool _is_tcp);

    port_t get_routing_port();

    void on_suspend();

    void try_to_send_before_stop();

    /**
     * @brief Remove all remote subscriptions.
     *
     * Currently used to clean up all remote subscriptions to services offered by this client.
     * This action is performed when SIGUSR1 is handled by host or when the client detects the
     * connections towards host has somehow become broken.
     */
    void clear_remote_subscriptions(std::scoped_lock<std::mutex> const& _provider_lock);

    void restart_sender(std::unique_lock<std::mutex> const& _sender_mutex);
    void debounce_restart_sender_done();

    /// @brief Provider-side cleanup for a failing/closing accepted local server endpoint.
    ///
    /// Drops the remote subscribers of the services WE offer to @p _client and
    /// removes the accepted provider endpoint. Leaves all consumer-side state
    /// (available services, outbound consumer endpoint, re-request) untouched.
    ///
    /// @param _client what client
    /// @param _due_to_error, true in case of error
    void remove_local_provider(client_t _client, bool _due_to_error);

    /// @brief Consumer-side cleanup for a failing/closing outbound consumer endpoint.
    ///
    /// Marks the services @p _client offered to us as unavailable and closes our
    /// outbound consumer endpoint to it. Leaves all provider-side state (the
    /// peer's subscriptions to our services, our accepted server endpoint)
    /// untouched.
    ///
    /// @param _client what client
    /// @param _due_to_error, true in case of error
    /// @param _requested_services what services were requested by us and offered by client;
    void remove_local_consumer(client_t _client, bool _due_to_error, local_service_table& _requested_services);

    void cleanup_consumer();
    void cleanup_subscriber(std::scoped_lock<std::mutex> const& _provider_lock);

    client_t find_local_client(service_t _service, instance_t _instance) const;
    bool send_event(client_t _client, std::shared_ptr<message> _message, bool _force) override;
    void remove_consumer_eventgroup_info(service_t _service, instance_t _instance, eventgroup_t _eventgroup);
    /**
     * @brief insert subscription into events/eventgroups
     *
     * Caller *MUST* hold `provider_mutex_` through not only call, but also during the subscription ack/nack and initial events
     */
    bool insert_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                             const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client,
                             std::scoped_lock<std::mutex> const& _lock);

    std::set<std::tuple<service_t, instance_t, eventgroup_t>> get_subscriptions(const client_t _client,
                                                                                std::scoped_lock<std::mutex> const& _provider_lock) const;
    bool is_subscribe_to_any_event_allowed(const vsomeip_sec_client_t* _sec_client, client_t _client, service_t _service,
                                           instance_t _instance, eventgroup_t _eventgroup, bool _is_provided);
    void stop_offer_service_base(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor,
                                 std::scoped_lock<std::mutex> const& _lock);

    void register_provider_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                 const std::set<eventgroup_t>& _eventgroups, const event_type_e _type, reliability_type_e _reliability,
                                 std::chrono::milliseconds _cycle, bool _change_resets_cycle, bool _update_on_change,
                                 epsilon_change_func_t _epsilon_change_func, bool _is_cache_placeholder,
                                 std::scoped_lock<std::mutex> const& _lock);
    void register_consumer_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                 const std::set<eventgroup_t>& _eventgroups, const event_type_e _type, reliability_type_e _reliability,
                                 std::chrono::milliseconds _cycle, bool _change_resets_cycle, bool _update_on_change,
                                 epsilon_change_func_t _epsilon_change_func, bool _is_cache_placeholder,
                                 std::scoped_lock<std::mutex> const& _lock);

    // event_dispatcher iface
    session_t get_event_session() override;

    bool send_event_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target, std::shared_ptr<message> _message);

    // routing_host
    client_t get_client() const override;
    void on_message(const byte_t* _data, length_t _length, const local_client_data& _peer_data) override;
    void lazy_load(const std::string& _client_host) override;

    void collect_pending_subscriptions(service_t _service, instance_t _instance, major_version_t _major,
                                       std::vector<subscription_data_t>& _collected_subscriptions, std::scoped_lock<std::mutex> const&);

    // Eventgroups
    using eventgroups_t = service_instance_map<std::unordered_map<eventgroup_t, std::shared_ptr<eventgroupinfo>>>;
    std::shared_ptr<eventgroupinfo> find_consumer_eventgroup(service_t _service, instance_t _instance, eventgroup_t _eventgroup) const;
    std::shared_ptr<eventgroupinfo> find_consumer_eventgroup(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                             std::scoped_lock<std::mutex> const&) const;

    std::set<std::shared_ptr<event>> find_provided_events_by_group(service_t _service, instance_t _instance, eventgroup_t _group,
                                                                   std::scoped_lock<std::mutex> const& _provider_lock) const;

    void finish_shutdown();

    std::shared_ptr<local_endpoint> find_or_create_consumer_ep(client_t _client);
    std::shared_ptr<local_endpoint> find_consumer_ep(client_t _client);

    void remove_consumer(client_t _client, bool _due_to_error, std::scoped_lock<std::mutex> const& _consumer_lock);

    async::hook flush_consumer();

private:
    routing_manager_host* host_;
    boost::asio::io_context& io_;

    std::shared_ptr<configuration> configuration_;

    const std::string env_;

    std::shared_ptr<trace::connector_impl> tc_;

    std::shared_ptr<timer> status_logger_;
    std::shared_ptr<timer> version_logger_;

    mutable std::mutex sender_mutex_;
    bool sender_debounce_active_{false};
    bool start_sender_after_debounce_{false};
    std::shared_ptr<local_endpoint> sender_; // --> stub

    mutable std::mutex receiver_mutex_;
    std::shared_ptr<local_server> tcp_receiver_; // --> from everybody
    std::shared_ptr<local_server> uds_receiver_; // --> from everybody

    const bool client_side_logging_;
    const std::set<std::tuple<service_t, instance_t>> client_side_logging_filter_;

    routing_mode_e const routing_mode_;

    std::unique_ptr<routing_client_state_machine> state_machine_;

    std::mutex lazy_load_mtx_;

    std::shared_ptr<timer> sender_debounce_;

    std::shared_ptr<endpoint_manager_base> ep_mgr_;

    // This mutex should be used whenever the client
    // is trying to accessing data relevant for its
    // "provider" side (offering of events, pending_offers, offered services etc.)
    mutable std::mutex provider_mutex_;
    // Set of services provided by this client
    service_instance_map<std::unordered_map<event_t, std::shared_ptr<event>>> provided_events_;
    service_instance_map<std::map<eventgroup_t, uint32_t>> remote_subscriber_count_;
    local_service_table offered_services_;
    // Event registrations offered by this client, awaiting (re)send to the routing manager.
    std::vector<protocol::register_event_data> pending_provided_event_registrations_;
    // lc_count is bumped on every rmc::stop and on any reconnect invocation,
    // protected by the provider_mutex_, but it may be read during a start of the
    // sender at an arbitrary moment in time - although it shouldn't.
    // These reads do not require synchronization with the subscription set,
    // nor with the stopping - this needs to be guaranteed by the rmc book-keeping itself.
    std::atomic<uint32_t> lc_count_{0};

    // This mutex should be used whenever the client
    // is trying to access data relevant for its "consumer" side
    mutable std::mutex consumer_mutex_;

    struct consumer_data {
        std::shared_ptr<local_endpoint> ep_;
        boost::asio::ip::address address_;
        port_t port_;
    };
    std::unordered_map<client_t, consumer_data> consumer_;

    bool request_debounce_timer_running_;
    boost::asio::steady_timer request_debounce_timer_;

    local_service_table requests_;
    local_service_table requests_to_debounce_;
    local_offering_table available_services_;

    struct consumed_event_entry_t {
        std::shared_ptr<event> event_;

        // Per-eventgroup subscription bookkeeping. An entry exists for an eventgroup if and only if the consumer is
        // subscribing/subscribed to that eventgroup for this event; `state_` tracks the SUBSCRIBE/ACK/NACK
        // lifecycle and `initial_notification_received_` records whether the initial (field) value was already
        // delivered for that eventgroup (used to replay the cached value on a re-subscribe).
        struct subscription_t {
            subscription_state_e state_{subscription_state_e::IS_SUBSCRIBING};
            bool initial_notification_received_{false};
        };
        std::map<eventgroup_t, subscription_t> subscriptions_;
    };

    service_instance_map<std::unordered_map<event_t, consumed_event_entry_t>> consumed_events_;
    eventgroups_t consumed_eventgroups_;

    struct subscription_data_t {
        service_instance_t service_instance_;
        eventgroup_t eventgroup_;
        major_version_t major_;
        event_t event_;
        std::shared_ptr<debounce_filter_impl_t> filter_;

        bool operator<(const subscription_data_t& _other) const {
            return std::tie(service_instance_, eventgroup_, event_) < std::tie(_other.service_instance_, _other.eventgroup_, _other.event_);
        }
    };
    std::set<subscription_data_t> pending_subscriptions_;
    // Event registrations consumed (subscribed) by this client, awaiting (re)send to the routing manager.
    std::vector<protocol::register_event_data> pending_consumed_event_registrations_;

    async::trigger on_sender_stopped_;
    async::trigger on_consumer_flushed_;
};

} // namespace vsomeip_v3

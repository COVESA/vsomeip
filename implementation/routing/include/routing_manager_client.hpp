// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <mutex>
#include <atomic>
#include <tuple>

#include <boost/asio/steady_timer.hpp>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/handler.hpp>

#include "routing_manager_base.hpp"
#include "types.hpp"
#include "../../protocol/include/protocol.hpp"
#include "../../endpoints/include/local_endpoint_manager_host.hpp"

namespace vsomeip_v3 {

class configuration;
class event;
class timer;
class local_server;
class routing_manager_host;
class routing_client_state_machine;

namespace protocol {
class offered_services_response_command;
class update_security_credentials_command;
}

class routing_manager_client : public routing_manager_base,
                               public local_endpoint_manager_host,
                               public std::enable_shared_from_this<routing_manager_client> {
public:
    routing_manager_client(routing_manager_host* _host, bool _client_side_logging,
                           const std::set<std::tuple<service_t, instance_t>>& _client_side_logging_filter);
    virtual ~routing_manager_client();

    void init();
    void start();
    void stop();

    std::shared_ptr<configuration> get_configuration() const;

    void start_keepalive();
    void check_keepalive();
    void cancel_keepalive();
    void ping_host();
    void on_pong(client_t _client);

    bool offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    void stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    void request_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    void release_service(client_t _client, service_t _service, instance_t _instance);

    void subscribe(client_t _client, const vsomeip_sec_client_t* _sec_client, service_t _service, instance_t _instance,
                   eventgroup_t _eventgroup, major_version_t _major, event_t _event,
                   const std::shared_ptr<debounce_filter_impl_t>& _filter);

    void unsubscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);
    void unsubscribe_base(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    bool send(client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance, bool _reliable, client_t _bound_client,
              const vsomeip_sec_client_t* _sec_client, uint8_t _status_check, bool _sent_from_remote, bool _force);

    bool send_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target, std::shared_ptr<message> _message);

    bool send_to(const std::shared_ptr<endpoint_definition>& _target, const byte_t* _data, uint32_t _size, instance_t _instance);

    void register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                        const std::set<eventgroup_t>& _eventgroups, const event_type_e _type, reliability_type_e _reliability,
                        std::chrono::milliseconds _cycle, bool _change_resets_cycle, bool _update_on_change,
                        epsilon_change_func_t _epsilon_change_func, bool _is_provided);

    void unregister_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier, bool _is_provided);
    void unregister_event_base(client_t _client, service_t _service, instance_t _instance, event_t _event, bool _is_provided);

    void on_message(const byte_t* _data, length_t _length, const local_client_data& _peer_data) override;

    void on_routing_info(const byte_t* _data, uint32_t _size);

    void register_client_error_handler(client_t _client, const std::shared_ptr<local_endpoint>& _endpoint);
    void cleanup_client(client_t _client);

    // local_endpoint_manager_host
    client_t get_client_id() override;
    void set_port(port_t _port) override;
    bool get_connection_param(client_t _client, boost::asio::ip::address& _address, port_t& _port) override;
    void add_connection_param(client_t _client, boost::asio::ip::address const& _address, port_t const& _port) override;
    void register_error_handler(client_t _client, std::shared_ptr<local_endpoint> _ep) override;

    void on_offered_services_info(protocol::offered_services_response_command& _command);

    void send_get_offered_services_info(client_t _client, offer_type_e _offer_type);
    bool send(client_t _client, std::shared_ptr<message> _message, bool _force);
    bool is_available(service_t _service, instance_t _instance, major_version_t _major) const;

    // that this function is provided to the application_impl feels pretty strange
    std::shared_ptr<serviceinfo> find_service(service_t _service, instance_t _instance) const override;
    std::set<std::shared_ptr<event>> find_events(service_t _service, instance_t _instance, eventgroup_t _eventgroup) const;
    void notify_one(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload, client_t _client,
                    bool _force);
    void notify(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload, bool _force);
    /**
     * @brief Notify current value for event/eventgroup
     *
     * Caller *MUST* hold `subscription_mutex` through not only call, but also during the subscription insertion + subscription ack/nack
     */
    void notify_one_current_value(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);
    std::shared_ptr<event> find_event(service_t _service, instance_t _instance, event_t _event) const;
    std::shared_ptr<eventgroupinfo> find_eventgroup(service_t _service, instance_t _instance, eventgroup_t _eventgroup) const;

private:
    void send_pending_subscriptions(service_t _service, instance_t _instance, major_version_t _major);
    void remove_pending_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    client_t get_client_by_address(const boost::asio::ip::address& _address, port_t _port) const;

    [[nodiscard]] bool is_local_client(client_t _client) const override;

    void register_application(client_t _client);

    void reconnect();

    void send_pong() const;

    bool send_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    void send_release_service(client_t _client, service_t _service, instance_t _instance);

    [[nodiscard]] bool send_pending_event_registrations(client_t _client);

    void send_register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                             const std::set<eventgroup_t>& _eventgroups, const event_type_e _type, reliability_type_e _reliability,
                             bool _is_provided, bool _is_cyclic);

    void send_subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
                        event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter);

    void send_subscribe_nack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                             remote_subscription_id_t _id);

    void send_subscribe_ack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                            remote_subscription_id_t _id);

    bool send_local_notification(client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance, bool _reliable,
                                 uint8_t _status_check, bool _force);

    bool is_field(service_t _service, instance_t _instance, event_t _event) const;

    void on_subscribe_nack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    void on_subscribe_ack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    void cache_event_payload(const std::shared_ptr<message>& _message);

    void on_stop_offer_service(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    [[nodiscard]] bool send_pending_commands();

    void init_receiver_side([[maybe_unused]] std::unique_lock<std::mutex> const& _receive_lock);

    void notify_remote_initially(service_t _service, instance_t _instance, eventgroup_t _eventgroup);

    uint32_t get_remote_subscriber_count(service_t _service, instance_t _instance, eventgroup_t _eventgroup, bool _increment);
    void clear_remote_subscriber_count(service_t _service, instance_t _instance);

    bool create_placeholder_event_and_subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _notifier,
                                                const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client);

    void request_debounce_timeout_cbk(boost::system::error_code const& _error);

    bool send_request_services(const std::set<protocol::service>& _requests);

    void send_unsubscribe_ack(service_t _service, instance_t _instance, eventgroup_t _eventgroup, remote_subscription_id_t _id);

    void resend_provided_event_registrations();
    void send_resend_provided_event_response(pending_remote_offer_id_t _id);
    void status_log_timer_cbk(boost::system::error_code const& _error);
    void version_log_timer_cbk(boost::system::error_code const& _error);
#ifndef VSOMEIP_DISABLE_SECURITY
    void send_update_security_policy_response(pending_security_update_id_t _update_id);
    void send_remove_security_policy_response(pending_security_update_id_t _update_id);
    void on_update_security_credentials(const protocol::update_security_credentials_command& _command);
#endif
    void on_client_assign_ack(const client_t& _client);

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
    void clear_remote_subscriptions();

    void restart_sender(std::unique_lock<std::recursive_mutex> const& _sender_mutex);
    void debounce_restart_sender_done();

    void lazy_load(const std::string& _client_host) override;

    /// @brief Remove local client
    ///
    /// This will remove all information about local client, its' offered services, and also close the client endpoint to it
    ///
    /// @param _client what client
    /// @param _subscribed_eventgroups what eventgroups to unsubscribe to
    /// @param _requested_services what services were requested by us and offered by client; will be filled if not nullptr
    void remove_local(client_t _client, const std::set<std::tuple<service_t, instance_t, eventgroup_t>>& _subscribed_eventgroups,
                      std::set<protocol::service>* _requested_services);

    void cleanup_routing_data();
    void cleanup_subscriber();

    client_t find_local_client(service_t _service, instance_t _instance) const;
    bool is_response_allowed(client_t _sender, service_t _service, instance_t _instance, method_t _method);
    bool send_event(client_t _client, std::shared_ptr<message> _message, bool _force) override;
    void remove_eventgroup_info(service_t _service, instance_t _instance, eventgroup_t _eventgroup);
    std::vector<event_t> find_events(service_t _service, instance_t _instance) const;
    /**
     * @brief insert subscription into events/eventgroups
     *
     * Caller *MUST* hold `subscription_mutex` through not only call, but also during the subscription ack/nack and initial events
     */
    bool insert_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                             const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client);

    std::set<std::tuple<service_t, instance_t, eventgroup_t>> get_subscriptions(const client_t _client);
    bool is_subscribe_to_any_event_allowed(const vsomeip_sec_client_t* _sec_client, client_t _client, service_t _service,
                                           instance_t _instance, eventgroup_t _eventgroup);
    void stop_offer_service_base(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                 minor_version_t _minor);

    void clear_service_info(service_t _service, instance_t _instance);
    std::shared_ptr<serviceinfo> find_service(service_t _service, instance_t _instance, std::scoped_lock<std::mutex> const&) const;
    void register_event_base(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                             const std::set<eventgroup_t>& _eventgroups, const event_type_e _type, reliability_type_e _reliability,
                             std::chrono::milliseconds _cycle, bool _change_resets_cycle, bool _update_on_change,
                             epsilon_change_func_t _epsilon_change_func, bool _is_provided, bool _is_cache_placeholder);

private:
    boost::asio::steady_timer keepalive_timer_;
    std::mutex log_timer_mutex_;
    boost::asio::steady_timer status_log_timer_;
    boost::asio::steady_timer version_log_timer_;
    bool keepalive_active_;
    bool keepalive_is_alive_;
    std::mutex keepalive_mutex_;

    mutable std::recursive_mutex sender_mutex_;
    bool sender_debounce_active_{false};
    bool start_sender_after_debounce_{false};
    std::shared_ptr<local_endpoint> sender_; // --> stub

    mutable std::mutex receiver_mutex_;
    std::shared_ptr<local_server> tcp_receiver_; // --> from everybody
    std::shared_ptr<local_server> uds_receiver_; // --> from everybody

    std::mutex pending_offers_mutex_;
    std::set<protocol::service> pending_offers_;
    std::mutex requests_mutex_;
    std::set<protocol::service> requests_;
    std::mutex requests_to_debounce_mutex_;
    std::set<protocol::service> requests_to_debounce_;

    struct event_data_t {
        service_t service_;
        instance_t instance_;
        event_t notifier_;
        event_type_e type_;
        reliability_type_e reliability_;
        bool is_provided_;
        bool is_cyclic_;
        std::set<eventgroup_t> eventgroups_;

        bool operator<(const event_data_t& _other) const {
            return std::tie(service_, instance_, notifier_, type_, reliability_, is_provided_, is_cyclic_, eventgroups_)
                    < std::tie(_other.service_, _other.instance_, _other.notifier_, _other.type_, _other.reliability_, _other.is_provided_,
                               _other.is_cyclic_, _other.eventgroups_);
        }
    };
    std::mutex pending_event_registrations_mutex_;
    std::set<event_data_t> pending_event_registrations_;

    std::mutex remote_subscriber_count_mutex_;
    std::map<service_t, std::map<instance_t, std::map<eventgroup_t, uint32_t>>> remote_subscriber_count_;

    boost::asio::steady_timer request_debounce_timer_;
    std::atomic<bool> request_debounce_timer_running_;

    const bool client_side_logging_;
    const std::set<std::tuple<service_t, instance_t>> client_side_logging_filter_;

    std::shared_ptr<routing_client_state_machine> state_machine_;

    mutable std::mutex available_services_mutex_;
    std::map<client_t, std::pair<boost::asio::ip::address, port_t>> address_table_;
    local_service_table available_services_;
    std::map<service_t, std::map<instance_t, std::set<client_t>>> available_services_history_;

    struct subscription_data_t {
        service_t service_;
        instance_t instance_;
        eventgroup_t eventgroup_;
        major_version_t major_;
        event_t event_;
        std::shared_ptr<debounce_filter_impl_t> filter_;
        vsomeip_sec_client_t sec_client_;

        bool operator<(const subscription_data_t& _other) const {
            return (service_ < _other.service_ || (service_ == _other.service_ && instance_ < _other.instance_)
                    || (service_ == _other.service_ && instance_ == _other.instance_ && eventgroup_ < _other.eventgroup_)
                    || (service_ == _other.service_ && instance_ == _other.instance_ && eventgroup_ == _other.eventgroup_
                        && event_ < _other.event_));
        }
    };
    std::set<subscription_data_t> pending_subscriptions_;
    std::mutex pending_subscription_mutex_;

    // Eventgroups
    mutable std::mutex eventgroups_mutex_;
    using eventgroups_t = service_instance_map<std::unordered_map<eventgroup_t, std::shared_ptr<eventgroupinfo>>>;
    eventgroups_t eventgroups_;

    // Events (part of one or more eventgroups)
    mutable std::mutex events_mutex_;
    service_instance_map<std::unordered_map<event_t, std::shared_ptr<event>>> events_;

    // covers subscriptions (e.g., state in `insert_subscription`) and anything that uses subscription state, namely `notify`
    std::mutex subscription_mutex;

    std::mutex event_registration_mutex_;

    std::mutex stop_mutex_;
    std::mutex lazy_load_mtx_;

    std::shared_ptr<timer> sender_debounce_;

    std::shared_ptr<endpoint_manager_base> ep_mgr_;

    bool const is_uds_preferred_;
    bool const is_local_routing_;

    // This mutex should be used whenever the client
    // is trying to accessing data relevant for its
    // "provider" side (offering of events, pending_offers, offered services etc.)
    mutable std::mutex provider_mutex_;
    // Set of services provided by this client
    services_t provided_services_;
};

} // namespace vsomeip_v3

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <mutex>
#include <unordered_set>
#include <queue>
#include <condition_variable>

#include <vsomeip/constants.hpp>
#include <vsomeip/vsomeip_sec.h>

#include "types.hpp"
#include "event.hpp"
#include "serviceinfo.hpp"
#include "routing_host.hpp"
#include "eventgroupinfo.hpp"
#include "event_dispatcher.hpp"
#include "routing_manager_host.hpp"

#include "../../message/include/serializer.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../protocol/include/protocol.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/debounce_filter_impl.hpp"
#include "../../endpoints/include/endpoint_manager_base.hpp"

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif
#include "../../utility/include/service_instance_map.hpp"

namespace vsomeip_v3 {

namespace trace {
class connector_impl;
} // namespace trace

class serializer;
class local_endpoint;

class routing_manager_base : public event_dispatcher, public routing_host {

public:
    routing_manager_base(routing_manager_host* _host);
    virtual ~routing_manager_base() = default;

    virtual boost::asio::io_context& get_io();
    virtual client_t get_client() const;

    virtual std::string get_client_host() const;
    virtual void set_client_host(const std::string& _client_host);
    virtual void set_client(const client_t& _client);
    virtual session_t get_session(bool _is_request);

    virtual vsomeip_sec_client_t get_sec_client() const;
    virtual void set_sec_client_port(port_t _port);

    virtual bool is_routing_manager() const;

    virtual void init() = 0;
    void init(const std::shared_ptr<endpoint_manager_base>& _endpoint_manager);

    virtual bool offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor);

    virtual void stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                    minor_version_t _minor);

    virtual void request_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                 minor_version_t _minor);

    virtual void release_service(client_t _client, service_t _service, instance_t _instance);

    virtual void register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                const std::set<eventgroup_t>& _eventgroups, const event_type_e _type, reliability_type_e _reliability,
                                std::chrono::milliseconds _cycle, bool _change_resets_cycle, bool _update_on_change,
                                epsilon_change_func_t _epsilon_change_func, bool _is_provided, bool _is_shadow = false,
                                bool _is_cache_placeholder = false);

    virtual void unregister_event(client_t _client, service_t _service, instance_t _instance, event_t _event, bool _is_provided);

    virtual std::set<std::shared_ptr<event>> find_events(service_t _service, instance_t _instance, eventgroup_t _eventgroup) const;

    virtual void subscribe(client_t _client, const vsomeip_sec_client_t* _sec_client, service_t _service, instance_t _instance,
                           eventgroup_t _eventgroup, major_version_t _major, event_t _event,
                           const std::shared_ptr<debounce_filter_impl_t>& _filter) = 0;

    virtual void unsubscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    virtual void notify(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload, bool _force);

    virtual void notify_one(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload, client_t _client,
                            bool _force);

    virtual bool send(client_t _client, std::shared_ptr<message> _message, bool _force);

    virtual bool send(client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance, bool _reliable,
                      client_t _bound_client = VSOMEIP_ROUTING_CLIENT, const vsomeip_sec_client_t* _sec_client = nullptr,
                      uint8_t _status_check = 0, bool _sent_from_remote = false, bool _force = true) = 0;

    virtual void register_client_error_handler(client_t _client, const std::shared_ptr<local_endpoint>& _endpoint) = 0;

    virtual void send_get_offered_services_info(client_t _client, offer_type_e _offer_type) = 0;

    std::set<client_t> find_local_clients(service_t _service, instance_t _instance);

    std::shared_ptr<serviceinfo> find_service(service_t _service, instance_t _instance) const;

    client_t find_local_client(service_t _service, instance_t _instance) const;
    client_t find_local_client_unlocked(service_t _service, instance_t _instance) const;

    std::shared_ptr<event> find_event(service_t _service, instance_t _instance, event_t _event) const;

    // address data for vsomeip routing via TCP
    bool get_guest(client_t _client, boost::asio::ip::address& _address, port_t& _port) const;
    client_t get_guest_by_address(const boost::asio::ip::address& _address, port_t _port) const;
    void add_guest(client_t _client, const boost::asio::ip::address& _address, port_t _port);
    void remove_guest(client_t _client);

    std::string const& get_name() const;

    // Default implementation - overridden by routing_manager_impl to delegate to stub
    virtual void on_register_application(client_t _client, const boost::asio::ip::address& _address, port_t _port);

    virtual bool send_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target,
                         std::shared_ptr<message> _message) = 0;

    virtual bool send_to(const std::shared_ptr<endpoint_definition>& _target, const byte_t* _data, uint32_t _size,
                         instance_t _instance) = 0;

protected:
    [[nodiscard]] virtual bool is_local_client(client_t _client) const = 0;
    /**
     * \brief Log network state
     *
     * Uses /proc/net/tcp + /proc/net/udp to log network connections. This is, of course, only for IPv4
     *
     * \param _tcp if true, logs TCP connections, otherwise UDP connections
     * \param _only_external if true, logs *only* external connections (e.g., service-discovery), otherwise everything
     */
    void log_network_state(bool _tcp, bool _only_external) const;

    std::shared_ptr<serviceinfo> create_service_info(service_t _service, instance_t _instance, major_version_t _major,
                                                     minor_version_t _minor, ttl_t _ttl, bool _is_local_service);

    void clear_service_info(service_t _service, instance_t _instance, bool _reliable);
    services_t get_services() const;
    services_t get_services_remote() const;
    virtual bool is_available(service_t _service, instance_t _instance, major_version_t _major) const;

    std::set<std::shared_ptr<eventgroupinfo>> find_eventgroups(service_t _service, instance_t _instance) const;

    std::shared_ptr<eventgroupinfo> find_eventgroup(service_t _service, instance_t _instance, eventgroup_t _eventgroup) const;

    void remove_eventgroup_info(service_t _service, instance_t _instance, eventgroup_t _eventgroup);

    bool send_local_notification(client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance, bool _reliable,
                                 uint8_t _status_check, bool _force, std::shared_ptr<local_endpoint> _fallback = nullptr);

    bool send_local(std::shared_ptr<local_endpoint>& _target, client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance,
                    bool _reliable, protocol::id_e _command, uint8_t _status_check, client_t _sender) const;

    /**
     * @brief insert subscription into events/eventgroups
     *
     * Caller *MUST* hold `subscription_mutex` through not only call, but also during the subscription ack/nack and initial events
     */
    bool insert_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                             const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client);

    void clear_shadow_subscriptions(void);

    std::shared_ptr<serializer> get_serializer();
    void put_serializer(const std::shared_ptr<serializer>& _serializer);
    std::shared_ptr<deserializer> get_deserializer();
    void put_deserializer(const std::shared_ptr<deserializer>& _deserializer);

    void send_pending_subscriptions(service_t _service, instance_t _instance, major_version_t _major);

    virtual void send_subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                major_version_t _major, event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter) = 0;

    void remove_pending_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    void unset_all_eventpayloads(service_t _service, instance_t _instance);
    void unset_all_eventpayloads(service_t _service, instance_t _instance, eventgroup_t _eventgroup);

    /**
     * @brief Notify current value for event/eventgroup
     *
     * Caller *MUST* hold `subscription_mutex` through not only call, but also during the subscription insertion + subscription ack/nack
     */
    void notify_one_current_value(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    std::set<std::tuple<service_t, instance_t, eventgroup_t>> get_subscriptions(const client_t _client);

    std::vector<event_t> find_events(service_t _service, instance_t _instance) const;

    bool is_response_allowed(client_t _sender, service_t _service, instance_t _instance, method_t _method);
    bool is_subscribe_to_any_event_allowed(const vsomeip_sec_client_t* _sec_client, client_t _client, service_t _service,
                                           instance_t _instance, eventgroup_t _eventgroup);

    // event_dispatcher iface
    session_t get_event_session() override;

    bool send_event(client_t _client, std::shared_ptr<message> _message, bool _force) override;

    bool send_event_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target,
                       std::shared_ptr<message> _message) override;

private:
    virtual bool create_placeholder_event_and_subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                                        const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client) = 0;

protected:
    routing_manager_host* host_;
    boost::asio::io_context& io_;

    std::shared_ptr<configuration> configuration_;

    std::queue<std::shared_ptr<serializer>> serializers_;
    std::mutex serializer_mutex_;
    std::condition_variable serializer_condition_;

    std::queue<std::shared_ptr<deserializer>> deserializers_;
    std::mutex deserializer_mutex_;
    std::condition_variable deserializer_condition_;

    mutable std::mutex local_services_mutex_;
    typedef std::map<service_t, std::map<instance_t, std::tuple<major_version_t, minor_version_t, client_t>>> local_services_map_t;
    local_services_map_t local_services_;
    std::map<service_t, std::map<instance_t, std::set<client_t>>> local_services_history_;

    // Eventgroups
    mutable std::mutex eventgroups_mutex_;
    using eventgroups_t = service_instance_map<std::unordered_map<eventgroup_t, std::shared_ptr<eventgroupinfo>>>;
    eventgroups_t eventgroups_;

    // Events (part of one or more eventgroups)
    mutable std::mutex events_mutex_;
    service_instance_map<std::unordered_map<event_t, std::shared_ptr<event>>> events_;

    std::mutex event_registration_mutex_;

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

    services_t services_remote_;
    mutable std::mutex services_remote_mutex_;

    std::shared_ptr<endpoint_manager_base> ep_mgr_;

    mutable std::mutex guests_mutex_;
    std::map<client_t, std::pair<boost::asio::ip::address, port_t>> guests_;

    mutable std::mutex env_mutex_;
    std::string env_;

    std::shared_ptr<trace::connector_impl> tc_;

    // covers subscriptions (e.g., state in `insert_subscription`) and anything that uses subscription state, namely `notify`
    std::mutex subscription_mutex;

private:
    services_t services_;
    mutable std::mutex services_mutex_;
};

} // namespace vsomeip_v3

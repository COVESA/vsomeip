// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_BASE
#define VSOMEIP_ROUTING_MANAGER_BASE

#include <mutex>
#include <unordered_set>
#include <queue>
#include <condition_variable>

#include <vsomeip/constants.hpp>
#include "routing_manager.hpp"
#include "routing_manager_host.hpp"
#include "types.hpp"
#include "serviceinfo.hpp"
#include "event.hpp"
#include "eventgroupinfo.hpp"
#include "../../message/include/serializer.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../endpoints/include/endpoint_host.hpp"

namespace vsomeip {
#ifdef USE_DLT
namespace trace {
class connector_impl;
} // namespace trace
#endif

class serializer;

class routing_manager_base : public routing_manager,
    public endpoint_host,
    public std::enable_shared_from_this<routing_manager_base>{

public:
    routing_manager_base(routing_manager_host *_host);
    virtual ~routing_manager_base();

    virtual boost::asio::io_service & get_io();
    virtual client_t get_client() const;

    virtual void init();

    virtual bool offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor);

    virtual void stop_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major, minor_version_t _minor);

    virtual void request_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, bool _use_exclusive_proxy);

    virtual void release_service(client_t _client, service_t _service,
            instance_t _instance);

    virtual void register_event(client_t _client, service_t _service, instance_t _instance,
            event_t _event, const std::set<eventgroup_t> &_eventgroups, bool _is_field,
            std::chrono::milliseconds _cycle, bool _change_resets_cycle,
            epsilon_change_func_t _epsilon_change_func,
            bool _is_provided, bool _is_shadow = false, bool _is_cache_placeholder = false);

    virtual void unregister_event(client_t _client, service_t _service, instance_t _instance,
            event_t _event, bool _is_provided);

    virtual std::set<std::shared_ptr<event>> find_events(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup) const;

    virtual void subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, event_t _event,
            subscription_type_e _subscription_type);

    virtual void unsubscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    virtual void notify(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload,
            bool _force, bool _flush);

    virtual void notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload,
            client_t _client, bool _force, bool _flush, bool _remote_subscriber);

    virtual bool send(client_t _client, std::shared_ptr<message> _message,
            bool _flush);

    virtual bool send(client_t _client, const byte_t *_data, uint32_t _size,
            instance_t _instance, bool _flush, bool _reliable,
            client_t _bound_client = VSOMEIP_ROUTING_CLIENT,
            bool _is_valid_crc = true, bool _sent_from_remote = false) = 0;

    // Endpoint host ~> will be implemented by routing_manager_impl/_proxy/
    virtual void on_connect(std::shared_ptr<endpoint> _endpoint) = 0;
    virtual void on_disconnect(std::shared_ptr<endpoint> _endpoint) = 0;
    virtual void on_message(const byte_t *_data, length_t _length,
        endpoint *_receiver, const boost::asio::ip::address &_destination
            = boost::asio::ip::address(), client_t _bound_client = VSOMEIP_ROUTING_CLIENT,
            const boost::asio::ip::address &_remote_address = boost::asio::ip::address(),
            std::uint16_t _remote_port = 0) = 0;
    virtual void on_error(const byte_t *_data, length_t _length,
                          endpoint *_receiver,
                          const boost::asio::ip::address &_remote_address,
                          std::uint16_t _remote_port) = 0;

#ifndef _WIN32
    virtual bool check_credentials(client_t _client, uid_t _uid, gid_t _gid);
#endif

    virtual void set_routing_state(routing_state_e _routing_state) = 0;

    virtual std::shared_ptr<event> find_event(service_t _service, instance_t _instance,
            event_t _event) const;

    virtual void register_client_error_handler(client_t _client,
            const std::shared_ptr<endpoint> &_endpoint) = 0;

    virtual void send_get_offered_services_info(client_t _client, offer_type_e _offer_type) = 0;

    std::set<client_t> find_local_clients(service_t _service, instance_t _instance);

    void erase_incoming_subscription_state(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event);

protected:
    std::shared_ptr<serviceinfo> find_service(service_t _service, instance_t _instance) const;
    std::shared_ptr<serviceinfo> create_service_info(service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl, bool _is_local_service);

    void clear_service_info(service_t _service, instance_t _instance, bool _reliable);
    services_t get_services() const;
    services_t get_services_remote() const;
    bool is_available(service_t _service, instance_t _instance, major_version_t _major);
    client_t find_local_client(service_t _service, instance_t _instance);

    std::shared_ptr<endpoint> create_local(client_t _client);
    std::shared_ptr<endpoint> find_or_create_local(client_t _client);
    void remove_local(client_t _client, bool _remove_uid);
    void remove_local(client_t _client,
                      const std::set<std::tuple<service_t, instance_t, eventgroup_t>>& _subscribed_eventgroups,
                      bool _remove_uid);

    std::shared_ptr<endpoint> find_local(client_t _client);
    std::shared_ptr<endpoint> find_local(service_t _service,
            instance_t _instance);

    std::unordered_set<client_t> get_connected_clients();

    std::shared_ptr<eventgroupinfo> find_eventgroup(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) const;

    void remove_eventgroup_info(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);

    bool send_local_notification(client_t _client,
            const byte_t *_data, uint32_t _size, instance_t _instance,
            bool _flush = true, bool _reliable = false, bool _is_valid_crc = true);

    bool send_local(
            std::shared_ptr<endpoint> &_target, client_t _client,
            const byte_t *_data, uint32_t _size, instance_t _instance,
            bool _flush, bool _reliable, uint8_t _command, bool _is_valid_crc = true) const;

    bool insert_subscription(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event, client_t _client,
            std::set<event_t> *_already_subscribed_events);

    std::shared_ptr<deserializer> get_deserializer();
    void put_deserializer(std::shared_ptr<deserializer>);

    void send_pending_subscriptions(service_t _service,
            instance_t _instance, major_version_t _major);

    virtual void send_subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, event_t _event,
            subscription_type_e _subscription_type) = 0;

    void remove_pending_subscription(service_t _service, instance_t _instance,
                                     eventgroup_t _eventgroup, event_t _event);

    void send_pending_notify_ones(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, client_t _client, bool _remote_subscriber = false);

    void unset_all_eventpayloads(service_t _service, instance_t _instance);
    void unset_all_eventpayloads(service_t _service, instance_t _instance,
                                 eventgroup_t _eventgroup);

    void notify_one_current_value(client_t _client, service_t _service,
                                  instance_t _instance,
                                  eventgroup_t _eventgroup, event_t _event,
                                  const std::set<event_t> &_events_to_exclude);

    void send_identify_request(service_t _service, instance_t _instance,
            major_version_t _major, bool _reliable);

    std::map<client_t, std::shared_ptr<endpoint>> get_local_endpoints();

    std::set<std::tuple<service_t, instance_t, eventgroup_t>>
        get_subscriptions(const client_t _client);

    std::vector<event_t> find_events(service_t _service, instance_t _instance) const;

    bool is_response_allowed(client_t _sender, service_t _service,
            instance_t _instance, method_t _method);
    bool is_subscribe_to_any_event_allowed(client_t _client,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup);

    void set_incoming_subscription_state(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event, subscription_state_e _state);

    subscription_state_e get_incoming_subscription_state(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event);


private:
    std::shared_ptr<endpoint> create_local_unlocked(client_t _client);
    std::shared_ptr<endpoint> find_local_unlocked(client_t _client);


    virtual bool create_placeholder_event_and_subscribe(
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            event_t _event, client_t _client) = 0;

protected:
    routing_manager_host *host_;
    boost::asio::io_service &io_;
    client_t client_;

    std::shared_ptr<configuration> configuration_;
    std::shared_ptr<serializer> serializer_;
    std::mutex serialize_mutex_;

    std::queue<std::shared_ptr<deserializer>> deserializers_;
    std::mutex deserializer_mutex_;
    std::condition_variable deserializer_condition_;

    std::mutex local_services_mutex_;
    std::map<service_t, std::map<instance_t, std::tuple< major_version_t, minor_version_t, client_t> > > local_services_;
    std::map<service_t, std::map<instance_t, std::set<client_t> > > local_services_history_;

    // Eventgroups
    mutable std::mutex eventgroups_mutex_;
    std::map<service_t,
            std::map<instance_t,
                    std::map<eventgroup_t, std::shared_ptr<eventgroupinfo> > > > eventgroups_;
    // Events (part of one or more eventgroups)
    mutable std::mutex events_mutex_;
    std::map<service_t,
            std::map<instance_t, std::map<event_t, std::shared_ptr<event> > > > events_;

#ifdef USE_DLT
    std::shared_ptr<trace::connector_impl> tc_;
#endif

    struct subscription_data_t {
        service_t service_;
        instance_t instance_;
        eventgroup_t eventgroup_;
        major_version_t major_;
        event_t event_;
        subscription_type_e subscription_type_;

        bool operator<(const subscription_data_t &_other) const {
            return (service_ < _other.service_
                    || (service_ == _other.service_
                        && instance_ < _other.instance_)
                        || (service_ == _other.service_
                            && instance_ == _other.instance_
                            && eventgroup_ < _other.eventgroup_)
                            || (service_ == _other.service_
                                && instance_ == _other.instance_
                                && eventgroup_ == _other.eventgroup_
                                && event_ < _other.event_));
        }
    };
    std::set<subscription_data_t> pending_subscriptions_;

    services_t services_remote_;
    mutable std::mutex services_remote_mutex_;

private:
    services_t services_;
    mutable std::mutex services_mutex_;

    std::map<client_t, std::shared_ptr<endpoint> > local_endpoints_;
    mutable std::mutex local_endpoint_mutex_;

    std::map<service_t,
        std::map<instance_t,
            std::map<eventgroup_t,
                std::shared_ptr<message> > > > pending_notify_ones_;
    std::recursive_mutex pending_notify_ones_mutex_;

    std::mutex event_registration_mutex_;

    std::map<client_t,
        std::map<service_t,
            std::map<instance_t,
                std::map<eventgroup_t,
                    std::map<event_t,
                        subscription_state_e> > > > > incoming_subscription_state_;
    std::recursive_mutex incoming_subscription_state_mutex_;

};

} // namespace vsomeip

#endif //VSOMEIP_ROUTING_MANAGER_BASE

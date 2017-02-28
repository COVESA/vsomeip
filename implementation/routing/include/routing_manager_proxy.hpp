// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_PROXY_HPP
#define VSOMEIP_ROUTING_MANAGER_PROXY_HPP

#include <map>
#include <mutex>
#include <atomic>

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include "routing_manager_base.hpp"
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/handler.hpp>

namespace vsomeip {

class configuration;
class event;
class routing_manager_host;

class logger;

class routing_manager_proxy: public routing_manager_base {
public:
    routing_manager_proxy(routing_manager_host *_host);
    virtual ~routing_manager_proxy();

    void init();
    void start();
    void stop();

    const std::shared_ptr<configuration> get_configuration() const;

    bool offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor);

    void stop_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major, minor_version_t _minor);

    void request_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, bool _use_exclusive_proxy);

    void release_service(client_t _client, service_t _service,
            instance_t _instance);

    void subscribe(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major,
            subscription_type_e _subscription_type);

    void unsubscribe(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);

    bool send(client_t _client, const byte_t *_data, uint32_t _size,
            instance_t _instance, bool _flush = true, bool _reliable = false);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            std::shared_ptr<message> _message, bool _flush);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            const byte_t *_data, uint32_t _size, bool _flush);

    void register_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups, bool _is_field,
            std::chrono::milliseconds _cycle, bool _change_resets_cycle,
            epsilon_change_func_t _epsilon_change_func,
            bool _is_provided, bool _is_shadow, bool _is_cache_placeholder);

    void unregister_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event,
            bool _is_provided);

    void on_connect(std::shared_ptr<endpoint> _endpoint);
    void on_disconnect(std::shared_ptr<endpoint> _endpoint);
    void on_message(const byte_t *_data, length_t _length, endpoint *_receiver,
            const boost::asio::ip::address &_destination,
            client_t _bound_client,
            const boost::asio::ip::address &_remote_address,
            std::uint16_t _remote_port);
    void on_error(const byte_t *_data, length_t _length, endpoint *_receiver,
                  const boost::asio::ip::address &_remote_address,
                  std::uint16_t _remote_port);
    void release_port(uint16_t _port, bool _reliable);

    void on_routing_info(const byte_t *_data, uint32_t _size);

    void on_identify_response(client_t _client, service_t _service, instance_t _instance,
            bool _reliable);

private:
    void register_application();
    void deregister_application();

    void send_pong() const;
    void send_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor);
    void send_request_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, bool _use_exclusive_proxy);
    void send_release_service(client_t _client, service_t _service,
            instance_t _instance);
    void send_register_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroup,
            bool _is_field, bool _is_provided);
    void send_subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, subscription_type_e _subscription_type);

    void send_subscribe_nack(client_t _subscriber, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

    void send_subscribe_ack(client_t _subscriber, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

    bool is_field(service_t _service, instance_t _instance,
            event_t _event) const;

    void on_subscribe_nack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

    void on_subscribe_ack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

    void cache_event_payload(const std::shared_ptr<message> &_message);

    void on_stop_offer_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor);

    void send_pending_commands();

    void init_receiver();

    void notify_remote_initally(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);

    uint32_t get_remote_subscriber_count(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, bool _increment);

    void register_application_timeout_cbk(boost::system::error_code const &_error);

    void send_registered_ack();

    void set_routing_state(routing_state_e _routing_state) {
        (void)_routing_state;
    };

    bool is_client_known(client_t _client);

private:
    enum class inner_state_type_e : std::uint8_t {
        ST_REGISTERED = 0x0,
        ST_DEREGISTERED = 0x1,
        ST_REGISTERING = 0x2
    };

    bool is_connected_;
    std::atomic<bool> is_started_;
    inner_state_type_e state_;

    std::shared_ptr<endpoint> sender_;  // --> stub
    std::shared_ptr<endpoint> receiver_;  // --> from everybody

    std::mutex known_clients_mutex_;
    std::unordered_set<client_t> known_clients_;

    struct service_data_t {
        service_t service_;
        instance_t instance_;
        major_version_t major_;
        minor_version_t minor_;
        bool use_exclusive_proxy_; // only used for requests!

        bool operator<(const service_data_t &_other) const {
            return (service_ < _other.service_
                    || (service_ == _other.service_
                        && instance_ < _other.instance_));
        }
    };
    std::set<service_data_t> pending_offers_;
    std::set<service_data_t> pending_requests_;

    struct event_data_t {
        service_t service_;
        instance_t instance_;
        event_t event_;
        bool is_field_;
        bool is_provided_;
        std::set<eventgroup_t> eventgroups_;

        bool operator<(const event_data_t &_other) const {
            if (service_ < _other.service_) {
                return true;
            }
            if (service_ == _other.service_ && instance_ < _other.instance_) {
                return true;
            }
            if (service_ == _other.service_ && instance_ == _other.instance_
                    && event_ < _other.event_) {
                return true;
            }
            if (service_ == _other.service_ && instance_ == _other.instance_
                    && event_ == _other.event_
                    && is_provided_ != _other.is_provided_) {
                return true;
            }
            if (service_ == _other.service_
                    && instance_ == _other.instance_
                    && event_ == _other.event_
                    && is_provided_ == _other.is_provided_
                    && is_field_ != _other.is_field_) {
                return true;
            }
            if (service_ == _other.service_
                    && instance_ == _other.instance_
                    && event_ == _other.event_
                    && is_provided_ == _other.is_provided_
                    && is_field_ == _other.is_field_
                    && eventgroups_ < _other.eventgroups_) {
                return true;
            }
            return false;
    }
    };
    std::set<event_data_t> pending_event_registrations_;

    std::map<client_t, std::set<eventgroup_data_t>> pending_ingoing_subscripitons_;
    std::mutex pending_ingoing_subscripitons_mutex_;

    std::mutex deserialize_mutex_;

    std::mutex state_mutex_;
    std::condition_variable state_condition_;

    std::map<service_t,
                std::map<instance_t, std::map<eventgroup_t, uint32_t > > > remote_subscriber_count_;

    mutable std::mutex sender_mutex_;

    boost::asio::steady_timer register_application_timer_;

    std::shared_ptr<logger> logger_;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_PROXY_HPP

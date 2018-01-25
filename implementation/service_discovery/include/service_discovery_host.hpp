// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_HOST_HPP
#define VSOMEIP_SERVICE_DISCOVERY_HOST_HPP

#include <map>
#include <memory>
#include <chrono>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_service.hpp>

#include "../../routing/include/types.hpp"

#include <vsomeip/message.hpp>

namespace vsomeip {

class configuration;
class endpoint;
class endpoint_definition;

namespace sd {

class service_discovery_host {
public:
    virtual ~service_discovery_host() {
    }

    virtual boost::asio::io_service & get_io() = 0;
    virtual const std::shared_ptr<configuration> get_configuration() const = 0;

    virtual std::shared_ptr<endpoint> create_service_discovery_endpoint(
            const std::string &_address, uint16_t _port, bool _reliable) = 0;

    virtual services_t get_offered_services() const = 0;
    virtual std::shared_ptr<eventgroupinfo> find_eventgroup(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) const = 0;

    virtual bool send(client_t _client, std::shared_ptr<message> _message,
            bool _flush) = 0;

    virtual bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            const byte_t *_data, uint32_t _size, uint16_t _sd_port) = 0;

    virtual void add_routing_info(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl,
            const boost::asio::ip::address &_reliable_address,
            uint16_t _reliable_port,
            const boost::asio::ip::address &_unreliable_address,
            uint16_t _unreliable_port) = 0;

    virtual void del_routing_info(service_t _service, instance_t _instance,
            bool _has_reliable, bool _has_unreliable) = 0;

    virtual void update_routing_info(std::chrono::milliseconds _elapsed) = 0;

    virtual void on_unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup,
            std::shared_ptr<endpoint_definition> _target) = 0;

    virtual void on_subscribe_ack(service_t _service, instance_t _instance,
            const boost::asio::ip::address &_address, uint16_t _port) = 0;

    virtual void on_subscribe_ack(client_t _client,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            event_t _event, pending_subscription_id_t _subscription_id) = 0;

    virtual std::shared_ptr<endpoint> find_or_create_remote_client(
            service_t _service, instance_t _instance,
            bool _reliable, client_t _client) = 0;

    virtual void expire_subscriptions(const boost::asio::ip::address &_address) = 0;
    virtual void expire_services(const boost::asio::ip::address &_address) = 0;

    virtual void on_remote_subscription(
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            const std::shared_ptr<endpoint_definition> &_subscriber,
            const std::shared_ptr<endpoint_definition> &_target, ttl_t _ttl,
            const std::shared_ptr<sd_message_identifier_t> &_sd_message_id,
            const std::function<void(remote_subscription_state_e, client_t)>& _callback) = 0;

    virtual void on_subscribe_nack(client_t _client,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            event_t _event, pending_subscription_id_t _subscription_id) = 0;

    virtual bool has_identified(client_t _client, service_t _service,
            instance_t _instance, bool _reliable) = 0;

    virtual std::chrono::steady_clock::time_point expire_subscriptions() = 0;

    virtual std::shared_ptr<serviceinfo> get_offered_service(
            service_t _service, instance_t _instance) const = 0;
    virtual std::map<instance_t, std::shared_ptr<serviceinfo>> get_offered_service_instances(
            service_t _service) const = 0;

    virtual void send_initial_events(service_t _service, instance_t _instance,
                eventgroup_t _eventgroup,
                const std::shared_ptr<endpoint_definition>& _subscriber) = 0;
};

}  // namespace sd
}  // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_HOST_HPP

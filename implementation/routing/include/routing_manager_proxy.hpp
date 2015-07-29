// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_PROXY_HPP
#define VSOMEIP_ROUTING_MANAGER_PROXY_HPP

#include <map>
#include <mutex>

#include <boost/asio/io_service.hpp>

#include "routing_manager.hpp"
#include "../../endpoints/include/endpoint_host.hpp"

namespace vsomeip {

class configuration;
class event;
class routing_manager_host;

// TODO: encapsulate common parts of classes "routing_manager_impl"
// and "routing_manager_proxy" into a base class.

class routing_manager_proxy: public routing_manager,
        public endpoint_host,
        public std::enable_shared_from_this<routing_manager_proxy> {
public:
    routing_manager_proxy(routing_manager_host *_host);
    virtual ~routing_manager_proxy();

    boost::asio::io_service & get_io();
    client_t get_client() const;

    void init();
    void start();
    void stop();

    void offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl);

    void stop_offer_service(client_t _client, service_t _service,
            instance_t _instance);

    void request_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl, bool _has_selective);

    void release_service(client_t _client, service_t _service,
            instance_t _instance);

    void subscribe(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl);

    void unsubscribe(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);

    bool send(client_t _client, std::shared_ptr<message> _message, bool _flush);

    bool send(client_t _client, const byte_t *_data, uint32_t _size,
            instance_t _instance, bool _flush = true, bool _reliable = false);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            std::shared_ptr<message> _message);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            const byte_t *_data, uint32_t _size);

    void notify(service_t _service, instance_t _instance, event_t _event,
            std::shared_ptr<payload> _payload);

    void notify_one(service_t _service, instance_t _instance,
                event_t _event, std::shared_ptr<payload> _payload, client_t _client);

    void on_connect(std::shared_ptr<endpoint> _endpoint);
    void on_disconnect(std::shared_ptr<endpoint> _endpoint);
    void on_message(const byte_t *_data, length_t _length, endpoint *_receiver);

    void on_routing_info(const byte_t *_data, uint32_t _size);

    std::shared_ptr<endpoint> find_local(client_t _client);
    std::shared_ptr<endpoint> find_local(service_t _service,
            instance_t _instance);
    std::shared_ptr<endpoint> find_or_create_local(client_t _client);
    void remove_local(client_t _client);

private:
    void register_application();
    void deregister_application();

    std::shared_ptr<endpoint> create_local(client_t _client);

    void send_pong() const;
    void send_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl);
    void send_subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, ttl_t _ttl);

    void send_request_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl, bool _is_selective);

private:
    boost::asio::io_service &io_;bool is_connected_;bool is_started_;
    event_type_e state_;
    routing_manager_host *host_;
    client_t client_; // store locally as it is needed in each message

    std::shared_ptr<serializer> serializer_;
    std::shared_ptr<deserializer> deserializer_;

    std::shared_ptr<endpoint> sender_;	// --> stub
    std::shared_ptr<endpoint> receiver_;  // --> from everybody

    std::map<client_t, std::shared_ptr<endpoint> > local_endpoints_;
    std::map<service_t, std::map<instance_t, client_t> > local_services_;

    struct service_data_t {
        service_t service_;
        instance_t instance_;
        major_version_t major_;
        minor_version_t minor_;
        ttl_t ttl_;

        bool operator<(const service_data_t &_other) const {
            return (service_ < _other.service_
                    || (service_ == _other.service_
                        && instance_ < _other.instance_));
        }
    };
    std::set<service_data_t> pending_offers_;
    std::set<service_data_t> pending_requests_;

    struct eventgroup_data_t {
        service_t service_;
        instance_t instance_;
        eventgroup_t eventgroup_;
        major_version_t major_;
        ttl_t ttl_;

        bool operator<(const eventgroup_data_t &_other) const {
            return (service_ < _other.service_
                    || (service_ == _other.service_
                        && instance_ < _other.instance_)
                        || (service_ == _other.service_
                            && instance_ == _other.instance_
                            && eventgroup_ < _other.eventgroup_));
        }
    };
    std::set<eventgroup_data_t> pending_subscriptions_;

    std::map<service_t,
        std::map<instance_t,
            std::map<event_t,
                std::shared_ptr<message> > > > pending_notifications_;

    std::mutex send_mutex_;
    std::mutex serialize_mutex_;
    std::mutex deserialize_mutex_;
    std::mutex pending_mutex_;

    bool is_selective_;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_PROXY_HPP

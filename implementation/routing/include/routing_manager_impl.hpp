// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_IMPL_HPP
#define VSOMEIP_ROUTING_MANAGER_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_set>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>

#include "routing_manager.hpp"
#include "routing_manager_stub_host.hpp"
#include "../../endpoints/include/endpoint_host.hpp"
#include "../../service_discovery/include/service_discovery_host.hpp"

namespace vsomeip {

class client_endpoint;
class configuration;
class deserializer;
class eventgroupinfo;
class routing_manager_host;
class routing_manager_stub;
class servicegroup;
class serviceinfo;
class serializer;
class service_endpoint;

namespace sd {

class service_discovery;

}  // namespace sd

// TODO: encapsulate common parts of classes "routing_manager_impl"
// and "routing_manager_proxy" into a base class.

class routing_manager_impl: public routing_manager,
        public endpoint_host,
        public routing_manager_stub_host,
        public sd::service_discovery_host,
        public std::enable_shared_from_this<routing_manager_impl> {
public:
    routing_manager_impl(routing_manager_host *_host);
    ~routing_manager_impl();

    boost::asio::io_service & get_io();
    client_t get_client() const;
    std::shared_ptr<configuration> get_configuration() const;

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
            instance_t _instance, bool _flush, bool _reliable);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            std::shared_ptr<message> _message);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            const byte_t *_data, uint32_t _size);

    void notify(service_t _service, instance_t _instance, event_t _event,
            std::shared_ptr<payload> _payload);

    void notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload, client_t _client);

    // interface to stub
    std::shared_ptr<endpoint> create_local(client_t _client);
    std::shared_ptr<endpoint> find_local(client_t _client);
    std::shared_ptr<endpoint> find_or_create_local(client_t _client);
    void remove_local(client_t _client);
    std::shared_ptr<endpoint> find_local(service_t _service,
            instance_t _instance);
    void on_stop_offer_service(service_t _service, instance_t _instance);

    // interface "endpoint_host"
    std::shared_ptr<endpoint> find_or_create_remote_client(service_t _service,
            instance_t _instance,
            bool _reliable, client_t _client);
    void on_connect(std::shared_ptr<endpoint> _endpoint);
    void on_disconnect(std::shared_ptr<endpoint> _endpoint);
    void on_message(const byte_t *_data, length_t _length, endpoint *_receiver);
    void on_message(service_t _service, instance_t _instance,
            const byte_t *_data, length_t _size, bool _reliable);

    // interface "service_discovery_host"
    typedef std::map<std::string, std::shared_ptr<servicegroup> > servicegroups_t;
    const servicegroups_t & get_servicegroups() const;
    std::shared_ptr<eventgroupinfo> find_eventgroup(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) const;
    services_t get_offered_services(const std::string &_name) const;
    void create_service_discovery_endpoint(const std::string &_address,
            uint16_t _port, bool _reliable);
    void init_routing_info();
    void add_routing_info(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl,
            const boost::asio::ip::address &_address, uint16_t _port,
            bool _reliable);
    void del_routing_info(service_t _service, instance_t _instance,
            bool _reliable);

    void on_subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup,
            std::shared_ptr<endpoint_definition> _subscriber,
            std::shared_ptr<endpoint_definition> _target);
    void on_unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup,
            std::shared_ptr<endpoint_definition> _target);
    void on_subscribe_ack(service_t _service, instance_t _instance,
            const boost::asio::ip::address &_address, uint16_t _port);

    void init_event_routing_info();

private:
    bool deliver_message(const byte_t *_data, length_t _length,
            instance_t _instance, bool _reliable);
    bool send_local(
            std::shared_ptr<endpoint> &_target, client_t _client,
            const byte_t *_data, uint32_t _size, instance_t _instance,
            bool _flush, bool _reliable) const;

    client_t find_local_client(service_t _service, instance_t _instance);
    std::set<client_t> find_local_clients(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);
    instance_t find_instance(service_t _service, endpoint *_endpoint);

    std::shared_ptr<serviceinfo> find_service(service_t _service,
            instance_t _instance) const;
    std::shared_ptr<serviceinfo> create_service(service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl);

    std::shared_ptr<endpoint> create_client_endpoint(
            const boost::asio::ip::address &_address, uint16_t _port,
            bool _reliable, client_t _client);
    std::shared_ptr<endpoint> find_client_endpoint(
            const boost::asio::ip::address &_address, uint16_t _port,
            bool _reliable, client_t _client);
    std::shared_ptr<endpoint> find_or_create_client_endpoint(
            const boost::asio::ip::address &_address, uint16_t _port,
            bool _reliable, client_t _client);

    std::shared_ptr<endpoint> create_server_endpoint(uint16_t _port,
    bool _reliable);
    std::shared_ptr<endpoint> find_server_endpoint(uint16_t _port,
    bool _reliable);
    std::shared_ptr<endpoint> find_or_create_server_endpoint(uint16_t _port,
    bool _reliable);

    std::set<std::shared_ptr<event> > find_events(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);
    std::shared_ptr<event> find_event(service_t _service, instance_t _instance,
            event_t _event) const;

    std::shared_ptr<endpoint> find_remote_client(service_t _service,
            instance_t _instance,
            bool _reliable, client_t _client);

    std::shared_ptr<endpoint> create_remote_client(service_t _service,
                instance_t _instance,
                bool _reliable, client_t _client);

private:
    void send_subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, ttl_t _ttl);

    void send_unsubscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

    boost::asio::io_service &io_;
    routing_manager_host *host_;

    std::shared_ptr<configuration> configuration_;

    std::shared_ptr<deserializer> deserializer_;
    std::shared_ptr<serializer> serializer_;

    std::shared_ptr<routing_manager_stub> stub_;
    std::shared_ptr<sd::service_discovery> discovery_;

    // Routing info

    // Local
    std::map<client_t, std::shared_ptr<endpoint> > local_clients_;
    std::map<service_t, std::map<instance_t, client_t> > local_services_;

    // Server endpoints for local services
    std::map<uint16_t, std::map<bool, std::shared_ptr<endpoint> > > server_endpoints_;
    std::map<service_t, std::map<endpoint *, instance_t> > service_instances_;

    // Client endpoints for remote services
    std::map<boost::asio::ip::address,
            std::map<uint16_t, std::map<client_t, std::map<bool, std::shared_ptr<endpoint> > > > > client_endpoints_;
    std::map<service_t,
            std::map<instance_t, std::map<client_t, std::map<bool, std::shared_ptr<endpoint> > > > >remote_services_;

    // Servicegroups
    std::map<std::string, std::shared_ptr<servicegroup> > servicegroups_;
    std::map<service_t, std::map<instance_t, std::shared_ptr<serviceinfo> > > services_;

    // Eventgroups
    std::map<service_t,
            std::map<instance_t,
                    std::map<eventgroup_t, std::shared_ptr<eventgroupinfo> > > > eventgroups_;
    std::map<service_t,
            std::map<instance_t, std::map<event_t, std::shared_ptr<event> > > > events_;
    std::map<service_t,
            std::map<instance_t, std::map<eventgroup_t, std::set<client_t> > > > eventgroup_clients_;

    // Mutexes
    mutable std::recursive_mutex endpoint_mutex_;
    mutable std::mutex local_mutex_;
    std::mutex serialize_mutex_;

    std::map<client_t, std::shared_ptr<endpoint_definition>> remote_subscriber_map_;

    std::unordered_set<client_t> specific_endpoint_clients;
};

}  // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_IMPL_HPP

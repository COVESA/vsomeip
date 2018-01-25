// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_IMPL_HPP
#define VSOMEIP_ROUTING_MANAGER_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <list>
#include <unordered_set>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/handler.hpp>

#include "routing_manager_base.hpp"
#include "routing_manager_stub_host.hpp"

#include "../../endpoints/include/netlink_connector.hpp"
#include "../../service_discovery/include/service_discovery_host.hpp"

namespace vsomeip {

class configuration;
class deserializer;
class eventgroupinfo;
class routing_manager_host;
class routing_manager_stub;
class servicegroup;
class serializer;
class service_endpoint;

namespace sd {
class service_discovery;
} // namespace sd


// TODO: encapsulate common parts of classes "routing_manager_impl"
// and "routing_manager_proxy" into a base class.

class routing_manager_impl: public routing_manager_base,
        public routing_manager_stub_host,
        public sd::service_discovery_host {
public:
    routing_manager_impl(routing_manager_host *_host);
    ~routing_manager_impl();

    boost::asio::io_service & get_io();
    client_t get_client() const;
    const std::shared_ptr<configuration> get_configuration() const;

    void init();
    void start();
    void stop();

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
            eventgroup_t _eventgroup, major_version_t _major, event_t _event,
            subscription_type_e _subscription_type);

    void unsubscribe(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event);

    bool send(client_t _client, std::shared_ptr<message> _message, bool _flush);

    bool send(client_t _client, const byte_t *_data, uint32_t _size,
            instance_t _instance, bool _flush, bool _reliable, bool _is_valid_crc = true);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            std::shared_ptr<message> _message, bool _flush);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            const byte_t *_data, uint32_t _size,
            instance_t _instance, bool _flush);

    bool send_to(const std::shared_ptr<endpoint_definition> &_target,
            const byte_t *_data, uint32_t _size, uint16_t _sd_port);

    void register_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups, bool _is_field,
            std::chrono::milliseconds _cycle, bool _change_resets_cycle,
            epsilon_change_func_t _epsilon_change_func,
            bool _is_provided, bool _is_shadow, bool _is_cache_placeholder);

    void register_shadow_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups,
            bool _is_field, bool _is_provided);

    void unregister_shadow_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event,
            bool _is_provided);

    void notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload,
            client_t _client, bool _force, bool _flush);

    void on_subscribe_nack(client_t _client, service_t _service,
                    instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    void on_subscribe_ack(client_t _client, service_t _service,
                    instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    void on_identify_response(client_t _client, service_t _service, instance_t _instance,
            bool _reliable);

    // interface to stub
    inline std::shared_ptr<endpoint> find_local(client_t _client) {
        return routing_manager_base::find_local(_client);
    }
    inline std::shared_ptr<endpoint> find_or_create_local(
            client_t _client) {
        return routing_manager_base::find_or_create_local(_client);
    }

    void remove_local(client_t _client);
    void on_stop_offer_service(client_t _client, service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor);

    void on_availability(service_t _service, instance_t _instance,
            bool _is_available, major_version_t _major, minor_version_t _minor);

    void on_pong(client_t _client);

    // interface "endpoint_host"
    std::shared_ptr<endpoint> find_or_create_remote_client(service_t _service,
            instance_t _instance,
            bool _reliable, client_t _client);
    void on_connect(std::shared_ptr<endpoint> _endpoint);
    void on_disconnect(std::shared_ptr<endpoint> _endpoint);
    void on_error(const byte_t *_data, length_t _length, endpoint *_receiver,
                  const boost::asio::ip::address &_remote_address,
                  std::uint16_t _remote_port);
    void on_message(const byte_t *_data, length_t _length, endpoint *_receiver,
                    const boost::asio::ip::address &_destination,
                    client_t _bound_client,
                    const boost::asio::ip::address &_remote_address,
                    std::uint16_t _remote_port);
    void on_message(service_t _service, instance_t _instance,
            const byte_t *_data, length_t _size, bool _reliable, bool _is_valid_crc = true);
    void on_notification(client_t _client, service_t _service,
            instance_t _instance, const byte_t *_data, length_t _size,
            bool _notify_one);
    void release_port(uint16_t _port, bool _reliable);

    // interface "service_discovery_host"
    typedef std::map<std::string, std::shared_ptr<servicegroup> > servicegroups_t;
    const servicegroups_t & get_servicegroups() const;
    std::shared_ptr<eventgroupinfo> find_eventgroup(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) const;
    services_t get_offered_services() const;
    std::shared_ptr<endpoint> create_service_discovery_endpoint(const std::string &_address,
            uint16_t _port, bool _reliable);
    void init_routing_info();
    void add_routing_info(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl,
            const boost::asio::ip::address &_reliable_address,
            uint16_t _reliable_port,
            const boost::asio::ip::address &_unreliable_address,
            uint16_t _unreliable_port);
    void del_routing_info(service_t _service, instance_t _instance,
            bool _has_reliable, bool _has_unreliable);
    std::chrono::milliseconds update_routing_info(std::chrono::milliseconds _elapsed);

    void on_subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup,
            std::shared_ptr<endpoint_definition> _subscriber,
            std::shared_ptr<endpoint_definition> _target,
            const std::chrono::steady_clock::time_point &_expiration);
    bool on_subscribe_accepted(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup,
            std::shared_ptr<endpoint_definition> _target,
            const std::chrono::steady_clock::time_point &_expiration);
    void on_unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup,
            std::shared_ptr<endpoint_definition> _target);
    void on_subscribe_ack(service_t _service, instance_t _instance,
            const boost::asio::ip::address &_address, uint16_t _port);

    void expire_subscriptions(const boost::asio::ip::address &_address);
    void expire_services(const boost::asio::ip::address &_address);

    std::chrono::steady_clock::time_point expire_subscriptions();

    bool has_identified(client_t _client, service_t _service,
            instance_t _instance, bool _reliable);

    void register_client_error_handler(client_t _client,
            const std::shared_ptr<endpoint> &_endpoint);
    void handle_client_error(client_t _client);

    void set_routing_state(routing_state_e _routing_state);

private:
    bool deliver_message(const byte_t *_data, length_t _length,
            instance_t _instance, bool _reliable, bool _is_valid_crc = true);
    bool deliver_notification(service_t _service, instance_t _instance,
            const byte_t *_data, length_t _length, bool _reliable, bool _is_valid_crc = true);

    instance_t find_instance(service_t _service, endpoint *_endpoint);

    void init_service_info(service_t _service,
            instance_t _instance, bool _is_local_service);

    std::shared_ptr<endpoint> create_client_endpoint(
            const boost::asio::ip::address &_address,
            uint16_t _local_port, uint16_t _remote_port,
            bool _reliable, client_t _client);

    std::shared_ptr<endpoint> create_server_endpoint(uint16_t _port,
            bool _reliable, bool _start);
    std::shared_ptr<endpoint> find_server_endpoint(uint16_t _port,
            bool _reliable) const;
    std::shared_ptr<endpoint> find_or_create_server_endpoint(uint16_t _port,
            bool _reliable, bool _start);

    bool is_field(service_t _service, instance_t _instance,
            event_t _event) const;

    std::shared_ptr<endpoint> find_remote_client(service_t _service,
            instance_t _instance, bool _reliable, client_t _client);

    std::shared_ptr<endpoint> create_remote_client(service_t _service,
                instance_t _instance, bool _reliable, client_t _client);

    bool deliver_specific_endpoint_message(service_t _service, instance_t _instance,
            const byte_t *_data, length_t _size, endpoint *_receiver);

    void clear_client_endpoints(service_t _service, instance_t _instance, bool _reliable);
    void clear_multicast_endpoints(service_t _service, instance_t _instance);

    bool is_identifying(client_t _client, service_t _service,
                instance_t _instance, bool _reliable);

    std::set<eventgroup_t> get_subscribed_eventgroups(service_t _service,
            instance_t _instance);
private:
    return_code_e check_error(const byte_t *_data, length_t _size,
            instance_t _instance);

    void send_error(return_code_e _return_code, const byte_t *_data,
            length_t _size, instance_t _instance, bool _reliable,
            endpoint *_receiver,
            const boost::asio::ip::address &_remote_address,
            std::uint16_t _remote_port);

    void identify_for_subscribe(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            subscription_type_e _subscription_type);
    bool send_identify_message(client_t _client, service_t _service,
                               instance_t _instance, major_version_t _major,
                               bool _reliable);

    bool supports_selective(service_t _service, instance_t _instance);

    client_t find_client(service_t _service, instance_t _instance,
            const std::shared_ptr<eventgroupinfo> &_eventgroup,
            const std::shared_ptr<endpoint_definition> &_target) const;

    void clear_remote_subscriber(service_t _service, instance_t _instance,
            client_t _client,
            const std::shared_ptr<endpoint_definition> &_target);

    void log_version_timer_cbk(boost::system::error_code const & _error);

    void clear_remote_service_info(service_t _service, instance_t _instance, bool _reliable);

    bool handle_local_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,minor_version_t _minor);

    void remove_specific_client_endpoint(client_t _client, service_t _service, instance_t _instance, bool _reliable);

    void clear_identified_clients( service_t _service, instance_t _instance);

    void clear_identifying_clients( service_t _service, instance_t _instance);

    void remove_identified_client(service_t _service, instance_t _instance, client_t _client);

    void remove_identifying_client(service_t _service, instance_t _instance, client_t _client);

    void unsubscribe_specific_client_at_sd(service_t _service, instance_t _instance, client_t _client);

    inline std::shared_ptr<endpoint> find_local(service_t _service, instance_t _instance) {
        return routing_manager_base::find_local(_service, _instance);
    }

    void send_subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, event_t _event,
            subscription_type_e _subscription_type);

    void on_net_if_state_changed(std::string _if, bool _available);

    void start_ip_routing();

    void requested_service_add(client_t _client, service_t _service,
                       instance_t _instance, major_version_t _major,
                       minor_version_t _minor);
    void requested_service_remove(client_t _client, service_t _service,
                       instance_t _instance);

    void call_sd_reliable_endpoint_connected(const boost::system::error_code& _error,
                                             service_t _service, instance_t _instance,
                                             std::shared_ptr<endpoint> _endpoint,
                                             std::shared_ptr<boost::asio::steady_timer> _timer);

    bool create_placeholder_event_and_subscribe(service_t _service,
                                                instance_t _instance,
                                                eventgroup_t _eventgroup,
                                                event_t _event,
                                                client_t _client);

    void handle_subscription_state(client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event);

    client_t is_specific_endpoint_client(client_t _client, service_t _service, instance_t _instance);
    std::unordered_set<client_t> get_specific_endpoint_clients(service_t _service, instance_t _instance);

    bool remote_service_offered_via_tcp_and_udp(service_t _service, instance_t _instance) const;

    std::shared_ptr<routing_manager_stub> stub_;
    std::shared_ptr<sd::service_discovery> discovery_;

    // Server endpoints for local services
    std::map<uint16_t, std::map<bool, std::shared_ptr<endpoint> > > server_endpoints_;
    std::map<service_t, std::map<endpoint *, instance_t> > service_instances_;

    // Multicast endpoint info (notifications)
    std::map<service_t, std::map<instance_t, std::shared_ptr<endpoint_definition> > > multicast_info;

    // Client endpoints for remote services
    std::map<service_t,
            std::map<instance_t, std::map<bool, std::shared_ptr<endpoint_definition> > > > remote_service_info_;

    std::map<service_t,
            std::map<instance_t, std::map<client_t, std::map<bool, std::shared_ptr<endpoint> > > > >remote_services_;
    std::map<boost::asio::ip::address,
            std::map<uint16_t, std::map<bool, std::shared_ptr<endpoint> > > >  client_endpoints_by_ip_;
    std::map<client_t,
            std::map<service_t,
                    std::map<instance_t,
                            std::set<std::pair<major_version_t, minor_version_t>>>>> requested_services_;

    // Mutexes
    mutable std::recursive_mutex endpoint_mutex_;
    std::mutex identified_clients_mutex_;
    std::mutex requested_services_mutex_;

    std::mutex remote_subscribers_mutex_;
    std::map<service_t, std::map<instance_t, std::map<client_t,
        std::set<std::shared_ptr<endpoint_definition>>>>> remote_subscribers_;

    std::mutex specific_endpoint_clients_mutex_;
    std::map<service_t, std::map<instance_t, std::unordered_set<client_t>>>specific_endpoint_clients_;
    std::map<service_t, std::map<instance_t,
        std::map<bool, std::unordered_set<client_t> > > > identified_clients_;
    std::map<service_t, std::map<instance_t,
            std::map<bool, std::unordered_set<client_t> > > > identifying_clients_;

    std::shared_ptr<serviceinfo> sd_info_;

    std::map<bool, std::set<uint16_t>> used_client_ports_;
    std::mutex used_client_ports_mutex_;

    std::mutex version_log_timer_mutex_;
    boost::asio::steady_timer version_log_timer_;

    bool if_state_running_;
    std::mutex pending_sd_offers_mutex_;
    std::vector<std::pair<service_t, instance_t>> pending_sd_offers_;
#ifndef _WIN32
    std::shared_ptr<netlink_connector> netlink_connector_;
#endif

#ifndef WITHOUT_SYSTEMD
    std::mutex watchdog_timer_mutex_;
    boost::asio::steady_timer watchdog_timer_;
    void watchdog_cbk(boost::system::error_code const &_error);
#endif

    std::mutex pending_offers_mutex_;
    // map to store pending offers.
    // 1st client id in tuple: client id of new offering application
    // 2nd client id in tuple: client id of previously/stored offering application
    std::map<service_t,
        std::map<instance_t,
                std::tuple<major_version_t, minor_version_t,
                            client_t, client_t>>> pending_offers_;

    std::mutex pending_subscription_mutex_;

    std::mutex remote_subscription_state_mutex_;
    std::map<std::tuple<service_t, instance_t, eventgroup_t, client_t>,
        subscription_state_e> remote_subscription_state_;

    std::map<e2exf::data_identifier, std::shared_ptr<e2e::profile::profile_interface::protector>> custom_protectors;
    std::map<e2exf::data_identifier, std::shared_ptr<e2e::profile::profile_interface::checker>> custom_checkers;
};

}  // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_IMPL_HPP

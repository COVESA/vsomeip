// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ROUTING_MANAGER_STUB_
#define VSOMEIP_V3_ROUTING_MANAGER_STUB_

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <atomic>
#include <unordered_set>

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include <vsomeip/handler.hpp>

#include "../../endpoints/include/endpoint_host.hpp"
#include "../include/routing_host.hpp"

#include "types.hpp"

namespace vsomeip_v3 {

class configuration;
struct policy;
class routing_manager_stub_host;

class routing_manager_stub: public routing_host,
        public std::enable_shared_from_this<routing_manager_stub> {
public:
    routing_manager_stub(
            routing_manager_stub_host *_host,
            const std::shared_ptr<configuration>& _configuration);
    virtual ~routing_manager_stub();

    void init();
    void start();
    void stop();

    void on_message(const byte_t *_data, length_t _size, endpoint *_receiver,
            const boost::asio::ip::address &_destination,
            client_t _bound_client,
            credentials_t _credentials,
            const boost::asio::ip::address &_remote_address,
            std::uint16_t _remote_port);

    void on_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major, minor_version_t _minor);
    void on_stop_offer_service(client_t _client, service_t _service,
            instance_t _instance,  major_version_t _major, minor_version_t _minor);

    bool send_subscribe(const std::shared_ptr<endpoint>& _target,
            client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major, event_t _event,
            remote_subscription_id_t _id);

    bool send_unsubscribe(const std::shared_ptr<endpoint>& _target,
            client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            event_t _event, remote_subscription_id_t _id);

    void send_subscribe_nack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    void send_subscribe_ack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    bool contained_in_routing_info(client_t _client, service_t _service,
                                   instance_t _instance, major_version_t _major,
                                   minor_version_t _minor) const;

    void create_local_receiver();
    bool send_ping(client_t _client);
    bool is_registered(client_t _client) const;
    client_t get_client() const;
    void handle_credentials(const client_t _client, std::set<service_data_t>& _requests);
    void handle_requests(const client_t _client, std::set<service_data_t>& _requests);

    void update_registration(client_t _client, registration_type_e _type);

    void print_endpoint_status() const;

    bool send_provided_event_resend_request(client_t _client,
                                            pending_remote_offer_id_t _id);

    bool update_security_policy_configuration(uint32_t _uid, uint32_t _gid,
            const std::shared_ptr<policy> &_policy,
            const std::shared_ptr<payload> &_payload,
            const security_update_handler_t &_handler);
    bool remove_security_policy_configuration(uint32_t _uid, uint32_t _gid,
            const security_update_handler_t &_handler);
    void on_security_update_response(pending_security_update_id_t _id,
            client_t _client);

    void policy_cache_add(uint32_t _uid, const std::shared_ptr<payload>& _payload);
    void policy_cache_remove(uint32_t _uid);
    bool is_policy_cached(uint32_t _uid);

    bool send_update_security_policy_request(client_t _client,
            pending_security_update_id_t _update_id, uint32_t _uid,
            const std::shared_ptr<payload>& _payload);
    bool send_remove_security_policy_request(client_t _client,
            pending_security_update_id_t _update_id, uint32_t _uid, uint32_t _gid);

    bool send_cached_security_policies(client_t _client);

    bool add_requester_policies(uid_t _uid, gid_t _gid,
            const std::set<std::shared_ptr<policy> > &_policies);
    void remove_requester_policies(uid_t _uid, gid_t _gid);

private:
    void broadcast(const std::vector<byte_t> &_command) const;

    void on_register_application(client_t _client);
    void on_deregister_application(client_t _client);

    void distribute_credentials(client_t _hoster, service_t _service, instance_t _instance);

    void inform_requesters(client_t _hoster, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, routing_info_entry_e _entry,
            bool _inform_service);

    void broadcast_ping() const;
    void on_pong(client_t _client);
    void start_watchdog();
    void check_watchdog();

    void client_registration_func(void);
    void init_routing_endpoint();
    void on_ping_timer_expired(boost::system::error_code const &_error);
    void remove_from_pinged_clients(client_t _client);
    void set_routing_state(routing_state_e _routing_state) {
        (void)_routing_state;
    };

    bool is_already_connected(client_t _source, client_t _sink);
    void create_client_routing_info(const client_t _target);
    void insert_client_routing_info(client_t _target, routing_info_entry_e _entry,
            client_t _client, service_t _service = ANY_SERVICE,
            instance_t _instance = ANY_INSTANCE,
            major_version_t _major = ANY_MAJOR,
            minor_version_t _minor = ANY_MINOR);
    void send_client_routing_info(const client_t _target);

    void create_offered_services_info(const client_t _target);
    void insert_offered_services_info(client_t _target,
            routing_info_entry_e _entry,
            service_t _service,
            instance_t _instance,
            major_version_t _major,
            minor_version_t _minor);
    void send_offered_services_info(const client_t _target);

    void create_client_credentials_info(const client_t _target);
    void insert_client_credentials_info(client_t _target, std::set<std::pair<uint32_t, uint32_t>> _credentials);
    void send_client_credentials_info(const client_t _target);

    void on_client_id_timer_expired(boost::system::error_code const &_error);

    void get_requester_policies(uid_t _uid, gid_t _gid,
            std::set<std::shared_ptr<policy> > &_policies) const;
    bool send_requester_policies(const std::unordered_set<client_t> &_clients,
            const std::set<std::shared_ptr<policy> > &_policies);

    void on_security_update_timeout(
            const boost::system::error_code &_error,
            pending_security_update_id_t _id,
            std::shared_ptr<boost::asio::steady_timer> _timer);

    pending_security_update_id_t pending_security_update_add(
            const std::unordered_set<client_t> &_clients);

    std::unordered_set<client_t> pending_security_update_get(
            pending_security_update_id_t _id);

    bool pending_security_update_remove(
            pending_security_update_id_t _id, client_t _client);

    bool is_pending_security_update_finished(
            pending_security_update_id_t _id);

    void add_pending_security_update_handler(
            pending_security_update_id_t _id,
            security_update_handler_t _handler);
    void add_pending_security_update_timer(
            pending_security_update_id_t _id);

private:
    routing_manager_stub_host *host_;
    boost::asio::io_service &io_;
    std::mutex watchdog_timer_mutex_;
    boost::asio::steady_timer watchdog_timer_;

    boost::asio::steady_timer client_id_timer_;
    std::set<client_t> used_client_ids_;
    std::mutex used_client_ids_mutex_;

    std::shared_ptr<endpoint> endpoint_;
    std::shared_ptr<endpoint> local_receiver_;
    std::mutex local_receiver_mutex_;

    std::map<client_t,
            std::pair<uint8_t, std::map<service_t, std::map<instance_t, std::pair<major_version_t, minor_version_t>> > > > routing_info_;
    mutable std::mutex routing_info_mutex_;
    std::shared_ptr<configuration> configuration_;

    bool is_socket_activated_;
    std::atomic<bool> client_registration_running_;
    std::shared_ptr<std::thread> client_registration_thread_;
    std::mutex client_registration_mutex_;
    std::condition_variable client_registration_condition_;

    std::map<client_t, std::vector<registration_type_e>> pending_client_registrations_;
    const std::uint32_t max_local_message_size_;
    static const std::vector<byte_t> its_ping_;
    const std::chrono::milliseconds configured_watchdog_timeout_;
    boost::asio::steady_timer pinged_clients_timer_;
    std::mutex pinged_clients_mutex_;
    std::map<client_t, boost::asio::steady_timer::time_point> pinged_clients_;

    std::map<client_t, std::map<service_t, std::map<instance_t, std::pair<major_version_t, minor_version_t> > > > service_requests_;
    std::map<client_t, std::set<client_t>> connection_matrix_;

    std::map<client_t, std::vector<byte_t>> client_routing_info_;
    std::map<client_t, std::vector<byte_t>> offered_services_info_;
    std::map<client_t, std::vector<byte_t>> client_credentials_info_;

    std::mutex pending_security_updates_mutex_;
    pending_security_update_id_t pending_security_update_id_;
    std::map<pending_security_update_id_t, std::unordered_set<client_t>> pending_security_updates_;

    std::recursive_mutex security_update_handlers_mutex_;
    std::map<pending_security_update_id_t, security_update_handler_t> security_update_handlers_;

    std::mutex security_update_timers_mutex_;
    std::map<pending_security_update_id_t, std::shared_ptr<boost::asio::steady_timer>> security_update_timers_;

    std::mutex updated_security_policies_mutex_;
    std::map<uint32_t, std::shared_ptr<payload>> updated_security_policies_;

    mutable std::mutex requester_policies_mutex_;
    std::map<uint32_t,
        std::map<uint32_t,
            std::set<std::shared_ptr<policy> >
        >
    > requester_policies_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_ROUTING_MANAGER_STUB_

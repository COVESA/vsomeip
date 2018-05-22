// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_STUB
#define VSOMEIP_ROUTING_MANAGER_STUB

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <atomic>

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include "../../endpoints/include/endpoint_host.hpp"
#include "../../configuration/include/internal.hpp"
#include "types.hpp"

namespace vsomeip {

class configuration;
class routing_manager_stub_host;

class routing_manager_stub: public endpoint_host,
        public std::enable_shared_from_this<routing_manager_stub> {
public:
    routing_manager_stub(
            routing_manager_stub_host *_host,
            std::shared_ptr<configuration> _configuration);
    virtual ~routing_manager_stub();

    void init();
    void start();
    void stop();

    const std::shared_ptr<configuration> get_configuration() const;

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

    void on_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major, minor_version_t _minor);
    void on_stop_offer_service(client_t _client, service_t _service,
            instance_t _instance,  major_version_t _major, minor_version_t _minor);

    bool send_subscribe(std::shared_ptr<vsomeip::endpoint> _target,
            client_t _client, service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major,
            event_t _event, pending_subscription_id_t _subscription_id);

    bool send_unsubscribe(std::shared_ptr<vsomeip::endpoint> _target,
            client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            event_t _event, pending_subscription_id_t _unsubscription_id);

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
    void handle_requests(const client_t _client, std::set<service_data_t>& _requests);

    void send_identify_request_command(std::shared_ptr<vsomeip::endpoint> _target,
            service_t _service, instance_t _instance, major_version_t _major,
            bool _reliable);

#ifndef _WIN32
    virtual bool check_credentials(client_t _client, uid_t _uid, gid_t _gid);
#endif

    void update_registration(client_t _client, registration_type_e _type);

    void print_endpoint_status() const;
private:
    void broadcast(const std::vector<byte_t> &_command) const;

    void on_register_application(client_t _client);
    void on_deregister_application(client_t _client);

    void inform_requesters(client_t _hoster, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, routing_info_entry_e _entry,
            bool _inform_service);

    void broadcast_ping() const;
    void on_pong(client_t _client);
    void start_watchdog();
    void check_watchdog();
    void send_application_lost(std::list<client_t> &_lost);

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

    void on_client_id_timer_expired(boost::system::error_code const &_error);

private:
    routing_manager_stub_host *host_;
    boost::asio::io_service &io_;
    std::mutex watchdog_timer_mutex_;
    boost::asio::steady_timer watchdog_timer_;

    boost::asio::steady_timer client_id_timer_;
    std::set<client_t> used_client_ids_;
    std::mutex used_client_ids_mutex_;

    std::string endpoint_path_;
    std::string local_receiver_path_;
    std::shared_ptr<endpoint> endpoint_;
    std::shared_ptr<endpoint> local_receiver_;
    std::mutex local_receiver_mutex_;

    std::map<client_t,
            std::pair<uint8_t, std::map<service_t, std::map<instance_t, std::pair<major_version_t, minor_version_t>> > > > routing_info_;
    mutable std::mutex routing_info_mutex_;
    std::shared_ptr<configuration> configuration_;

    size_t routingCommandSize_;

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
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_STUB


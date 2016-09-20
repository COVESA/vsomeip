// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>

#include "../../endpoints/include/endpoint_host.hpp"

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

    void on_connect(std::shared_ptr<endpoint> _endpoint);
    void on_disconnect(std::shared_ptr<endpoint> _endpoint);
    void on_message(const byte_t *_data, length_t _length, endpoint *_receiver,
            const boost::asio::ip::address &_destination);
    void on_error(const byte_t *_data, length_t _length, endpoint *_receiver);
    void release_port(uint16_t _port, bool _reliable);

    void on_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major, minor_version_t _minor);
    void on_stop_offer_service(client_t _client, service_t _service,
            instance_t _instance,  major_version_t _major, minor_version_t _minor);

    bool queue_message(const byte_t *_data, uint32_t _size);

    void send_subscribe(std::shared_ptr<vsomeip::endpoint> _target,
            client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, bool _is_remote_subscriber);

    void send_unsubscribe(std::shared_ptr<vsomeip::endpoint> _target,
            client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

    void send_subscribe_nack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

    void send_subscribe_ack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup);

private:
    void broadcast(std::vector<byte_t> &_command) const;

    void on_register_application(client_t _client);
    void on_deregister_application(client_t _client);

    void broadcast_routing_info(bool _empty = false);
    void send_routing_info(client_t _client, bool _empty = false);

    void broadcast_ping() const;
    void on_pong(client_t _client);
    void start_watchdog();
    void check_watchdog();
    void send_application_lost(std::list<client_t> &_lost);

    void client_registration_func(void);

private:
    routing_manager_stub_host *host_;
    boost::asio::io_service &io_;
    boost::asio::system_timer watchdog_timer_;

    std::string endpoint_path_;
    std::string local_receiver_path_;
    std::shared_ptr<endpoint> endpoint_;
    std::shared_ptr<endpoint> local_receiver_;

    std::map<client_t,
            std::pair<uint8_t, std::map<service_t, std::map<instance_t, std::pair<major_version_t, minor_version_t>> > > > routing_info_;
    mutable std::mutex routing_info_mutex_;
    std::shared_ptr<configuration> configuration_;

    size_t routingCommandSize_;

    bool client_registration_running_;
    std::shared_ptr<std::thread> client_registration_thread_;
    std::mutex client_registration_mutex_;
    std::condition_variable client_registration_condition_;
    std::map<client_t, std::vector<bool>> pending_client_registrations_;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_STUB


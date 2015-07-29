// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL
#define VSOMEIP_SERVICE_DISCOVERY_IMPL

#include <map>
#include <memory>
#include <mutex>
#include <set>

#include <boost/asio/ip/address.hpp>

#include "service_discovery.hpp"
#include "../../routing/include/types.hpp"

namespace vsomeip {

class endpoint;
class serializer;

namespace sd {

class entry_impl;
class eventgroupentry_impl;
class option_impl;
class request;
class serviceentry_impl;
class service_discovery_fsm;
class service_discovery_host;
class subscription;

typedef std::map<service_t, std::map<instance_t, std::shared_ptr<request> > > requests_t;

class service_discovery_impl: public service_discovery,
        public std::enable_shared_from_this<service_discovery_impl> {
public:
    service_discovery_impl(service_discovery_host *_host);
    virtual ~service_discovery_impl();

    std::shared_ptr<configuration> get_configuration() const;
    boost::asio::io_service & get_io();

    void init();
    void start();
    void stop();

    void request_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl);
    void release_service(service_t _service, instance_t _instance);

    void subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl, client_t _client);
    void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);

    void send(const std::string &_name, bool _is_announcing);

    void on_message(const byte_t *_data, length_t _length);

    void on_offer_change(const std::string &_name);

private:
    session_t get_session(const boost::asio::ip::address &_address);
    void increment_session(const boost::asio::ip::address &_address);

    void insert_option(std::shared_ptr<message_impl> &_message,
            std::shared_ptr<entry_impl> _entry,
            const boost::asio::ip::address &_address, uint16_t _port,
            bool _is_reliable);
    void insert_find_entries(std::shared_ptr<message_impl> &_message,
            requests_t &_requests);
    void insert_offer_entries(std::shared_ptr<message_impl> &_message,
            services_t &_services);
    void insert_subscription(std::shared_ptr<message_impl> &_message,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            std::shared_ptr<subscription> &_subscription);
    void insert_subscription_ack(std::shared_ptr<message_impl> &_message,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            std::shared_ptr<eventgroupinfo> &_info);

    void process_serviceentry(std::shared_ptr<serviceentry_impl> &_entry,
            const std::vector<std::shared_ptr<option_impl> > &_options);
    void process_eventgroupentry(std::shared_ptr<eventgroupentry_impl> &_entry,
            const std::vector<std::shared_ptr<option_impl> > &_options);

    void handle_service_availability(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl,
            const boost::asio::ip::address &_address, uint16_t _port,
            bool _reliable);
    void handle_eventgroup_subscription(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, ttl_t _ttl,
            const boost::asio::ip::address &_reliable_address,
            uint16_t _reliable_port, uint16_t _unreliable_port);
    void handle_eventgroup_subscription_ack(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, ttl_t _ttl,
            const boost::asio::ip::address &_address, uint16_t _port);

private:
    boost::asio::io_service &io_;
    service_discovery_host *host_;

    boost::asio::ip::address unicast_;
    uint16_t port_;
    bool reliable_;

    std::shared_ptr<serializer> serializer_;
    std::shared_ptr<deserializer> deserializer_;

    std::shared_ptr<service_discovery_fsm> default_;
    std::map<std::string, std::shared_ptr<service_discovery_fsm> > additional_;

    requests_t requested_;
    std::mutex requested_mutex_;
    std::map<service_t,
            std::map<instance_t,
                    std::map<eventgroup_t, std::map<client_t, std::shared_ptr<subscription> > > > > subscribed_;

    std::mutex serialize_mutex_;

    // Sessions
    std::map<boost::asio::ip::address, session_t> sessions_;

    // Runtime
    std::weak_ptr<runtime> runtime_;
};

}  // namespace sd
}  // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL

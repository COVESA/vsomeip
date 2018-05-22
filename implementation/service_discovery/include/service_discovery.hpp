// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_HPP
#define VSOMEIP_SERVICE_DISCOVERY_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include "../../routing/include/serviceinfo.hpp"
#include "../../endpoints/include/endpoint.hpp"
#include "../include/service_discovery_host.hpp"

namespace vsomeip {

class configuration;

namespace sd {

class service_discovery {
public:
    virtual ~service_discovery() {
    }

    virtual std::shared_ptr<configuration> get_configuration() const = 0;
    virtual boost::asio::io_service & get_io() = 0;

    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

    virtual void request_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl) = 0;
    virtual void release_service(service_t _service, instance_t _instance) = 0;

    virtual void subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl, client_t _client,
            subscription_type_e _subscription_type) = 0;
    virtual void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, client_t _client) = 0;
    virtual void unsubscribe_all(service_t _service, instance_t _instance) = 0;
    virtual void unsubscribe_client(service_t _service, instance_t _instance,
                                    client_t _client) = 0;

    virtual bool send(bool _is_announcing) = 0;

    virtual void on_message(const byte_t *_data, length_t _length,
            const boost::asio::ip::address &_sender,
            const boost::asio::ip::address &_destination) = 0;

    virtual void send_subscriptions(service_t _service, instance_t _instance,
            client_t _client, bool _reliable) = 0;

    virtual void on_endpoint_connected(
            service_t _service, instance_t _instance,
            const std::shared_ptr<const vsomeip::endpoint> &_endpoint) = 0;

    virtual void offer_service(service_t _service, instance_t _instance,
                               std::shared_ptr<serviceinfo> _info) = 0;
    virtual void stop_offer_service(service_t _service, instance_t _instance,
                                    std::shared_ptr<serviceinfo> _info) = 0;

    virtual void set_diagnosis_mode(const bool _activate) = 0;

    virtual bool get_diagnosis_mode() = 0;

    virtual void remote_subscription_acknowledge(
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            client_t _client, bool _accepted,
            const std::shared_ptr<sd_message_identifier_t> &_sd_message_id) = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_HPP

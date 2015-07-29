// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_HPP
#define VSOMEIP_SERVICE_DISCOVERY_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>

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
            eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl, client_t _client) = 0;
    virtual void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup) = 0;

    virtual void send(const std::string &_name, bool _is_announcing) = 0;

    virtual void on_message(const byte_t *_data, length_t _length) = 0;

    virtual void on_offer_change(const std::string &_name) = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_HPP

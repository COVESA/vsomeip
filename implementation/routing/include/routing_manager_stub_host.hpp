// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_STUB_HOST
#define VSOMEIP_ROUTING_MANAGER_STUB_HOST

#include <boost/asio/io_service.hpp>

namespace vsomeip {

class routing_manager_stub_host {
public:
    virtual ~routing_manager_stub_host() {
    }

    virtual void offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl) = 0;

    virtual void stop_offer_service(client_t _client, service_t _service,
            instance_t _instance) = 0;

    virtual void request_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl, bool _has_selective) = 0;

    virtual void release_service(client_t _client, service_t _service,
            instance_t _instance) = 0;

    virtual void subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, ttl_t _ttl) = 0;

    virtual void unsubscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) = 0;

    virtual void on_message(service_t _service, instance_t _instance,
            const byte_t *_data, length_t _size, bool _reliable) = 0;

    virtual void on_stop_offer_service(service_t _service,
            instance_t _instance) = 0;

    virtual std::shared_ptr<endpoint> find_local(client_t _client) = 0;
    virtual std::shared_ptr<endpoint> find_local(service_t _service,
            instance_t _instance) = 0;
    virtual std::shared_ptr<endpoint> find_or_create_local(
            client_t _client) = 0;
    virtual void remove_local(client_t _client) = 0;

    virtual boost::asio::io_service & get_io() = 0;
    virtual client_t get_client() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_STUB_HOST

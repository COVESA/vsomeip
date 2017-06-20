// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_STUB_HOST
#define VSOMEIP_ROUTING_MANAGER_STUB_HOST

#include <boost/asio/io_service.hpp>
#include <vsomeip/handler.hpp>

namespace vsomeip {

class routing_manager_stub_host {
public:
    virtual ~routing_manager_stub_host() {
    }

    virtual bool offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor) = 0;

    virtual void stop_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major, minor_version_t _minor) = 0;

    virtual void request_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, bool _use_exclusive_proxy) = 0;

    virtual void release_service(client_t _client, service_t _service,
            instance_t _instance) = 0;

    virtual void register_shadow_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups,
            bool _is_field, bool _is_provided) = 0;

    virtual void unregister_shadow_event(client_t _client, service_t _service,
            instance_t _instance, event_t _event, bool _is_provided) = 0;

    virtual void subscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, event_t _event,
            subscription_type_e _subscription_type) = 0;

    virtual void on_subscribe_nack(client_t _client, service_t _service,
                instance_t _instance, eventgroup_t _eventgroup, event_t _event) = 0;

    virtual void on_subscribe_ack(client_t _client, service_t _service,
                instance_t _instance, eventgroup_t _eventgroup, event_t _event) = 0;

    virtual void unsubscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event) = 0;

    virtual void on_message(service_t _service, instance_t _instance,
            const byte_t *_data, length_t _size, bool _reliable, bool _is_valid_crc = true) = 0;

    virtual void on_notification(client_t _client,
            service_t _service, instance_t _instance,
            const byte_t *_data, length_t _size, bool _notify_one = false) = 0;

    virtual void on_stop_offer_service(client_t _client, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor) = 0;

    virtual void on_availability(service_t _service, instance_t _instance,
            bool _is_available, major_version_t _major, minor_version_t _minor) = 0;

    virtual std::shared_ptr<endpoint> find_local(client_t _client) = 0;

    virtual std::shared_ptr<endpoint> find_or_create_local(
            client_t _client) = 0;
    virtual void remove_local(client_t _client) = 0;

    virtual boost::asio::io_service & get_io() = 0;
    virtual client_t get_client() const = 0;

    virtual void on_identify_response(client_t _client, service_t _service, instance_t _instance,
            bool _reliable) = 0;

    virtual void on_pong(client_t _client) = 0;

    virtual void handle_client_error(client_t _client) = 0;

    virtual void set_routing_state(routing_state_e _routing_state) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_STUB_HOST

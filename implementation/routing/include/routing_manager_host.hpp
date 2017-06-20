// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_HOST
#define VSOMEIP_ROUTING_MANAGER_HOST

#include <memory>

#include <boost/asio/io_service.hpp>

#include <vsomeip/error.hpp>

namespace vsomeip {

class configuration;
class message;

class routing_manager_host {
public:
    virtual ~routing_manager_host() {
    }

    virtual client_t get_client() const = 0;
    virtual const std::string & get_name() const = 0;
    virtual std::shared_ptr<configuration> get_configuration() const = 0;
    virtual boost::asio::io_service & get_io() = 0;

    virtual void on_availability(service_t _service, instance_t _instance,
    bool _is_available, major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR) = 0;
    virtual void on_state(state_type_e _state) = 0;
    virtual void on_message(const std::shared_ptr<message> &&_message) = 0;
    virtual void on_error(error_code_e _error) = 0;
    virtual bool on_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            client_t _client, bool _subscribed) = 0;
    virtual void on_subscription_error(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, uint16_t _error) = 0;
    virtual void on_subscription_status(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, event_t _event, uint16_t _error) = 0;
    virtual void send(std::shared_ptr<message> _message, bool _flush) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_HOST

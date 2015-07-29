// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
    bool _is_available) const = 0;
    virtual void on_event(event_type_e _event) = 0;
    virtual void on_message(std::shared_ptr<message> _message) = 0;
    virtual void on_error(error_code_e _error) = 0;
    virtual bool on_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            client_t _client, bool _subscribed) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_HOST

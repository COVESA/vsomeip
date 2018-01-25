// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_HANDLER_HPP
#define VSOMEIP_HANDLER_HPP

#include <functional>
#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class message;

typedef std::function< void (state_type_e) > state_handler_t;
typedef std::function< void (const std::shared_ptr< message > &) > message_handler_t;
typedef std::function< void (service_t, instance_t, bool) > availability_handler_t;
typedef std::function< bool (client_t, bool) > subscription_handler_t;
typedef std::function< void (const uint16_t) > error_handler_t;
typedef std::function< void (const service_t, const instance_t, const eventgroup_t,
                             const event_t, const uint16_t) > subscription_status_handler_t;
typedef std::function< void (client_t, bool, std::function< void (const bool) > )> async_subscription_handler_t;

typedef std::function< void (const std::vector<std::pair<service_t, instance_t>> &_services) > offered_services_handler_t;
typedef std::function< void () > watchdog_handler_t;


} // namespace vsomeip

#endif // VSOMEIP_HANDLER_HPP

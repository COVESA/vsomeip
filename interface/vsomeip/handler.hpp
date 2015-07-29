// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

typedef std::function< void (event_type_e) > event_handler_t;
typedef std::function< void (const std::shared_ptr< message > &) > message_handler_t;
typedef std::function< void (service_t, instance_t, bool) > availability_handler_t;
typedef std::function< bool (client_t, bool) > subscription_handler_t;

} // namespace vsomeip

#endif // VSOMEIP_HANDLER_HPP

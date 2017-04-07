// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_TYPES_HPP
#define VSOMEIP_ROUTING_TYPES_HPP

#include <map>
#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class serviceinfo;

typedef std::map<service_t,
                 std::map<instance_t,
                          std::shared_ptr<serviceinfo> > > services_t;

class eventgroupinfo;

typedef std::map<service_t,
                 std::map<instance_t,
                          std::map<eventgroup_t,
                                   std::shared_ptr<
                                       eventgroupinfo> > > > eventgroups_t;

enum class registration_type_e : std::uint8_t {
    REGISTER = 0x1,
    DEREGISTER = 0x2,
    DEREGISTER_ON_ERROR = 0x3
};

}
// namespace vsomeip

#endif // VSOMEIP_ROUTING_TYPES_HPP

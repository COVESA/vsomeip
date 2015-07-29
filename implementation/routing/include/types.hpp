// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

}
// namespace vsomeip

#endif // VSOMEIP_ROUTING_TYPES_HPP

// Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_CLIENT_HPP
#define VSOMEIP_CFG_CLIENT_HPP

#include <map>
#include <memory>
#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct client {
    service_t service_;
    instance_t instance_;

    std::map<bool, std::set<uint16_t> > ports_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_CLIENT_HPP

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_E2E_HPP_
#define VSOMEIP_CFG_E2E_HPP_

#include <map>
#include <string>
#include <vector>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct e2e {
    typedef std::map<std::string, std::string> custom_parameters_t;

    e2e() :
        data_id(0),
        variant(""),
        profile(""),
        service_id(0),
        event_id(0) {
    }

    e2e(uint16_t _data_id, std::string _variant, std::string _profile, uint16_t _service_id,
        uint16_t _event_id, custom_parameters_t&& _custom_parameters) :
        data_id(_data_id),
        variant(_variant),
        profile(_profile),
        service_id(_service_id),
        event_id(_event_id),
        custom_parameters(_custom_parameters) {
    }

    // common config
    uint16_t data_id;
    std::string variant;
    std::string profile;
    uint16_t service_id;
    uint16_t event_id;

    // custom parameters
    custom_parameters_t custom_parameters;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_E2E_HPP_

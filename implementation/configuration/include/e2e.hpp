// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_E2E_HPP_
#define VSOMEIP_CFG_E2E_HPP_

#include <string>
#include <vector>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct e2e {

    e2e() :
        data_id(0),
        variant(""),
        profile(""),
        service_id(0),
        event_id(0),
        crc_offset(0),
        data_id_mode(0),
        data_length(0),
        data_id_nibble_offset(0),
        counter_offset(0) {
    }


    e2e(uint16_t _data_id, std::string _variant, std::string _profile, uint16_t _service_id,
        uint16_t _event_id,uint16_t _crc_offset,
        uint8_t  _data_id_mode, uint16_t _data_length, uint16_t _data_id_nibble_offset, uint16_t _counter_offset) :

        data_id(_data_id),
        variant(_variant),
        profile(_profile),
        service_id(_service_id),
        event_id(_event_id),
        crc_offset(_crc_offset),
        data_id_mode(_data_id_mode),
        data_length(_data_length),
        data_id_nibble_offset(_data_id_nibble_offset),
        counter_offset(_counter_offset) {

    }

    // common config
    uint16_t data_id;
    std::string variant;
    std::string profile;
    uint16_t service_id;
    uint16_t event_id;

    //profile 1 specific config
    // [SWS_E2E_00018]
    uint16_t crc_offset;
    uint8_t  data_id_mode;
    uint16_t data_length;
    uint16_t data_id_nibble_offset;
    uint16_t counter_offset;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_E2E_HPP_

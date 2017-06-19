// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../../../e2e_protection/include/e2e/profile/profile_custom/checker.hpp"
#include "../../../../../logging/include/logger.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <algorithm>

namespace e2e {
namespace profile {
namespace profile_custom {

void profile_custom_checker::check(const buffer::e2e_buffer &_buffer,
                                   e2e::profile::profile_interface::generic_check_status &_generic_check_status) {
    std::lock_guard<std::mutex> lock(check_mutex);
    _generic_check_status = e2e::profile::profile_interface::generic_check_status::E2E_ERROR;

   if(profile_custom::is_buffer_length_valid(config, _buffer)) {
        uint32_t received_crc(0);
        uint32_t calculated_crc(0);

        received_crc = read_crc(_buffer);
        calculated_crc = profile_custom::compute_crc(config, _buffer);
        if(received_crc == calculated_crc) {
            _generic_check_status = e2e::profile::profile_interface::generic_check_status::E2E_OK;
        } else {
            _generic_check_status = e2e::profile::profile_interface::generic_check_status::E2E_WRONG_CRC;
            VSOMEIP_INFO << std::hex << "E2E protection: CRC32 does not match: calculated CRC: " << (uint32_t) calculated_crc << " received CRC: " << (uint32_t) received_crc;
        }
    }
    return;
}

uint32_t profile_custom_checker::read_crc(const buffer::e2e_buffer &_buffer) const {
    return (static_cast<uint32_t>(_buffer[config.crc_offset ]) << 24U) |
           (static_cast<uint32_t>(_buffer[config.crc_offset + 1U]) << 16U) |
           (static_cast<uint32_t>(_buffer[config.crc_offset + 2U]) << 8U) |
           static_cast<uint32_t>(_buffer[config.crc_offset + 3U]);
}


}
}
}

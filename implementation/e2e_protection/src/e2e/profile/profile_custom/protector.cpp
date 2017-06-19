// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../../../e2e_protection/include/e2e/profile/profile_custom/protector.hpp"
#include "../../../../../logging/include/logger.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <sstream>


namespace e2e {
namespace profile {
namespace profile_custom {

void protector::protect(buffer::e2e_buffer &_buffer) {
    std::lock_guard<std::mutex> lock(protect_mutex);

    if(profile_custom::is_buffer_length_valid(config, _buffer)) {
        // compute the CRC over DataID and Data
        uint32_t computed_crc = profile_custom::compute_crc(config, _buffer);
        // write CRC in Data
        write_crc(_buffer, computed_crc);
    }
}

void protector::write_crc(buffer::e2e_buffer &_buffer, uint32_t _computed_crc) {
    _buffer[config.crc_offset] = static_cast<uint8_t>(_computed_crc >> 24U);
    _buffer[config.crc_offset + 1U] = static_cast<uint8_t>(_computed_crc >> 16U);
    _buffer[config.crc_offset + 2U] = static_cast<uint8_t>(_computed_crc >> 8U);
    _buffer[config.crc_offset + 3U] = static_cast<uint8_t>(_computed_crc);
}

}
}
}

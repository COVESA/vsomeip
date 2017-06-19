// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../../../e2e_protection/include/e2e/profile/profile01/protector.hpp"
#include "../../../../../logging/include/logger.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>


namespace e2e {
namespace profile {
namespace profile01 {

/** @req [SWS_E2E_00195] */
void protector::protect(buffer::e2e_buffer &_buffer) {
    std::lock_guard<std::mutex> lock(protect_mutex);

    if(profile_01::is_buffer_length_valid(config, _buffer)) {
        // compute the CRC over DataID and Data
        uint8_t computed_crc = profile_01::compute_crc(config, _buffer);
        // write CRC in Data
        write_crc(_buffer, computed_crc);
    }
}


/** @req [SRS_E2E_08528] */
void protector::write_crc(buffer::e2e_buffer &_buffer, uint8_t _computed_crc) {
    _buffer[config.crc_offset] = _computed_crc;
}

}
}
}

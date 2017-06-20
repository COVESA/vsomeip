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
        // write the current Counter value in Data
        write_counter(_buffer);

        // write DataID nibble in Data (E2E_P01_DATAID_NIBBLE) in Data
        write_data_id(_buffer);

        // compute the CRC over DataID and Data
        uint8_t computed_crc = profile_01::compute_crc(config, _buffer);
        // write CRC in Data
        write_crc(_buffer, computed_crc);

        // increment the Counter (new value will be used in the next invocation of E2E_P01Protect()),
        increment_counter();
    }
}

/** @req [SRS_E2E_08528] */
void protector::write_counter(buffer::e2e_buffer &_buffer) {
    if(config.counter_offset % 8 == 0) {
        // write write counter value into low nibble
        _buffer[config.counter_offset / 8] = static_cast<uint8_t>((_buffer[config.counter_offset / 8] & 0xF0) | (counter & 0x0F));
    } else {
        // write counter into high nibble
        _buffer[config.counter_offset / 8] = static_cast<uint8_t>((_buffer[config.counter_offset / 8] & 0x0F) | ((counter << 4) & 0xF0));
    }
}

/** @req [SRS_E2E_08528] */
void protector::write_data_id(buffer::e2e_buffer &_buffer) {
    if(config.data_id_mode == p01_data_id_mode::E2E_P01_DATAID_NIBBLE) {
        if(config.data_id_nibble_offset % 8 == 0) {
            // write low nibble of high byte of Data ID
            _buffer[config.data_id_nibble_offset / 8] = static_cast<uint8_t>((_buffer[config.data_id_nibble_offset / 8] & 0xF0) | ((config.data_id >> 8) & 0x0F));
        } else {
            // write low nibble of high byte of Data ID
            _buffer[config.data_id_nibble_offset / 8] = static_cast<uint8_t>((_buffer[config.data_id_nibble_offset / 8] & 0x0F) | ((config.data_id >> 4) & 0xF0));
        }
    }
}

/** @req [SRS_E2E_08528] */
void protector::write_crc(buffer::e2e_buffer &_buffer, uint8_t _computed_crc) {
    _buffer[config.crc_offset] = _computed_crc;
}

/** @req [SWS_E2E_00075] */
void protector::increment_counter(void) {
    counter = static_cast<uint8_t>((counter + 1U) % 15);
}


}
}
}

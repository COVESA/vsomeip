// Copyright (C) 2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include "../../../../../e2e_protection/include/e2e/profile/profile04/protector.hpp"

#include <vsomeip/internal/logger.hpp>
#include "../../utility/include/byteorder.hpp"

namespace vsomeip_v3 {
namespace e2e {
namespace profile04 {

/** @req [SWS_E2E_00195] */
void
protector::protect(e2e_buffer &_buffer, instance_t _instance) {
    std::lock_guard<std::mutex> lock(protect_mutex_);

    if (_instance > VSOMEIP_E2E_PROFILE04_MAX_INSTANCE) {
        VSOMEIP_ERROR << "E2E Profile 4 can only be used for instances [1-255]";
        return;
    }

    /** @req: [SWS_E2E_00363] */
    if (verify_inputs(_buffer)) {

        /** @req [SWS_E2E_00364] */
        write_16(_buffer, static_cast<uint16_t>(_buffer.size()), 0);

        /** @req [SWS_E2E_00365] */
        write_16(_buffer, counter_, 2);

        /** @req [SWS_E2E_00366] */
        uint32_t its_data_id(uint32_t(_instance) << 24 | config_.data_id_);
        write_32(_buffer, its_data_id, 4);

        /** @req [SWS_E2E_00367] */
        uint32_t its_crc = profile_04::compute_crc(config_, _buffer);

        /** @req [SWS_E2E_0368] */
        write_32(_buffer, its_crc, 8);

        /** @req [SWS_E2E_00369] */
        increment_counter();
    }
}

bool
protector::verify_inputs(e2e_buffer &_buffer) {

    return (_buffer.size() >= config_.min_data_length_
            && _buffer.size() <= config_.max_data_length_);
}

void
protector::write_16(e2e_buffer &_buffer, uint16_t _data, size_t _index) {

    _buffer[config_.offset_ + _index] = VSOMEIP_WORD_BYTE1(_data);
    _buffer[config_.offset_ + _index + 1] = VSOMEIP_WORD_BYTE0(_data);
}

void
protector::write_32(e2e_buffer &_buffer, uint32_t _data, size_t _index) {

    _buffer[config_.offset_ + _index] = VSOMEIP_LONG_BYTE3(_data);
    _buffer[config_.offset_ + _index + 1] = VSOMEIP_LONG_BYTE2(_data);
    _buffer[config_.offset_ + _index + 2] = VSOMEIP_LONG_BYTE1(_data);
    _buffer[config_.offset_ + _index + 3] = VSOMEIP_LONG_BYTE0(_data);
}

void
protector::increment_counter() {
    counter_++;
}

} // namespace profile04
} // namespace e2e
} // namespace vsomeip_v3

// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <vsomeip/internal/logger.hpp>
#include "../../../../include/e2e/profile/profile07/protector.hpp"
#include "../../../../../utility/include/byteorder.hpp"

namespace vsomeip_v3 {
namespace e2e {
namespace profile07 {

/** @req [SWS_E2E_00486] */
void
protector::protect(e2e_buffer &_buffer, instance_t _instance) {
    std::lock_guard<std::mutex> lock(protect_mutex_);

    /** @req: [SWS_E2E_00487] */
    if (verify_inputs(_buffer)) {

        /** @req [SWS_E2E_00489] */
        write_32(_buffer, static_cast<uint16_t>(_buffer.size()), PROFILE_07_SIZE_OFFSET);    

        /** @req [SWS_E2E_00490] */
        write_32(_buffer, get_counter(_instance), PROFILE_07_COUNTER_OFFSET);

        /** @req [SWS_E2E_00491] */
        write_32(_buffer, config_.data_id_, PROFILE_07_DATAID_OFFSET);

        /** @req [SWS_E2E_00492] */
        uint64_t its_crc = profile_07::compute_crc(config_, _buffer);

        /** @req [SWS_E2E_00493] */
        write_64(_buffer, its_crc, PROFILE_07_CRC_OFFSET);

        /** @req [SWS_E2E_00494] */
        increment_counter(_instance);
    }
}

bool
protector::verify_inputs(e2e_buffer &_buffer) {

    return (_buffer.size() >= config_.min_data_length_
            && _buffer.size() <= config_.max_data_length_);
}

// Write uint32_t as big-endian
void
protector::write_32(e2e_buffer &_buffer, uint32_t _data, size_t _index) {

    _buffer[config_.offset_ + _index] = VSOMEIP_LONG_BYTE3(_data);
    _buffer[config_.offset_ + _index + 1] = VSOMEIP_LONG_BYTE2(_data);
    _buffer[config_.offset_ + _index + 2] = VSOMEIP_LONG_BYTE1(_data);
    _buffer[config_.offset_ + _index + 3] = VSOMEIP_LONG_BYTE0(_data);
}

// Write uint64_t as big-endian
void
protector::write_64(e2e_buffer &_buffer, uint64_t _data, size_t _index) {

    _buffer[config_.offset_ + _index] = VSOMEIP_LONG_LONG_BYTE7(_data);
    _buffer[config_.offset_ + _index + 1] = VSOMEIP_LONG_LONG_BYTE6(_data);
    _buffer[config_.offset_ + _index + 2] = VSOMEIP_LONG_LONG_BYTE5(_data);
    _buffer[config_.offset_ + _index + 3] = VSOMEIP_LONG_LONG_BYTE4(_data);
    _buffer[config_.offset_ + _index + 4] = VSOMEIP_LONG_LONG_BYTE3(_data);
    _buffer[config_.offset_ + _index + 5] = VSOMEIP_LONG_LONG_BYTE2(_data);
    _buffer[config_.offset_ + _index + 6] = VSOMEIP_LONG_LONG_BYTE1(_data);
    _buffer[config_.offset_ + _index + 7] = VSOMEIP_LONG_LONG_BYTE0(_data);
}

uint32_t
protector::get_counter(instance_t _instance) const {

    uint32_t its_counter(0);

    auto find_counter = counter_.find(_instance);
    if (find_counter != counter_.end())
        its_counter = find_counter->second;

    return its_counter;
}

void
protector::increment_counter(instance_t _instance) {

    auto find_counter = counter_.find(_instance);
    if (find_counter != counter_.end())
        find_counter->second++;
    else
        counter_[_instance] = 1;
}

} // namespace profile07
} // namespace e2e
} // namespace vsomeip_v3

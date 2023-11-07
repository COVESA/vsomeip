// Copyright (C) 2020-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <vsomeip/internal/logger.hpp>
#include "../../../../include/e2e/profile/profile05/protector.hpp"
#include "../../../../../utility/include/byteorder.hpp"

namespace vsomeip_v3 {
namespace e2e {
namespace profile05 {

void
protector::protect(e2e_buffer &_buffer, instance_t _instance) {
    std::lock_guard<std::mutex> lock(protect_mutex_);

    if (_instance > VSOMEIP_E2E_PROFILE05_MAX_INSTANCE) {
        VSOMEIP_ERROR << "E2E Profile 5 can only be used for instances [1-255]";
        return;
    }

    if (verify_inputs(_buffer)) {
        write_8(_buffer, get_counter(_instance), 2);
        uint16_t its_crc = profile_05::compute_crc(config_, _buffer);
        write_16(_buffer, its_crc, 0);
        increment_counter(_instance);
    }
}

bool
protector::verify_inputs(e2e_buffer &_buffer) {

    return (_buffer.size() >= config_.min_data_length_
            && _buffer.size() <= config_.max_data_length_);
}

void
protector::write_8(e2e_buffer &_buffer, uint8_t _data, size_t _index) {

    _buffer[config_.offset_ + _index] = _data;
}

void
protector::write_16(e2e_buffer &_buffer, uint16_t _data, size_t _index) {

    _buffer[config_.offset_ + _index] = VSOMEIP_WORD_BYTE0(_data);
    _buffer[config_.offset_ + _index + 1] = VSOMEIP_WORD_BYTE1(_data);
}

uint8_t
protector::get_counter(instance_t _instance) const {

    uint8_t its_counter(0);

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

} // namespace profile05
} // namespace e2e
} // namespace vsomeip_v3

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_E2E_PROFILE_CUSTOM_PROFILE_CUSTOM_HPP
#define VSOMEIP_E2E_PROFILE_CUSTOM_PROFILE_CUSTOM_HPP

#include <cstdint>
#include "../../../buffer/buffer.hpp"

namespace vsomeip {
namespace e2e {
namespace profile_custom {

struct profile_config;

class profile_custom {
  public:
    static uint32_t compute_crc(const profile_config &_config, const e2e_buffer &_buffer);

    static bool is_buffer_length_valid(const profile_config &_config, const e2e_buffer &_buffer);
};

struct profile_config {
    uint16_t crc_offset_;

    profile_config() = delete;

    profile_config(uint16_t _crc_offset)
        : crc_offset_(_crc_offset) {
    }
    profile_config(const profile_config &_config) = default;
    profile_config &operator=(const profile_config &_config) = default;
};

} // namespace profile_custom
} // namespace e2e
} // namespace vsomeip

#endif // VSOMEIP_E2E_PROFILE_CUSTOM_PROFILE_CUSTOM_HPP

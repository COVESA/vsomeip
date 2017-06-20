// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef E2E_PROFILE_PROFILE01_PROFILE01_HPP
#define E2E_PROFILE_PROFILE01_PROFILE01_HPP

#include <cstdint>
#include "../../../buffer/buffer.hpp"

namespace e2e {
namespace profile {
namespace profile01 {

struct Config;

class profile_01 {
  public:
    static uint8_t compute_crc(const Config &_config, const buffer::e2e_buffer &_buffer);

    static bool is_buffer_length_valid(const Config &_config, const buffer::e2e_buffer &_buffer);
};

// [SWS_E2E_00200]
enum class p01_data_id_mode : uint8_t { E2E_P01_DATAID_BOTH, E2E_P01_DATAID_ALT, E2E_P01_DATAID_LOW, E2E_P01_DATAID_NIBBLE};

struct Config {
    // [SWS_E2E_00018]
    uint16_t crc_offset;
    uint16_t data_id;
    p01_data_id_mode data_id_mode;
    uint16_t data_length;
    uint16_t counter_offset;
    uint16_t data_id_nibble_offset;

#ifndef E2E_DEVELOPMENT
    Config() = delete;
#else
    Config() = default;
#endif
    Config(uint16_t _crc_offset, uint16_t _data_id, p01_data_id_mode _data_id_mode, uint16_t _data_length, uint16_t _counter_offset, uint16_t _data_id_nibble_offset)

        : crc_offset(_crc_offset), data_id(_data_id),
          data_id_mode(_data_id_mode), data_length(_data_length), counter_offset(_counter_offset), data_id_nibble_offset(_data_id_nibble_offset) {
    }
    Config(const Config &_config) = default;
    Config &operator=(const Config &_config) = default;
};
}
}
}

#endif

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef E2E_PROFILE_PROFILE_CUSTOM_PROFILE_CUSTOM_HPP
#define E2E_PROFILE_PROFILE_CUSTOM_PROFILE_CUSTOM_HPP

#include <cstdint>
#include "../../../buffer/buffer.hpp"

namespace e2e {
namespace profile {
namespace profile_custom {

struct Config;

class profile_custom {
  public:
    static uint32_t compute_crc(const Config &_config, const buffer::e2e_buffer &_buffer);

    static bool is_buffer_length_valid(const Config &_config, const buffer::e2e_buffer &_buffer);
};

struct Config {
    uint16_t crc_offset;

#ifndef E2E_DEVELOPMENT
    Config() = delete;
#else
    Config() = default;
#endif
    Config(uint16_t _crc_offset)

        : crc_offset(_crc_offset) {
    }
    Config(const Config &_config) = default;
    Config &operator=(const Config &_config) = default;
};
}
}
}

#endif

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../../../e2e_protection/include/e2e/profile/profile_custom/profile_custom.hpp"
#include "../../../../../e2e_protection/include/crc/crc.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

namespace e2e {
namespace profile {
namespace profile_custom {

uint32_t profile_custom::compute_crc(const Config &_config, const buffer::e2e_buffer &_buffer) {
    uint32_t computed_crc = crc::e2e_crc::calculate_profile_custom(buffer::buffer_view(_buffer, _config.crc_offset + 4, _buffer.size()));
    return computed_crc;
}

bool profile_custom::is_buffer_length_valid(const Config &_config, const buffer::e2e_buffer &_buffer) {

   return ( (_config.crc_offset + 4U) <=_buffer.size());
}

}
}
}

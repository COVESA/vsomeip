// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CRC_CRC_HPP
#define CRC_CRC_HPP

#include <cstdint>
#include "../buffer/buffer.hpp"

namespace crc {
class e2e_crc {
  public:
    static uint8_t calculate_profile_01(buffer::buffer_view _buffer_view,
                                      const uint8_t _start_value = 0x00U);
    static uint32_t calculate_profile_04(buffer::buffer_view _buffer_view,
                                       const uint32_t _start_value = 0x00000000U);

    static uint32_t calculate_profile_custom(buffer::buffer_view _buffer_view);

  private:
    static const uint8_t lookup_table_profile_01[256];
    static const uint32_t lookup_table_profile_04[256];
    static const uint32_t lookup_table_profile_custom[256];

};
}

#endif

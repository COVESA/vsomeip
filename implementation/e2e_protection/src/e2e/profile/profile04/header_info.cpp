// Copyright (C) 2025-2026 Valeo
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/*
** file   header_info.cpp
** brief  Implementation of header info interface to save header info for profile 04
*/

#include <vsomeip/internal/logger.hpp>

#include "../../../../include/e2e/profile/profile04/header_info.hpp"

namespace vsomeip_v3 {
namespace e2e {
namespace profile04 {

constexpr int HEADER_LENGTH=12;

profile_04_header_info::profile_04_header_info(length_t _offet)
    : header_len_(HEADER_LENGTH), crc_offset_(_offet) {}

length_t profile_04_header_info::get_e2e_header_offset() { return crc_offset_; }

length_t profile_04_header_info::get_e2e_header_length() { return header_len_; }

bool profile_04_header_info::should_remove_header() {
    return true;
}

} // namespace profile04
} // namespace e2e
} // namespace vsomeip_v3

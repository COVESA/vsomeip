// Copyright (C) 2025-2026 Valeo
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/*
** file   header_info.hpp
** brief  Implementation of header info interface to save header info for profile 04
*/

#ifndef VSOMEIP_V3_E2E_PROFILE04_HEADER_INFO_HPP
#define VSOMEIP_V3_E2E_PROFILE04_HEADER_INFO_HPP

#include <map>

#include "../profile04/profile_04.hpp"
#include "../profile_interface/header_info.hpp"

namespace vsomeip_v3 {
namespace e2e {
namespace profile04 {

class profile_04_header_info final : public e2e::profile_interface::profile_header_info {

public:
    profile_04_header_info(void) = delete;

    explicit profile_04_header_info(length_t _offet_);
    length_t get_e2e_header_offset() override;
    length_t get_e2e_header_length() override;
    bool should_remove_header() override;

private:
    length_t header_len_;
    length_t crc_offset_;
};

} // namespace profile_04
} // namespace e2e
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_E2E_PROFILE04_HEADER_INFO_HPP

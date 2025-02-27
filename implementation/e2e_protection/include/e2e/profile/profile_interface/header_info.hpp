// Copyright (C) 2025-2026 Valeo
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/*
** file   header_info.hpp
** brief  Interface to retrive header info
*/

#ifndef VSOMEIP_V3_E2E_PROFILE_INTERFACE_HEADER_INFO_HPP
#define VSOMEIP_V3_E2E_PROFILE_INTERFACE_HEADER_INFO_HPP

#include <cstdint>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {
namespace e2e {
namespace profile_interface {


class profile_header_info {
public:
    virtual ~profile_header_info() {}
    virtual length_t get_e2e_header_offset() = 0;
    virtual length_t get_e2e_header_length() = 0;
    virtual bool should_remove_header() = 0;
};

} // namespace profile_interface
} // namespace e2e
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_E2E_PROFILE_INTERFACE_HEADER_INFO_HPP

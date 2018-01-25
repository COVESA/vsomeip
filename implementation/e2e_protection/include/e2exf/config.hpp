// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_E2EXF_CONFIG_HPP
#define VSOMEIP_E2EXF_CONFIG_HPP

#include "../e2e/profile/profile_interface/checker.hpp"
#include "../e2e/profile/profile_interface/protector.hpp"

#include <memory>
#include <map>

namespace vsomeip {
namespace e2exf {

using session_id = uint16_t;
using instance_id = uint16_t;

using data_identifier = std::pair<session_id, instance_id>;

std::ostream &operator<<(std::ostream &_os, const e2exf::data_identifier &_data_identifier);

} // namespace e2exf
} // namespace vsomeip

#endif // VSOMEIP_E2EXF_CONFIG_HPP

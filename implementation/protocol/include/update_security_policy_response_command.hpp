// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "security_policy_response_command_base.hpp"

namespace vsomeip_v3 {
namespace protocol {

class update_security_policy_response_command : public security_policy_response_command_base {

public:
    update_security_policy_response_command();
};

} // namespace protocol
} // namespace vsomeip_v3

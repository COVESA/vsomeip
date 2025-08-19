// Copyright (C) 2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/update_security_policy_response_command.hpp"

namespace vsomeip_v3 {
namespace protocol {

update_security_policy_response_command::update_security_policy_response_command() :
    security_policy_response_command_base(id_e::UPDATE_SECURITY_POLICY_RESPONSE_ID) { }

} // namespace protocol
} // namespace vsomeip

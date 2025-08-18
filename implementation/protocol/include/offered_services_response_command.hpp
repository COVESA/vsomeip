// Copyright (C) 2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_PROTOCOL_OFFERED_SERVICES_RESPONSE_COMMAND_HPP_
#define VSOMEIP_V3_PROTOCOL_OFFERED_SERVICES_RESPONSE_COMMAND_HPP_

#include "multiple_services_command_base.hpp"

namespace vsomeip_v3 {
namespace protocol {

class offered_services_response_command : public multiple_services_command_base {
public:
    offered_services_response_command();
};

} // namespace protocol
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_PROTOCOL_OFFERED_SERVICES_RESPONSE_COMMAND_HPP_

// Copyright (C) 2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_PROTOCOL_SUBSCRIBE_NACK_COMMAND_HPP_
#define VSOMEIP_V3_PROTOCOL_SUBSCRIBE_NACK_COMMAND_HPP_

#include "subscribe_ack_command_base.hpp"

namespace vsomeip_v3 {
namespace protocol {

class subscribe_nack_command : public subscribe_ack_command_base {

public:
    subscribe_nack_command();
};

} // namespace protocol
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_PROTOCOL_SUBSCRIBE_NACK_COMMAND_HPP_

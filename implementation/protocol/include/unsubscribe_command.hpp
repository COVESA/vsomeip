// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include <vsomeip/structured_types.hpp>

#include "subscribe_command_base.hpp"

namespace vsomeip_v3 {
namespace protocol {

class unsubscribe_command : public subscribe_command_base {

public:
    unsubscribe_command();

    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);
};

} // namespace protocol
} // namespace vsomeip_v3

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include "command.hpp"

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif

namespace vsomeip_v3 {

struct policy;

namespace protocol {

class security_policy_response_command_base : public command {
public:
    security_policy_response_command_base(id_e _id);

    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);

    // specific
    uint32_t get_update_id() const;
    void set_update_id(uint32_t _update_id);

private:
    uint32_t update_id_;
};

} // namespace protocol
} // namespace vsomeip_v3

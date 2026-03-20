// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command.hpp"

namespace vsomeip_v3 {
namespace protocol {

class assign_client_ack_command : public command {
public:
    assign_client_ack_command();

    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);

    // specific
    client_t get_assigned() const;
    void set_assigned(client_t _assigned);

private:
    client_t assigned_;
};

client_t read_client_id(byte_t const* _data, uint32_t _size);

} // namespace protocol
} // namespace vsomeip_v3

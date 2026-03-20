// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string>

#include "command.hpp"

namespace vsomeip_v3 {
namespace protocol {

class assign_client_command : public command {
public:
    assign_client_command();

    // command
    id_e get_id() const;

    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);

    // specific
    std::string get_name() const;
    void set_name(const std::string& _name);

private:
    std::string name_;
};

} // namespace protocol
} // namespace vsomeip_v3

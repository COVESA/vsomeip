// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <set>

#include "command.hpp"

namespace vsomeip_v3 {
namespace protocol {

class multiple_services_command_base : public command {
public:
    multiple_services_command_base(id_e _id);

    // command
    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);

    // specific
    std::set<service> get_services() const;
    void set_services(const std::set<service>& _services);
    void add_service(const service& _service);

private:
    std::set<service> services_;
};

} // namespace protocol
} // namespace vsomeip_v3

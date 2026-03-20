// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command.hpp"

namespace vsomeip_v3 {
namespace protocol {

class offered_services_request_command : public command {

public:
    offered_services_request_command();

    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);

    offer_type_e get_offer_type() const;
    void set_offer_type(offer_type_e _offer_type);

private:
    offer_type_e offer_type_;
};

} // namespace protocol
} // namespace vsomeip_v3

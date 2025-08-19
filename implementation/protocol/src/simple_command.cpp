// Copyright (C) 2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <limits>

#include "../include/simple_command.hpp"

namespace vsomeip_v3 {
namespace protocol {

simple_command::simple_command(id_e _id) : command(_id) { }

void simple_command::serialize(std::vector<byte_t>& _buffer, error_e& _error) const {

    // no size check as we know this is small enough

    // resize buffer
    _buffer.resize(COMMAND_HEADER_SIZE);

    // set size
    size_ = 0;

    // serialize header
    command::serialize(_buffer, _error);
}

void simple_command::deserialize(const std::vector<byte_t>& _buffer, error_e& _error) {

    if (_buffer.size() < COMMAND_HEADER_SIZE) {

        _error = error_e::ERROR_NOT_ENOUGH_BYTES;
        return;
    }

    command::deserialize(_buffer, _error);
}

} // namespace protocol
} // namespace vsomeip

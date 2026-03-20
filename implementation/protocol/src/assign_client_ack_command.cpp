// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <limits>

#include "../include/assign_client_ack_command.hpp"

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

namespace vsomeip_v3 {
namespace protocol {

assign_client_ack_command::assign_client_ack_command() : command(id_e::ASSIGN_CLIENT_ACK_ID) { }

void assign_client_ack_command::serialize(std::vector<byte_t>& _buffer) const {

    size_t its_size(COMMAND_HEADER_SIZE + sizeof(assigned_));

    // resize buffer
    _buffer.resize(its_size);

    // set size
    size_ = static_cast<command_size_t>(sizeof(assigned_));

    // serialize header
    command::serialize(_buffer);

    // serialize payload
    std::memcpy(&_buffer[COMMAND_POSITION_PAYLOAD], &assigned_, sizeof(assigned_));
}

void assign_client_ack_command::deserialize(const std::vector<byte_t>& _buffer, error_e& _error) {

    if (COMMAND_HEADER_SIZE + sizeof(assigned_) > _buffer.size()) {

        _error = error_e::ERROR_NOT_ENOUGH_BYTES;
        return;
    }

    // deserialize header
    command::deserialize(_buffer, _error);
    if (_error != error_e::ERROR_OK)
        return;

    // deserialize payload
    std::memcpy(&assigned_, &_buffer[COMMAND_POSITION_PAYLOAD], sizeof(assigned_));
}

client_t assign_client_ack_command::get_assigned() const {

    return assigned_;
}

void assign_client_ack_command::set_assigned(client_t _assigned) {

    assigned_ = _assigned;
}

client_t read_client_id(byte_t const* _data, uint32_t _size) {
    client_t id{VSOMEIP_CLIENT_UNSET};
    if (_data[protocol::COMMAND_POSITION_ID] == static_cast<byte_t>(id_e::ASSIGN_CLIENT_ACK_ID)
        && COMMAND_POSITION_PAYLOAD + sizeof(id) == _size) {
        std::memcpy(&id, &_data[COMMAND_POSITION_PAYLOAD], sizeof(id));
    }
    return id;
}

} // namespace protocol
} // namespace vsomeip

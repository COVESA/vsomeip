// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>

#include "protocol.hpp"

#include <type_traits>

namespace vsomeip_v3::protocol {

struct command_header {
    auto operator<=>(command_header const&) const = default;

    static command_header create(id_e _id, uint32_t _length, client_t _client) {
        return {.id_ = _id, .version_ = IPC_VERSION, .client_ = _client, .length_ = _length};
    }

    id_e id_;
    version_t version_;
    client_t client_; // Sender's client identifier.
    uint32_t length_; // Payload length (bytes following the header).
};

template<typename T>
inline id_e get_id(T const& _cmd) {
    if constexpr (std::is_same_v<T, command_header>) {
        return _cmd.id_;
    } else {
        return _cmd.header_.id_;
    }
}

// Factory functions
inline command_header create_ping_cmd(client_t _sender_id) {
    return command_header::create(id_e::PING_ID, 0, _sender_id);
}
inline command_header create_pong_cmd(client_t _sender_id) {
    return command_header::create(id_e::PONG_ID, 0, _sender_id);
}
inline command_header create_suspend_cmd(client_t _sender_id) {
    return command_header::create(id_e::SUSPEND_ID, 0, _sender_id);
}

} // namespace vsomeip_v3::protocol

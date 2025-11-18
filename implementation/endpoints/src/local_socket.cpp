// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/local_socket.hpp"

namespace vsomeip_v3 {
char const* to_string(socket_role_e _role) {
    switch (_role) {
    case socket_role_e::SENDER:
        return "sender";
    case socket_role_e::RECEIVER:
        return "receiver";
    }
    return "unknown";
}
}

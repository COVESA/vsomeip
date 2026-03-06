// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string>
#include <memory>

namespace vsomeip_v3::testing {

/**
 * unique identifier type for any socket/acceptor.
 **/
using fd_t = unsigned short;

enum class socket_role { unspecified, client, server };
enum class socket_type { uds, tcp, udp };
struct socket_id {
    fd_t fd_{};
    socket_role role_{socket_role::unspecified};
    socket_type type_{socket_type::tcp};
    std::string app_name_{};
};

std::ostream& operator<<(std::ostream& o, socket_id const& _id);

class socket_manager;

struct fake_socket_handle : std::enable_shared_from_this<fake_socket_handle> {
    fake_socket_handle() = default;
    fake_socket_handle(fake_socket_handle const&) = delete;
    fake_socket_handle& operator=(fake_socket_handle const&) = delete;
    virtual ~fake_socket_handle() = default;

    virtual void init(fd_t _fd, socket_type _type, std::weak_ptr<socket_manager> _socket_manager) = 0;

    virtual void cancel() = 0;

    virtual void clear_handler() = 0;

    virtual void set_app_name(std::string const& _name) = 0;

    virtual std::string get_app_name() const = 0;
};

}

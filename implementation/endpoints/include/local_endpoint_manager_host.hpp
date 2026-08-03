// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>

#include <utility> // asio misses std::exchange
#include <boost/asio.hpp>

#include <cstdint>
#include <memory>

namespace vsomeip_v3 {

class local_endpoint;

/**
 * Role a local connection serves. A peer (client_t) can ride two sockets at
 * once (one per role); a failure on one must only affect that role's state.
 **/
enum class connection_role_e : uint8_t {
    // A connection WE opened towards a peer that offers a service we consume
    // (outbound consumer endpoint).
    consumer,
    // A connection ACCEPTED from a peer that consumes a service we offer
    // (accepted local server endpoint).
    provider
};

/**
 * An implementation is expected to not call into the endpoint manager in any
 * of the provided callbacks.
 **/
class local_endpoint_manager_host {
public:
    virtual ~local_endpoint_manager_host() = default;

    virtual void set_port(port_t _port) = 0;
    virtual client_t get_client_id() = 0;

    virtual void register_error_handler(client_t _client, std::shared_ptr<local_endpoint> _ep, connection_role_e _role) = 0;
};

} // namespace vsomeip_v3

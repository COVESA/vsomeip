// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>

#include <boost/asio.hpp>

#include <memory>

namespace vsomeip_v3 {

class local_endpoint;

/**
 * An implementation is expected to not call into the endpoint manager in any
 * of the provided callbacks.
 **/
class local_endpoint_manager_host {
public:
    virtual ~local_endpoint_manager_host() = default;

    virtual void set_port(port_t _port) = 0;
    virtual client_t get_client_id() = 0;

    [[nodiscard]] virtual bool get_connection_param(client_t _client, boost::asio::ip::address& _address, port_t& _port) = 0;
    virtual void add_connection_param(client_t _client, boost::asio::ip::address const& _address, port_t const& _port) = 0;

    virtual void register_error_handler(client_t _client, std::shared_ptr<local_endpoint> _ep) = 0;
};

} // namespace vsomeip_v3

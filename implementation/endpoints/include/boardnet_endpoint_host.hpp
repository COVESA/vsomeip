// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>

#include "internal.hpp"

namespace vsomeip_v3 {

class configuration;
class boardnet_endpoint;

class boardnet_endpoint_host {
public:
    virtual ~boardnet_endpoint_host() = default;

    virtual void on_connect(std::shared_ptr<boardnet_endpoint> _endpoint) = 0;
    virtual void on_disconnect(std::shared_ptr<boardnet_endpoint> _endpoint) = 0;
    virtual bool on_bind_error(std::shared_ptr<boardnet_endpoint> _endpoint, const boost::asio::ip::address& _remote_address,
                               uint16_t _remote_port, uint16_t& _local_port) = 0;
    /**
     * @brief Find instance for a given service+endpoint pair
     *
     * @return instance_t the instance id, or ANY_INSTANCE if not found
     */
    virtual instance_t find_instance(service_t _service, boardnet_endpoint* const _endpoint) const = 0;
};

} // namespace vsomeip_v3

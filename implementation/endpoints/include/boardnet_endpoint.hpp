// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/constants.hpp>

#include <boost/asio/ip/address.hpp>

#include <vector>

namespace vsomeip_v3 {

class endpoint_definition;

class boardnet_endpoint {
public:
    virtual ~boardnet_endpoint() = default;

    virtual void start() = 0;
    /**
     * @brief Stop endpoint
     *
     * @param _due_to_error if true, we are stopping due to an error - do not bother with graceful closure/ensuring message delivery/etc
     */
    virtual void stop(bool _due_to_error) = 0;
    virtual void restart(bool _force = false) = 0;

    virtual bool send(const byte_t* _data, uint32_t _size) = 0;
    virtual bool send_to(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) = 0;
    virtual bool send_error(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) = 0;
    virtual void receive() = 0;

    virtual void add_default_target(service_t _service, const std::string& _address, uint16_t _port) = 0;
    virtual void remove_default_target(service_t _service) = 0;

    virtual bool is_established() const = 0;
    virtual bool is_established_or_connected() const = 0;
    virtual bool is_reliable() const = 0;
    virtual bool is_local() const = 0;

    virtual std::uint16_t get_local_port() const = 0;

    virtual void print_status() = 0;
    virtual size_t get_queue_size() const = 0;

    virtual void set_established(bool _established) = 0;
    virtual void set_connected(bool _connected) = 0;
};

} // namespace vsomeip_v3

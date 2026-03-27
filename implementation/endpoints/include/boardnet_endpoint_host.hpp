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

struct multicast_option_t {
    std::shared_ptr<boardnet_endpoint> endpoint_;
    bool is_join_;
    boost::asio::ip::address address_;
};

class boardnet_endpoint_host {
public:
    virtual ~boardnet_endpoint_host() = default;

    virtual void on_connect(std::shared_ptr<boardnet_endpoint> _endpoint) = 0;
    virtual void on_disconnect(std::shared_ptr<boardnet_endpoint> _endpoint) = 0;
    virtual bool on_bind_error(std::shared_ptr<boardnet_endpoint> _endpoint, const boost::asio::ip::address& _remote_address,
                               uint16_t _remote_port, uint16_t& _local_port) = 0;
    virtual void on_error(const byte_t* _data, length_t _length, boardnet_endpoint* const _receiver,
                          const boost::asio::ip::address& _remote_address, std::uint16_t _remote_port) = 0;
    virtual client_t get_client() const = 0;
    virtual std::string get_client_host() const = 0;
    virtual instance_t find_instance(service_t _service, boardnet_endpoint* const _endpoint) const = 0;
    virtual void add_multicast_option(const multicast_option_t& _option) = 0;
};

} // namespace vsomeip_v3

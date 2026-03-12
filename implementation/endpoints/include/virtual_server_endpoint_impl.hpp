// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "boardnet_endpoint.hpp"

#include <boost/asio/io_context.hpp>

namespace vsomeip_v3 {

class virtual_server_endpoint_impl : public boardnet_endpoint, public std::enable_shared_from_this<virtual_server_endpoint_impl> {
public:
    virtual_server_endpoint_impl(const std::string& _address, uint16_t _port, bool _reliable, boost::asio::io_context& _io);

    virtual ~virtual_server_endpoint_impl();

    void start();
    void stop(bool _due_to_error);

    bool is_established() const;
    bool is_established_or_connected() const;
    void set_established(bool _established);
    void set_connected(bool _connected);

    bool send(const byte_t* _data, uint32_t _size);
    bool send_to(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size);
    bool send_error(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size);
    void receive();

    void add_default_target(service_t _service, const std::string& _address, uint16_t _port);
    void remove_default_target(service_t _service);

    bool get_remote_address(boost::asio::ip::address& _address) const;
    std::uint16_t get_local_port() const;
    std::uint16_t get_remote_port() const;
    bool is_reliable() const;
    bool is_local() const;

    void restart(bool _force);

    void print_status();

    size_t get_queue_size() const;

private:
    std::string address_;
    uint16_t port_;
    bool reliable_;
};

} // namespace vsomeip_v3

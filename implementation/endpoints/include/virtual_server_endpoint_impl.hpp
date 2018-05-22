// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_VIRTUAL_SERVER_ENDPOINT_IMPL_HPP
#define VSOMEIP_VIRTUAL_SERVER_ENDPOINT_IMPL_HPP

#include <vsomeip/primitive_types.hpp>

#include "../include/endpoint.hpp"

namespace vsomeip {

class virtual_server_endpoint_impl : public endpoint {
public:
    virtual_server_endpoint_impl(
            const std::string &_address,
            uint16_t _port,
            bool _reliable);

    virtual ~virtual_server_endpoint_impl();

    void start();
    void stop();

    bool is_connected() const;
    void set_connected(bool _connected);

    bool send(const byte_t *_data, uint32_t _size, bool _flush);
    bool send(const std::vector<byte_t>& _cmd_header, const byte_t *_data,
              uint32_t _size, bool _flush);
    bool send_to(const std::shared_ptr<endpoint_definition> _target,
            const byte_t *_data, uint32_t _size, bool _flush);
    void enable_magic_cookies();
    void receive();

    void join(const std::string &_address);
    void leave(const std::string &_address);

    void add_default_target(service_t _service,
            const std::string &_address, uint16_t _port);
    void remove_default_target(service_t _service);

    bool get_remote_address(boost::asio::ip::address &_address) const;
    std::uint16_t get_local_port() const;
    std::uint16_t get_remote_port() const;
    bool is_reliable() const;
    bool is_local() const;

    void increment_use_count();
    void decrement_use_count();
    uint32_t get_use_count();

    void restart(bool _force);

    void register_error_handler(error_handler_t _handler);
    void print_status();

private:
    std::string address_;
    uint16_t port_;
    bool reliable_;

    uint32_t use_count_;
};

} // namespace vsomeip

#endif // VSOMEIP_VIRTUAL_SERVER_ENDPOINT_IMPL_HPP

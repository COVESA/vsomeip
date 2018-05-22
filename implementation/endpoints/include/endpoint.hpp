// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENDPOINT_HPP
#define VSOMEIP_ENDPOINT_HPP

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>

#include <vector>

namespace vsomeip {

class endpoint_definition;

class endpoint {
public:
    typedef std::function<void()> error_handler_t;

    virtual ~endpoint() {}

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual bool is_connected() const = 0;

    virtual bool send(const byte_t *_data, uint32_t _size,
            bool _flush = true) = 0;
    virtual bool send(const std::vector<byte_t>& _cmd_header, const byte_t *_data,
              uint32_t _size, bool _flush = true) = 0;
    virtual bool send_to(const std::shared_ptr<endpoint_definition> _target,
            const byte_t *_data, uint32_t _size, bool _flush = true) = 0;
    virtual void enable_magic_cookies() = 0;
    virtual void receive() = 0;

    virtual void join(const std::string &_address) = 0;
    virtual void leave(const std::string &_address) = 0;

    virtual void add_default_target(service_t _service,
            const std::string &_address, uint16_t _port) = 0;
    virtual void remove_default_target(service_t _service) = 0;

    virtual std::uint16_t get_local_port() const = 0;
    virtual bool is_reliable() const = 0;
    virtual bool is_local() const = 0;

    virtual void increment_use_count() = 0;
    virtual void decrement_use_count() = 0;
    virtual uint32_t get_use_count() = 0;

    virtual void restart(bool _force = false) = 0;

    virtual void register_error_handler(error_handler_t _error) = 0;

    virtual void print_status() = 0;

    virtual void set_connected(bool _connected) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_HPP

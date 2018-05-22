// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include "../include/virtual_server_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"

namespace vsomeip {

virtual_server_endpoint_impl::virtual_server_endpoint_impl(
        const std::string &_address, uint16_t _port, bool _reliable)
    : address_(_address), port_(_port), reliable_(_reliable), use_count_(0) {
}

virtual_server_endpoint_impl::~virtual_server_endpoint_impl() {
}

void virtual_server_endpoint_impl::start() {
}

void virtual_server_endpoint_impl::stop() {
}

bool virtual_server_endpoint_impl::is_connected() const {
    return false;
}

void virtual_server_endpoint_impl::set_connected(bool _connected) {
    (void) _connected;
}

bool virtual_server_endpoint_impl::send(const byte_t *_data, uint32_t _size,
        bool _flush) {
    (void)_data;
    (void)_size;
    (void)_flush;
    return false;
}

bool virtual_server_endpoint_impl::send(const std::vector<byte_t>& _cmd_header,
                                      const byte_t *_data, uint32_t _size,
                                      bool _flush) {
    (void)_cmd_header;
    (void)_data;
    (void)_size;
    (void)_flush;
    return false;
}

bool virtual_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data, uint32_t _size, bool _flush) {
    (void)_target;
    (void)_data;
    (void)_size;
    (void)_flush;
    return false;
}

void virtual_server_endpoint_impl::enable_magic_cookies() {
}

void virtual_server_endpoint_impl::receive() {
}

void virtual_server_endpoint_impl::join(const std::string &_address) {
    (void)_address;
}

void virtual_server_endpoint_impl::leave(const std::string &_address) {
    (void)_address;
}

void virtual_server_endpoint_impl::add_default_target(
        service_t _service,
        const std::string &_address, uint16_t _port) {
    (void)_service;
    (void)_address;
    (void)_port;
}

void virtual_server_endpoint_impl::remove_default_target(
        service_t _service) {
    (void)_service;
}

bool virtual_server_endpoint_impl::get_remote_address(
        boost::asio::ip::address &_address) const {
    (void)_address;
    return false;
}

std::uint16_t virtual_server_endpoint_impl::get_local_port() const {
    return port_;
}

std::uint16_t virtual_server_endpoint_impl::get_remote_port() const {
    return ILLEGAL_PORT;
}

bool virtual_server_endpoint_impl::is_reliable() const {
    return reliable_;
}

bool virtual_server_endpoint_impl::is_local() const {
    return true;
}


void virtual_server_endpoint_impl::increment_use_count() {
    use_count_++;
}

void virtual_server_endpoint_impl::decrement_use_count() {
    if (use_count_ > 0)
        use_count_--;
}

uint32_t virtual_server_endpoint_impl::get_use_count() {
    return use_count_;
}

void virtual_server_endpoint_impl::restart(bool _force) {
    (void)_force;
}

void virtual_server_endpoint_impl::register_error_handler(
        error_handler_t _handler) {
    (void)_handler;
}

void virtual_server_endpoint_impl::print_status() {

}

} // namespace vsomeip

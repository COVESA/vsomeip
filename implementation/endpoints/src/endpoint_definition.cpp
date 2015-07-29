// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include "../include/endpoint_definition.hpp"

namespace vsomeip {

endpoint_definition::endpoint_definition()
        : port_(ILLEGAL_PORT) {
}

endpoint_definition::endpoint_definition(
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _is_reliable)
        : address_(_address), port_(_port), is_reliable_(_is_reliable), remote_port_(_port) {
}

const boost::asio::ip::address & endpoint_definition::get_address() const {
    return address_;
}

void endpoint_definition::set_address(
        const boost::asio::ip::address &_address) {
    address_ = _address;
}

uint16_t endpoint_definition::get_port() const {
    return port_;
}

void endpoint_definition::set_port(uint16_t _port) {
    port_ = _port;
}

bool endpoint_definition::is_reliable() const {
    return is_reliable_;
}

void endpoint_definition::set_reliable(bool _is_reliable) {
    is_reliable_ = _is_reliable;
}

uint16_t endpoint_definition::get_remote_port() const {
    return remote_port_;
}

void endpoint_definition::set_remote_port(uint16_t _port) {
    remote_port_ = _port;
}


} // namespace vsomeip

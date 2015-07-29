// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include "../include/constants.hpp"
#include "../include/ipv4_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

ipv4_option_impl::ipv4_option_impl(bool _is_multicast) {
    length_ = (1 + 4 + 1 + 1 + 2);
    type_ = (
            _is_multicast ?
                    option_type_e::IP4_MULTICAST : option_type_e::IP4_ENDPOINT);
    is_udp_ = _is_multicast;
}

ipv4_option_impl::~ipv4_option_impl() {
}

bool ipv4_option_impl::operator ==(const option_impl &_other) const {
    if (type_ != _other.get_type())
        return false;

    const ipv4_option_impl & other =
            dynamic_cast<const ipv4_option_impl &>(_other);
    return true;
}

const ipv4_address_t & ipv4_option_impl::get_address() const {
    return address_;
}

void ipv4_option_impl::set_address(const ipv4_address_t &_address) {
    address_ = _address;
}

unsigned short ipv4_option_impl::get_port() const {
    return port_;
}

void ipv4_option_impl::set_port(unsigned short _port) {
    port_ = _port;
}

bool ipv4_option_impl::is_udp() const {
    return is_udp_;
}

void ipv4_option_impl::set_udp(bool _is_udp) {
    is_udp_ = _is_udp;
}

bool ipv4_option_impl::is_multicast() const {
    return (type_ == option_type_e::IP4_MULTICAST);
}

bool ipv4_option_impl::serialize(vsomeip::serializer *_to) const {
    bool is_successful = option_impl::serialize(_to);
    _to->serialize(&address_[0], address_.size());
    _to->serialize(protocol::reserved_byte);
    _to->serialize(is_udp_ ? protocol::udp : protocol::tcp);
    _to->serialize(port_);
    return is_successful;
}

bool ipv4_option_impl::deserialize(vsomeip::deserializer *_from) {
    bool is_successful = option_impl::deserialize(_from);
    uint8_t its_reserved;
    _from->deserialize(address_.data(), 4);
    _from->deserialize(its_reserved);
    _from->deserialize(its_reserved);
    is_udp_ = (protocol::udp == its_reserved);
    _from->deserialize(port_);
    return is_successful;
}

} // namespace sd
} // namespace vsomeip


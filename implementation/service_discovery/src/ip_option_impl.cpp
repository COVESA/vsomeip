// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include "../include/constants.hpp"
#include "../include/ip_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

ip_option_impl::ip_option_impl() :
        protocol_(layer_four_protocol_e::UNKNOWN),
        port_(0xFFFF) {
}

ip_option_impl::~ip_option_impl() {
}

bool ip_option_impl::operator ==(const ip_option_impl &_other) const {
    return (option_impl::operator ==(_other)
            && protocol_ == _other.protocol_
            && port_ == _other.port_);
}

unsigned short ip_option_impl::get_port() const {
    return port_;
}

void ip_option_impl::set_port(unsigned short _port) {
    port_ = _port;
}

layer_four_protocol_e ip_option_impl::get_layer_four_protocol() const {
    return protocol_;
}

void ip_option_impl::set_layer_four_protocol(
        layer_four_protocol_e _protocol) {
    protocol_ = _protocol;
}

} // namespace sd
} // namespace vsomeip


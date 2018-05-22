// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include "../include/constants.hpp"
#include "../include/defines.hpp"
#include "../include/ipv4_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

ipv4_option_impl::ipv4_option_impl(bool _is_multicast) :
        address_({0}) {
    length_ = (1 + 4 + 1 + 1 + 2);
    type_ = (
            _is_multicast ?
                    option_type_e::IP4_MULTICAST : option_type_e::IP4_ENDPOINT);
}

ipv4_option_impl::~ipv4_option_impl() {
}

bool ipv4_option_impl::operator ==(const ipv4_option_impl &_other) const {
    return (ip_option_impl::operator ==(_other)
            && address_ == _other.address_);
}

const ipv4_address_t & ipv4_option_impl::get_address() const {
    return address_;
}

void ipv4_option_impl::set_address(const ipv4_address_t &_address) {
    address_ = _address;
}

bool ipv4_option_impl::is_multicast() const {
    return (type_ == option_type_e::IP4_MULTICAST);
}

bool ipv4_option_impl::serialize(vsomeip::serializer *_to) const {
    bool is_successful = option_impl::serialize(_to);
    _to->serialize(&address_[0], uint32_t(address_.size()));
    _to->serialize(protocol::reserved_byte);
    _to->serialize(static_cast<uint8_t>(protocol_));
    _to->serialize(port_);
    return is_successful;
}

bool ipv4_option_impl::deserialize(vsomeip::deserializer *_from) {
    bool is_successful = option_impl::deserialize(_from)
                            && length_ == VSOMEIP_SD_IPV4_OPTION_LENGTH;
    uint8_t its_reserved(static_cast<std::uint8_t>(layer_four_protocol_e::UNKNOWN));
    _from->deserialize(address_.data(), 4);
    _from->deserialize(its_reserved);
    _from->deserialize(its_reserved);
    switch (static_cast<layer_four_protocol_e>(its_reserved)) {
        case layer_four_protocol_e::TCP:
        case layer_four_protocol_e::UDP:
            protocol_ = static_cast<layer_four_protocol_e>(its_reserved);
            break;
        default:
            protocol_ = layer_four_protocol_e::UNKNOWN;
    }
    _from->deserialize(port_);
    return is_successful;
}

} // namespace sd
} // namespace vsomeip


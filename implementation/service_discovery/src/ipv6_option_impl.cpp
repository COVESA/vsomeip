// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>

#include "../include/constants.hpp"
#include "../include/defines.hpp"
#include "../include/ipv6_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip_v3 {
namespace sd {

ipv6_option_impl::ipv6_option_impl()
    : address_({0}) {
    length_ = (1 + 16 + 1 + 1 + 2);
}

ipv6_option_impl::ipv6_option_impl(const boost::asio::ip::address &_address,
        const uint16_t _port, const bool _is_reliable)
    : ip_option_impl(_port, _is_reliable), address_(_address.to_v6().to_bytes()) {
    type_ = (_address.is_multicast() ?
        option_type_e::IP6_MULTICAST : option_type_e::IP6_ENDPOINT);
    length_ = (1 + 16 + 1 + 1 + 2);
}

ipv6_option_impl::~ipv6_option_impl() {
}

bool
ipv6_option_impl::equals(const option_impl &_other) const {
    bool is_equal(ip_option_impl::equals(_other));

    if (is_equal) {
        const ipv6_option_impl &its_other
            = dynamic_cast<const ipv6_option_impl &>(_other);
        is_equal = (address_ == its_other.address_);
    }

    return is_equal;
}

const ipv6_address_t & ipv6_option_impl::get_address() const {
    return address_;
}

void ipv6_option_impl::set_address(const ipv6_address_t &_address) {
    address_ = _address;

    boost::asio::ip::address_v6 its_address(_address);
    type_ = (its_address.is_multicast() ?
            option_type_e::IP6_MULTICAST : option_type_e::IP6_ENDPOINT);
}

bool ipv6_option_impl::is_multicast() const {
    return (type_ == option_type_e::IP6_MULTICAST);
}

bool ipv6_option_impl::serialize(vsomeip_v3::serializer *_to) const {
    bool is_successful = option_impl::serialize(_to);
    _to->serialize(&address_[0], uint32_t(address_.size()));
    _to->serialize(protocol::reserved_byte);
    _to->serialize(static_cast<uint8_t>(protocol_));
    _to->serialize(port_);
    return is_successful;
}

bool ipv6_option_impl::deserialize(vsomeip_v3::deserializer *_from) {
    bool is_successful = option_impl::deserialize(_from)
                            && length_ == VSOMEIP_SD_IPV6_OPTION_LENGTH;
    uint8_t its_reserved(static_cast<std::uint8_t>(layer_four_protocol_e::UNKNOWN));
    _from->deserialize(address_.data(), 16);
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
} // namespace vsomeip_v3

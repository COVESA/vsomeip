// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_IPV6_OPTION_IMPL_HPP
#define VSOMEIP_SD_IPV6_OPTION_IMPL_HPP

#include <vector>

#include <vsomeip/primitive_types.hpp>

#include "option_impl.hpp"

namespace vsomeip {
namespace sd {

class ipv6_option_impl: public option_impl {
public:
    ipv6_option_impl(bool _is_multicast);
    virtual ~ipv6_option_impl();
    bool operator ==(const option_impl &_option) const;

    const ipv6_address_t & get_address() const;
    void set_address(const ipv6_address_t &_address);

    unsigned short get_port() const;
    void set_port(unsigned short _port);

    bool is_udp() const;
    void set_udp(bool _is_udp);

    bool is_multicast() const;

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

protected:
    ipv6_address_t address_;
    unsigned short port_;
    bool is_udp_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_IPV6_OPTION_IMPL_HPP

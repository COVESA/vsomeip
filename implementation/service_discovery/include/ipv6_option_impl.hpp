// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_IPV6_OPTION_IMPL_HPP
#define VSOMEIP_SD_IPV6_OPTION_IMPL_HPP

#include <vsomeip/primitive_types.hpp>

#include "ip_option_impl.hpp"

namespace vsomeip {
namespace sd {

class ipv6_option_impl: public ip_option_impl {
public:
    ipv6_option_impl(bool _is_multicast);
    virtual ~ipv6_option_impl();
    bool operator ==(const ipv6_option_impl &_other) const;

    const ipv6_address_t & get_address() const;
    void set_address(const ipv6_address_t &_address);

    bool is_multicast() const;
    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    ipv6_address_t address_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_IPV6_OPTION_IMPL_HPP

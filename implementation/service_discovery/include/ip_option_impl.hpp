// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_IP_OPTION_IMPL_HPP
#define VSOMEIP_SD_IP_OPTION_IMPL_HPP

#include <vsomeip/primitive_types.hpp>

#include "option_impl.hpp"

namespace vsomeip {
namespace sd {

class ip_option_impl: public option_impl {
public:
    ip_option_impl();
    virtual ~ip_option_impl();
    virtual bool operator ==(const ip_option_impl &_option) const;

    uint16_t get_port() const;
    void set_port(uint16_t _port);

    layer_four_protocol_e get_layer_four_protocol() const;
    void set_layer_four_protocol(layer_four_protocol_e _protocol);

    virtual bool is_multicast() const = 0;

    virtual bool serialize(vsomeip::serializer *_to) const = 0;
    virtual bool deserialize(vsomeip::deserializer *_from) = 0;

protected:
    layer_four_protocol_e protocol_;
    uint16_t port_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_IP_OPTION_IMPL_HPP

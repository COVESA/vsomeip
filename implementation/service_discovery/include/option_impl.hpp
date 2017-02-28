// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_OPTION_IMPL_HPP
#define VSOMEIP_SD_OPTION_IMPL_HPP

#include <cstdint>

#include "enumeration_types.hpp"
#include "message_element_impl.hpp"

namespace vsomeip {

class serializer;
class deserializer;

namespace sd {

class message_impl;

class option_impl: public message_element_impl {
public:
    option_impl();
    virtual ~option_impl();

    virtual bool operator ==(const option_impl &_other) const;

    uint16_t get_length() const;
    option_type_e get_type() const;

    virtual bool serialize(vsomeip::serializer *_to) const;
    virtual bool deserialize(vsomeip::deserializer *_from);

protected:
    uint16_t length_;
    option_type_e type_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_OPTION_IMPL_HPP

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_LOAD_BALANCING_OPTION_IMPL_HPP
#define VSOMEIP_SD_LOAD_BALANCING_OPTION_IMPL_HPP

#include "primitive_types.hpp"
#include "option_impl.hpp"

namespace vsomeip {
namespace sd {

class load_balancing_option_impl: public option_impl {
public:
    load_balancing_option_impl();
    virtual ~load_balancing_option_impl();
    bool operator ==(const load_balancing_option_impl &_other) const;

    priority_t get_priority() const;
    void set_priority(priority_t _priority);

    weight_t get_weight() const;
    void set_weight(weight_t _weight);

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    priority_t priority_;
    weight_t weight_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_LOAD_BALANCING_OPTION_IMPL_HPP

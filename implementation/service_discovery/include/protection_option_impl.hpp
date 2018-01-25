// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_PROTECTION_OPTION_IMPL_HPP
#define VSOMEIP_SD_PROTECTION_OPTION_IMPL_HPP

#include "../include/primitive_types.hpp"
#include "../include/option_impl.hpp"

namespace vsomeip {
namespace sd {

class protection_option_impl: public option_impl {
public:
    protection_option_impl();
    virtual ~protection_option_impl();
    bool operator ==(const protection_option_impl &_other) const;

    alive_counter_t get_alive_counter() const;
    void set_alive_counter(alive_counter_t _counter);

    crc_t get_crc() const;
    void set_crc(crc_t _crc);

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    alive_counter_t counter_;
    crc_t crc_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_PROTECTION_OPTION_IMPL_HPP

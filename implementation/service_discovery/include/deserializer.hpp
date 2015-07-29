// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_DESERIALIZER_HPP
#define VSOMEIP_SD_DESERIALIZER_HPP

#include "../../message/include/deserializer.hpp"

namespace vsomeip {
namespace sd {

class message_impl;

class deserializer: public vsomeip::deserializer {
public:
    deserializer();
    deserializer(uint8_t *_data, std::size_t _length);
    deserializer(const deserializer &_other);
    virtual ~deserializer();

    message_impl * deserialize_sd_message();
};

} // namespace sd
} // vsomeip

#endif // VSOMEIP_SD_DESERIALIZER_HPP

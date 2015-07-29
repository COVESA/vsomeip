// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/deserializer.hpp"
#include "../include/message_impl.hpp"

namespace vsomeip {
namespace sd {

deserializer::deserializer()
        : vsomeip::deserializer() {
}

deserializer::deserializer(uint8_t *_data, std::size_t _length)
        : vsomeip::deserializer(_data, _length) {
}

deserializer::deserializer(const deserializer &_other)
        : vsomeip::deserializer(_other) {
}

deserializer::~deserializer() {
}

message_impl * deserializer::deserialize_sd_message() {
    message_impl* deserialized_message = new message_impl;
    if (0 != deserialized_message) {
        if (false == deserialized_message->deserialize(this)) {
            delete deserialized_message;
            deserialized_message = 0;
        }
    }

    return deserialized_message;
}

} // namespace sd
} // namespace vsomeip

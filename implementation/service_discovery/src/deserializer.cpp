// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/deserializer.hpp"
#include "../include/message_impl.hpp"

namespace vsomeip_v3 {
namespace sd {

deserializer::deserializer(uint32_t _shrink_buffer_threshold) : vsomeip_v3::deserializer(_shrink_buffer_threshold) { }

deserializer::deserializer(uint8_t* _data, size_t _length, uint32_t _shrink_buffer_threshold) :
    vsomeip_v3::deserializer(_data, _length, _shrink_buffer_threshold) { }

deserializer::deserializer(const deserializer& _other) : vsomeip_v3::deserializer(_other) { }

deserializer::~deserializer() { }

std::shared_ptr<message_impl> deserializer::deserialize_sd_message() {
    auto deserialized_message = std::make_shared<message_impl>();
    if (!deserialized_message->deserialize(this)) {
        deserialized_message.reset();
    }

    return deserialized_message;
}

} // namespace sd
} // namespace vsomeip_v3

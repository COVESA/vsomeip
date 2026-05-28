// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command_types.hpp"

#include <cstring> // memcpy

namespace vsomeip_v3 {
namespace protocol {

template<typename T>
uint32_t write_field(unsigned char* _mem, T const& _value) {
    std::memcpy(_mem, &_value, sizeof(T));
    return sizeof(T);
}

template<typename... Ts>
uint32_t write_fields(unsigned char* _mem, Ts const&... _fields) {
    uint32_t written{0};
    ((written += write_field(_mem + written, _fields)), ...);
    return written;
}

constexpr uint32_t wire_size(command_header const&) {
    return sizeof(id_e) + sizeof(version_t) + sizeof(client_t) + sizeof(uint32_t);
}

inline uint32_t serialize(command_header const& _in, unsigned char* _mem) {
    return write_fields(_mem, _in.id_, _in.version_, _in.client_, _in.length_);
}

} // namespace protocol
} // namespace vsomeip_v3

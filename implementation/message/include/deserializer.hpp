// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vector>
#include <memory>

#include <vsomeip/primitive_types.hpp>

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif

namespace vsomeip_v3 {

class message_impl;

class deserializer {
public:
    deserializer(uint32_t _buffer_shrink_threshold);
    deserializer(byte_t* _data, size_t _length, uint32_t _buffer_shrink_threshold);
    deserializer(const deserializer& _other);
    virtual ~deserializer();

    void set_data(const byte_t* _data, size_t _length);
    void set_data(const std::vector<byte_t>& _data);
    void append_data(const byte_t* _data, size_t _length);
    void drop_data(size_t _length);

    size_t get_available() const;
    size_t get_remaining() const;
    void set_remaining(size_t _remaining);

    // to be used by applications to deserialize a message
    std::shared_ptr<message_impl> deserialize_message();

    // to be used (internally) by objects to deserialize their members
    // Note: this needs to be encapsulated!
    bool deserialize(uint8_t& _value);
    bool deserialize(uint16_t& _value);
    bool deserialize(uint32_t& _value, bool _omit_last_byte = false);
    bool deserialize(uint8_t* _data, size_t _length);
    bool deserialize(std::string& _target, size_t _length);
    bool deserialize(std::vector<uint8_t>& _value);

    bool look_ahead(size_t _index, uint8_t& _value) const;
    bool look_ahead(size_t _index, uint16_t& _value) const;
    bool look_ahead(size_t _index, uint32_t& _value) const;

    void reset();

protected:
    std::vector<byte_t> data_;
    std::vector<byte_t>::iterator position_;
    size_t remaining_;

private:
    const uint32_t buffer_shrink_threshold_;
    uint32_t shrink_count_;
};

} // namespace vsomeip_v3

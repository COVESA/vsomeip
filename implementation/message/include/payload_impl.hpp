// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/payload.hpp>

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif

namespace vsomeip_v3 {

class serializer;
class deserializer;

class payload_impl : public payload {
public:
    payload_impl();
    virtual ~payload_impl() = default;

    bool operator==(const payload& _other) const override;

    byte_t* get_data() override;
    const byte_t* get_data() const override;
    length_t get_length() const override;

    void set_capacity(length_t _capacity) override;

    void set_data(const byte_t* _data, length_t _length) override;
    void set_data(const std::vector<byte_t>& _data) override;
    void set_data(std::vector<byte_t>&& _data) override;

    bool serialize(serializer* _to) const override;
    bool deserialize(deserializer* _from) override;

public:
    // not exported, these are internal to the codebase
    payload_impl(const byte_t* _data, uint32_t _size);
    payload_impl(const std::vector<byte_t>& _data);
    payload_impl(const payload_impl& _payload);

private:
    std::vector<byte_t> data_;
};

} // namespace vsomeip_v3

// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_PAYLOAD_HPP
#define VSOMEIP_PAYLOAD_HPP

#include <vector>

#include <vsomeip/export.hpp>
#include <vsomeip/deserializable.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

class payload: public serializable, public deserializable {
public:
    VSOMEIP_EXPORT virtual ~payload() {
    }

    VSOMEIP_EXPORT virtual bool operator ==(const payload &_other) = 0;

    VSOMEIP_EXPORT virtual byte_t * get_data() = 0;
    VSOMEIP_EXPORT virtual const byte_t * get_data() const = 0;
    VSOMEIP_EXPORT virtual void set_data(const byte_t *_data,
            length_t _length) = 0;
    VSOMEIP_EXPORT virtual void set_data(
            const std::vector<byte_t> &_data) = 0;

    VSOMEIP_EXPORT virtual length_t get_length() const = 0;

    VSOMEIP_EXPORT virtual void set_capacity(length_t _length) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_PAYLOAD_HPP

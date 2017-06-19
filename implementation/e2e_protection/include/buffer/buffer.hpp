// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef BUFFER_BUFFER_HPP
#define BUFFER_BUFFER_HPP

#include <stdexcept>
#include <cstdint>
#include <ostream>
#include <vector>

namespace buffer {

using e2e_buffer = std::vector<uint8_t>;

class buffer_view {
  public:
    buffer_view(const uint8_t *_data_ptr, size_t _data_length) : data_ptr(_data_ptr), data_length(_data_length) {

    }

    buffer_view(const buffer::e2e_buffer &_buffer) : data_ptr(_buffer.data()), data_length(_buffer.size()) {}

    buffer_view(const buffer::e2e_buffer &_buffer, size_t _length) : data_ptr(_buffer.data()), data_length(_length) {

    }

     buffer_view(const buffer::e2e_buffer &_buffer, size_t _begin, size_t _end)
        : data_ptr(_buffer.data() + _begin), data_length(_end - _begin) {

    }

    const uint8_t *begin(void) const { return data_ptr; }

    const uint8_t *end(void) const { return data_ptr + data_length; }

private:
    const uint8_t *data_ptr;
    size_t data_length;
};
}

std::ostream &operator<<(std::ostream &_os, const buffer::e2e_buffer &_buffer);

#endif

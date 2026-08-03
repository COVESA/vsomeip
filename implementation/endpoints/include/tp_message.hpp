// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <set>
#include <chrono>
#include <compare>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>

#include "buffer.hpp"

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif
namespace vsomeip_v3 {
namespace tp {

class tp_message {
public:
    tp_message(const byte_t* const _data, uint32_t _data_length, uint32_t _max_message_size);

    bool add_segment(const byte_t* const _data, uint32_t _data_length);

    message_buffer_t get_message();

    std::chrono::steady_clock::time_point get_creation_time() const;

private:
    std::string get_message_id(const byte_t* const _data, uint32_t _data_length);
    bool check_lengths(const byte_t* const _data, uint32_t _data_length, length_t _segment_size, bool _more_fragments);

private:
    std::chrono::steady_clock::time_point timepoint_creation_;
    uint32_t max_message_size_;
    uint32_t current_message_size_;
    bool last_segment_received_;
    bool header_received_;

    struct segment_t {
        segment_t(uint32_t _start, uint32_t _end) : start_(_start), end_(_end) { }

        auto operator<=>(const segment_t& _other) const = default;

        uint32_t start_;
        uint32_t end_;
    };
    std::set<segment_t> segments_;
    message_buffer_t message_;
};

} // namespace tp
} // namespace vsomeip_v3

// Copyright (C) 2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TC_TRACE_HEADER_HPP
#define VSOMEIP_TC_TRACE_HEADER_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>

#define VSOMEIP_TRACE_HEADER_SIZE 8

namespace vsomeip {

class endpoint;

namespace tc {

enum class protocol_e : uint8_t {
    local = 0x0,
    udp = 0x1,
    tcp = 0x2,
    unknown = 0xFF
};

struct trace_header {
    bool prepare(const std::shared_ptr<endpoint> &_endpoint, bool _is_sending);
    bool prepare(const endpoint* _endpoint, bool _is_sending);

    byte_t data_[VSOMEIP_TRACE_HEADER_SIZE];
};

} // namespace tc
} // namespace vsomeip

#endif // VSOMEIP_TC_TRACE_HEADER_HPP

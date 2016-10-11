// Copyright (C) 2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>

#include "../include/trace_header.hpp"
#include "../../endpoints/include/endpoint.hpp"
#include "../../utility/include/byteorder.hpp"

namespace vsomeip {
namespace tc {

bool trace_header::prepare(const std::shared_ptr<endpoint> &_endpoint, bool _is_sending) {
    return prepare(_endpoint.get(), _is_sending);
}

bool trace_header::prepare(const endpoint *_endpoint, bool _is_sending) {
    boost::asio::ip::address its_address;
    if (_endpoint && _endpoint->get_remote_address(its_address)) {
        if (its_address.is_v6())
            return false;

        unsigned long its_address_as_long = its_address.to_v4().to_ulong();

        data_[0] = VSOMEIP_LONG_BYTE0(its_address_as_long);
        data_[1] = VSOMEIP_LONG_BYTE1(its_address_as_long);
        data_[2] = VSOMEIP_LONG_BYTE2(its_address_as_long);
        data_[3] = VSOMEIP_LONG_BYTE3(its_address_as_long);

        unsigned short its_port = _endpoint->get_remote_port();
        data_[4] = VSOMEIP_WORD_BYTE0(its_port);
        data_[5] = VSOMEIP_WORD_BYTE1(its_port);

        if (_endpoint->is_local()) {
            data_[6] = static_cast<byte_t>(protocol_e::local);
        } else {
            if (_endpoint->is_reliable()) {
                data_[6] = static_cast<byte_t>(protocol_e::tcp);
            } else {
                data_[6] = static_cast<byte_t>(protocol_e::udp);
            }
        }

        data_[7] = static_cast<byte_t>(_is_sending);

    } else {
        std::memset(data_, 0, VSOMEIP_TRACE_HEADER_SIZE);
    }
    return true;
}

} // namespace tc
} // namespace vsomeip

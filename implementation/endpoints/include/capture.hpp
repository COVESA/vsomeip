// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_CAPTURE_HPP_
#define VSOMEIP_V3_CAPTURE_HPP_

#include <cstddef>
#include <optional>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

enum class capture_direction_e {
    RX,
    TX
};

enum class capture_transport_e {
    UDP,
    TCP
};

struct capture_metadata_t {
    capture_direction_e direction;
    capture_transport_e transport;
    boost::asio::ip::address local_address;
    port_t local_port;
    boost::asio::ip::address remote_address;
    port_t remote_port;
    std::optional<boost::asio::ip::address> destination_address;
};

void capture(const byte_t* _bytes, std::size_t _len, const capture_metadata_t& _metadata);

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_CAPTURE_HPP_

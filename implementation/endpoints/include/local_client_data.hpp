// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "internal.hpp"

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/vsomeip_sec.h>

#include <boost/asio/ip/address.hpp>

#include <string>

namespace vsomeip_v3 {

struct local_client_data {
    client_t id_{VSOMEIP_CLIENT_UNSET};
    std::string env_;
    vsomeip_sec_client_t sec_client_{};
    /// TCP endpoint the peer advertised during the assign_client handshake (server-side, routing clients only).
    boost::asio::ip::address routing_address_{};
    port_t routing_port_{ILLEGAL_PORT};
};

}

// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SERVER_ENDPOINT_HPP_
#define VSOMEIP_V3_SERVER_ENDPOINT_HPP_

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

/// @brief An endpoint to which clients connect.
class server_endpoint {
public:
    /// @brief Destructor.
    virtual ~server_endpoint() = default;

    /// @brief Disconnects from the given client.
    ///
    /// @param _client ID of the remote client.
    virtual void disconnect_from(const client_t _client) = 0;
};

}

#endif //  VSOMEIP_V3_SERVER_ENDPOINT_HPP_

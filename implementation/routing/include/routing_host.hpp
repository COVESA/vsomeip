// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ROUTING_HOST_
#define VSOMEIP_V3_ROUTING_HOST_

#include <memory>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

namespace vsomeip_v3 {

class endpoint;

class routing_host {
public:
    virtual ~routing_host() = default;

    virtual void on_message(const byte_t *_data, length_t _length,
                            endpoint *_receiver,
                            const boost::asio::ip::address &_destination =
                                    boost::asio::ip::address(),
                            client_t _bound_client = VSOMEIP_ROUTING_CLIENT,
                            credentials_t _credentials = {ANY_UID, ANY_GID},
                            const boost::asio::ip::address &_remote_address =
                                    boost::asio::ip::address(),
                            std::uint16_t _remote_port = 0) = 0;

    virtual client_t get_client() const = 0;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_ROUTING_HOST_

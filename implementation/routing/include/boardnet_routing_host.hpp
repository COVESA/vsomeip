// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/vsomeip_sec.h>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

namespace vsomeip_v3 {

class boardnet_endpoint;

class boardnet_routing_host {
public:
    virtual ~boardnet_routing_host() = default;

    /**
     * @brief Receive a message from the boardnet
     *
     * Whether it is TCP or UDP depends on `_receiver->reliable()`, otherwise the parameters are straightforward
     *
     * @param _data pointer to message
     * @param _length length of message
     * @param _receiver endpoint which received the message
     * @param _remote_address address of sender
     * @param _remote_port port of sender
     * @param _is_multicast whether the message was received via multicast (of course, only makes sense for UDP)
     */
    virtual void on_message(const byte_t* _data, length_t _length, boardnet_endpoint* _receiver,
                            const boost::asio::ip::address& _remote_address, port_t _remote_port, bool _is_multicast) = 0;

    virtual void remove_subscriptions(port_t _local_port, const boost::asio::ip::address& _remote_address, port_t _remote_port) = 0;
};

} // namespace vsomeip_v3

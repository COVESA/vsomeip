// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__) || defined(__QNX__)

#pragma once

#include <boost/asio/ip/udp.hpp>

#include <string>

#include <sys/time.h>

namespace vsomeip_v3 {

template<typename Protocol>
struct bind_to_device {
    explicit bind_to_device(std::string const& _device) : data_(static_cast<const void*>(_device.data())), size_(_device.size()) { }
    static int level(Protocol const&) { return SOL_SOCKET; };
    static int name(Protocol const&) { return SO_BINDTODEVICE; };
    const void* data(Protocol const&) const { return data_; }
    size_t size(Protocol const&) const { return size_; }

    const void* data_;
    size_t size_;
};

template<typename Protocol>
struct receive_buffer_force {
    static int level(Protocol const&) { return SOL_SOCKET; };
    static int name(Protocol const&) { return SO_RCVBUFFORCE; };
    const void* data(Protocol const&) const { return static_cast<const void*>(&size_); }
    size_t size(Protocol const&) const { return sizeof(size_); }

    int size_{};
};

template<typename Protocol>
struct send_timeout {
    static int level(Protocol const&) { return SOL_SOCKET; };
    static int name(Protocol const&) { return SO_SNDTIMEO; };
    const void* data(Protocol const&) const { return static_cast<const void*>(&opt_); }
    size_t size(Protocol const&) const { return sizeof(opt_); }

    timeval opt_{};
};
template<typename Protocol>
struct receive_timeout {
    static int level(Protocol const&) { return SOL_SOCKET; };
    static int name(Protocol const&) { return SO_RCVTIMEO; };
    const void* data(Protocol const&) const { return static_cast<const void*>(&opt_); }
    size_t size(Protocol const&) const { return sizeof(opt_); }

    timeval opt_{};
};

template<typename Protocol>
struct packet_info_ip4 {
    static constexpr int level(Protocol const&) { return IPPROTO_IP; };
    static constexpr int name(Protocol const&) { return IP_PKTINFO; };
    static constexpr const void* data(Protocol const&) { return static_cast<const void*>(&opt_); }
    static constexpr size_t size(Protocol const&) { return sizeof(opt_); }

    static constexpr int opt_{1};
};
template<typename Protocol>
struct packet_info_ip6 {
    static constexpr int level(Protocol const&) { return IPPROTO_IPV6; };
    static constexpr int name(Protocol const&) { return IPV6_RECVPKTINFO; };
    static constexpr const void* data(Protocol const&) { return static_cast<const void*>(&opt_); }
    static constexpr size_t size(Protocol const&) { return sizeof(opt_); }

    static constexpr int opt_{1};
};

using udp_bind_to_device = bind_to_device<boost::asio::ip::udp::endpoint::protocol_type>;
using udp_receive_buffer_force = receive_buffer_force<boost::asio::ip::udp::endpoint::protocol_type>;
using udp_send_timeout = send_timeout<boost::asio::ip::udp::endpoint::protocol_type>;
using udp_receive_timeout = receive_timeout<boost::asio::ip::udp::endpoint::protocol_type>;
using udp_packet_info_ip4 = packet_info_ip4<boost::asio::ip::udp::endpoint::protocol_type>;
using udp_packet_info_ip6 = packet_info_ip6<boost::asio::ip::udp::endpoint::protocol_type>;

}
#endif

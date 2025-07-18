// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_NETLINK_CONNECTOR_HPP_
#define VSOMEIP_V3_NETLINK_CONNECTOR_HPP_

#if defined(__linux__) || defined(ANDROID)

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <map>
#include <mutex>

#include <boost/asio/basic_raw_socket.hpp>
#include <boost/asio/ip/address.hpp>

#ifdef ANDROID
#    include "../../configuration/include/internal_android.hpp"
#else
#    include "../../configuration/include/internal.hpp"
#endif

#include "../../endpoints/include/buffer.hpp"
#include "abstract_netlink_connector.hpp"

namespace vsomeip_v3 {

template <typename Protocol>
class nl_endpoint {
public:
    /// The protocol type associated with the endpoint.
    typedef Protocol protocol_type;
    typedef boost::asio::detail::socket_addr_type data_type;

    /// Default constructor.
    nl_endpoint()
    {
        sockaddr.nl_family = PF_NETLINK;
        sockaddr.nl_groups = 0;
        sockaddr.nl_pid = 0; // Let the kernel do the assignment
    }

    /// Construct an endpoint using the specified path name.
    nl_endpoint(int group)
    {
        sockaddr.nl_family = PF_NETLINK;
        sockaddr.nl_groups = static_cast<unsigned int>(group);
        sockaddr.nl_pid = 0;
    }

    /// Copy constructor.
    nl_endpoint(const nl_endpoint& other)
    {
        sockaddr = other.sockaddr;
    }

    /// Assign from another endpoint.
    nl_endpoint& operator=(const nl_endpoint& other)
    {
        sockaddr = other.sockaddr;
        return *this;
    }

    /// The protocol associated with the endpoint.
    protocol_type protocol() const
    {
        return protocol_type();
    }

    /// Get the underlying endpoint in the native type.
    data_type* data()
    {
        return reinterpret_cast<struct sockaddr*>(&sockaddr);
    }

    /// Get the underlying endpoint in the native type.
    const data_type* data() const
    {
        return reinterpret_cast<const struct sockaddr*>(&sockaddr);
    }

    /// Get the underlying size of the endpoint in the native type.
    std::size_t size() const
    {
        return sizeof(sockaddr);
    }

    /// Set the underlying size of the endpoint in the native type.
    void resize(std::size_t size)
    {
        (void)size;
    /* nothing we can do here */
    }

    /// Get the capacity of the endpoint in the native type.
    std::size_t capacity() const
    {
        return sizeof(sockaddr);
    }

private:
    sockaddr_nl sockaddr;
};

class nl_protocol {
public:
    nl_protocol() {
        proto = 0;
    }
    nl_protocol(int proto) {
        this->proto = proto;
    }
    /// Obtain an identifier for the type of the protocol.
    int type() const
    {
        return SOCK_RAW;
    }
    /// Obtain an identifier for the protocol.
    int protocol() const
    {
        return proto;
    }
    /// Obtain an identifier for the protocol family.
    int family() const
    {
        return PF_NETLINK;
    }

    typedef nl_endpoint<nl_protocol> endpoint;
    typedef boost::asio::basic_raw_socket<nl_protocol> socket;

private:
    int proto;
};

typedef std::function<void(bool, // true = is interface, false = is route
                           std::string, // interface name
                           bool) // available?
                      >
        net_if_changed_handler_t;

class netlink_connector : public abstract_netlink_connector,
                          public std::enable_shared_from_this<netlink_connector> {
    enum class state_e { INIT, RESET, DOWN, UP, UP_MULTICAST };

public:
    netlink_connector(boost::asio::io_context& _io, const boost::asio::ip::address& _address,
                      const boost::asio::ip::address& _multicast_address,
                      bool _is_requiring_link = true) :
        net_if_index_for_address_(0), handler_(nullptr), socket_(_io),
        recv_buffer_(_multicast_address.is_v4() || _multicast_address.is_v6()
                             ? multicast_buffer_size
                             : default_buffer_size,
                     0),
        address_(_address), multicast_address_(_multicast_address),
        is_requiring_link_(_is_requiring_link) { }
    ~netlink_connector() {}

    void register_net_if_changes_handler(const net_if_changed_handler_t& _handler) override;
    void unregister_net_if_changes_handler() override;

    void start() override;
    void stop() override;

private:
    void set_state(state_e _state);

    bool has_address(const struct ifaddrmsg* ifa_struct, size_t length) const;
    void send_ifa_request(std::uint32_t _retry = 0);
    void send_ifi_request(std::uint32_t _retry = 0);
    void send_rt_request(std::uint32_t _retry = 0);

    void receive_cbk(boost::system::error_code const &_error, std::size_t _bytes);
    void send_cbk(boost::system::error_code const &_error, std::size_t _bytes);

    bool check_sd_multicast_route_match(const struct rtmsg* _routemsg, size_t _length,
                                        std::string* _routename) const;

    std::map<int, unsigned int> net_if_flags_;
    int net_if_index_for_address_;

    net_if_changed_handler_t handler_;

    std::mutex socket_mutex_;
    boost::asio::basic_raw_socket<nl_protocol> socket_;

    // The following observations were made: setting SO_RCVBUF to 8192 or
    // 16384 causes an ENOBUFS when NetLink is configured to listen for
    // IPv6 messages. However, the largest message was only 940 bytes and
    // the total size of all messages wass less than 1500 bytes.
    // Below is an attempt to explain this behaviour. First, Linux treats
    // the SO_RCVBUF parameter as a maximum value, which is 256K by default.
    // It simulates the POSIX behaviour with the SOCK_RCVBUF_LOCK flag.
    // https://elixir.bootlin.com/linux/v6.15.1/source/net/core/sock.c#L982
    // In addition, NetLink manages this parameter in its own way.
    // https://elixir.bootlin.com/linux/v6.15.1/C/ident/sk_rcvbuf
    // It seems that Linux allocates a block (sk_buff) for each message and,
    // as a result, SO_RCVBUFF limits not only the size of messages but also
    // their number.
    // As consequence, we do not modify the SO_RCVBUF parameter. The
    // consequences are unpredictable, and it does not reduce the memory
    // used. But we limit the size of the application buffer.

    const size_t multicast_buffer_size = 16384;
    const size_t default_buffer_size = 8192;
    message_buffer_t recv_buffer_;

    boost::asio::ip::address address_;
    boost::asio::ip::address multicast_address_;
    bool is_requiring_link_;
    bool interface_sync_done_ = false;
    bool route_sync_done_ = false;
    bool multicast_route_found_ = false;
    state_e current_state_ = state_e::INIT;

    static const std::uint32_t max_retries_ = VSOMEIP_MAX_NETLINK_RETRIES;
    static const std::uint32_t retry_bit_shift_ = 8;
    static const std::uint32_t request_sequence_bitmask_ = 0xFF;
    static const std::uint32_t ifa_request_sequence_ = 1;
    static const std::uint32_t ifi_request_sequence_ = 2;
    static const std::uint32_t rt_request_sequence_ = 3;
};

} // namespace vsomeip_v3

#endif // __linux__ || ANDROID

#endif // VSOMEIP_V3_NETLINK_CONNECTOR_HPP_

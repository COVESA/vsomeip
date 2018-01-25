// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_NETLINK_CONNECTOR_HPP
#define VSOMEIP_NETLINK_CONNECTOR_HPP

#ifndef _WIN32

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <map>
#include <mutex>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/basic_raw_socket.hpp>

#include "../../endpoints/include/buffer.hpp"

namespace vsomeip {

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
        sockaddr.nl_pid = getpid();
    }

    /// Construct an endpoint using the specified path name.
    nl_endpoint(int group, int pid=getpid())
    {
        sockaddr.nl_family = PF_NETLINK;
        sockaddr.nl_groups = group;
        sockaddr.nl_pid = pid;
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
        return &sockaddr;
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

typedef std::function< void (bool, std::string, bool) > net_if_changed_handler_t;

class netlink_connector : public std::enable_shared_from_this<netlink_connector> {
public:
    netlink_connector(boost::asio::io_service& _io, boost::asio::ip::address _address,
                      boost::asio::ip::address _multicast_address):
        net_if_index_for_address_(0),
        handler_(nullptr),
        socket_(_io),
        recv_buffer_(recv_buffer_size, 0),
        address_(_address),
        multicast_address_(_multicast_address) {
    }
    ~netlink_connector() {}

    void register_net_if_changes_handler(net_if_changed_handler_t _handler);
    void unregister_net_if_changes_handler();

    void start();
    void stop();

private:
    bool has_address(struct ifaddrmsg * ifa_struct,
            size_t length,
            const unsigned int address);
    void send_ifa_request();
    void send_ifi_request();
    void send_rt_request();

    void receive_cbk(boost::system::error_code const &_error, std::size_t _bytes);
    void send_cbk(boost::system::error_code const &_error, std::size_t _bytes);

    bool check_sd_multicast_route_match(struct rtmsg* _routemsg,
                                        size_t _length,
                                        std::string* _routename) const;

    std::map<int, unsigned int> net_if_flags_;
    int net_if_index_for_address_;

    net_if_changed_handler_t handler_;

    std::mutex socket_mutex_;
    boost::asio::basic_raw_socket<nl_protocol> socket_;

    const size_t recv_buffer_size = 16384;
    message_buffer_t recv_buffer_;

    boost::asio::ip::address address_;
    boost::asio::ip::address multicast_address_;
};

}

#endif // NOT _WIN32

#endif // VSOMEIP_NETLINK_CONNECTOR_HPP

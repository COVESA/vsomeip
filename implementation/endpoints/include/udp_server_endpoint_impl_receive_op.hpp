// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_RECEIVE_OP_HPP_
#define VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_RECEIVE_OP_HPP_

#if VSOMEIP_BOOST_VERSION >= 106600
#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)

#include <iomanip>

#include <boost/asio/ip/udp.hpp>

#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {

struct udp_server_endpoint_impl_receive_op {

    typedef boost::asio::ip::udp::socket socket_type_t;
    typedef boost::asio::ip::udp::endpoint endpoint_type_t;
    typedef std::function<
        void (boost::system::error_code const &_error, size_t _size,
              uint8_t, const boost::asio::ip::address &)> receive_handler_t;

    socket_type_t &socket_;
    endpoint_type_t &sender_;
    receive_handler_t handler_;
    byte_t *buffer_;
    size_t length_;
    uint8_t multicast_id_;
    bool is_v4_;
    boost::asio::ip::address destination_;
    size_t bytes_;

    void operator()(boost::system::error_code _error) {

        sender_ = endpoint_type_t(); // reset

        if (!_error) {

            if (!socket_.native_non_blocking())
                socket_.native_non_blocking(true, _error);

            for (;;) {
                ssize_t its_result;
                int its_flags(0);

                // Create control elements
                msghdr its_header = msghdr();
                struct iovec its_vec[1];

                // Prepare
                its_vec[0].iov_base = buffer_;
                its_vec[0].iov_len = length_;

                // Add io buffer
                its_header.msg_iov = its_vec;
                its_header.msg_iovlen = 1;

                // Sender & destination address info
                union {
                    struct sockaddr_in v4;
                    struct sockaddr_in6 v6;
                } addr;

                union {
                    struct cmsghdr cmh;
                    union {
                        char   v4[CMSG_SPACE(sizeof(struct in_pktinfo))];
                        char   v6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
                    } control;
                } control_un;

                // Prepare
                if (is_v4_) {
                    its_header.msg_name = &addr;
                    its_header.msg_namelen = sizeof(sockaddr_in);

                    its_header.msg_control = control_un.control.v4;
                    its_header.msg_controllen = sizeof(control_un.control.v4);
                } else {
                    its_header.msg_name = &addr;
                    its_header.msg_namelen = sizeof(sockaddr_in6);

                    its_header.msg_control = control_un.control.v6;
                    its_header.msg_controllen = sizeof(control_un.control.v6);
                }

                // Call recvmsg and handle its result
                errno = 0;
                its_result = ::recvmsg(socket_.native_handle(), &its_header, its_flags);

                _error = boost::system::error_code(its_result < 0 ? errno : 0,
                        boost::asio::error::get_system_category());
                bytes_ += _error ? 0 : static_cast<size_t>(its_result);

                if (_error == boost::asio::error::interrupted)
                    continue;

                if (_error == boost::asio::error::would_block
                        || _error == boost::asio::error::try_again) {

                    socket_.async_wait(socket_type_t::wait_read, *this);
                    return;
                }

                if (_error)
                    break;

                if (bytes_ == 0)
                    _error = boost::asio::error::eof;

                // Extract sender & destination addresses
                if (is_v4_) {
                    // sender
                    boost::asio::ip::address_v4 its_sender_address(
                            ntohl(addr.v4.sin_addr.s_addr));
                    in_port_t its_sender_port(ntohs(addr.v4.sin_port));
                    sender_ = endpoint_type_t(its_sender_address, its_sender_port);

                    // destination
                    struct in_pktinfo *its_pktinfo_v4;
                    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&its_header);
                         cmsg != NULL;
                         cmsg = CMSG_NXTHDR(&its_header, cmsg)) {

                        if (cmsg->cmsg_level == IPPROTO_IP
                            && cmsg->cmsg_type == IP_PKTINFO
                            && cmsg->cmsg_len == CMSG_LEN(sizeof(*its_pktinfo_v4))) {

                            its_pktinfo_v4 = (struct in_pktinfo*) CMSG_DATA(cmsg);
                            if (its_pktinfo_v4) {
                                destination_ = boost::asio::ip::address_v4(
                                        ntohl(its_pktinfo_v4->ipi_addr.s_addr));
                                break;
                            }
                        }
                    }
                } else {
                    boost::asio::ip::address_v6::bytes_type its_bytes;

                    // sender
                    for (size_t i = 0; i < its_bytes.size(); i++)
                        its_bytes[i] = addr.v6.sin6_addr.s6_addr[i];
                    boost::asio::ip::address_v6 its_sender_address(its_bytes);
                    in_port_t its_sender_port(ntohs(addr.v6.sin6_port));
                    sender_ = endpoint_type_t(its_sender_address, its_sender_port);

                    struct in6_pktinfo *its_pktinfo_v6;
                    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&its_header);
                         cmsg != NULL;
                         cmsg = CMSG_NXTHDR(&its_header, cmsg)) {

                        if (cmsg->cmsg_level == IPPROTO_IPV6
                            && cmsg->cmsg_type == IPV6_PKTINFO
                            && cmsg->cmsg_len == CMSG_LEN(sizeof(*its_pktinfo_v6))) {

                            its_pktinfo_v6 = (struct in6_pktinfo *) CMSG_DATA(cmsg);
                            if (its_pktinfo_v6) {
                                for (size_t i = 0; i < its_bytes.size(); i++)
                                    its_bytes[i] = its_pktinfo_v6->ipi6_addr.s6_addr[i];
                                destination_ = boost::asio::ip::address_v6(its_bytes);
                                break;
                            }
                        }
                    }
                }

                break;
            }
        }

        // Call the handler
        handler_(_error, bytes_, multicast_id_, destination_);
    }
};

} // namespace vsomeip

#endif // __linux__ || ANDROID
#endif // VSOMEIP_BOOST_VERSION >= 106600

#endif // VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_RECEIVE_OP_HPP_

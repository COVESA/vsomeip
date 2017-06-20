// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32

#include <thread>

#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "../include/netlink_connector.hpp"
#include "../../logging/include/logger.hpp"

namespace vsomeip {

void netlink_connector::register_net_if_changes_handler(net_if_changed_handler_t _handler) {
    handler_ = _handler;
}

void netlink_connector::unregister_net_if_changes_handler() {
    handler_ = nullptr;
}

void netlink_connector::stop() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    boost::system::error_code its_error;
    socket_.shutdown(socket_.shutdown_both, its_error);
    socket_.close(its_error);
    if (its_error) {
        VSOMEIP_WARNING << "Error closing NETLINK socket!";
    }
}

void netlink_connector::start() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    boost::system::error_code ec;
    if (socket_.is_open()) {
        socket_.close(ec);
        if (ec) {
            VSOMEIP_WARNING << "Error closing NETLINK socket: " << ec.message();
        }
    }
    socket_.open(nl_protocol(NETLINK_ROUTE), ec);
    if (ec) {
        VSOMEIP_WARNING << "Error opening NETLINK socket: " << ec.message();
        if (handler_) {
            handler_("n/a", true);
        }
        return;
    }
    if (socket_.is_open()) {
        socket_.bind(nl_endpoint<nl_protocol>(RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR), ec);

        if (ec) {
            VSOMEIP_WARNING << "Error binding NETLINK socket: " << ec.message();
            if (handler_) {
                handler_("n/a", true);
            }
            return;
        }

        send_ifa_request();

        socket_.async_receive(
            boost::asio::buffer(&recv_buffer_[0], recv_buffer_size),
            std::bind(
                &netlink_connector::receive_cbk,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    } else {
        VSOMEIP_WARNING << "Error opening NETLINK socket!";
        if (handler_) {
            handler_("n/a", true);
        }
    }
}

void netlink_connector::receive_cbk(boost::system::error_code const &_error,
                 std::size_t _bytes) {
    if (!_error) {
        size_t len = _bytes;

        unsigned int address(0);
        if (address_.is_v4()) {
            inet_pton(AF_INET, address_.to_string().c_str(), &address);
        } else {
            inet_pton(AF_INET6, address_.to_string().c_str(), &address);
        }

        struct nlmsghdr *nlh = (struct nlmsghdr *)&recv_buffer_[0];

        while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
            char ifname[1024];
            struct ifinfomsg *ifi = (ifinfomsg *)NLMSG_DATA(nlh);
            struct ifaddrmsg *ifa = (ifaddrmsg *)NLMSG_DATA(nlh);
            switch (nlh->nlmsg_type) {
                case RTM_NEWADDR:
                    // New Address information
                    if (has_address((struct ifaddrmsg *)NLMSG_DATA(nlh),
                            IFA_PAYLOAD(nlh), address)) {
                        net_if_index_for_address_ = ifa->ifa_index;
                        auto its_if = net_if_flags_.find(ifa->ifa_index);
                        if (its_if != net_if_flags_.end()) {
                            if ((its_if->second & IFF_UP) &&
                                    (its_if->second & IFF_RUNNING)) {
                                if (handler_) {
                                    if_indextoname(ifa->ifa_index,ifname);
                                    handler_(ifname, true);
                                }
                            } else {
                                if (handler_) {
                                    if_indextoname(ifa->ifa_index,ifname);
                                    handler_(ifname, false);
                                }
                            }
                        } else {
                            // Request interface information
                            // as we don't know about up/running state!
                           send_ifi_request();
                        }
                    }
                    break;
                case RTM_NEWLINK:
                    // New Interface information
                    net_if_flags_[ifi->ifi_index] = ifi->ifi_flags;
                    if (net_if_index_for_address_ == ifi->ifi_index) {
                        if ((ifi->ifi_flags & IFF_UP) &&
                            (ifi->ifi_flags & IFF_RUNNING)) {
                            if (handler_) {
                                if_indextoname(ifi->ifi_index,ifname);
                                handler_(ifname, true);
                            }
                        } else {
                            if (handler_) {
                                if_indextoname(ifi->ifi_index,ifname);
                                handler_(ifname, false);
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
            nlh = NLMSG_NEXT(nlh, len);
        }
        {
            std::lock_guard<std::mutex> its_lock(socket_mutex_);
            if (socket_.is_open()) {
                socket_.async_receive(
                    boost::asio::buffer(&recv_buffer_[0], recv_buffer_size),
                    std::bind(
                        &netlink_connector::receive_cbk,
                        shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2
                    )
                );
            }
        }
    } else {
        if (_error != boost::asio::error::operation_aborted) {
            VSOMEIP_WARNING << "Error receive_cbk NETLINK socket!" << _error.message();
            boost::system::error_code its_error;
            {
                std::lock_guard<std::mutex> its_lock(socket_mutex_);
                if (socket_.is_open()) {
                    socket_.shutdown(socket_.shutdown_both, its_error);
                    socket_.close(its_error);
                    if (its_error) {
                        VSOMEIP_WARNING << "Error closing NETLINK socket!"
                                << its_error.message();
                    }
                }
            }
            if (handler_) {
                handler_("n/a", true);
            }
        }
    }
}

void netlink_connector::send_cbk(boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_bytes;
    if (_error) {
        VSOMEIP_WARNING << "Netlink send error : " << _error.message();
        if (handler_) {
            handler_("n/a", true);
        }
    }
}

void netlink_connector::send_ifa_request() {
    typedef struct {
        struct nlmsghdr nlhdr;
        struct ifaddrmsg addrmsg;
    } netlink_address_msg;
    netlink_address_msg get_address_msg;
    memset(&get_address_msg, 0, sizeof(get_address_msg));
    get_address_msg.nlhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    get_address_msg.nlhdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    get_address_msg.nlhdr.nlmsg_type = RTM_GETADDR;
    if (address_.is_v4()) {
        get_address_msg.addrmsg.ifa_family = AF_INET;
    } else {
        get_address_msg.addrmsg.ifa_family = AF_INET6;
    }

    socket_.async_send(
        boost::asio::buffer(&get_address_msg, get_address_msg.nlhdr.nlmsg_len),
        std::bind(
            &netlink_connector::send_cbk,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void netlink_connector::send_ifi_request() {
    typedef struct {
        struct nlmsghdr nlhdr;
        struct ifinfomsg infomsg;
    } netlink_link_msg;
    netlink_link_msg get_link_msg;
    memset(&get_link_msg, 0, sizeof(get_link_msg));
    get_link_msg.nlhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    get_link_msg.nlhdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    get_link_msg.nlhdr.nlmsg_type = RTM_GETLINK;
    get_link_msg.infomsg.ifi_family = AF_UNSPEC;

    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        socket_.async_send(
            boost::asio::buffer(&get_link_msg, get_link_msg.nlhdr.nlmsg_len),
            std::bind(
                &netlink_connector::send_cbk,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    }
}

bool netlink_connector::has_address(struct ifaddrmsg * ifa_struct,
        size_t length,
        const unsigned int address) {

    struct rtattr *retrta;
    retrta = static_cast<struct rtattr *>(IFA_RTA(ifa_struct));
    while RTA_OK(retrta, length) {
        if (retrta->rta_type == IFA_ADDRESS) {
            char pradd[128];
            unsigned int * tmp_address = (unsigned int *)RTA_DATA(retrta);
            if (address_.is_v4()) {
                inet_ntop(AF_INET, tmp_address, pradd, sizeof(pradd));
            } else {
                inet_ntop(AF_INET6, tmp_address, pradd, sizeof(pradd));
            }
            if (address == *tmp_address) {
                return true;
            }
        }
        retrta = RTA_NEXT(retrta, length);
    }

    return false;
}

} // namespace vsomeip

#endif


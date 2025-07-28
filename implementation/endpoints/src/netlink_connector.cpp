// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__) || defined(ANDROID)

#include <thread>

#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <sstream>

#include <vsomeip/internal/logger.hpp>

#include "../include/netlink_connector.hpp"

namespace vsomeip_v3 {

void netlink_connector::register_net_if_changes_handler(const net_if_changed_handler_t& _handler) {
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
    std::unique_lock its_lock(socket_mutex_);
    boost::system::error_code ec;
    if (socket_.is_open()) {
        // It is inherently unsafe to receive multiple starts
        // due to internal data not being protected by mutexes.

        socket_.close(ec);
        if (ec) {
            VSOMEIP_WARNING << "Error closing NETLINK socket: " << ec.message();
        }
    }
    socket_.open(nl_protocol(NETLINK_ROUTE), ec);
    if (ec) {
        VSOMEIP_ERROR << "Error opening NETLINK socket: " << ec.message();
        return;
    }

    if (socket_.is_open()) {
        int group = RTMGRP_LINK;

        if (address_.is_v4()) {
            group |= RTMGRP_IPV4_IFADDR;
        } else if (address_.is_v6()) {
            group |= RTMGRP_IPV6_IFADDR;
        }

        if (multicast_address_.is_v4() && !multicast_address_.is_unspecified()) {
            group |= RTMGRP_IPV4_ROUTE;
        } else if (multicast_address_.is_v6() && !multicast_address_.is_unspecified()) {
            group |= RTMGRP_IPV6_ROUTE;
        }

        socket_.bind(nl_endpoint<nl_protocol>(group), ec);

        if (ec && ec != boost::asio::error::address_in_use) {
            VSOMEIP_ERROR << "Error binding NETLINK socket: " << ec.message();
            return;
        }

        // TODO: replace socket_mutex_ by a proper locking mechanism
        its_lock.unlock();
        set_state(state_e::RESET);
        socket_.async_receive(boost::asio::buffer(&recv_buffer_[0], recv_buffer_.size()),
                              std::bind(&netlink_connector::receive_cbk, shared_from_this(),
                                        std::placeholders::_1, std::placeholders::_2));
    } else {
        VSOMEIP_ERROR << "Error opening NETLINK socket!";
    }
}

void netlink_connector::set_state(state_e _state) {
    // This code is based on the assumption that it cannot be
    // executed in parallel. This is true unless start is called
    // multiple times.

    if (_state == current_state_) {
        return;
    }

    VSOMEIP_INFO << "netlink: from " << static_cast<int>(current_state_) << " to "
                 << static_cast<int>(_state) << ", if=" << net_if_index_for_address_
                 << ", mc=" << multicast_route_found_ << ", count=" << net_if_flags_.size();

    if (current_state_ == state_e::INIT) {
        current_state_ = state_e::RESET;
    }

    if (_state == state_e::RESET) {
        net_if_flags_.clear();
        interface_sync_done_ = false;
        route_sync_done_ = false;
        multicast_route_found_ = false;
        net_if_index_for_address_ = 0;
        send_ifa_request();
    }

    if (_state == state_e::DOWN) {
        if (!interface_sync_done_) {
            send_ifi_request();
            interface_sync_done_ = true;
        }
    }

    if (_state == state_e::UP) {
        if (!route_sync_done_ && !multicast_address_.is_unspecified()) {
            send_rt_request();
            route_sync_done_ = true;
        }
    }

    if (handler_) {
        char ifname[IF_NAMESIZE];

        if (net_if_index_for_address_ > 0) {
            if (if_indextoname(static_cast<unsigned>(net_if_index_for_address_), ifname)
                == nullptr) {
                ::strncpy(ifname, "error", IF_NAMESIZE - 1);
            }
        } else {
            ::strncpy(ifname, "n/a", IF_NAMESIZE - 1);
        }

        if (_state == state_e::RESET || _state == state_e::DOWN) {
            if (current_state_ == state_e::UP || current_state_ == state_e::UP_MULTICAST) {
                handler_(true, ifname, false);
            }

            if (current_state_ == state_e::UP_MULTICAST) {
                handler_(false, ifname, false);
            }
        }

        if ((_state == state_e::UP || _state == state_e::UP_MULTICAST)
            && (current_state_ == state_e::RESET || current_state_ == state_e::DOWN)) {
            handler_(true, ifname, true);
        }

        if (_state == state_e::UP_MULTICAST) {
            handler_(false, ifname, true);
        }

        if (_state == state_e::UP && current_state_ == state_e::UP_MULTICAST) {
            handler_(false, ifname, false);
        }
    }

    current_state_ = _state;
}

void netlink_connector::receive_cbk(boost::system::error_code const &_error,
                 std::size_t _bytes) {
    bool error_detected = false;

    if (!_error) {
        size_t len = _bytes;
        struct nlmsghdr *nlh = (struct nlmsghdr *)&recv_buffer_[0];

        while (NLMSG_OK(nlh, len)) {
            switch (nlh->nlmsg_type) {
                case RTM_NEWADDR: {
                    // New Address information
                    auto ifa = static_cast<const struct ifaddrmsg*>(NLMSG_DATA(nlh));
                    if (has_address(ifa, IFA_PAYLOAD(nlh))) {
                        net_if_index_for_address_ = static_cast<int>(ifa->ifa_index);
                        auto its_if = net_if_flags_.find(net_if_index_for_address_);
                        if (its_if != net_if_flags_.end()) {
                            if ((its_if->second & IFF_UP) &&
                                    (is_requiring_link_ ? (its_if->second & IFF_RUNNING) : true)) {
                                set_state(multicast_route_found_ ? state_e::UP_MULTICAST
                                                                 : state_e::UP);
                            } else {
                                set_state(state_e::DOWN);
                            }
                        } else {
                            set_state(state_e::DOWN);
                        }
                    }
                    break;
                }
                case RTM_NEWLINK: {
                    // New Interface information
                    auto ifi = static_cast<const struct ifinfomsg*>(NLMSG_DATA(nlh));
                    net_if_flags_[ifi->ifi_index] = ifi->ifi_flags;
                    if (net_if_index_for_address_ == ifi->ifi_index) {
                        if ((ifi->ifi_flags & IFF_UP) &&
                            (is_requiring_link_ ? (ifi->ifi_flags & IFF_RUNNING) : true)) {
                            set_state(multicast_route_found_ ? state_e::UP_MULTICAST : state_e::UP);
                        } else {
                            set_state(state_e::DOWN);
                        }
                    }
                    break;
                }
                case RTM_NEWROUTE: {
                    auto routemsg = static_cast<const struct rtmsg*>(NLMSG_DATA(nlh));
                    std::string its_route_name;
                    if (check_sd_multicast_route_match(routemsg, RTM_PAYLOAD(nlh),
                            &its_route_name)) {
                        multicast_route_found_ = true;
                        set_state(current_state_ == state_e::UP ? state_e::UP_MULTICAST
                                                                : current_state_);
                    }
                    break;
                }
                case RTM_DELROUTE: {
                    auto routemsg = static_cast<const struct rtmsg*>(NLMSG_DATA(nlh));
                    std::string its_route_name;
                    if (check_sd_multicast_route_match(routemsg, RTM_PAYLOAD(nlh),
                            &its_route_name)) {
                        multicast_route_found_ = true;
                        set_state(current_state_ == state_e::UP_MULTICAST ? state_e::UP
                                                                          : state_e::DOWN);
                    }
                    break;
                }
                case RTM_DELADDR: {
                    auto ifa = static_cast<const struct ifaddrmsg*>(NLMSG_DATA(nlh));
                    if (net_if_index_for_address_ == static_cast<int>(ifa->ifa_index)) {
                        set_state(state_e::RESET);
                    }
                    break;
                }
                case RTM_DELLINK: {
                    auto ifi = static_cast<const struct ifinfomsg*>(NLMSG_DATA(nlh));
                    if (net_if_index_for_address_ == ifi->ifi_index) {
                        set_state(state_e::RESET);
                    }
                    break;
                }
                case NLMSG_ERROR: {
                    struct nlmsgerr *errmsg = (nlmsgerr *)NLMSG_DATA(nlh);
                    if (errmsg->error != 0) {
                        VSOMEIP_ERROR << "NLMSG_ERROR received" << errmsg->error;
                        error_detected = true;
                    }
                    break;
                }
                case NLMSG_DONE:
                case NLMSG_NOOP:
                default:
                    break;
            }
            nlh = NLMSG_NEXT(nlh, len);
        }

        if (len != 0) {
            VSOMEIP_ERROR << "NetLink message not fully parsed, bytes skipped=" << len;
            error_detected = true;
        }
    } else if (_error != boost::asio::error::operation_aborted) {
        VSOMEIP_ERROR << "Error receive_cbk NETLINK socket!" << _error.message();
        error_detected = true;
    } else {
        return;
    }

    if (error_detected) {
        set_state(state_e::RESET);
    }

    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (socket_.is_open()) {
        socket_.async_receive(boost::asio::buffer(&recv_buffer_[0], recv_buffer_.size()),
                              std::bind(&netlink_connector::receive_cbk, shared_from_this(),
                                        std::placeholders::_1, std::placeholders::_2));
    }
}

void netlink_connector::send_cbk(boost::system::error_code const &_error, std::size_t _bytes) {
    // TODO: fix thread safety and then handle send errors, or
    // replace asynchronous send by synchronous send

    (void)_bytes;
    if (_error) {
        VSOMEIP_ERROR << "Netlink send error : " << _error.message();
        if (handler_) {
            handler_(true, "n/a", true);
            handler_(false, "n/a", true);
        }
    }
}

void netlink_connector::send_ifa_request(std::uint32_t _retry) {
    typedef struct {
        struct nlmsghdr nlhdr;
        struct ifaddrmsg addrmsg;
    } netlink_address_msg;

    auto get_address_msg = std::make_shared<netlink_address_msg>();
    memset(get_address_msg.get(), 0, sizeof(*get_address_msg));
    get_address_msg->nlhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    get_address_msg->nlhdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    get_address_msg->nlhdr.nlmsg_type = RTM_GETADDR;
    // the sequence number has stored the request sequence and the retry count.
    // request sequece is stored in the LSB (least significant byte) and
    // retry is stored in the 2nd LSB.
    get_address_msg->nlhdr.nlmsg_seq = ifa_request_sequence_ | (_retry << retry_bit_shift_);
    if (address_.is_v4()) {
        get_address_msg->addrmsg.ifa_family = AF_INET;
    } else {
        get_address_msg->addrmsg.ifa_family = AF_INET6;
    }

    {
        std::lock_guard its_lock(socket_mutex_);
        socket_.async_send(
                boost::asio::buffer(get_address_msg.get(), get_address_msg->nlhdr.nlmsg_len),
                [this, data = get_address_msg](auto... args) mutable {
                    send_cbk(args...);
                    data.reset();
                });
    }
}

void netlink_connector::send_ifi_request(std::uint32_t _retry) {
    typedef struct {
        struct nlmsghdr nlhdr;
        alignas(NLMSG_ALIGNTO) struct ifinfomsg infomsg;
        alignas(RTA_ALIGNTO) struct rtattr filter;
        alignas(RTA_ALIGNTO) int filter_value;
    } netlink_link_msg;

    auto get_link_msg = std::make_shared<netlink_link_msg>();
    memset(get_link_msg.get(), 0, sizeof(*get_link_msg));
    get_link_msg->nlhdr.nlmsg_len = NLMSG_LENGTH(NLMSG_ALIGN(sizeof(struct ifinfomsg))
                                                 + RTA_ALIGN(sizeof(get_link_msg->filter))
                                                 + RTA_ALIGN(sizeof(get_link_msg->filter_value)));
    get_link_msg->nlhdr.nlmsg_flags = NLM_F_REQUEST;
    get_link_msg->nlhdr.nlmsg_type = RTM_GETLINK;
    get_link_msg->infomsg.ifi_family = AF_UNSPEC;
    get_link_msg->infomsg.ifi_index = net_if_index_for_address_;
    get_link_msg->filter.rta_type = IFLA_EXT_MASK;
    get_link_msg->filter.rta_len = RTA_LENGTH(sizeof(get_link_msg->filter_value));
    get_link_msg->filter_value = RTEXT_FILTER_SKIP_STATS;
    // the sequence number has stored the request sequence and the retry count.
    // request sequece is stored in the LSB (least significant byte) and
    // retry is stored in the 2nd LSB.
    get_link_msg->nlhdr.nlmsg_seq = ifi_request_sequence_ | (_retry << retry_bit_shift_);

    {
        std::lock_guard its_lock(socket_mutex_);
        socket_.async_send(boost::asio::buffer(get_link_msg.get(), sizeof(*get_link_msg)),
                           [this, data = get_link_msg](auto... args) mutable {
                               send_cbk(args...);
                               data.reset();
                           });
    }
}

void netlink_connector::send_rt_request(std::uint32_t _retry) {
    typedef struct {
        struct nlmsghdr nlhdr;
        struct rtgenmsg routemsg;
    } netlink_route_msg;

    auto get_route_msg = std::make_shared<netlink_route_msg>();
    memset(get_route_msg.get(), 0, sizeof(*get_route_msg));
    get_route_msg->nlhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    get_route_msg->nlhdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    get_route_msg->nlhdr.nlmsg_type = RTM_GETROUTE;
    // the sequence number has stored the request sequence and the retry count.
    // request sequece is stored in the LSB (least significant byte) and
    // retry is stored in the 2nd LSB.
    get_route_msg->nlhdr.nlmsg_seq = rt_request_sequence_ | (_retry << retry_bit_shift_);
    if (multicast_address_.is_v6()) {
        get_route_msg->routemsg.rtgen_family = AF_INET6;
    } else {
        get_route_msg->routemsg.rtgen_family = AF_INET;
    }

    {
        std::lock_guard its_lock(socket_mutex_);
        socket_.async_send(boost::asio::buffer(get_route_msg.get(), sizeof(*get_route_msg)),
                           [this, data = get_route_msg](auto... args) mutable {
                               send_cbk(args...);
                               data.reset();
                           });
    }
}

bool netlink_connector::has_address(const struct ifaddrmsg* ifa_struct, size_t length) const {
    auto retrta = static_cast<const struct rtattr*>(IFA_RTA(ifa_struct));
    while RTA_OK(retrta, length) {
        if (retrta->rta_type == IFA_ADDRESS) {
            if (address_.is_v4() && RTA_PAYLOAD(retrta) == sizeof(struct in_addr)
                && ::memcmp(RTA_DATA(retrta), address_.to_v4().to_bytes().data(),
                            sizeof(struct in_addr))
                        == 0) {
                return true;
            } else if (address_.is_v6() && RTA_PAYLOAD(retrta) == sizeof(struct in6_addr)
                       && ::memcmp(RTA_DATA(retrta), address_.to_v6().to_bytes().data(),
                                   sizeof(struct in6_addr))
                               == 0) {
                return true;
            }
        }
        retrta = RTA_NEXT(retrta, length);
    }

    return false;
}

bool netlink_connector::check_sd_multicast_route_match(const struct rtmsg* _routemsg,
                                                       size_t _length,
                                                       std::string* _routename) const {
    struct rtattr *retrta;
    retrta = static_cast<struct rtattr *>(RTM_RTA(_routemsg));
    int if_index(0);
    char if_name[IF_NAMESIZE] = "n/a";
    char address[INET6_ADDRSTRLEN] = "n/a";
    char gateway[INET6_ADDRSTRLEN] = "n/a";
    bool matches_sd_multicast(false);
    while (RTA_OK(retrta, _length)) {
        if (retrta->rta_type == RTA_DST) {
            // check if added/removed route matches on configured SD multicast address
            size_t rtattr_length = RTA_PAYLOAD(retrta);
            if (rtattr_length == 4 && multicast_address_.is_v4()) { // IPv4 route
                inet_ntop(AF_INET, RTA_DATA(retrta), address, sizeof(address));
                std::uint32_t netmask(0);
                for (int i = 31; i > 31 - _routemsg->rtm_dst_len; i--) {
                    netmask |= static_cast<std::uint32_t>(1 << i);
                }
                const std::uint32_t dst_addr = ntohl(*((std::uint32_t *)RTA_DATA(retrta)));
                const std::uint32_t dst_net = (dst_addr & netmask);
                const auto sd_addr = multicast_address_.to_v4().to_uint();
                const std::uint32_t sd_net = (sd_addr & netmask);
                matches_sd_multicast = !(dst_net ^ sd_net);
            } else if (rtattr_length == 16 && multicast_address_.is_v6()) { // IPv6 route
                inet_ntop(AF_INET6, RTA_DATA(retrta), address, sizeof(address));
                std::uint32_t netmask2[4] = {0,0,0,0};
                for (int i = 127; i > 127 - _routemsg->rtm_dst_len; i--) {
                    if (i > 95) {
                        netmask2[0] |= static_cast<std::uint32_t>(1 << (i-96));
                    } else if (i > 63) {
                        netmask2[1] |= static_cast<std::uint32_t>(1 << (i-64));
                    } else if (i > 31) {
                        netmask2[2] |= static_cast<std::uint32_t>(1 << (i-32));
                    } else {
                        netmask2[3] |= static_cast<std::uint32_t>(1 << i);
                    }
                }

                for (int i = 0; i < 4; i++) {
#ifndef ANDROID
                    const std::uint32_t dst = ntohl((*(struct in6_addr*)RTA_DATA(retrta)).__in6_u.__u6_addr32[i]);
#else
                    const std::uint32_t dst = ntohl((*(struct in6_addr*)RTA_DATA(retrta)).in6_u.u6_addr32[i]);
#endif
                    const std::uint32_t sd = ntohl(reinterpret_cast<std::uint32_t*>(multicast_address_.to_v6().to_bytes().data())[i]);
                    const std::uint32_t dst_net = dst & netmask2[i];
                    const std::uint32_t sd_net = sd & netmask2[i];
                    matches_sd_multicast = !(dst_net ^ sd_net);
                    if (!matches_sd_multicast) {
                        break;
                    }
                }
            }
        } else if (retrta->rta_type == RTA_OIF) {
            if_index = *(int *)(RTA_DATA(retrta));
            if_indextoname(static_cast<unsigned int>(if_index),if_name);
        } else if (retrta->rta_type == RTA_GATEWAY) {
            size_t rtattr_length = RTA_PAYLOAD(retrta);
            if (rtattr_length == 4) {
                inet_ntop(AF_INET, RTA_DATA(retrta), gateway, sizeof(gateway));
            } else if (rtattr_length == 16) {
                inet_ntop(AF_INET6, RTA_DATA(retrta), gateway, sizeof(gateway));
            }
        }
        retrta = RTA_NEXT(retrta, _length);
    }
    if (matches_sd_multicast && net_if_index_for_address_ == if_index) {
        std::stringstream stream;
        stream << address << "/" <<  (static_cast<uint32_t>(_routemsg->rtm_dst_len))
                << " if: " << if_name << " gw: " << gateway;
        *_routename = stream.str();
        return true;
    }
    return false;
}

} // namespace vsomeip_v3

#endif // __linux__ or ANDROID

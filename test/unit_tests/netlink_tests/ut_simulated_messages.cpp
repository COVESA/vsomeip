// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <arpa/inet.h>
#include <asm/types.h>
#include <errno.h>
#include <net/if.h>
#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/primitive_types.hpp>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif

#define private public
#define protected public
#include "../../../implementation/endpoints/include/netlink_connector.hpp"

struct simulation_netlink_fixture : public ::testing::Test {
    boost::asio::io_context context_;
    boost::asio::ip::address listening_address_;
    boost::asio::ip::address multicast_address_;
    std::shared_ptr<vsomeip_v3::netlink_connector> connector_;
    std::vector<std::byte> message_;
    size_t recv_buffer_cursor_;

    void SetUp() override {
        connector_ = std::make_shared<vsomeip_v3::netlink_connector>(context_, listening_address_,
                                                                     multicast_address_);
        recv_buffer_cursor_ = 0;
    }

    void TearDown() override {
        connector_.reset();
        message_.clear();
    }

    struct nlmsghdr* nl_new_message_(uint16_t _type) {
        message_.clear();
        auto header = nl_append<struct nlmsghdr>();
        header->nlmsg_type = _type;
        return header;
    }

    std::byte* nl_append_data(size_t _len) {
        size_t data_pos = NLMSG_ALIGN(message_.size());
        message_.insert(message_.end(), _len + (data_pos - message_.size()), (std::byte)0);
        auto header = reinterpret_cast<struct nlmsghdr*>(message_.data());
        header->nlmsg_len = static_cast<__u32>(data_pos + _len);
        return message_.data() + data_pos;
    }

    template<typename T>
    T* nl_append() {
        return reinterpret_cast<T*>(nl_append_data(sizeof(T)));
    }

    std::byte* nl_add_attribute(uint16_t _type, size_t _len) {
        size_t header_len = RTA_LENGTH(0);
        std::byte* data = nl_append_data(header_len + _len);
        auto attribute = reinterpret_cast<struct rtattr*>(data);
        attribute->rta_type = _type;
        attribute->rta_len = static_cast<unsigned short>(header_len + _len);
        return data + header_len;
    }

    template<typename T>
    T* nl_add_attribute(uint16_t _type) {
        return reinterpret_cast<T*>(nl_add_attribute(_type, sizeof(T)));
    }

    void nl_copy_to_recv_buffer(bool _reset = false) {
        if (_reset) {
            recv_buffer_cursor_ = 0;
        }

        recv_buffer_cursor_ = NLMSG_ALIGN(recv_buffer_cursor_);
        std::transform(message_.begin(), message_.end(),
                       connector_->recv_buffer_.begin() + static_cast<int32_t>(recv_buffer_cursor_),
                       [](std::byte v) -> uint8_t { return static_cast<uint8_t>(v); });
        recv_buffer_cursor_ += message_.size();
    }
};

struct ipv4_netlink_fixture : public simulation_netlink_fixture {
    static constexpr uint32_t IP_ADDR = 0x7F000001;
    static constexpr uint32_t MULTICAST_ADDR = 0xDF000001;
    static constexpr int INTERFACE_IDX = 19;

    ipv4_netlink_fixture() {
        listening_address_ = boost::asio::ip::address_v4(IP_ADDR);
        multicast_address_ = boost::asio::ip::address_v4(MULTICAST_ADDR);
    }

    void simulate_localhost_availability();
};

void ipv4_netlink_fixture::simulate_localhost_availability() {
    using namespace std::chrono_literals;

    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                }
            });

    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    *nl_add_attribute<uint32_t>(IFA_ADDRESS) = ::htonl(IP_ADDR);

    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    nl_new_message_(RTM_NEWLINK);
    auto interfacemsg = nl_append<struct ifinfomsg>();
    interfacemsg->ifi_family = AF_UNSPEC;
    interfacemsg->ifi_type = ARPHRD_ETHER;
    interfacemsg->ifi_index = INTERFACE_IDX;
    interfacemsg->ifi_flags = IFF_LOOPBACK;
    interfacemsg->ifi_change = 0xFFFFFFFFU;

    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    interfacemsg->ifi_flags = IFF_LOOPBACK | IFF_UP;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    interfacemsg->ifi_flags = IFF_LOOPBACK | IFF_UP | IFF_RUNNING;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_TRUE(localhost_available);

    connector_->unregister_net_if_changes_handler();
}

TEST_F(ipv4_netlink_fixture, start_stop) {
    using namespace std::chrono_literals;

    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string _name, bool _available) {
                if (_interface && _name == "lo") {
                    localhost_available = _available;
                }
            });

    // TODO: NetLink incorrectly handles double start, it
    // doesn't wait until the previous operations have been
    // finished before to reuse the same buffers, resulting
    // in a buffer overwrite.
    EXPECT_NO_THROW(connector_->start());
    EXPECT_NO_THROW(connector_->start());
    while (!localhost_available) {
        context_.run_one_for(2ms);
    }
    EXPECT_NO_THROW(connector_->stop());
    EXPECT_TRUE(localhost_available);
}

TEST_F(ipv4_netlink_fixture, address_availability) {
    simulate_localhost_availability();

    bool localhost_available = true;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                }
            });

    nl_new_message_(RTM_DELADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    nl_new_message_(RTM_NEWADDR);
    addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    *nl_add_attribute<uint32_t>(IFA_ADDRESS) = ::htonl(IP_ADDR);
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);

    // TODO: This test has to be reneabled, vsomeip incorrectly
    // delete the interface when the address has been removed
    // EXPECT_TRUE(localhost_available);
}

TEST_F(ipv4_netlink_fixture, interface_availability) {
    simulate_localhost_availability();

    bool localhost_available = true;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                }
            });

    nl_new_message_(RTM_NEWLINK);
    auto interfacemsg = nl_append<struct ifinfomsg>();
    interfacemsg->ifi_family = AF_UNSPEC;
    interfacemsg->ifi_type = ARPHRD_ETHER;
    interfacemsg->ifi_index = INTERFACE_IDX;
    interfacemsg->ifi_flags = IFF_LOOPBACK;
    interfacemsg->ifi_change = 0xFFFFFFFFU;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    nl_new_message_(RTM_DELLINK);
    interfacemsg = nl_append<struct ifinfomsg>();
    interfacemsg->ifi_family = AF_UNSPEC;
    interfacemsg->ifi_type = ARPHRD_ETHER;
    interfacemsg->ifi_index = INTERFACE_IDX;
    interfacemsg->ifi_flags = IFF_LOOPBACK;
    interfacemsg->ifi_change = 0xFFFFFFFFU;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);
}

TEST_F(ipv4_netlink_fixture, interface_down) {
    simulate_localhost_availability();

    bool localhost_available = true;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                }
            });

    nl_new_message_(RTM_NEWLINK);
    auto interfacemsg = nl_append<struct ifinfomsg>();
    interfacemsg->ifi_family = AF_UNSPEC;
    interfacemsg->ifi_type = ARPHRD_ETHER;
    interfacemsg->ifi_index = INTERFACE_IDX;
    interfacemsg->ifi_flags = IFF_LOOPBACK;
    interfacemsg->ifi_change = 0xFFFFFFFFU;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);
}

TEST_F(ipv4_netlink_fixture, address_available_after_interface) {
    using namespace std::chrono_literals;

    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                }
            });

    nl_new_message_(RTM_NEWLINK);
    auto interfacemsg = nl_append<struct ifinfomsg>();
    interfacemsg->ifi_family = AF_UNSPEC;
    interfacemsg->ifi_type = ARPHRD_ETHER;
    interfacemsg->ifi_index = INTERFACE_IDX;
    interfacemsg->ifi_flags = IFF_LOOPBACK | IFF_UP | IFF_RUNNING;
    interfacemsg->ifi_change = 0xFFFFFFFFU;

    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    *nl_add_attribute<uint32_t>(IFA_ADDRESS) = ::htonl(IP_ADDR);

    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_TRUE(localhost_available);
}

TEST_F(ipv4_netlink_fixture, address_available_after_interface_not_up_and_running) {
    using namespace std::chrono_literals;

    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                }
            });

    nl_new_message_(RTM_NEWLINK);
    auto interfacemsg = nl_append<struct ifinfomsg>();
    interfacemsg->ifi_family = AF_UNSPEC;
    interfacemsg->ifi_type = ARPHRD_ETHER;
    interfacemsg->ifi_index = INTERFACE_IDX;
    interfacemsg->ifi_flags = IFF_LOOPBACK;
    interfacemsg->ifi_change = 0xFFFFFFFFU;

    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);

    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    *nl_add_attribute<uint32_t>(IFA_ADDRESS) = ::htonl(IP_ADDR);

    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);
}

TEST_F(ipv4_netlink_fixture, start_error_and_stop) {
    int calls_count = 0;
    connector_->register_net_if_changes_handler([&](bool /*_interface*/, std::string /*_name*/,
                                                    bool /*_available*/) { calls_count += 1; });
    EXPECT_NO_THROW(connector_->start());
    EXPECT_NO_THROW(connector_->send_cbk(boost::asio::error::connection_aborted, 0));
    EXPECT_NO_THROW(connector_->stop());
    EXPECT_EQ(calls_count, 2);

    connector_->unregister_net_if_changes_handler();
    EXPECT_NO_THROW(connector_->start());
    EXPECT_NO_THROW(connector_->send_cbk(boost::asio::error::connection_aborted, 0));
    EXPECT_NO_THROW(connector_->stop());
    EXPECT_EQ(calls_count, 2);
}

TEST_F(ipv4_netlink_fixture, receive_cbk_error) {
    simulate_localhost_availability();
    bool interface_available = true;
    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    interface_available = _available;
                }
            });
    connector_->receive_cbk(boost::asio::error::already_open, recv_buffer_cursor_);
    EXPECT_FALSE(interface_available);
}

TEST_F(ipv4_netlink_fixture, recover_after_first_request_generate_error) {
    using namespace std::chrono_literals;

    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string _name, bool _available) {
                if (_interface && _name == "lo") {
                    localhost_available = _available;
                }
            });

    boost::system::error_code ec;

    connector_->socket_.open(vsomeip_v3::nl_protocol(NETLINK_ROUTE), ec);
    connector_->socket_.bind(vsomeip_v3::nl_endpoint<vsomeip_v3::nl_protocol>(), ec);

    nl_new_message_(NLMSG_ERROR);
    auto errmsg = nl_append<struct nlmsgerr>();
    errmsg->error = -ENOBUFS;
    errmsg->msg.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    errmsg->msg.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    errmsg->msg.nlmsg_type = RTM_GETADDR;
    errmsg->msg.nlmsg_seq = vsomeip_v3::netlink_connector::ifa_request_sequence_;
    nl_copy_to_recv_buffer();

    connector_->address_ = boost::asio::ip::address_v4(0x7F000001);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);

    while (!localhost_available) {
        context_.run_one_for(2ms);
    }

    EXPECT_NO_THROW(connector_->stop());
}

TEST_F(ipv4_netlink_fixture, address_in_the_middle) {
    nl_new_message_(NLMSG_NOOP);
    nl_copy_to_recv_buffer();
    nl_copy_to_recv_buffer();
    nl_copy_to_recv_buffer();
    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    *nl_add_attribute<uint32_t>(IFA_ADDRESS) = ::htonl(IP_ADDR);
    nl_copy_to_recv_buffer();
    nl_new_message_(NLMSG_DONE);
    nl_copy_to_recv_buffer();
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    connector_->send_ifi_request(1);
    EXPECT_EQ(connector_->net_if_index_for_address_, INTERFACE_IDX);
}

TEST_F(ipv4_netlink_fixture, wrong_address) {
    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_PERMANENT;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    *nl_add_attribute<uint32_t>(IFA_ADDRESS) = ::htonl(IP_ADDR + 1);
    nl_copy_to_recv_buffer();
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    connector_->send_ifi_request(1);
    EXPECT_NE(connector_->net_if_index_for_address_, INTERFACE_IDX);
}

TEST_F(ipv4_netlink_fixture, missing_address) {
    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_PERMANENT;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = 13;
    *nl_add_attribute<uint32_t>(IFA_ADDRESS) = ::htonl(IP_ADDR);
    nl_copy_to_recv_buffer();
    nl_new_message_(RTM_NEWADDR);
    addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_PERMANENT;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = 14;
    nl_copy_to_recv_buffer();
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    connector_->send_ifi_request(1);
    EXPECT_EQ(connector_->net_if_index_for_address_, 13);
}

TEST_F(ipv4_netlink_fixture, broken_packet) {
    simulate_localhost_availability();

    bool localhost_available = true;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                }
            });

    nl_new_message_(RTM_NEWADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_PERMANENT;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = 13;
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_ - 1);

    EXPECT_FALSE(localhost_available);
}

TEST_F(ipv4_netlink_fixture, multicast_route_available) {
    simulate_localhost_availability();

    bool multicast_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (!_interface) {
                    multicast_available = _available;
                }
            });

    // First route with the wrong interface index
    nl_new_message_(RTM_NEWROUTE);
    auto routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 24;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<uint32_t>(RTA_DST) = ::htonl(MULTICAST_ADDR);
    *nl_add_attribute<int>(RTA_OIF) = INTERFACE_IDX + 1;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(multicast_available);

    // Second route with the correct interface index
    nl_new_message_(RTM_NEWROUTE);
    routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 24;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<uint32_t>(RTA_DST) = ::htonl(MULTICAST_ADDR + 100);
    *nl_add_attribute<int>(RTA_OIF) = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_TRUE(multicast_available);

    // Second route removed
    nl_new_message_(RTM_DELROUTE);
    routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 24;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<uint32_t>(RTA_DST) = ::htonl(MULTICAST_ADDR + 1);
    *nl_add_attribute<int>(RTA_OIF) = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(multicast_available);
}

TEST_F(ipv4_netlink_fixture, multicast_down_when_address_removed) {
    simulate_localhost_availability();

    bool multicast_available = false;
    bool localhost_available = true;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (_interface) {
                    localhost_available = _available;
                } else {
                    multicast_available = _available;
                }
            });

    // Route available
    nl_new_message_(RTM_NEWROUTE);
    auto routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 24;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<uint32_t>(RTA_DST) = ::htonl(MULTICAST_ADDR + 100);
    *nl_add_attribute<int>(RTA_OIF) = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_TRUE(localhost_available);
    EXPECT_TRUE(multicast_available);

    // Address removed
    nl_new_message_(RTM_DELADDR);
    auto addrmsg = nl_append<struct ifaddrmsg>();
    addrmsg->ifa_family = AF_INET;
    addrmsg->ifa_prefixlen = 28;
    addrmsg->ifa_flags = IFA_F_SECONDARY;
    addrmsg->ifa_scope = RT_SCOPE_UNIVERSE;
    addrmsg->ifa_index = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(localhost_available);
    EXPECT_FALSE(multicast_available);
}

TEST_F(ipv4_netlink_fixture, default_route_available_on_another_interface) {
    simulate_localhost_availability();

    bool multicast_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (!_interface) {
                    multicast_available = _available;
                }
            });

    nl_new_message_(RTM_NEWROUTE);
    auto routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 0;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<uint32_t>(RTA_DST) = ::htonl(~MULTICAST_ADDR);
    *nl_add_attribute<int>(RTA_OIF) = INTERFACE_IDX + 1;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_FALSE(multicast_available);
}

TEST_F(ipv4_netlink_fixture, default_route_available_on_expected_interface) {
    simulate_localhost_availability();

    bool multicast_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (!_interface) {
                    multicast_available = _available;
                }
            });

    nl_new_message_(RTM_NEWROUTE);
    auto routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 0;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<uint32_t>(RTA_DST) = ::htonl(~MULTICAST_ADDR);
    *nl_add_attribute<int>(RTA_OIF) = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_TRUE(multicast_available);
}

TEST_F(ipv4_netlink_fixture, default_route_with_gateway_on_expected_interface) {
    simulate_localhost_availability();

    bool multicast_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (!_interface) {
                    multicast_available = _available;
                }
            });

    nl_new_message_(RTM_NEWROUTE);
    auto routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 0;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<uint32_t>(RTA_DST) = ::htonl(~MULTICAST_ADDR);
    *nl_add_attribute<uint32_t>(RTA_GATEWAY) = ::htonl(IP_ADDR + 10);
    *nl_add_attribute<int>(RTA_OIF) = INTERFACE_IDX;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);

    // TODO: Multicasting via gateway isn't supported
    // Reenable it when fixed
    // EXPECT_FALSE(multicast_available);
}

struct ipv4_no_multicast_netlink_fixture : public simulation_netlink_fixture {
    ipv4_no_multicast_netlink_fixture() {
        listening_address_ = boost::asio::ip::address_v4(ipv4_netlink_fixture::IP_ADDR);
    }
};

TEST_F(ipv4_no_multicast_netlink_fixture, start_stop) {
    using namespace std::chrono_literals;

    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string _name, bool _available) {
                if (_interface && _name == "lo") {
                    localhost_available = _available;
                }
            });

    EXPECT_NO_THROW(connector_->start());
    while (!localhost_available) {
        context_.run_one_for(2ms);
    }
    EXPECT_NO_THROW(connector_->stop());
    EXPECT_TRUE(localhost_available);
}

struct ipv6_netlink_fixture : public simulation_netlink_fixture {
    ipv6_netlink_fixture() {
        listening_address_ = boost::asio::ip::address_v6::from_string("::1");
        multicast_address_ = boost::asio::ip::address_v6::from_string("ff02::1");
    }
};

TEST_F(ipv6_netlink_fixture, start_stop) {
    using namespace std::chrono_literals;

    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string _name, bool _available) {
                if (_interface && _name == "lo") {
                    localhost_available = _available;
                }
            });

    EXPECT_NO_THROW(connector_->start());
    while (!localhost_available) {
        context_.run_one_for(2ms);
    }
    EXPECT_NO_THROW(connector_->stop());
    EXPECT_TRUE(localhost_available);
}

TEST_F(ipv6_netlink_fixture, multicast_route_available) {
    using namespace std::chrono_literals;

    bool multicast_available = false;
    bool localhost_available = false;

    connector_->register_net_if_changes_handler(
            [&](bool _interface, std::string /*_name*/, bool _available) {
                if (!_interface) {
                    multicast_available = _available;
                } else {
                    localhost_available = _available;
                }
            });

    EXPECT_NO_THROW(connector_->start());
    while (!localhost_available) {
        context_.run_one_for(2ms);
    }

    auto ipv6addr = multicast_address_.to_v6();

    nl_new_message_(RTM_NEWROUTE);
    auto routemsg = nl_append<struct rtmsg>();
    routemsg->rtm_family = AF_INET;
    routemsg->rtm_dst_len = 24;
    routemsg->rtm_src_len = 0;
    routemsg->rtm_tos = 0;
    routemsg->rtm_table = 13;
    routemsg->rtm_protocol = RTPROT_STATIC;
    routemsg->rtm_scope = RT_SCOPE_HOST;
    routemsg->rtm_type = RTN_MULTICAST;
    *nl_add_attribute<struct in6_addr>(RTA_DST) =
            *reinterpret_cast<struct in6_addr*>(ipv6addr.to_bytes().data());
    *nl_add_attribute<struct in6_addr>(RTA_GATEWAY) =
            *reinterpret_cast<struct in6_addr*>(ipv6addr.to_bytes().data());
    *nl_add_attribute<int>(RTA_OIF) = connector_->net_if_index_for_address_;
    nl_copy_to_recv_buffer(true);
    connector_->receive_cbk(boost::system::error_code(), recv_buffer_cursor_);
    EXPECT_TRUE(multicast_available);
}

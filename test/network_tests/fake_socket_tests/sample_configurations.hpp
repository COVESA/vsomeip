// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "helpers/ecu_config.hpp"
#include "sample_interfaces.hpp"

namespace vsomeip_v3::testing {
namespace local {

inline const ecu_config uds_config = []() {
    ecu_config cfg;
    cfg.apps_ = {application_config{"routingmanagerd", std::nullopt}, application_config{"server", 0x3489},
                 application_config{"client", 0x3490}};
    cfg.network_ = "vsomeip";
    cfg.routing_config_ = std::string("routingmanagerd");
    cfg.sd_ = false;
    return cfg;
}();

inline const ecu_config tcp_config = []() {
    ecu_config cfg;
    cfg.apps_ = {application_config{"routingmanagerd", 0x0100}, application_config{"server", 0x3489}, application_config{"client", 0x3490},
                 application_config{"client-one", 0x3491}, application_config{"client-two", 0x3492}};
    cfg.routing_config_ = local_tcp_config{.router_name_ = "routingmanagerd",
                                           .host_ = boost::asio::ip::make_address("127.0.0.1"),
                                           .guest_ = boost::asio::ip::make_address("127.0.0.1")};
    cfg.sd_ = true;
    return cfg;
}();

}

namespace hybrid {

/// Host ECU (127.0.0.1) with uds_preferred=true.
/// router_one is the routing manager; service_3344 is offered locally.
/// Apps on the same host connect via UDS; remote guests are forced to use TCP.
inline const ecu_config host_uds_preferred_config = []() {
    ecu_config cfg{{interfaces::boardnet::service_3344}};
    cfg.unicast_ip_ = boost::asio::ip::make_address("127.0.0.1");
    cfg.apps_ = {application_config{"router_one", 0x32fe}};
    cfg.routing_config_ = local_tcp_config{.router_name_ = "router_one",
                                           .host_ = boost::asio::ip::make_address("127.0.0.1"),
                                           .guest_ = boost::asio::ip::make_address("127.0.0.1")};
    cfg.client_ports_ = client_port_config{.remote_ports_ = {30501, 30599}, .client_ports_ = {30491, 30499}};
    cfg.uds_preferred_ = true;
    return cfg;
}();

/// Host ECU (127.0.0.1) without uds_preferred (TCP-only local transport).
/// Same topology as host_uds_preferred_config but clients connect via TCP.
inline const ecu_config host_tcp_config = []() {
    ecu_config cfg{{interfaces::boardnet::service_3344}};
    cfg.unicast_ip_ = boost::asio::ip::make_address("127.0.0.1");
    cfg.apps_ = {application_config{"router_one", 0x32fe}};
    cfg.routing_config_ = local_tcp_config{.router_name_ = "router_one",
                                           .host_ = boost::asio::ip::make_address("127.0.0.1"),
                                           .guest_ = boost::asio::ip::make_address("127.0.0.1")};
    cfg.client_ports_ = client_port_config{.remote_ports_ = {30501, 30599}, .client_ports_ = {30491, 30499}};
    cfg.uds_preferred_ = false;
    return cfg;
}();
}

namespace boardnet {
// ecu_one is the client side — no offered services, only routing + client port config
inline const ecu_config ecu_one_config = []() {
    ecu_config cfg;
    cfg.unicast_ip_ = boost::asio::ip::make_address("160.48.199.99");
    cfg.routing_config_ = local_tcp_config{.router_name_ = "router_one",
                                           .host_ = boost::asio::ip::make_address("160.48.199.253"),
                                           .guest_ = boost::asio::ip::make_address("160.48.199.253")};
    cfg.network_ = "vsomeip-one";
    return cfg;
}();

// ecu_two offers service_3344 on the boardnet
inline const ecu_config ecu_two_config = []() {
    ecu_config cfg = {{interfaces::boardnet::service_3344}};

    cfg.unicast_ip_ = boost::asio::ip::make_address("160.48.199.98");
    cfg.routing_config_ = local_tcp_config{.router_name_ = "router_two",
                                           .host_ = boost::asio::ip::make_address("160.48.199.181"),
                                           .guest_ = boost::asio::ip::make_address("160.48.199.181")};
    cfg.network_ = "vsomeip-two";
    return cfg;
}();

}
}

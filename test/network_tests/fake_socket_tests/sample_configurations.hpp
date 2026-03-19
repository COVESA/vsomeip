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

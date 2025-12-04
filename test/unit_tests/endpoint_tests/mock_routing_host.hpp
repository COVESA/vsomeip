// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_MOCK_ROUTING_HOST_
#define VSOMEIP_V3_MOCK_ROUTING_HOST_

#include "../../../implementation/routing/include/routing_host.hpp"

#include <gmock/gmock.h>

namespace vsomeip_v3::testing {

class mock_routing_host : public routing_host {
public:
    mock_routing_host();
    ~mock_routing_host();

    MOCK_METHOD(void, on_message,
                (const byte_t*, length_t, endpoint*, bool, client_t, const vsomeip_sec_client_t*, const boost::asio::ip::address&,
                 std::uint16_t),
                (override));
    MOCK_METHOD(client_t, get_client, (), (const, override));
    MOCK_METHOD(void, add_known_client, (client_t, const std::string&), (override));
    MOCK_METHOD(void, remove_known_client, (client_t), (override));
    MOCK_METHOD(client_t, get_guest_by_address, (const boost::asio::ip::address&, port_t), (const, override));
    MOCK_METHOD(void, add_guest, (client_t, const boost::asio::ip::address&, port_t), (override));
    MOCK_METHOD(void, remove_local, (client_t, bool, bool), (override));
    MOCK_METHOD(std::string, get_env, (client_t), (const, override));
    MOCK_METHOD(void, remove_subscriptions, (port_t, const boost::asio::ip::address&, port_t), (override));
};
}

#endif

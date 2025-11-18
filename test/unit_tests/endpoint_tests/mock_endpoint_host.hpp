// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_MOCK_ENDPOINT_HOST_
#define VSOMEIP_V3_MOCK_ENDPOINT_HOST_

#include "../../../implementation/endpoints/include/endpoint_host.hpp"

#include <gmock/gmock.h>

namespace vsomeip_v3::testing {

class mock_endpoint_host : public endpoint_host {
public:
    mock_endpoint_host();
    ~mock_endpoint_host();

    MOCK_METHOD(void, on_connect, (std::shared_ptr<endpoint>), (override));
    MOCK_METHOD(void, on_disconnect, (std::shared_ptr<endpoint>), (override));
    MOCK_METHOD(bool, on_bind_error, (std::shared_ptr<endpoint>, const boost::asio::ip::address&, uint16_t, uint16_t&), (override));
    MOCK_METHOD(void, on_error, (const byte_t*, length_t, endpoint* const, const boost::asio::ip::address&, std::uint16_t), (override));
    MOCK_METHOD(void, release_port, (uint16_t, bool), (override));
    MOCK_METHOD(client_t, get_client, (), (const, override));
    MOCK_METHOD(std::string, get_client_host, (), (const, override));
    MOCK_METHOD(instance_t, find_instance, (service_t, endpoint* const), (const, override));
    MOCK_METHOD(void, add_multicast_option, (const multicast_option_t&), (override));
};
}

#endif

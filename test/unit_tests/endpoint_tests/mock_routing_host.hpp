// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../implementation/routing/include/routing_host.hpp"

#include <gmock/gmock.h>

namespace vsomeip_v3::testing {

class mock_routing_host : public routing_host {
public:
    mock_routing_host();
    ~mock_routing_host();

    MOCK_METHOD(void, on_message, (const byte_t*, length_t, const local_client_data&), (override));
    MOCK_METHOD(client_t, get_client, (), (const, override));
    MOCK_METHOD(void, lazy_load, (const std::string&), (override));
};
}

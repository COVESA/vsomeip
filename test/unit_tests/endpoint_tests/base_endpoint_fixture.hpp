// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_BASE_ENDPOINT_FIXTURE
#define VSOMEIP_V3_BASE_ENDPOINT_FIXTURE

#include "delegating_socket_factory.hpp"

#include <gtest/gtest.h>

namespace vsomeip_v3::testing {

struct base_endpoint_fixture : ::testing::Test {

    static void SetUpTestSuite() { vsomeip_v3::set_abstract_factory(delegate_); }
    static std::shared_ptr<delegating_socket_factory> delegate_;
};

}

#endif

// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "base_endpoint_fixture.hpp"

namespace vsomeip_v3::testing {

std::shared_ptr<delegating_socket_factory> base_endpoint_fixture::delegate_ = std::make_shared<delegating_socket_factory>();
}

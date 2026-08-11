// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "mock_configuration.hpp"

namespace vsomeip_v3::testing {
mock_configuration::mock_configuration() : configuration_impl("") { }
mock_configuration::~mock_configuration() = default;
}

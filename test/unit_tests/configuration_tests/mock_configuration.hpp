// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../implementation/configuration/include/configuration_impl.hpp"

#include <gmock/gmock.h>

namespace vsomeip_v3::testing {

class mock_configuration : public cfg::configuration_impl {
public:
    mock_configuration();
    ~mock_configuration();

    MOCK_METHOD(bool, load, (const std::string&), (override));
};
}

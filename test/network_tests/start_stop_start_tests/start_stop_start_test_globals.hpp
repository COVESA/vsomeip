// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once

#include <cassert>
#include <cstdint>
#include <string_view>

#include <common/vsomeip_app.hpp>
#include <common/scenario.hpp>

namespace start_stop_start {

using namespace common;

/* clang-format on */

enum class state_t : uint32_t {
    REGISTERED = 0x1,
    NOT_REGISTERED = 0x1 << 1,
    DEREGISTERED = 0x1 << 2,

    STOPPED = 0x1 << 3,
};

constexpr static std::string_view TEST_NAME = "StartStopStartSteps";

// App1 always index 0
constexpr static int APP1_IDX = 0;
// APP2  always index 1
constexpr static int APP2_IDX = 1;

constexpr static int N_COMPONENTS_T1 = 2;
constexpr static int N_COMPONENTS_T2 = 3;

constexpr static size_t N_TEST_STEPS = static_cast<size_t>(Steps::_5);
} // namespace start_stop_start

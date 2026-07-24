// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "abstract_clock.hpp"

namespace vsomeip_v3 {
/**
 *  Production clock: reads the real monotonic std::chrono::steady_clock.
 *  Stateless, hence safely shared.
 **/
class steady_clock final : public abstract_clock {
public:
    std::chrono::steady_clock::time_point now() const override { return std::chrono::steady_clock::now(); }
};
}

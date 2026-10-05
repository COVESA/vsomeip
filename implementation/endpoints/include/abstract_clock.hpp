// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>

namespace vsomeip_v3 {
/**
 *  abstraction of "the current time", injected via abstract_socket_factory::get_clock(),
 *  so time-dependent logic can be driven deterministically in tests.
 *  Mirrors the abstract_timer injection seam.
 **/
class abstract_clock {
public:
    virtual ~abstract_clock() = default;

    virtual std::chrono::steady_clock::time_point now() const = 0;
};
}

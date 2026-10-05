// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../../implementation/endpoints/include/abstract_clock.hpp"

#include <atomic>
#include <chrono>

namespace vsomeip_v3::testing {

/**
 *  Manually-controlled test clock. Injected via a fake abstract_socket_factory so
 *  time-dependent logic can be tested deterministically without real sleeps.
 *
 *  Thread-safe: now() may be read from an io thread (e.g. a cyclic tick) while the
 *  test thread advances time. Single writer (the test) → load/store is sufficient.
 **/
class fake_clock final : public abstract_clock {
public:
    std::chrono::steady_clock::time_point now() const override { return now_.load(); }

    void set(std::chrono::steady_clock::time_point _tp) { now_.store(_tp); }

    void advance(std::chrono::milliseconds _by) { now_.store(now_.load() + _by); }

private:
    std::atomic<std::chrono::steady_clock::time_point> now_{std::chrono::steady_clock::time_point{}};
};

}

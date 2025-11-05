// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ABSTRACT_TIMER_HPP_
#define VSOMEIP_V3_ABSTRACT_TIMER_HPP_

#include <boost/system/error_code.hpp>

#include <functional>
#include <chrono>

namespace vsomeip_v3 {

/**
 *  abstraction for the boost::asio::steady_timer to allow for fake injections
 **/
class abstract_timer {
public:
    using handler_t = std::function<void(boost::system::error_code)>;

    virtual ~abstract_timer() = default;

    /// @see boost::asio::steady_timer::cancel()
    virtual void cancel() = 0;

    /// @see boost::asio::steady_timer::expires_after()
    virtual void expires_after(std::chrono::milliseconds _timeout) = 0;

    /// @see boost::asio::steady_timer::async_wait()
    virtual void async_wait(handler_t _handler) = 0;
};
}

#endif

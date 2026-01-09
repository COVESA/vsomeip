// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ASIO_TIMER_HPP_
#define VSOMEIP_V3_ASIO_TIMER_HPP_

#include "abstract_timer.hpp"
#include <boost/asio.hpp>

namespace vsomeip_v3 {

class asio_timer : public abstract_timer {
public:
    asio_timer(boost::asio::io_context& _io) : timer_(_io) { }
    virtual ~asio_timer() override = default;

private:
    virtual void cancel() override { timer_.cancel(); }

    virtual void expires_after(std::chrono::milliseconds _timeout) override { timer_.expires_after(_timeout); }

    virtual void async_wait(handler_t _handler) override { timer_.async_wait(_handler); }

    boost::asio::steady_timer timer_;
};
}

#endif

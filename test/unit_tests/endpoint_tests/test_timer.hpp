// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TIMER_TESTING_
#define VSOMEIP_V3_TIMER_TESTING_

#include "base_endpoint_fixture.hpp"

#include "../../../implementation/endpoints/include/timer.hpp"
#include "../../../implementation/endpoints/include/asio_timer.hpp"
#include "../../../implementation/endpoints/include/abstract_socket_factory.hpp"
#include "../../../implementation/endpoints/include/abstract_netlink_connector.hpp"
#include "../../../implementation/endpoints/include/abstract_timer.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/system/error_code.hpp>
#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <memory>
#include <optional>

namespace vsomeip_v3::testing {

struct timer_state {
    uint32_t cancel_count_{0};
    uint32_t start_count_{0};
    std::optional<std::chrono::milliseconds> timeout_;
    abstract_timer::handler_t handler_;
};

class fake_timer : public abstract_timer {
public:
    fake_timer(std::shared_ptr<timer_state> _state) : state_(std::move(_state)) { }
    virtual void cancel() override { ++state_->cancel_count_; }

    virtual void expires_after(std::chrono::milliseconds _timeout) override { state_->timeout_ = _timeout; }

    virtual void async_wait(handler_t _handler) {
        state_->handler_ = std::move(_handler);
        ++state_->start_count_;
    }

    std::shared_ptr<timer_state> state_;
};

class fake_factory : public abstract_socket_factory {
public:
    std::shared_ptr<abstract_netlink_connector> create_netlink_connector(boost::asio::io_context&, const boost::asio::ip::address&,
                                                                         const boost::asio::ip::address&, bool) override {
        return nullptr;
    }

    virtual std::unique_ptr<tcp_socket> create_tcp_socket(boost::asio::io_context&) override { return nullptr; }
    virtual std::unique_ptr<tcp_acceptor> create_tcp_acceptor(boost::asio::io_context&) override { return nullptr; }
    virtual std::unique_ptr<abstract_timer> create_timer(boost::asio::io_context&) override { return std::move(timer_); }

    // assumed to be filled by the fixtures.
    std::unique_ptr<abstract_timer> timer_;
};

struct test_timer_base : base_endpoint_fixture {
    test_timer_base() : factory_(std::make_shared<fake_factory>()) { delegate_->impl_ = factory_; }

    std::shared_ptr<fake_factory> factory_;
};
}
#endif

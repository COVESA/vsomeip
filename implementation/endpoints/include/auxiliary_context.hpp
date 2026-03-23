// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <thread>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

namespace vsomeip_v3 {

// For TCP connections, we must accept them before processing the related
// subscriptions. This is very simple with Linux. We just need to process
// events related to listener sockets first in the select, but it is impossible
// with Boost ASIO. Because it doesn't guarantee the order and then it imposes
// the random order it has chosen. Indeed, if the subscription message is in
// the same queue as the incoming TCP connection notification, then we cannot
// block the handling of the subscription message until the incoming connection
// has been processed, without a dead lock. To avoid this dead lock, we
// have to separate these two messages into two different queues and it
// requires a separate context. In the future, we could reuse this
// context for other purposes.

class auxiliary_context {
    boost::asio::io_context context_;
    std::thread thread_;
    const int thread_niceness_;

public:
    auxiliary_context(int thread_niceness);
    ~auxiliary_context();

    boost::asio::io_context& get_context();
    void start();
    void stop();
};

} // namespace vsomeip_v3

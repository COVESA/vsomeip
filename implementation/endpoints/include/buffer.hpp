// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_BUFFER_HPP_
#define VSOMEIP_V3_BUFFER_HPP_

#include <array>
#include <chrono>
#include <memory>
#include <set>

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/primitive_types.hpp>

#ifdef _WIN32
    #define DEFAULT_NANOSECONDS_MAX 1000000000
#else
    #define DEFAULT_NANOSECONDS_MAX std::chrono::nanoseconds::max()
#endif

namespace vsomeip_v3 {

typedef std::vector<byte_t> message_buffer_t;
typedef std::shared_ptr<message_buffer_t> message_buffer_ptr_t;

struct timing {
    timing() : debouncing_(0), maximum_retention_(DEFAULT_NANOSECONDS_MAX) {};

    std::chrono::nanoseconds debouncing_;
    std::chrono::nanoseconds maximum_retention_;
};

struct train {
    train(boost::asio::io_service& _io) : buffer_(std::make_shared<message_buffer_t>()),
              departure_(DEFAULT_NANOSECONDS_MAX),
              minimal_debounce_time_(DEFAULT_NANOSECONDS_MAX),
              minimal_max_retention_time_(DEFAULT_NANOSECONDS_MAX),
              last_departure_(std::chrono::steady_clock::now() - std::chrono::hours(1)),
              departure_timer_(std::make_shared<boost::asio::steady_timer>(_io)) {};

    message_buffer_ptr_t buffer_;
    std::chrono::nanoseconds departure_;
    std::chrono::nanoseconds minimal_debounce_time_;
    std::chrono::nanoseconds minimal_max_retention_time_;
    std::chrono::steady_clock::time_point last_departure_;
    std::shared_ptr<boost::asio::steady_timer> departure_timer_;
    std::set<std::pair<service_t, method_t> > passengers_;

    void update_departure_time_and_stop_departure() {
        departure_ = departure_timer_->expires_from_now();
        boost::system::error_code ec;
        departure_timer_->cancel(ec);
    }
};


} // namespace vsomeip_v3

#endif // VSOMEIP_V3_BUFFER_HPP_

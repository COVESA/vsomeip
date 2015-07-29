// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_FSM_BASE_HPP
#define VSOMEIP_FSM_BASE_HPP

#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>

namespace vsomeip {
namespace sd {

class fsm_base: public std::enable_shared_from_this<fsm_base> {
public:
    fsm_base(boost::asio::io_service &_io);
    virtual ~fsm_base();

    void start_timer(uint32_t _ms);
    void stop_timer();

    uint32_t expired_from_now();

    virtual void timer_expired(const boost::system::error_code &_error) = 0;

private:
    boost::asio::system_timer timer_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_FSM_BASE_HPP

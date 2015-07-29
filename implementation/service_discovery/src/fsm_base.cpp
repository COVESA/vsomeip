// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/fsm_base.hpp"

namespace vsomeip {
namespace sd {

fsm_base::fsm_base(boost::asio::io_service &_io)
        : timer_(_io) {
}

fsm_base::~fsm_base() {
}

void fsm_base::start_timer(uint32_t _milliseconds) {
    timer_.expires_from_now(std::chrono::milliseconds(_milliseconds));
    timer_.async_wait(
            std::bind(&fsm_base::timer_expired, shared_from_this(),
                    std::placeholders::_1));
}

void fsm_base::stop_timer() {
    timer_.cancel();
}

uint32_t fsm_base::expired_from_now() {
    return (uint32_t) std::chrono::duration_cast < std::chrono::milliseconds
            > (timer_.expires_from_now()).count();
}

} // namespace sd
} // namespace vsomeip

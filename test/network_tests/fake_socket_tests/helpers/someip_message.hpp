// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SOMEIP_MESSAGE_HPP_
#define VSOMEIP_V3_SOMEIP_MESSAGE_HPP_

#include "../../../../implementation/service_discovery/include/message_impl.hpp"
#include "../../../../implementation/message/include/message_impl.hpp"
#include "service_state.hpp"

#include <vsomeip/vsomeip.hpp>
#include "to_string.hpp"

namespace vsomeip_v3::testing {
struct someip_message {
    std::shared_ptr<message_impl> msg_;
    std::shared_ptr<sd::message_impl> sd_;
};

[[nodiscard]] size_t parse(std::vector<unsigned char>& message, someip_message& _out_message);
[[nodiscard]] std::shared_ptr<vsomeip_v3::sd::message_impl> parse_sd(std::vector<unsigned char>& _message);
std::vector<unsigned char> construct_subscription(event_ids const& _subscription, boost::asio::ip::address _address, uint16_t _port);
std::vector<unsigned char> construct_offer(event_ids const& _offer, boost::asio::ip::address _address, uint16_t _port);

std::ostream& operator<<(std::ostream& _out, someip_message const& _m);
}

#endif

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../../implementation/service_discovery/include/message_impl.hpp"
#include "../../../../implementation/message/include/message_impl.hpp"
#include "service_state.hpp"

#include <vsomeip/vsomeip.hpp>
#include "to_string.hpp"
#include "message_common.hpp"

namespace vsomeip_v3::testing {
struct someip_message {
    std::shared_ptr<message_impl> msg_;
    std::shared_ptr<sd::message_impl> sd_;
};

struct someip_sd_record_message {
    sd::entry_type_e id_;
    ttl_t ttl_;

    bool operator==(const someip_sd_record_message& _other) const { return id_ == _other.id_ && ttl_ == _other.ttl_; }
};

[[nodiscard]] size_t parse(std::vector<unsigned char>& message, someip_message& _out_message);
[[nodiscard]] size_t parse_sequential_someip(unsigned char* _message, size_t _message_size, someip_message& _out_message);
[[nodiscard]] std::shared_ptr<vsomeip_v3::sd::message_impl> parse_sd(std::vector<unsigned char>& _message);
std::vector<unsigned char> construct_subscription(event_ids const& _subscription, boost::asio::ip::address _address, uint16_t _port);
std::vector<unsigned char> construct_offer(event_ids const& _offer, boost::asio::ip::address _address, uint16_t _port);

std::ostream& operator<<(std::ostream& _out, someip_message const& _m);

/**
 * @brief Constructs someip message, example:
 * construct_someip_raw_message(static_cast<uint16_t>(service), static_cast<uint16_t>(method), static_cast<uint32_t>(length),
                               static_cast<uint16_t>(client), static_cast<uint16_t>(session), static_cast<uint8_t>(protocol),
                               static_cast<uint8_t>(interface), static_cast<uint8_t>(message_type), static_cast<uint8_t>(return_code));
 *
 * @param payload All fields needed to construct the message.
 * @return std::vector<unsigned char> Constructed raw message.
 */
template<typename... Ts>
std::vector<unsigned char> construct_someip_raw_message(Ts&&... payload) {
    std::vector<unsigned char> message;
    (..., ([&] {
         using U = std::decay_t<decltype(payload)>;
         if constexpr (std::is_same_v<U, std::string> || is_std_vector<U>::value) {
             message.insert(message.end(), payload.begin(), payload.end());
         } else if constexpr (std::is_convertible_v<U, const char*>) {
             std::string s(payload);
             message.insert(message.end(), s.begin(), s.end());
         } else {
             // Byte-swap into big-endian order.
             auto bytes = reinterpret_cast<unsigned char*>(&payload);
             for (std::size_t i = sizeof(payload); i > 0; --i) {
                 message.push_back(bytes[i - 1]);
             }
         }
     }()));
    return message;
}
}

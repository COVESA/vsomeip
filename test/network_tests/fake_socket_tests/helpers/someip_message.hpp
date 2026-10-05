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

#include <cstdint>
#include <iomanip>
#include <optional>
#include <ostream>

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

/// Lightweight struct capturing the header fields of a SOME/IP message for recording.
/// Used with attribute_recorder to allow tests to assert on boardnet SOME/IP traffic.
struct someip_record_message {
    /// Wildcard sentinels for client_ / session_: when either side of a comparison holds
    /// these, that field is ignored and matches any value. Using dedicated sentinels
    /// (rather than 0) keeps 0 usable as a real value one might want to test against.
    static constexpr client_t ANY_CLIENT = 0xFFFF;
    static constexpr session_t ANY_SESSION = 0xFFFF;

    service_t service_{};
    method_t method_{};
    client_t client_{ANY_CLIENT};
    session_t session_{ANY_SESSION};
    message_type_e message_type_{message_type_e::MT_UNKNOWN};
    return_code_e return_code_{return_code_e::E_UNKNOWN};

    /// Equality comparison. ANY_CLIENT / ANY_SESSION act as wildcards matching any value.
    [[nodiscard]] bool operator==(someip_record_message const& _other) const {
        if (service_ != _other.service_ || method_ != _other.method_) {
            return false;
        }
        if (client_ != ANY_CLIENT && _other.client_ != ANY_CLIENT && client_ != _other.client_) {
            return false;
        }
        if (session_ != ANY_SESSION && _other.session_ != ANY_SESSION && session_ != _other.session_) {
            return false;
        }
        return message_type_ == _other.message_type_ && return_code_ == _other.return_code_;
    }
    [[nodiscard]] bool operator!=(someip_record_message const& _other) const { return !(*this == _other); }
};

inline std::ostream& operator<<(std::ostream& _out, someip_record_message const& _m) {
    _out << "{service=0x" << std::hex << std::setfill('0') << std::setw(4) << _m.service_ << " method=0x" << std::setw(4) << _m.method_
         << " client=0x" << std::setw(4) << _m.client_ << " session=0x" << std::setw(4) << _m.session_ << std::dec
         << " type=" << static_cast<int>(_m.message_type_) << " rc=" << static_cast<int>(_m.return_code_) << "}";
    return _out;
}

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
             for (size_t i = sizeof(payload); i > 0; --i) {
                 message.push_back(bytes[i - 1]);
             }
         }
     }()));
    return message;
}

/// SOME/IP-TP flag OR'd into the message type byte to mark a segmented (transport protocol) message.
static constexpr uint8_t SOMEIP_TP_FLAG = 0x20;

/// Bytes added after the 16-byte SOME/IP header for the TP offset/flags field.
static constexpr uint32_t SOMEIP_TP_HEADER_SIZE = 4;

/**
 * @brief Parameters describing a single SOME/IP-TP segment.
 *
 * Defaults describe a well-formed notification segment; override individual members to craft
 * malformed or edge-case segments (wrong offset, missing flags, spoofed length, ...) for tests.
 */
struct someip_tp_segment {
    service_t service_{};
    method_t method_{};
    client_t client_{};
    session_t session_{};
    uint8_t protocol_version_{VSOMEIP_PROTOCOL_VERSION};
    uint8_t interface_version_{0xFF};
    message_type_e message_type_{message_type_e::MT_REQUEST};
    return_code_e return_code_{return_code_e::E_OK};

    /// Byte offset of this segment within the reassembled payload. Per SOME/IP-TP this must be a
    /// multiple of 16: the lower 4 bits of the wire field carry the reserved bits and the flag.
    uint32_t offset_{0};

    /// More-segments flag, set for every segment except the last one.
    bool more_segments_{false};

    /// This segment's slice of the overall payload.
    std::vector<unsigned char> payload_{};

    /// When set, overrides the computed SOME/IP length field (useful for length-spoofing tests).
    std::optional<uint32_t> length_override_{};
};

/**
 * @brief Constructs a single raw SOME/IP-TP segment: 16-byte SOME/IP header + 4-byte TP header + payload.
 *
 * Builds on construct_someip_raw_message. By default the length field is computed from the segment
 * payload and the TP flag is set on the message type; both can be overridden via someip_tp_segment
 * to produce intentionally malformed segments.
 *
 * @param _segment The segment description.
 * @return std::vector<unsigned char> The constructed raw segment.
 */
inline std::vector<unsigned char> construct_someip_tp_segment(someip_tp_segment _segment) {
    uint8_t msg_type = static_cast<uint8_t>(_segment.message_type_) | SOMEIP_TP_FLAG;

    // Length covers everything after the length field: the remaining 8 header bytes (client, session,
    // versions, message type, return code), the 4-byte TP header and the segment payload.
    auto length = _segment.length_override_.value_or(static_cast<uint32_t>((VSOMEIP_FULL_HEADER_SIZE - VSOMEIP_SOMEIP_HEADER_SIZE)
                                                                           + SOMEIP_TP_HEADER_SIZE + _segment.payload_.size()));

    auto tp_header = static_cast<uint32_t>((_segment.offset_ & 0xFFFFFFF0) | (_segment.more_segments_ ? 0x1u : 0x0u));

    return construct_someip_raw_message(static_cast<uint16_t>(_segment.service_), static_cast<uint16_t>(_segment.method_), length,
                                        static_cast<uint16_t>(_segment.client_), static_cast<uint16_t>(_segment.session_),
                                        _segment.protocol_version_, _segment.interface_version_, msg_type,
                                        static_cast<uint8_t>(_segment.return_code_), tp_header, _segment.payload_);
}
}

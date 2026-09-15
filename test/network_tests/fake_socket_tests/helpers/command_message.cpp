// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "command_message.hpp"

#include "../../../../implementation/protocol/include/deserialize.hpp"
#include "../../../../implementation/utility/include/is_value.hpp"
#include "test_logging.hpp"
#include <cstdint>
#include <ostream>
#include <utility>
#include <iomanip>
#include <sstream>
#include <cstring> // memcpy
#include <vsomeip/payload.hpp>

namespace vsomeip_v3::testing {

[[nodiscard]] size_t parse(unsigned char const* _data, size_t _len, command_message& _out_message) {
    if (_len < protocol::COMMAND_POSITION_PAYLOAD) {
        TEST_LOG << "wire bytes were not long enough to contain the header";
        return 0;
    }
    if (std::numeric_limits<uint32_t>::max() < _len) {
        TEST_LOG << "wire bytes are too much to interpret";
        return 0;
    }
    uint32_t size = static_cast<uint32_t>(_len);

    auto parsed_hdr_bytes = protocol::deserialize(_out_message.header_, _data, size);
    if (parsed_hdr_bytes == 0) {
        TEST_LOG << "ERROR message dump: " << utility::dump(_data, size);
        return 0;
    }
    _out_message.id_ = _out_message.header_.id_;

    if (size - parsed_hdr_bytes < _out_message.header_.length_) {
        TEST_LOG << "ERROR length is not contained in the memory slice, message dump: " << utility::dump(_data, size);
        return 0;
    }
    auto const deal_with_payload = [&](auto payload) -> uint32_t {
        if (auto parsed = protocol::deserialize(payload, _data + parsed_hdr_bytes, _out_message.header_.length_); parsed != 0) {
            _out_message.payload_ = command_payload(std::move(payload));
            return parsed + parsed_hdr_bytes;
        }
        TEST_LOG << "ERROR payload could not be parsed for header: " << to_string(_out_message.header_)
                 << ", message dump: " << utility::dump(_data, size);
        return 0;
    };
    if (_out_message.header_.id_ == protocol::id_e::ROUTING_INFO_ID) {
        return deal_with_payload(std::vector<protocol::routing_info_entry_data>{});
    } else if (is_value(_out_message.header_.id_).any_of(protocol::id_e::OFFER_SERVICE_ID, protocol::id_e::STOP_OFFER_SERVICE_ID)) {
        return deal_with_payload(protocol::service_data{});
    } else if (_out_message.header_.id_ == protocol::id_e::CONFIG_ID) {
        return deal_with_payload(std::vector<std::pair<std::string, std::string>>{});
    } else if (_out_message.header_.id_ == protocol::id_e::REQUEST_SERVICE_ID) {
        return deal_with_payload(std::vector<protocol::service_data>{});
    } else if (_out_message.header_.id_ == protocol::id_e::RELEASE_SERVICE_ID) {
        return deal_with_payload(protocol::service_key{});
    } else if (_out_message.header_.id_ == protocol::id_e::REGISTER_EVENT_ID) {
        return deal_with_payload(std::vector<protocol::register_event_data>{});
    } else if (_out_message.header_.id_ == protocol::id_e::UNREGISTER_EVENT_ID) {
        return deal_with_payload(protocol::unregister_event_data{});
    } else if (_out_message.header_.id_ == protocol::id_e::UNSUBSCRIBE_ID) {
        // SUBSCRIBE_ID is deliberately absent: its payload carries a trailing debounce filter, so a
        // subscribe_data parse would report fewer bytes than the message holds and mis-frame the stream.
        return deal_with_payload(protocol::subscribe_data{});
    } else if (is_value(_out_message.header_.id_).any_of(protocol::id_e::SUBSCRIBE_ACK_ID, protocol::id_e::SUBSCRIBE_NACK_ID)) {
        return deal_with_payload(protocol::subscribe_answer_data{});
    } else if (_out_message.header_.id_ == protocol::id_e::UNSUBSCRIBE_ACK_ID) {
        return deal_with_payload(protocol::unsubscribe_ack_data{});
    } else {
        std::vector<unsigned char> payload;
        payload.reserve(_out_message.header_.length_);
        std::copy(_data + parsed_hdr_bytes, _data + parsed_hdr_bytes + _out_message.header_.length_, std::back_inserter(payload));
        _out_message.payload_ = command_payload(std::move(payload));
        return parsed_hdr_bytes + _out_message.header_.length_;
    }
}

[[nodiscard]] size_t parse(const std::vector<unsigned char>& _message, command_message& _out_message) {
    return parse(_message.data(), _message.size(), _out_message);
}

std::ostream& operator<<(std::ostream& _out, command_message const& _m) {
    _out << "{ header: " << to_string(_m.header_) << ", payload: " << to_string(_m.payload_) << "}";
    return _out;
}
}

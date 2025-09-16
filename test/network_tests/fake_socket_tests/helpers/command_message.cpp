// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "command_message.hpp"

#include "test_logging.hpp"
#include <cstdint>
#include <ostream>
#include <utility>
#include <iomanip>
#include <sstream>
#include <cstring> // memcpy
#include <vsomeip/payload.hpp>

namespace vsomeip_v3::testing {

[[nodiscard]] bool parse(std::vector<unsigned char> const& _message, command_message& _out) {
    if (_message.size() < protocol::COMMAND_POSITION_PAYLOAD + 2 * protocol::TAG_SIZE) {
        TEST_LOG << "wire bytes were not long enough to contain the header";
        return false;
    }
    auto const deal_with_important_command = [&](auto command) {
        std::vector<unsigned char> payload;
        payload.reserve(_message.size() - 2 * protocol::TAG_SIZE);
        std::copy(_message.begin() + protocol::TAG_SIZE, _message.end() - protocol::TAG_SIZE, std::back_inserter(payload));
        protocol::error_e e;
        command.deserialize(payload, e);
        if (e == protocol::error_e::ERROR_OK) {
            _out.id_ = command.get_id();
            _out.client_id_ = command.get_client();
            _out.payload_ = command_payload(std::move(command));
            return true;
        }
        return false;
    };

    // understand how to parse the data
    std::memcpy(&_out.id_, &_message[protocol::TAG_SIZE + protocol::COMMAND_POSITION_ID], 1);
    if (_out.id_ == protocol::id_e::ROUTING_INFO_ID && deal_with_important_command(protocol::routing_info_command{})) {
        return true;
    } else if (_out.id_ == protocol::id_e::CONFIG_ID && deal_with_important_command(protocol::config_command{})) {
        return true;
    }

    // the data is not important enough to parse the command payload. Lets parse the client
    // and copy the payload as is.
    std::memcpy(&_out.client_id_, &_message[protocol::TAG_SIZE + protocol::COMMAND_POSITION_CLIENT], 2);
    uint32_t length;
    std::memcpy(&length, &_message[protocol::TAG_SIZE + protocol::COMMAND_POSITION_SIZE], 4);
    std::vector<unsigned char> payload;
    payload.reserve(length);
    std::copy(_message.begin() + protocol::TAG_SIZE + protocol::COMMAND_POSITION_PAYLOAD, _message.end() - protocol::TAG_SIZE,
              std::back_inserter(payload));
    _out.payload_ = command_payload(std::move(payload));

    return true;
}

std::ostream& operator<<(std::ostream& _out, command_message const& _m) {
    _out << "{id: " << to_string(_m.id_);
    _out << ", client_id: " << std::hex << std::setfill('0') << std::setw(4) << _m.client_id_;
    _out << ", payload: [" << to_string(_m.payload_) << "]}";
    return _out;
}
}

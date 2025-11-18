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

[[nodiscard]] bool parse(std::vector<unsigned char> const& _message, std::vector<command_message>& _out) {
    if (_message.size() < protocol::COMMAND_POSITION_PAYLOAD) {
        TEST_LOG << "wire bytes were not long enough to contain the header";
        return false;
    }

    auto handle_message = [&_out](uint8_t const* _begin, size_t _size) {
        command_message out;
        if (_size < protocol::COMMAND_POSITION_PAYLOAD) {
            TEST_LOG << "wire bytes were not long enough to contain the header";
            return false;
        }
        auto const deal_with_important_command = [&](auto command) {
            std::vector<unsigned char> payload;
            payload.reserve(_size);
            std::copy(_begin, _begin + _size, std::back_inserter(payload));
            protocol::error_e e;
            command.deserialize(payload, e);
            if (e == protocol::error_e::ERROR_OK) {
                out.id_ = command.get_id();
                out.client_id_ = command.get_client();
                out.payload_ = command_payload(std::move(command));
                return true;
            }
            return false;
        };

        // understand how to parse the data
        std::memcpy(&out.id_, &_begin[protocol::COMMAND_POSITION_ID], 1);

        if (out.id_ == protocol::id_e::ROUTING_INFO_ID && deal_with_important_command(protocol::routing_info_command{})) {
            _out.push_back(std::move(out));
        } else if (out.id_ == protocol::id_e::CONFIG_ID && deal_with_important_command(protocol::config_command{})) {
            _out.push_back(std::move(out));
        } else {
            // the data is not important enough to parse the command payload. Lets parse the client
            // and copy the payload as is.
            std::memcpy(&out.client_id_, &_begin[protocol::COMMAND_POSITION_CLIENT], 2);
            uint32_t length;
            std::memcpy(&length, &_begin[protocol::COMMAND_POSITION_SIZE], 4);
            std::vector<unsigned char> payload;
            payload.reserve(length);
            std::copy(_begin + protocol::COMMAND_POSITION_PAYLOAD, _begin + _size, std::back_inserter(payload));
            out.payload_ = command_payload(std::move(payload));
            _out.push_back(std::move(out));
        }
        return true;
    };
    uint32_t length = 0;
    size_t consumed_bytes = 0;
    size_t remaining_bytes = _message.size();
    while (remaining_bytes >= protocol::COMMAND_POSITION_SIZE + sizeof(length)) {
        memcpy(&length, &_message[consumed_bytes + protocol::COMMAND_POSITION_SIZE], sizeof(length));
        if (std::numeric_limits<uint32_t>::max() - protocol::COMMAND_HEADER_SIZE < length) {
            TEST_LOG << "ERROR message length: " << length << " exceeded allowed message size";
            return false;
        }
        auto const size = length + protocol::COMMAND_HEADER_SIZE;
        if (size > remaining_bytes) {
            TEST_LOG << "ERROR remaining_bytes are insufficient";
            return false;
        }
        if (size <= std::numeric_limits<uint32_t>::max()) {
            if (!handle_message(&_message[consumed_bytes], static_cast<uint32_t>(size))) {
                TEST_LOG << "ERROR message could not be parsed";
                return false;
            }
        }
        // guaranteed to work by above checks
        consumed_bytes += size;
        remaining_bytes -= size;
    }
    return true;
}

std::ostream& operator<<(std::ostream& _out, command_message const& _m) {
    _out << "{id: " << to_string(_m.id_);
    _out << ", client_id: " << std::hex << std::setfill('0') << std::setw(4) << _m.client_id_;
    _out << ", payload: [" << to_string(_m.payload_) << "]}";
    return _out;
}
}

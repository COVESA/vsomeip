// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <limits>

#include <boost/asio/ip/address_v4.hpp>

#include <vsomeip/internal/logger.hpp>
#include "../include/assign_client_command.hpp"

namespace vsomeip_v3 {
namespace protocol {

assign_client_command::assign_client_command() : command(id_e::ASSIGN_CLIENT_ID) { }

void assign_client_command::serialize(std::vector<byte_t>& _buffer) const {

    // Layout: header | name_length(4) | name | has_address(1) | address_v4(4) | port(2)
    // Local TCP communication is IPv4-only
    const bool has_address = !address_.is_unspecified();
    size_t its_size(COMMAND_HEADER_SIZE + sizeof(uint32_t) + name_.length() + sizeof(uint8_t));
    if (has_address)
        its_size += sizeof(boost::asio::ip::address_v4::bytes_type) + sizeof(port_t);

    _buffer.resize(its_size);

    // set size
    size_ = static_cast<command_size_t>(its_size - COMMAND_HEADER_SIZE);

    // serialize header
    command::serialize(_buffer);

    // serialize payload
    size_t write_position(COMMAND_POSITION_PAYLOAD);

    auto name_length = static_cast<uint32_t>(name_.length());
    std::memcpy(&_buffer[write_position], &name_length, sizeof(uint32_t));
    write_position += sizeof(uint32_t);

    if (!name_.empty()) {
        std::memcpy(&_buffer[write_position], name_.data(), name_.length());
        write_position += name_.length();
    }

    _buffer[write_position++] = has_address ? 1 : 0;

    if (has_address) {
        auto bytes = address_.to_v4().to_bytes();
        std::memcpy(&_buffer[write_position], bytes.data(), bytes.size());
        write_position += bytes.size();
        std::memcpy(&_buffer[write_position], &port_, sizeof(port_t));
    }
}

void assign_client_command::deserialize(const std::vector<byte_t>& _buffer, error_e& _error) {

    if (_buffer.size() < COMMAND_HEADER_SIZE) {
        _error = error_e::ERROR_NOT_ENOUGH_BYTES;
        return;
    }

    command::deserialize(_buffer, _error);
    if (_error != error_e::ERROR_OK)
        return;

    size_t read_position(COMMAND_POSITION_PAYLOAD);
    size_t remaining(_buffer.size() - COMMAND_HEADER_SIZE);

    // deserialize name length and name
    if (remaining < sizeof(uint32_t)) {
        _error = error_e::ERROR_NOT_ENOUGH_BYTES;
        return;
    }

    uint32_t name_length = 0;
    std::memcpy(&name_length, &_buffer[read_position], sizeof(uint32_t));
    read_position += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    if (remaining < name_length) {
        _error = error_e::ERROR_NOT_ENOUGH_BYTES;
        return;
    }

    if (name_length > 0) {
        name_.assign(&_buffer[read_position], &_buffer[read_position + name_length]);
        read_position += name_length;
        remaining -= name_length;
    }

    if (remaining < sizeof(uint8_t)) {
        _error = error_e::ERROR_NOT_ENOUGH_BYTES;
        return;
    }

    uint8_t has_address = 0;
    std::memcpy(&has_address, &_buffer[read_position], sizeof(uint8_t));
    read_position += sizeof(uint8_t);
    remaining -= sizeof(uint8_t);

    if (has_address) {
        if (remaining < sizeof(boost::asio::ip::address_v4::bytes_type) + sizeof(port_t)) {
            _error = error_e::ERROR_NOT_ENOUGH_BYTES;
            return;
        }
        boost::asio::ip::address_v4::bytes_type its_array;
        std::memcpy(&its_array, &_buffer[read_position], its_array.size());
        address_ = boost::asio::ip::address_v4(its_array);
        read_position += its_array.size();
        std::memcpy(&port_, &_buffer[read_position], sizeof(port_t));
    }
}

std::string assign_client_command::get_name() const {

    return name_;
}

void assign_client_command::set_name(const std::string& _name) {

    name_ = _name;
}

port_t assign_client_command::get_port() const {

    return port_;
}

void assign_client_command::set_port(port_t _port) {

    port_ = _port;
}

boost::asio::ip::address assign_client_command::get_address() const {

    return address_;
}

void assign_client_command::set_address(const boost::asio::ip::address& _address) {

    address_ = _address;
}

} // namespace protocol
} // namespace vsomeip

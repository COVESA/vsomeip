// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command_types.hpp"
#include "vsomeip/defines.hpp"

#include <cstdint>
#include <cstring> // memcpy
#include <iterator>
#include <iostream>

namespace vsomeip_v3::protocol {

template<typename T>
concept has_header = requires(T t) { t.header_; };
template<typename T>
concept has_payload = requires(T t) { t.payload_; };
template<typename T>
concept is_pair = requires(T t) {
    t.first;
    t.second;
};
template<typename T>
concept is_serializable_range = requires(T const& t) {
    std::begin(t);
    std::end(t);
};

static_assert(has_header<service_command_data>);
static_assert(has_payload<service_command_data>);
static_assert(has_header<single_field_command_data<offer_type_e>>);
static_assert(has_payload<single_field_command_data<offer_type_e>>);
static_assert(is_pair<std::pair<int, int>>);
static_assert(!is_pair<int>);

// Forward declaration so write_fields can call serialize in its fold expression.
template<typename T>
uint32_t serialize(T const& _value, unsigned char* _mem);

template<typename T>
uint32_t write_field(unsigned char* _mem, T const& _value) {
    std::memcpy(_mem, &_value, sizeof(T));
    return sizeof(T);
}

template<typename... Ts>
uint32_t write_fields(unsigned char* _mem, Ts const&... _fields) {
    uint32_t written{0};
    ((written += serialize(_fields, _mem + written)), ...);
    return written;
}

inline uint32_t write_string(unsigned char* _mem, std::string_view _value) {
    auto len = static_cast<uint32_t>(_value.size());
    std::memcpy(_mem, &len, sizeof(uint32_t));
    std::memcpy(_mem + sizeof(uint32_t), _value.data(), _value.size());
    return sizeof(uint32_t) + len;
}

template<is_serializable_range R>
uint32_t write_range(unsigned char* _mem, R const& _range) {
    uint32_t written = 0;
    for (auto const& v : _range) {
        written += serialize(v, _mem + written);
    }
    return written;
}

template<typename T>
uint32_t write_be_field(unsigned char* _mem, T _value) {
    if constexpr (std::is_same_v<T, uint8_t>) {
        _mem[0] = _value;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        _mem[0] = static_cast<uint8_t>(_value >> 8);
        _mem[1] = static_cast<uint8_t>(_value);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        _mem[0] = static_cast<uint8_t>(_value >> 24);
        _mem[1] = static_cast<uint8_t>(_value >> 16);
        _mem[2] = static_cast<uint8_t>(_value >> 8);
        _mem[3] = static_cast<uint8_t>(_value);
    } else {
        static_assert(!std::is_same_v<T, T>);
    }
    return sizeof(T);
}

template<typename... Ts>
uint32_t write_be(unsigned char* _mem, Ts const&... _fields) {
    uint32_t written{0};
    ((written += write_be_field(_mem + written, _fields)), ...);
    return written;
}

template<typename T>
uint32_t serialize(T const& _value, unsigned char* _mem) {
    if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
        std::memcpy(_mem, &_value, sizeof(T));
        return sizeof(T);
    } else if constexpr (std::is_same_v<T, command_header>) {
        return write_fields(_mem, _value.id_, _value.version_, _value.client_, _value.length_);
    } else if constexpr (std::is_same_v<T, service_data>) {
        return write_fields(_mem, _value.service_, _value.instance_, _value.major_version_, _value.minor_version_);
    } else if constexpr (std::is_same_v<T, std::string_view>) {
        return write_string(_mem, _value);
    } else if constexpr (std::is_same_v<T, config_entry>) {
        return write_fields(_mem, _value.key_, _value.value_);
    } else if constexpr (std::is_same_v<T, assign_client_data>) {
        uint32_t written = write_string(_mem, _value.name_);
        uint8_t has_addr = _value.has_address_ ? 1 : 0;
        written += serialize(has_addr, _mem + written);
        if (_value.has_address_) {
            written += write_range(_mem + written, _value.address_bytes_);
            written += serialize(_value.port_, _mem + written);
        }
        return written;
    } else if constexpr (std::is_same_v<T, register_event_data>) {
        return write_fields(_mem, _value.service_, _value.instance_, _value.event_, _value.event_type_, _value.is_provided_,
                            _value.reliability_, _value.is_cyclic_, static_cast<uint16_t>(_value.eventgroups_.size()), _value.eventgroups_);
    } else if constexpr (std::is_same_v<T, ipc_message_header>) {
        return write_fields(_mem, _value.instance_, _value.reliable_, _value.status_, _value.target_);
    } else if constexpr (std::is_same_v<T, extended_someip_message>) {
        uint32_t written = serialize(_value.auxiliary_header_, _mem);
        // the remainder is standard someip-encoding
        return written + serialize(_value.data_, _mem + written);
    } else if constexpr (std::is_same_v<T, raw_someip_message>) {
        uint32_t written = serialize(_value.auxiliary_header_, _mem);
        std::memcpy(_mem + written, _value.data_, _value.size_);
        return written + _value.size_;
    } else if constexpr (std::is_same_v<T, std::shared_ptr<message>>) {
        if (!_value) {
            return 0;
        }
        auto payload = _value->get_payload();
        uint32_t const length = payload ? payload->get_length() : 0;
        // SOME/IP header is big-endian (network byte order)
        uint32_t written =
                write_be(_mem, _value->get_service(), _value->get_method(), length + VSOMEIP_SOMEIP_HEADER_SIZE, _value->get_client(),
                         _value->get_session(), _value->get_protocol_version(), _value->get_interface_version(),
                         static_cast<uint8_t>(_value->get_message_type()), static_cast<uint8_t>(_value->get_return_code()));

        // the payload needs encoding only
        if (length > 0) {
            std::memcpy(_mem + written, payload->get_data(), length);
        }
        written += length;
        return written;
    } else if constexpr (std::is_same_v<T, release_service_data>) {
        return write_fields(_mem, _value.service_, _value.instance_);
    } else if constexpr (std::is_same_v<T, unregister_event_data>) {
        return write_fields(_mem, _value.service_, _value.instance_, _value.event_, _value.is_provided_);
    } else if constexpr (std::is_same_v<T, unsubscribe_ack_data>) {
        return write_fields(_mem, _value.service_, _value.instance_, _value.eventgroup_, _value.pending_id_);
    } else if constexpr (std::is_same_v<T, remove_security_policy_data>) {
        return write_fields(_mem, _value.update_id_, _value.uid_, _value.gid_);
    } else if constexpr (std::is_same_v<T, subscribe_data>) {
        return write_fields(_mem, _value.service_, _value.instance_, _value.eventgroup_, _value.major_, _value.event_, _value.pending_id_);
    } else if constexpr (is_pair<T>) {
        return write_fields(_mem, _value.first, _value.second);
    } else if constexpr (std::is_same_v<T, std::shared_ptr<debounce_filter_impl_t>>) {
        return _value ? write_fields(_mem, _value->on_change_, _value->on_change_resets_interval_, _value->interval_, _value->ignore_,
                                     _value->send_current_value_after_)
                      : 0;
    } else if constexpr (std::is_same_v<T, subscribe_with_filter_data>) {
        return write_fields(_mem, _value.data_, _value.filter_);
    } else if constexpr (std::is_same_v<T, subscribe_answer_data>) {
        return write_fields(_mem, _value.service_, _value.instance_, _value.eventgroup_, _value.subscriber_, _value.event_,
                            _value.pending_id_);
    } else if constexpr (std::is_same_v<T, update_security_policy_view>) {
        return write_fields(_mem, _value.update_id_, _value.policy_);
    } else if constexpr (std::is_same_v<T, routing_info_entry_data>) {
        bool const has_address = !_value.address_.is_unspecified();

        uint32_t its_client_size = static_cast<uint32_t>(sizeof(client_t));
        if (has_address) {
            its_client_size += static_cast<uint32_t>(sizeof(boost::asio::ip::address_v4::bytes_type) + sizeof(port_t));
        }
        // Size of the remainder of the entry (everything after the type byte and this size field).
        uint32_t const its_entry_size = wire_size(_value) - static_cast<uint32_t>(sizeof(uint32_t)) - 1;
        uint32_t const its_services_size = static_cast<uint32_t>(_value.services_.size()) * service_data::wire_size_;

        uint32_t written = write_fields(_mem, static_cast<byte_t>(_value.type_), its_entry_size, its_client_size, _value.client_);

        if (has_address) {
            auto const its_bytes = _value.address_.to_bytes();
            std::memcpy(_mem + written, its_bytes.data(), its_bytes.size());
            written += static_cast<uint32_t>(its_bytes.size());
            written += write_fields(_mem + written, _value.port_);
        }

        written += write_fields(_mem + written, its_services_size, _value.services_);
        return written;
    } else if constexpr (std::is_same_v<T, subscribe_answer_data>) {
        return write_fields(_mem, _value.service_, _value.instance_, _value.eventgroup_, _value.subscriber_, _value.event_,
                            _value.pending_id_);
    } else if constexpr (is_serializable_range<T>) {
        if constexpr (std::is_same_v<T, std::vector<std::span<const byte_t>>>) {
            auto count = static_cast<uint32_t>(_value.size());
            uint32_t written = serialize(count, _mem);
            for (auto const& range : _value) {
                auto length = static_cast<uint32_t>(range.size());
                written += serialize(length, _mem + written);
                written += serialize(range, _mem + written);
            }
            return written;
        } else {
            // covers span, vector, set, map etc..
            uint32_t written = 0;
            for (auto const& v : _value) {
                written += serialize(v, _mem + written);
            }
            return written;
        }
    } else if constexpr (has_header<T>) {
        if constexpr (has_payload<T>) {
            return write_fields(_mem, _value.header_, _value.payload_);
        } else {
            return serialize(_value.header_, _mem); // simple commands (PING, PONG, SUSPEND)
        }
    } else {
        static_assert(!std::is_same_v<T, T>, "Unsupported type requested to be serialized");
    }
    return 0;
}

} // namespace vsomeip_v3::protocol

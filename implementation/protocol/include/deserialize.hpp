// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command_types.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/payload_impl.hpp"
#include "../../security/include/policy.hpp"

#include <vsomeip/defines.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>

#include <cstdint>
#include <cstring> // memcpy
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace vsomeip_v3::protocol {

uint32_t deserialize(std::map<std::size_t, byte_t>& _out, unsigned char const* _mem, uint32_t _size);
uint32_t deserialize(std::shared_ptr<debounce_filter_impl_t>& _out, unsigned char const* _mem, uint32_t _size);
uint32_t deserialize(std::vector<std::pair<uid_t, gid_t>>& _out, unsigned char const* _mem, uint32_t _size);

template<typename T>
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
inline uint32_t deserialize(T& _out, unsigned char const* _mem, uint32_t _size) {
    static constexpr auto size = sizeof(T);
    if (size > _size) {
        return 0;
    }
    std::memcpy(&_out, _mem, size);
    return size;
}

//! This function does not check the memory size, as this should be covered by an earlier check from the caller already
template<typename T>
inline uint32_t deserialize_be(T& _out, unsigned char const* _mem) {
    if constexpr (std::is_same_v<T, uint8_t>) {
        _out = _mem[0];
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        _out = static_cast<uint16_t>(_mem[1] | (static_cast<uint16_t>(_mem[0]) << 8));
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        _out = static_cast<uint32_t>(_mem[3] | (static_cast<uint32_t>(_mem[2]) << 8) | (static_cast<uint32_t>(_mem[1]) << 16)
                                     | (static_cast<uint32_t>(_mem[0]) << 24));
    } else {
        static_assert(!std::is_same_v<T, T>, "unsupported type passed in");
    }
    return sizeof(T);
}

template<typename... Ts>
uint32_t parse(unsigned char const* _mem, uint32_t _size, Ts&... _ts) {
    uint32_t parsed{0};
    auto checked_parse = [&](auto& _out) {
        auto const result = deserialize(_out, _mem, _size);
        if (result == 0) {
            return false;
        }
        parsed += result;
        _mem += result;
        _size -= result;
        return true;
    };
    return (checked_parse(_ts) && ...) ? parsed : 0;
}

inline uint32_t deserialize(command_header& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.id_, _out.version_, _out.client_, _out.length_);
}

inline uint32_t deserialize(service_data& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.service_, _out.instance_, _out.major_version_, _out.minor_version_);
}

// Deserializes a length-prefixed string. Returns bytes parsed (0 on error); _out owns the data.
inline uint32_t deserialize(std::string& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t pos = 0;
    uint32_t len = 0;
    if (pos + sizeof(uint32_t) > _size) {
        return 0;
    }
    std::memcpy(&len, _mem + pos, sizeof(uint32_t));
    pos += sizeof(uint32_t);

    // _size >= pos here, so the subtraction cannot underflow and the check cannot overflow.
    if (len > _size - pos) {
        return 0;
    }
    _out.assign(reinterpret_cast<const char*>(_mem + pos), len);
    pos += len;
    return pos;
}

// Deserializes a sequence of length-prefixed key/value string pairs (e.g. a config payload).
// Returns bytes parsed (0 on error); each pair owns its data.
inline uint32_t deserialize(std::vector<std::pair<std::string, std::string>>& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t acc = 0;
    while (acc < _size) {
        std::string key;
        auto const key_read = deserialize(key, _mem + acc, _size - acc);
        if (key_read == 0) {
            return 0;
        }
        acc += key_read;

        std::string value;
        auto const value_read = deserialize(value, _mem + acc, _size - acc);
        if (value_read == 0) {
            return 0;
        }
        acc += value_read;

        _out.emplace_back(std::move(key), std::move(value));
    }
    return acc;
}

/// Zero-copy deserialization for assign_client payload.
/// Returns bytes parsed (0 on error). name_ is a view into _mem.
inline uint32_t deserialize(assign_client_data& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t pos = 0;

    if (sizeof(uint32_t) > _size) {
        return 0;
    }
    uint32_t name_len = 0;
    std::memcpy(&name_len, _mem + pos, sizeof(uint32_t));
    pos += sizeof(uint32_t);

    // _size >= pos here, so the subtraction cannot underflow and the check cannot overflow.
    if (name_len > _size - pos) {
        return 0;
    }
    _out.name_ = std::string_view(reinterpret_cast<const char*>(_mem + pos), name_len);
    pos += name_len;

    if (sizeof(uint8_t) > _size - pos) {
        return 0;
    }
    uint8_t has_addr = 0;
    std::memcpy(&has_addr, _mem + pos, sizeof(uint8_t));
    pos += sizeof(uint8_t);
    _out.has_address_ = (has_addr != 0);

    if (_out.has_address_) {
        if (_out.address_bytes_.size() + sizeof(port_t) > _size - pos) {
            return 0;
        }
        std::memcpy(_out.address_bytes_.data(), _mem + pos, _out.address_bytes_.size());
        pos += static_cast<uint32_t>(_out.address_bytes_.size());
        std::memcpy(&_out.port_, _mem + pos, sizeof(port_t));
        pos += sizeof(port_t);
    }
    return pos;
}

inline uint32_t deserialize(register_event_data& _out, unsigned char const* _mem, uint32_t _size) {
    uint16_t num_eventgroups{0};
    auto const parsed = parse(_mem, _size, _out.service_, _out.instance_, _out.event_, _out.event_type_, _out.is_provided_,
                              _out.reliability_, _out.is_cyclic_, num_eventgroups);
    if (parsed == 0) {
        return 0;
    }

    _out.eventgroups_.clear();
    _out.eventgroups_.reserve(num_eventgroups);
    uint32_t acc = parsed;
    for (uint16_t i = 0; i < num_eventgroups; ++i) {
        eventgroup_t its_eventgroup{};
        auto const read = deserialize(its_eventgroup, _mem + acc, _size - acc);
        if (read == 0) {
            return 0;
        }
        acc += read;
        _out.eventgroups_.push_back(its_eventgroup);
    }
    return acc;
}

inline uint32_t deserialize(std::vector<register_event_data>& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t acc = 0;
    while (acc < _size) {
        register_event_data its_registration{};
        auto const read = deserialize(its_registration, _mem + acc, _size - acc);
        if (read == 0) {
            return 0;
        }
        acc += read;
        _out.push_back(std::move(its_registration));
    }
    return acc;
}

inline uint32_t deserialize(std::vector<service_data>& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t acc = 0;
    auto elems = _size / service_data::wire_size_;
    if (elems * service_data::wire_size_ != _size) {
        // some remaining bytes?
        return 0;
    }
    _out.resize(elems);
    for (auto& out : _out) {
        // there is no need to check for the amount of parsed bytes,
        // as we only allocated enough space to also deserialize the payload
        acc += deserialize(out, _mem + acc, _size - acc);
    }
    return acc;
}

inline uint32_t deserialize(ipc_message_header& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.instance_, _out.reliable_, _out.status_, _out.target_);
}

inline uint32_t deserialize(release_service_data& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.service_, _out.instance_);
}

inline uint32_t deserialize(unregister_event_data& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.service_, _out.instance_, _out.event_, _out.is_provided_);
}

inline uint32_t deserialize(unsubscribe_ack_data& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.service_, _out.instance_, _out.eventgroup_, _out.pending_id_);
}

inline uint32_t deserialize(remove_security_policy_data& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.update_id_, _out.uid_, _out.gid_);
}
inline uint32_t deserialize(subscribe_answer_data& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.service_, _out.instance_, _out.eventgroup_, _out.subscriber_, _out.event_, _out.pending_id_);
}

inline uint32_t deserialize(subscribe_data& _out, unsigned char const* _mem, uint32_t _size) {
    return parse(_mem, _size, _out.service_, _out.instance_, _out.eventgroup_, _out.major_, _out.event_, _out.pending_id_);
}

inline uint32_t deserialize(std::map<std::size_t, byte_t>& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t its_offset = 0;
    size_t its_key;
    byte_t its_value;
    while (_size - its_offset > sizeof(size_t) + sizeof(byte_t)) {
        uint32_t parsed = parse(_mem + its_offset, _size - its_offset, its_key, its_value);
        if (parsed == 0) {
            break;
        }
        _out.emplace(std::pair(its_key, its_value));
        its_offset += parsed;
    }
    return its_offset;
}

inline uint32_t deserialize(std::shared_ptr<debounce_filter_impl_t>& _out, unsigned char const* _mem, uint32_t _size) {
    if (_size > 0) {
        _out = std::make_shared<debounce_filter_impl_t>();
    } else {
        return 0;
    }

    uint32_t parsed = parse(_mem, _size, _out->on_change_, _out->on_change_resets_interval_, _out->interval_);
    // this step is optional and may return 0
    parsed += deserialize(_out->ignore_, _mem + parsed, _size - parsed);
    return parsed + deserialize(_out->send_current_value_after_, _mem + parsed, _size - parsed);
}

inline uint32_t deserialize(subscribe_with_filter_data& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t parsed = deserialize(_out.data_, _mem, _size);
    if (parsed == 0) {
        return 0;
    }
    // do not use "parse" here, as the filter is optional!
    return parsed + deserialize(_out.filter_, _mem + parsed, _size - parsed);
}

inline uint32_t deserialize(update_security_policy_data& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t const parsed = deserialize(_out.update_id_, _mem, _size);
    if (parsed == 0) {
        return 0;
    }
    auto const left = _size - parsed;
    if (left > 0) {
        _out.policy_.reserve(left);
        _out.policy_.insert(_out.policy_.end(), _mem + parsed, _mem + _size);
    }
    return _size;
}
inline uint32_t deserialize(std::vector<std::pair<uid_t, gid_t>>& _out, unsigned char const* _mem, uint32_t _size) {
    static constexpr uint32_t const element_size = sizeof(uid_t) + sizeof(gid_t);
    uint32_t const elems = _size / element_size;
    if (elems * element_size != _size) {
        // some remaining bytes?
        return 0;
    }
    _out.resize(elems);
    uint32_t acc = 0;
    for (auto& out : _out) {
        // there is no need to check for the amount of parsed bytes,
        // as we only allocated enough space to also deserialize the payload
        acc += parse(_mem + acc, _size - acc, out.first, out.second);
    }
    return acc;
}

inline uint32_t deserialize(routing_info_entry_data& _out, unsigned char const* _mem, uint32_t _size) {
    // Wire layout: type (1) | entry-size (4) | client-info-size (4) | client (2)
    //              [ IPv4 address (4) | port (2) ] | services-size (4) | N * service (wire_size_)
    byte_t its_type{0};
    uint32_t its_entry_size{0};
    uint32_t its_client_size{0};
    uint32_t parsed = parse(_mem, _size, its_type, its_entry_size, its_client_size, _out.client_);
    if (parsed == 0) {
        return 0;
    }

    _out.type_ = static_cast<routing_info_entry_type_e>(its_type);
    if (_out.type_ == routing_info_entry_type_e::RIE_UNKNOWN) {
        return 0;
    }

    if (_out.type_ != routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE
        && _out.type_ != routing_info_entry_type_e::RIE_DELETE_SERVICE_INSTANCE) {
        return 0;
    }

    // The address and port are optional; they are present iff the client-info section is
    // larger than the client id. Only IPv4 addresses are supported.
    if (its_client_size > sizeof(client_t)) {
        boost::asio::ip::address_v4::bytes_type its_address;
        if (its_client_size != sizeof(client_t) + its_address.size() + sizeof(port_t) || _size - parsed < its_address.size()) {
            return 0;
        }
        std::memcpy(its_address.data(), _mem + parsed, its_address.size());
        _out.address_ = boost::asio::ip::address_v4(its_address);
        parsed += static_cast<uint32_t>(its_address.size());

        uint32_t const consumed = parse(_mem + parsed, _size - parsed, _out.port_);
        if (consumed == 0) {
            return 0;
        }
        parsed += consumed;
    }

    uint32_t its_services_size{0};
    uint32_t consumed = parse(_mem + parsed, _size - parsed, its_services_size);
    if (consumed == 0) {
        return 0;
    }
    parsed += consumed;

    if (_size - parsed < its_services_size) {
        return 0;
    }

    // Reuse the std::vector<service_data> overload; it validates that the section is an
    // exact multiple of service_data::wire_size_ and returns 0 otherwise.
    consumed = deserialize(_out.services_, _mem + parsed, its_services_size);
    if (consumed != its_services_size) {
        return 0;
    }
    parsed += consumed;

    return parsed;
}

inline uint32_t deserialize(std::vector<routing_info_entry_data>& _out, unsigned char const* _mem, uint32_t _size) {
    uint32_t acc = 0;
    while (acc < _size) {
        routing_info_entry_data its_entry;
        uint32_t const consumed = deserialize(its_entry, _mem + acc, _size - acc);
        if (consumed == 0) {
            return 0;
        }
        _out.emplace_back(std::move(its_entry));
        acc += consumed;
    }
    return acc;
}

inline uint32_t deserialize(std::vector<std::shared_ptr<policy>>& _out, unsigned char const* _mem, uint32_t _size) {
    static constexpr uint32_t su32 = sizeof(uint32_t);
    if (_size < su32) {
        return 0;
    }
    uint32_t total_size = _size;
    uint32_t element_count{0};
    deserialize(element_count, _mem, _size);
    _size -= su32;
    _mem += su32;
    // no reserve call to avoid over-allocation
    for (uint32_t i = 0; i < element_count; ++i) {
        if (_size < su32) {
            return 0;
        }
        uint32_t policy_length;
        deserialize(policy_length, _mem, _size);
        _size -= su32;
        _mem += su32;

        if (_size < policy_length) {
            _out.clear();
            return 0;
        }
        auto its_policy = std::make_shared<policy>();
        // policy::deserialize takes both the pointer and the size by reference: it
        // advances _mem and decrements the available size by the number of bytes it
        // consumed. Pass a copy as the available size so policy_length keeps the
        // declared length and _mem is advanced exactly once.
        auto remaining = policy_length;
        if (policy_length == 0 || !its_policy->deserialize(_mem, remaining)) {
            _out.clear();
            return 0;
        }
        // a policy must consume exactly its declared length
        if (remaining != 0) {
            _out.clear();
            return 0;
        }
        _size -= policy_length;
        _out.push_back(its_policy);
    }

    if (_size != 0) {
        _out.clear();
        return 0;
    }
    return total_size;
}

inline uint32_t deserialize(std::shared_ptr<message_impl>& _out, unsigned char const* _mem, uint32_t _size) {
    if (_size < ipc_message_header::wire_size_ + VSOMEIP_FULL_HEADER_SIZE) {
        return 0;
    }

    // first we need to read our own auxiliary data
    ipc_message_header ipc_header;
    uint32_t its_offset = deserialize(ipc_header, _mem, _size);

    message_header_impl its_header;
    its_header.instance_ = ipc_header.instance_;

    // now comes the "official" someip message part
    its_offset += deserialize_be(its_header.service_, _mem + its_offset);
    its_offset += deserialize_be(its_header.method_, _mem + its_offset);

    length_t length;
    its_offset += deserialize_be(length, _mem + its_offset);
    // length is derived from the payload, no need to set it explicitly
    if (length != _size - its_offset) {
        // this implies that the command header total size does not match the nested someip message size
        // --> Something is wrong with the message
        return 0;
    }

    // the "target" of the former "command" was only of interest in the rmc
    // in which only the ipc_header is read. In the rmc reading the whole
    // message the client of the message is the one of importance.
    its_offset += deserialize_be(its_header.client_, _mem + its_offset);
    its_offset += deserialize_be(its_header.session_, _mem + its_offset);
    its_offset += deserialize_be(its_header.protocol_version_, _mem + its_offset);
    its_offset += deserialize_be(its_header.interface_version_, _mem + its_offset);

    uint8_t message_type;
    its_offset += deserialize_be(message_type, _mem + its_offset);
    its_header.type_ = static_cast<message_type_e>(message_type);

    uint8_t return_code;
    its_offset += deserialize_be(return_code, _mem + its_offset);
    its_header.code_ = static_cast<return_code_e>(return_code);

    _out = std::make_shared<message_impl>(its_header, ipc_header.reliable_, ipc_header.status_);

    auto const payload_size = _size - its_offset;
    if (payload_size > 0) {
        std::vector<unsigned char> message(payload_size);
        std::memcpy(message.data(), &_mem[its_offset], payload_size);
        auto payload = std::make_shared<payload_impl>();
        payload->set_data(std::move(message));
        _out->set_payload(std::move(payload));
    }

    // due to the last guard ensuring that the length of the someip message + the ipc header size
    // match _size + the assumed payload size we can savely return the _size
    return _size;
}

} // namespace vsomeip_v3::protocol

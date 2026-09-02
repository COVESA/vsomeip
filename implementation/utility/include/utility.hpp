// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <ostream>
#include <span>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/vsomeip_sec.h>

namespace vsomeip_v3 {

class configuration;

class utility {
public:
    static inline bool is_request(std::shared_ptr<message> _message) { return _message ? is_request(_message->get_message_type()) : false; }

    static inline bool is_request(byte_t _type) { return is_request(static_cast<message_type_e>(_type)); }

    static inline bool is_request(message_type_e _type) {
        return ((_type < message_type_e::MT_NOTIFICATION)
                || (_type >= message_type_e::MT_REQUEST_ACK && _type <= message_type_e::MT_REQUEST_NO_RETURN_ACK));
    }

    static inline bool is_request_no_return(std::shared_ptr<message> _message) {
        return (_message && is_request_no_return(_message->get_message_type()));
    }

    static inline bool is_request_no_return(byte_t _type) { return is_request_no_return(static_cast<message_type_e>(_type)); }

    static inline bool is_request_no_return(message_type_e _type) {
        return (_type == message_type_e::MT_REQUEST_NO_RETURN || _type == message_type_e::MT_REQUEST_NO_RETURN_ACK);
    }

    static inline bool is_response(byte_t _type) { return is_response(static_cast<message_type_e>(_type)); }

    static inline bool is_response(message_type_e _type) { return _type == message_type_e::MT_RESPONSE; }

    static inline bool is_error(byte_t _type) { return is_error(static_cast<message_type_e>(_type)); }

    static inline bool is_error(message_type_e _type) { return _type == message_type_e::MT_ERROR; }

    static inline bool is_notification(byte_t _type) { return is_notification(static_cast<message_type_e>(_type)); }

    static inline bool is_notification(message_type_e _type) { return (_type == message_type_e::MT_NOTIFICATION); }

    static uint32_t get_message_size(const byte_t* _data, size_t _size);
    static uint32_t get_payload_size(const byte_t* _data, uint32_t _size);

    static bool is_routing_manager(const std::string& _network);
    static void remove_lockfile(const std::string& _network);
    static bool exists(const std::string& _path);
    static bool is_file(const std::string& _path);
    static bool is_folder(const std::string& _path);

    static std::string get_base_path(const std::string& _network);

    /// Lowest client id of the block owned by a host with this diagnosis address and mask.
    ///
    /// The mask selects the bits that identify the host; the freed bits (`~_mask`) are handed out
    /// to its applications. A mask that clears bits of `_diagnosis << 8` is valid: it simply
    /// enlarges the block, which then spans several diagnosis prefixes (e.g. diagnosis `0x98` with
    /// mask `0xEB00` yields 1024 ids in `0x88xx`, `0x8Cxx`, `0x98xx` and `0x9Cxx`).
    static client_t get_client_id_base(diagnosis_t _diagnosis, diagnosis_t _mask);

    /// Whether `_client` belongs to the client-id block of a host with this diagnosis and mask.
    static bool is_client_id_in_range(client_t _client, diagnosis_t _diagnosis, diagnosis_t _mask);

    /// Whether `_client` is plausible for an incoming message of type `_type`.
    ///
    /// Returns `false` only for the two cases that break routing:
    ///
    /// - a response whose client id is *not* one of this host's ids - it cannot be routed to a
    ///   local application;
    /// - a request whose client id *is* one of this host's ids - the response would not be routed
    ///   back to the remote host.
    ///
    /// Every other message type is accepted.
    static bool is_valid_client_id(client_t _client, diagnosis_t _diagnosis, diagnosis_t _mask, message_type_e _type);

    static client_t request_client_id(const std::shared_ptr<configuration>& _config, std::string_view _name, client_t _client);
    static std::string get_client_name(const std::shared_ptr<configuration>& _config, client_t _client);
    static void release_client_id(const std::string& _network, client_t _client);
    static std::set<client_t> get_used_client_ids(const std::string& _network);
    static void reset_client_ids(const std::string& _network);

    static inline bool is_valid_message_type(message_type_e _type) {
        return (_type == message_type_e::MT_REQUEST || _type == message_type_e::MT_REQUEST_NO_RETURN
                || _type == message_type_e::MT_NOTIFICATION || _type == message_type_e::MT_REQUEST_ACK
                || _type == message_type_e::MT_REQUEST_NO_RETURN_ACK || _type == message_type_e::MT_NOTIFICATION_ACK
                || _type == message_type_e::MT_RESPONSE || _type == message_type_e::MT_ERROR || _type == message_type_e::MT_RESPONSE_ACK
                || _type == message_type_e::MT_ERROR_ACK || _type == message_type_e::MT_UNKNOWN);
    }

    static inline bool is_valid_return_code(return_code_e _code) {
        return (_code == return_code_e::E_OK || _code == return_code_e::E_NOT_OK || _code == return_code_e::E_UNKNOWN_SERVICE
                || _code == return_code_e::E_UNKNOWN_METHOD || _code == return_code_e::E_NOT_READY
                || _code == return_code_e::E_NOT_REACHABLE || _code == return_code_e::E_TIMEOUT
                || _code == return_code_e::E_WRONG_PROTOCOL_VERSION || _code == return_code_e::E_WRONG_INTERFACE_VERSION
                || _code == return_code_e::E_MALFORMED_MESSAGE || _code == return_code_e::E_WRONG_MESSAGE_TYPE
                || (static_cast<uint8_t>(_code) >= 0x20 && static_cast<uint8_t>(_code) <= 0x5E));
    }

    static inline bool compare(const vsomeip_sec_client_t& _lhs, const vsomeip_sec_client_t& _rhs) {

        bool is_equal(false);
        if (_lhs.port == _rhs.port) {
            if (_lhs.port == VSOMEIP_SEC_PORT_UNUSED) {
                is_equal = (_lhs.user == _rhs.user && _lhs.group == _rhs.group);
            } else {
                is_equal = (_lhs.host == _rhs.host && _lhs.port == _rhs.port);
            }
        }
        return is_equal;
    }

    static void set_thread_niceness(int _nice) noexcept;

    class Hex {
    public:
        constexpr Hex(uint32_t _v, uint16_t _width) noexcept : value_(_v), width_(_width) { }
        uint32_t get() const { return value_; }
        uint16_t get_width() const { return width_; }

    private:
        uint32_t value_;
        uint16_t width_;
    };

    static std::string dump(const byte_t* _data, size_t _length);

    static void log_network_state(const std::shared_ptr<configuration>& _configuration, bool _tcp, bool _only_external);

private:
    struct data_t {
        data_t();

        client_t next_client_;
        std::map<client_t, std::string> used_clients_;
#ifdef _WIN32
        HANDLE lock_handle_;
#else
        int lock_fd_;
#endif
    };

private:
    static uint16_t get_max_client_number(const std::shared_ptr<configuration>& _config);

    static std::mutex& get_utility_mutex();
    static std::map<std::string, data_t>& get_utility_data();
};

inline const char* to_string(message_type_e _type) {
    switch (_type) {
    case message_type_e::MT_REQUEST:
        return "REQUEST";
    case message_type_e::MT_REQUEST_NO_RETURN:
        return "REQUEST_NO_RETURN";
    case message_type_e::MT_NOTIFICATION:
        return "NOTIFICATION";
    case message_type_e::MT_REQUEST_ACK:
        return "REQUEST_ACK";
    case message_type_e::MT_REQUEST_NO_RETURN_ACK:
        return "REQUEST_NO_RETURN_ACK";
    case message_type_e::MT_NOTIFICATION_ACK:
        return "NOTIFICATION_ACK";
    case message_type_e::MT_RESPONSE:
        return "RESPONSE";
    case message_type_e::MT_ERROR:
        return "ERROR";
    case message_type_e::MT_RESPONSE_ACK:
        return "RESPONSE_ACK";
    case message_type_e::MT_ERROR_ACK:
        return "ERROR_ACK";
    case message_type_e::MT_UNKNOWN:
        return "UNKNOWN";
    }
    return "INVALID";
}

inline utility::Hex hex2(uint8_t _v) {
    return utility::Hex(_v, 2);
}

inline utility::Hex hex4(uint16_t _v) {
    return utility::Hex(_v, 4);
}

inline utility::Hex hex8(uint32_t _v) {
    return utility::Hex(_v, 8);
}

// Precomputed "00".."ff" hex-pair table, shared by operator<<(ostream&, utility::Hex) and
// utility::dump() below, and usable directly by any hot path that needs to format bytes as hex.
inline constexpr std::array<char, 512> hex_byte_table = [] {
    std::array<char, 512> table{};
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < 256; ++i) {
        table[i * 2] = digits[i >> 4];
        table[i * 2 + 1] = digits[i & 0xf];
    }
    return table;
}();

// The two hex characters for a single byte, e.g. "0a" for 0x0a (no separator, no null terminator).
inline constexpr std::span<const char, 2> hex_chars(uint8_t _byte) {
    return std::span<const char, 2>{&hex_byte_table[static_cast<size_t>(_byte) * 2], 2};
}

// Appends the two hex characters for a single byte to _out, e.g. "0a" for 0x0a.
inline void append_hex_byte(std::string& _out, uint8_t _byte) {
    const auto pair = hex_chars(_byte);
    _out.append(pair.data(), pair.size());
}

// Appends the hex representation of _data to _out, e.g. "0a1b2c" - no separators.
inline void append_hex(std::string& _out, std::span<const byte_t> _data) {
    _out.reserve(_out.size() + _data.size() * 2);
    for (byte_t b : _data) {
        append_hex_byte(_out, b);
    }
}

inline std::ostream& operator<<(std::ostream& os, utility::Hex v) {
    std::array<char, 8> buf{};
    uint32_t value = v.get();
    for (uint16_t i = v.get_width(); i > 0; i -= 2) {
        const auto pair = hex_chars(static_cast<uint8_t>(value & 0xff));
        buf[i - 2] = pair[0];
        buf[i - 1] = pair[1];
        value >>= 8;
    }
    os.write(buf.data(), v.get_width());
    return os;
}

/**
 * @brief Dump the content of a message in hex format to a string, usually for logging
 *
 * Does trimming, DLT messages have an upper size which causes headaches
 */
inline std::string utility::dump(const byte_t* _data, size_t _length) {
    // way under 1390, or DLT might blow up; see `DLT_USER_BUF_MAX_SIZE`
    constexpr size_t max_dump_length = 512; // *2 = 1024
    size_t show_length = std::min<size_t>(_length, max_dump_length);

    std::string s;
    append_hex(s, std::span<const byte_t>(_data, show_length));

    if (_length > max_dump_length) {
        s += " (truncated, total length: ";
        s += std::to_string(_length);
        s += ")";
    }

    return s;
}

} // namespace vsomeip_v3

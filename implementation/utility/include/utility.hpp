// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_UTILITY_HPP
#define VSOMEIP_V3_UTILITY_HPP

#include <map>
#include <set>
#include <memory>
#include <vector>
#include <atomic>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message.hpp>
#include "criticalsection.hpp"

namespace vsomeip_v3 {

class configuration;

class utility {
public:
    static inline bool is_request(std::shared_ptr<message> _message) {
        return (_message ? is_request(_message->get_message_type()) : false);
    }

    static inline bool is_request(byte_t _type) {
        return (is_request(static_cast<message_type_e>(_type)));
    }

    static inline bool is_request(message_type_e _type) {
        return (_type < message_type_e::MT_NOTIFICATION);
    }

    static inline bool is_request_no_return(std::shared_ptr<message> _message) {
        return (_message && is_request_no_return(_message->get_message_type()));
    }

    static inline bool is_request_no_return(byte_t _type) {
        return (is_request_no_return(static_cast<message_type_e>(_type)));
    }

    static inline bool is_request_no_return(message_type_e _type) {
        return (_type == message_type_e::MT_REQUEST_NO_RETURN
                || _type == message_type_e::MT_REQUEST_NO_RETURN_ACK);
    }

    static inline bool is_response(byte_t _type) {
        return is_response(static_cast<message_type_e>(_type));
    }

    static inline bool is_response(message_type_e _type) {
        return _type == message_type_e::MT_RESPONSE;
    }

    static inline bool is_error(byte_t _type) {
        return is_error(static_cast<message_type_e>(_type));
    }

    static inline bool is_error(message_type_e _type) {
        return _type == message_type_e::MT_ERROR;
    }

    static inline bool is_event(byte_t _data) {
        return (0x80 & _data) > 0;
    }

    static inline bool is_notification(byte_t _type) {
        return (is_notification(static_cast<message_type_e>(_type)));
    }

    static inline bool is_notification(message_type_e _type) {
        return (_type == message_type_e::MT_NOTIFICATION);
    }

    static uint64_t get_message_size(const byte_t *_data, size_t _size);
    static inline uint64_t get_message_size(std::vector<byte_t> &_data) {
        if (_data.size() > 0) {
            return (get_message_size(&_data[0], _data.size()));
        }
        return 0;
    }

    static uint32_t get_payload_size(const byte_t *_data, uint32_t _size);

    static bool is_routing_manager(const std::shared_ptr<configuration> &_config);
    static void remove_lockfile(const std::shared_ptr<configuration> &_config);
    static bool exists(const std::string &_path);
    static bool VSOMEIP_IMPORT_EXPORT is_file(const std::string &_path);
    static bool VSOMEIP_IMPORT_EXPORT is_folder(const std::string &_path);

    static const std::string get_base_path(const std::shared_ptr<configuration> &_config);

    static client_t request_client_id(const std::shared_ptr<configuration> &_config,
            const std::string &_name, client_t _client);
    static void release_client_id(client_t _client);
    static std::set<client_t> get_used_client_ids();
    static void reset_client_ids();

    static inline bool is_valid_message_type(message_type_e _type) {
        return (_type == message_type_e::MT_REQUEST
                || _type == message_type_e::MT_REQUEST_NO_RETURN
                || _type == message_type_e::MT_NOTIFICATION
                || _type == message_type_e::MT_REQUEST_ACK
                || _type == message_type_e::MT_REQUEST_NO_RETURN_ACK
                || _type == message_type_e::MT_NOTIFICATION_ACK
                || _type == message_type_e::MT_RESPONSE
                || _type == message_type_e::MT_ERROR
                || _type == message_type_e::MT_RESPONSE_ACK
                || _type == message_type_e::MT_ERROR_ACK
                || _type == message_type_e::MT_UNKNOWN);
    }

    static inline bool is_valid_return_code(return_code_e _code) {
        return (_code == return_code_e::E_OK
                || _code == return_code_e::E_NOT_OK
                || _code == return_code_e::E_UNKNOWN_SERVICE
                || _code == return_code_e::E_UNKNOWN_METHOD
                || _code == return_code_e::E_NOT_READY
                || _code == return_code_e::E_NOT_REACHABLE
                || _code == return_code_e::E_TIMEOUT
                || _code == return_code_e::E_WRONG_PROTOCOL_VERSION
                || _code == return_code_e::E_WRONG_INTERFACE_VERSION
                || _code == return_code_e::E_MALFORMED_MESSAGE
                || _code == return_code_e::E_WRONG_MESSAGE_TYPE
                || (static_cast<std::uint8_t>(_code) >= 0x20
                    && static_cast<std::uint8_t>(_code) <= 0x5E));
    }

private:
    static std::uint16_t get_max_client_number(const std::shared_ptr<configuration> &_config);

    static std::mutex mutex__;
    static client_t next_client__;
    static std::map<client_t, std::string> used_clients__;
#ifdef _WIN32
    static HANDLE lock_handle__;
#else
    static int lock_fd__;
#endif
#ifndef VSOMEIP_ENABLE_CONFIGURATION_OVERLAYS
    static bool is_checked__;
#else
    static std::set<std::string> is_checked__;
#endif
};

}  // namespace vsomeip_v3

#endif // VSOMEIP_V3_UTILITY_HPP

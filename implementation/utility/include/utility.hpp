// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_UTILITY_HPP
#define VSOMEIP_UTILITY_HPP

#include <memory>
#include <vector>
#include <set>
#include <atomic>

#ifdef _WIN32
    #include <stdlib.h>
    #define bswap_16(x) _byteswap_ushort(x)
    #define bswap_32(x) _byteswap_ulong(x)
#else
    #include <byteswap.h>
#endif

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message.hpp>
#include "criticalsection.hpp"
#include "../../../implementation/configuration/include/policy.hpp"

namespace vsomeip {

class configuration;
struct policy;

class utility {
public:
    static inline bool is_request(std::shared_ptr<message> _message) {
        return (_message ? is_request(_message->get_message_type()) : false);
    }

    static inline bool is_request(byte_t _type) {
        return (is_request(static_cast<message_type_e>(_type)));
    }

    static inline bool is_request(message_type_e _type) {
        return ((_type < message_type_e::MT_NOTIFICATION)
                || (_type >= message_type_e::MT_REQUEST_ACK
                        && _type <= message_type_e::MT_REQUEST_NO_RETURN_ACK));
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

    static bool exists(const std::string &_path);
    static bool VSOMEIP_IMPORT_EXPORT is_file(const std::string &_path);
    static bool VSOMEIP_IMPORT_EXPORT is_folder(const std::string &_path);

    static const std::string get_base_path(const std::shared_ptr<configuration> &_config);
    static const std::string get_shm_name(const std::shared_ptr<configuration> &_config);

    static CriticalSection its_local_configuration_mutex__;

    static struct configuration_data_t *the_configuration_data__;
    static bool auto_configuration_init(const std::shared_ptr<configuration> &_config);
    static void auto_configuration_exit(client_t _client,
            const std::shared_ptr<configuration> &_config);

    static bool is_routing_manager_host(client_t _client);
    static void set_routing_manager_host(client_t _client);

    static bool is_used_client_id(client_t _client,
            const std::shared_ptr<configuration> &_config);
    static client_t request_client_id(const std::shared_ptr<configuration> &_config,
            const std::string &_name, client_t _client);
    static void release_client_id(client_t _client);
    static std::set<client_t> get_used_client_ids();

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
                || _code == return_code_e::E_WRONG_MESSAGE_TYPE);
    }

    VSOMEIP_EXPORT static bool parse_policy(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_uid, uint32_t &_gid, ::std::shared_ptr<policy> &_policy);
    VSOMEIP_EXPORT static bool parse_uid_gid(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_uid, uint32_t &_gid);

private:
    static void check_client_id_consistency();
    static std::uint16_t get_max_number_of_clients(std::uint16_t _diagnosis_max);
    static inline bool parse_range(const byte_t* &_buffer, uint32_t &_buffer_size, uint16_t &_first, uint16_t &_last);
    static inline bool parse_id(const byte_t* &_buffer, uint32_t &_buffer_size, uint16_t &_id);
    static inline bool get_struct_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length);
    static inline bool get_union_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length);
    static inline bool get_array_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length);
    static inline bool is_range(const byte_t* &_buffer, uint32_t &_buffer_size);
    static inline bool parse_id_item(const byte_t* &_buffer, uint32_t& parsed_ids_bytes, ranges_t& its_ranges, uint32_t &_buffer_size);

    static std::atomic<std::uint16_t> its_configuration_refs__;
    static std::uint16_t* used_client_ids__;

    static const uint8_t uid_width_;
    static const uint8_t gid_width_;
    static const uint8_t id_width_;
    static const uint8_t range_width_;
    static const uint8_t skip_union_length_ ;
    static const uint8_t skip_union_type_ ;
    static const uint8_t skip_union_length_type_ ;
    static const uint8_t skip_struct_length_;
    static const uint8_t skip_array_length_;
};

}  // namespace vsomeip

#endif // VSOMEIP_UTILITY_HPP

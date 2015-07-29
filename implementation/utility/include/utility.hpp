// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_UTILITY_HPP
#define VSOMEIP_UTILITY_HPP

#include <memory>
#include <vector>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message.hpp>

namespace vsomeip {

class utility {
public:
    static void * load_library(const std::string &_path,
            const std::string &_symbol);

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

    static inline bool is_event(byte_t _data) {
        return (0x80 & _data);
    }

    static inline bool is_notification(byte_t _type) {
        return (is_notification(static_cast<message_type_e>(_type)));
    }

    static inline bool is_notification(message_type_e _type) {
        return (_type == message_type_e::MT_NOTIFICATION);
    }

    static uint32_t get_message_size(const byte_t *_data, uint32_t _size);
    static inline uint32_t get_message_size(std::vector<byte_t> &_data) {
        if (_data.size() > 0) {
            return (get_message_size(&_data[0], _data.size()));
        }
        return 0;
    }

    static uint32_t get_payload_size(const byte_t *_data, uint32_t _size);

    static bool exists(const std::string &_path);
};

}  // namespace vsomeip

#endif // VSOMEIP_UTILITY_HPP

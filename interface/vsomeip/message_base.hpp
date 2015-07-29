// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_MESSAGE_BASE_HPP
#define VSOMEIP_MESSAGE_BASE_HPP

#include <vsomeip/export.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/deserializable.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

class message_base
        : public serializable,
          public deserializable {
public:
    VSOMEIP_EXPORT virtual ~message_base() {};

    VSOMEIP_EXPORT virtual message_t get_message() const = 0;
    VSOMEIP_EXPORT virtual void set_message(message_t _message) = 0;

    VSOMEIP_EXPORT virtual service_t get_service() const = 0;
    VSOMEIP_EXPORT virtual void set_service(service_t _service) = 0;

    VSOMEIP_EXPORT virtual instance_t get_instance() const = 0;
    VSOMEIP_EXPORT virtual void set_instance(instance_t _instance) = 0;

    VSOMEIP_EXPORT virtual method_t get_method() const = 0;
    VSOMEIP_EXPORT virtual void set_method(method_t _method) = 0;

    VSOMEIP_EXPORT virtual length_t get_length() const = 0;

    VSOMEIP_EXPORT virtual request_t get_request() const = 0;

    VSOMEIP_EXPORT virtual client_t get_client() const = 0;
    VSOMEIP_EXPORT virtual void set_client(client_t _client) = 0;

    VSOMEIP_EXPORT virtual session_t get_session() const = 0;
    VSOMEIP_EXPORT virtual void set_session(session_t _session) = 0;

    VSOMEIP_EXPORT virtual protocol_version_t get_protocol_version() const = 0;

    VSOMEIP_EXPORT virtual interface_version_t get_interface_version() const = 0;
    VSOMEIP_EXPORT virtual void set_interface_version(interface_version_t _version) = 0;

    VSOMEIP_EXPORT virtual message_type_e get_message_type() const = 0;
    VSOMEIP_EXPORT virtual void set_message_type(message_type_e _type) = 0;

    VSOMEIP_EXPORT virtual return_code_e get_return_code() const = 0;
    VSOMEIP_EXPORT virtual void set_return_code(return_code_e _code) = 0;

    // No part of the SOME/IP protocol header
    VSOMEIP_EXPORT virtual bool is_reliable() const = 0;
    VSOMEIP_EXPORT virtual void set_reliable(bool _is_reliable) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_BASE_HPP

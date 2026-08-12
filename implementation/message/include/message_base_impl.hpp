// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/message.hpp>

#include "message_header_impl.hpp"

namespace vsomeip_v3 {

class message_base_impl : virtual public message_base {
public:
    message_base_impl();
    virtual ~message_base_impl();

    message_t get_message() const override;
    void set_message(message_t _message) override;

    service_t get_service() const override;
    void set_service(service_t _service) override;

    instance_t get_instance() const override;
    void set_instance(instance_t _instance) override;

    method_t get_method() const override;
    void set_method(method_t _method) override;

    request_t get_request() const override;

    client_t get_client() const override;
    void set_client(client_t _client) override;

    session_t get_session() const override;
    void set_session(session_t _session) override;

    protocol_version_t get_protocol_version() const override;

    interface_version_t get_interface_version() const override;
    void set_interface_version(interface_version_t _interface_version) override;

    message_type_e get_message_type() const override;
    void set_message_type(message_type_e _type) override;

    return_code_e get_return_code() const override;
    void set_return_code(return_code_e _code) override;

    bool is_reliable() const override;
    void set_reliable(bool _is_reliable) override;

    bool is_initial() const override;
    void set_initial(bool _is_initial) override;

public:
    // not exported, these are internal to the codebase
    message_base_impl(const message_header_impl& _header, bool _reliable);

    message* get_owner() const;
    void set_owner(message* _owner);

    void set_protocol_version(protocol_version_t _protocol_version);

protected: // members
    message_header_impl header_;
    bool is_reliable_;
    bool is_initial_;
};

} // namespace vsomeip_v3

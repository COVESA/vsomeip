// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/vsomeip_sec.h>

#include <memory>

namespace vsomeip_v3 {

class policy_manager_impl;
class security;
struct local_client_data;

class routing_host {
public:
    virtual ~routing_host() = default;

    /**
     * @brief Receive a local message from another application
     *
     * @param _data pointer to message
     * @param _length length of message
     * @param _bound_client client-id of sender
     * @param _sec_client security client of sender
     * @param _peer_data data of the sender
     */
    virtual void on_message(const byte_t* _data, length_t _length, const local_client_data& _peer_data) = 0;

    virtual client_t get_client() const = 0;
    virtual void lazy_load(const std::string& _client_host) = 0;

    virtual std::shared_ptr<policy_manager_impl> get_policy_manager() const = 0;
    virtual std::shared_ptr<security> get_security() const = 0;
};

} // namespace vsomeip_v3

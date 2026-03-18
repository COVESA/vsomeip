// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/vsomeip_sec.h>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

namespace vsomeip_v3 {

class boardnet_endpoint;

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
     */
    virtual void on_message(const byte_t* _data, length_t _length, client_t _bound_client, const vsomeip_sec_client_t* _sec_client) = 0;

    virtual client_t get_client() const = 0;
    virtual void add_known_client(client_t _client, const std::string& _client_host) = 0;
    virtual void remove_known_client(client_t _client) = 0;
    /// @brief Get guest client-id by address/port
    ///
    /// @return client-id of found guest, or VSOMEIP_CLIENT_UNSET
    virtual client_t get_guest_by_address(const boost::asio::ip::address& _address, port_t _port) const = 0;
    virtual void add_guest(client_t _client, const boost::asio::ip::address& _address, port_t _port) = 0;

    /// @brief Remove local client
    ///
    /// This will remove all information about local client, its' offered services, and also close the client endpoint to it
    ///
    /// @param _remove_due_to_error whether we are removing due to an error - do not bother with graceful endpoint closure
    virtual void remove_local(client_t _client, bool _remove_due_to_error) = 0;

    virtual std::string get_env(client_t _client) const = 0;
};

} // namespace vsomeip_v3

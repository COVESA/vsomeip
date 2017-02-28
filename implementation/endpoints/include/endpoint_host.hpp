// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENDPOINT_HOST_HPP
#define VSOMEIP_ENDPOINT_HOST_HPP

#include <memory>

#include <boost/asio/ip/address.hpp>
#include "../../configuration/include/internal.hpp"

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class configuration;
class endpoint;

class endpoint_host {
public:
    virtual ~endpoint_host() {}

    virtual const std::shared_ptr<configuration> get_configuration() const = 0;
    virtual void on_connect(std::shared_ptr<endpoint> _endpoint) = 0;
    virtual void on_disconnect(std::shared_ptr<endpoint> _endpoint) = 0;
    virtual void on_message(const byte_t *_data, length_t _length,
        endpoint *_receiver, const boost::asio::ip::address &_destination
            = boost::asio::ip::address(),
            client_t _bound_client = VSOMEIP_ROUTING_CLIENT,
            const boost::asio::ip::address &_remote_address = boost::asio::ip::address(),
            std::uint16_t _remote_port = 0) = 0;
    virtual void on_error(const byte_t *_data, length_t _length,
                          endpoint *_receiver,
                          const boost::asio::ip::address &_remote_address,
                          std::uint16_t _remote_port) = 0;
    virtual void release_port(uint16_t _port, bool _reliable) = 0;
    virtual client_t get_client() const = 0;
#ifndef _WIN32
    virtual bool check_credentials(client_t _client, uid_t _uid, gid_t _gid) = 0;
#endif
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_HOST_HPP

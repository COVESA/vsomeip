// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENDPOINT_IMPL_HPP
#define VSOMEIP_ENDPOINT_IMPL_HPP

#include <map>
#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>

#include "buffer.hpp"
#include "endpoint.hpp"

namespace vsomeip {

class endpoint_host;

template<int MaxBufferSize>
class endpoint_impl: public endpoint {
public:
    endpoint_impl(std::shared_ptr<endpoint_host> _adapter,
                  boost::asio::io_service &_io);
    virtual ~endpoint_impl();

    void enable_magic_cookies();

    // Dummy implementations as we only need these for UDP (servers)
    // TODO: redesign
    void join(const std::string &);
    void leave(const std::string &);
    void add_multicast(service_t, event_t, const std::string &, uint16_t);
    void remove_multicast(service_t, event_t);

    // Dummy implementation as we only need this for IP client endpoints
    // TODO: redesign
    bool get_remote_address(boost::asio::ip::address &_address) const;

    // Dummy implementations as we only need these for server endpoints
    // TODO: redesign
    unsigned short get_local_port() const;
    unsigned short get_remote_port() const;
    bool is_reliable() const;

public:
    // required
    virtual bool is_client() const = 0;
    virtual void receive() = 0;
    virtual void restart() = 0;

protected:
    virtual bool is_magic_cookie() const;
    uint32_t find_magic_cookie(message_buffer_t &_buffer);

protected:
    // Reference to service context
    boost::asio::io_service &service_;

    // Reference to host
    std::weak_ptr<endpoint_host> host_;

    bool is_supporting_magic_cookies_;bool has_enabled_magic_cookies_;

    // Filter configuration
    std::map<service_t, uint8_t> opened_;
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_IMPL_HPP

// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CLIENT_ENDPOINT_IMPL_HPP
#define VSOMEIP_CLIENT_ENDPOINT_IMPL_HPP

#include <deque>
#include <mutex>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/utility.hpp>

#include "buffer.hpp"
#include "endpoint_impl.hpp"

namespace vsomeip {

class endpoint;
class endpoint_host;

template<typename Protocol, int MaxBufferSize>
class client_endpoint_impl: public endpoint_impl<MaxBufferSize>,
        public std::enable_shared_from_this<
                client_endpoint_impl<Protocol, MaxBufferSize> > {
public:
    typedef typename Protocol::socket socket_type;
    typedef typename Protocol::endpoint endpoint_type;

    client_endpoint_impl(std::shared_ptr<endpoint_host> _host,
            endpoint_type _remote, boost::asio::io_service &_io);
    virtual ~client_endpoint_impl();

    bool send(const uint8_t *_data, uint32_t _size, bool _flush);bool send_to(
            const std::shared_ptr<endpoint_definition> _target,
            const byte_t *_data, uint32_t _size, bool _flush = true);bool flush();

    void stop();
    void restart();

    bool is_client() const;

    bool is_connected() const;

public:
    void connect_cbk(boost::system::error_code const &_error);
    void wait_connect_cbk(boost::system::error_code const &_error);
    void send_cbk(message_buffer_ptr_t _buffer,
            boost::system::error_code const &_error, std::size_t _bytes);
    void flush_cbk(boost::system::error_code const &_error);

public:
    virtual void connect() = 0;
    virtual void receive() = 0;

protected:
    virtual void send_queued(message_buffer_ptr_t) = 0;

    socket_type socket_;
    endpoint_type remote_;

    boost::asio::system_timer flush_timer_;
    boost::asio::system_timer connect_timer_;
    uint32_t connect_timeout_;bool is_connected_;

    // send data
    message_buffer_ptr_t packetizer_;

    // receive data
    message_buffer_t message_;

    std::mutex mutex_;

    uint32_t queued_;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_ENDPOINT_IMPL_HPP

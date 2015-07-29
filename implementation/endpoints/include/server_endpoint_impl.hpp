// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVER_IMPL_HPP
#define VSOMEIP_SERVER_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>

#include "buffer.hpp"
#include "endpoint_impl.hpp"

namespace vsomeip {

template<typename Protocol, int MaxBufferSize>
class server_endpoint_impl: public endpoint_impl<MaxBufferSize>,
        public std::enable_shared_from_this<
                server_endpoint_impl<Protocol, MaxBufferSize> > {
public:
    typedef typename Protocol::socket socket_type;
    typedef typename Protocol::endpoint endpoint_type;
    typedef boost::array<uint8_t, MaxBufferSize> buffer_type;

    server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                         endpoint_type _local, boost::asio::io_service &_io);

    bool is_client() const;
    bool is_connected() const;

    bool send(const uint8_t *_data, uint32_t _size, bool _flush);
    bool flush(endpoint_type _target);

public:
    void connect_cbk(boost::system::error_code const &_error);
    void send_cbk(message_buffer_ptr_t _buffer,
                  boost::system::error_code const &_error, std::size_t _bytes);
    void flush_cbk(endpoint_type _target,
                   const boost::system::error_code &_error);

public:
    virtual bool send_intern(endpoint_type _target, const byte_t *_data,
                             uint32_t _port, bool _flush);
    virtual void send_queued(endpoint_type _target,
                             message_buffer_ptr_t _buffer) = 0;

    virtual endpoint_type get_remote() const = 0;
    virtual bool get_multicast(service_t _service, event_t _event,
                               endpoint_type &_target) const = 0;

protected:
    std::map<endpoint_type, message_buffer_ptr_t> packetizer_;
    std::map<client_t, std::map<session_t, endpoint_type> > clients_;

    boost::asio::system_timer flush_timer_;

    endpoint_type local_;

    std::mutex mutex_;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICE_ENDPOINT_IMPL_HPP

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVER_IMPL_HPP
#define VSOMEIP_SERVER_IMPL_HPP

#include <deque>
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

template<typename Protocol>
class server_endpoint_impl: public endpoint_impl<Protocol>,
        public std::enable_shared_from_this<server_endpoint_impl<Protocol> > {
public:
    typedef typename Protocol::socket socket_type;
    typedef typename Protocol::endpoint endpoint_type;
    typedef typename std::map<endpoint_type, std::deque<message_buffer_ptr_t> > queue_type;
    typedef typename queue_type::iterator queue_iterator_type;

    server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                         endpoint_type _local, boost::asio::io_service &_io,
                         std::uint32_t _max_message_size);
    virtual ~server_endpoint_impl();

    bool is_client() const;
    bool is_connected() const;

    bool send(const uint8_t *_data, uint32_t _size, bool _flush);

    virtual void stop();
    bool flush(endpoint_type _target);

public:
    void connect_cbk(boost::system::error_code const &_error);
    void send_cbk(queue_iterator_type _queue_iterator,
                  boost::system::error_code const &_error, std::size_t _bytes);
    void flush_cbk(endpoint_type _target,
                   const boost::system::error_code &_error);

public:
    virtual bool send_intern(endpoint_type _target, const byte_t *_data,
                             uint32_t _port, bool _flush);
    virtual void send_queued(queue_iterator_type _queue_iterator) = 0;

    virtual bool get_default_target(service_t _service,
            endpoint_type &_target) const = 0;

    virtual client_t get_client(std::shared_ptr<endpoint_definition> /*_endpoint*/);
protected:
    std::map<endpoint_type, message_buffer_ptr_t> packetizer_;
    queue_type queues_;

    std::mutex clients_mutex_;
    std::map<client_t, std::map<session_t, endpoint_type> > clients_;

    boost::asio::steady_timer flush_timer_;

    std::mutex mutex_;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICE_ENDPOINT_IMPL_HPP

// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
#define VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include <vsomeip/defines.hpp>
#include "server_endpoint_impl.hpp"

namespace vsomeip {

typedef server_endpoint_impl<boost::asio::ip::udp,
        VSOMEIP_MAX_UDP_MESSAGE_SIZE> udp_server_endpoint_base_impl;

class udp_server_endpoint_impl: public udp_server_endpoint_base_impl {

public:
    udp_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
                             boost::asio::io_service &_io);
    virtual ~udp_server_endpoint_impl();

    void start();
    void stop();

    void restart();
    void receive();

    bool send_to(const std::shared_ptr<endpoint_definition> _target,
            const byte_t *_data, uint32_t _size, bool _flush);
    void send_queued(endpoint_type _target, message_buffer_ptr_t _buffer);

    endpoint_type get_remote() const;
    bool get_multicast(service_t _service, event_t _event,
                       endpoint_type &_target) const;

    void join(const std::string &_address);
    void leave(const std::string &_address);
    void add_multicast(service_t _service, instance_t _instance,
                       const std::string &_address, uint16_t _port);
    void remove_multicast(service_t _service, instance_t _instance);

    unsigned short get_local_port() const;
    bool is_local() const;

    client_t get_client(std::shared_ptr<endpoint_definition> _endpoint);

public:
    void receive_cbk(packet_buffer_ptr_t _buffer,
                     boost::system::error_code const &_error,
                     std::size_t _size);

private:
    void set_broadcast();

private:
    socket_type socket_;
    endpoint_type remote_;
    std::map<service_t, std::map<instance_t, endpoint_type> > multicasts_;
    message_buffer_t message_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/multicast.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/udp_client_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

udp_client_endpoint_impl::udp_client_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local,
        endpoint_type _remote,
        boost::asio::io_service &_io)
    : udp_client_endpoint_base_impl(_host, _local, _remote, _io,
            VSOMEIP_MAX_UDP_MESSAGE_SIZE),
      recv_buffer_(VSOMEIP_MAX_UDP_MESSAGE_SIZE, 0),
      remote_address_(_remote.address()),
      remote_port_(_remote.port()) {
}

udp_client_endpoint_impl::~udp_client_endpoint_impl() {
    std::shared_ptr<endpoint_host> its_host = host_.lock();
    if (its_host) {
        its_host->release_port(local_.port(), false);
    }
}

bool udp_client_endpoint_impl::is_local() const {
    return false;
}

void udp_client_endpoint_impl::connect() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    // In case a client endpoint port was configured,
    // bind to it before connecting
    if (local_.port() != ILLEGAL_PORT) {
        boost::system::error_code its_bind_error;
        socket_->bind(local_, its_bind_error);
        if(its_bind_error) {
            VSOMEIP_WARNING << "tcp_client_endpoint::connect: "
                    "Error binding socket: " << its_bind_error.message();
        }
    }

    socket_->async_connect(
        remote_,
        std::bind(
            &udp_client_endpoint_base_impl::connect_cbk,
            shared_from_this(),
            std::placeholders::_1
        )
    );
}

void udp_client_endpoint_impl::start() {
    boost::system::error_code its_error;
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        socket_->open(remote_.protocol(), its_error);
    }
    if (!its_error || its_error == boost::asio::error::already_open) {
        connect();
    } else {
        VSOMEIP_WARNING << "udp_client_endpoint::connect: Error opening socket: "
                << its_error.message();
    }
}

void udp_client_endpoint_impl::restart() {
    is_connected_ = false;
    {
        std::lock_guard<std::mutex> its_lock(mutex_);
        queue_.clear();
    }
    shutdown_and_close_socket();
    start_connect_timer();
}

void udp_client_endpoint_impl::send_queued() {
    message_buffer_ptr_t its_buffer;
    if(queue_.size()) {
        its_buffer = queue_.front();
    } else {
        return;
    }
#if 0
    std::stringstream msg;
    msg << "ucei<" << remote_.address() << ":"
        << std::dec << remote_.port()  << ">::sq: ";
    for (std::size_t i = 0; i < its_buffer->size(); i++)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int)(*its_buffer)[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        socket_->async_send(
            boost::asio::buffer(*its_buffer),
            std::bind(
                &udp_client_endpoint_base_impl::send_cbk,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    }
}

void udp_client_endpoint_impl::receive() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (!socket_->is_open()) {
        return;
    }
    socket_->async_receive_from(
        boost::asio::buffer(&recv_buffer_[0], max_message_size_),
        remote_,
        std::bind(
            &udp_client_endpoint_impl::receive_cbk,
            std::dynamic_pointer_cast<
                udp_client_endpoint_impl
            >(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

bool udp_client_endpoint_impl::get_remote_address(
        boost::asio::ip::address &_address) const {
    if (remote_address_.is_unspecified()) {
        return false;
    }
    _address = remote_address_;
    return true;
}

void udp_client_endpoint_impl::set_local_port() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    boost::system::error_code its_error;
    if (socket_->is_open()) {
        endpoint_type its_endpoint = socket_->local_endpoint(its_error);
        if (!its_error) {
            local_port_ = its_endpoint.port();
        } else {
            VSOMEIP_WARNING << "udp_client_endpoint_impl::set_local_port() "
                    << " couldn't get local_endpoint: " << its_error.message();
        }
    }
}

std::uint16_t udp_client_endpoint_impl::get_remote_port() const {
    return remote_port_;
}

void udp_client_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    if (_error == boost::asio::error::operation_aborted) {
        // endpoint was stopped
        return;
    }
    std::shared_ptr<endpoint_host> its_host = host_.lock();
    if (!_error && 0 < _bytes && its_host) {
#if 0
        std::stringstream msg;
        msg << "ucei::rcb(" << _error.message() << "): ";
        for (std::size_t i = 0; i < _bytes + recv_buffer_size_; ++i)
            msg << std::hex << std::setw(2) << std::setfill('0')
                << (int) recv_buffer_[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif
        std::size_t remaining_bytes = _bytes;
        std::size_t i = 0;

        do {
            uint64_t read_message_size
                = utility::get_message_size(&this->recv_buffer_[i],
                        remaining_bytes);
            if (read_message_size > MESSAGE_SIZE_UNLIMITED) {
                VSOMEIP_ERROR << "Message size exceeds allowed maximum!";
                return;
            }
            uint32_t current_message_size = static_cast<uint32_t>(read_message_size);
            if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                    current_message_size <= remaining_bytes) {
                if (remaining_bytes - current_message_size > remaining_bytes) {
                    VSOMEIP_ERROR << "buffer underflow in udp client endpoint ~> abort!";
                    return;
                }
                remaining_bytes -= current_message_size;

                its_host->on_message(&recv_buffer_[i], current_message_size,
                        this, boost::asio::ip::address(),
                        VSOMEIP_ROUTING_CLIENT, remote_address_,
                        remote_port_);
            } else {
                VSOMEIP_ERROR << "Received a unreliable vSomeIP message with bad "
                        "length field. Message size: " << current_message_size
                        << " Bytes. From: " << remote_.address() << ":"
                        << remote_.port() << ". Dropping message.";
                remaining_bytes = 0;
            }
            i += current_message_size;
        } while (remaining_bytes > 0);
    }
    if (!_error) {
        receive();
    } else {
        if (_error == boost::asio::error::connection_refused) {
            shutdown_and_close_socket();
        } else {
            receive();
        }
    }
}

} // namespace vsomeip

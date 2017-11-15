// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <deque>
#include <string>
#include <vector>
#include <mutex>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>

#include "../include/buffer.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/utility.hpp"
#include "../include/dtls_client_endpoint_impl.hpp"
#include "../include/endpoint_host.hpp"

namespace vsomeip {

dtls_client_endpoint_impl::dtls_client_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _local,
        endpoint_type _remote, boost::asio::io_service &_io, bool _confidential,
        const std::vector<std::uint8_t> &_psk, const std::string &_pskid)
        : secure_endpoint_base(_confidential, _psk, _pskid),
          udp_client_endpoint_base_impl(_host, _local, _remote, _io,
                                        get_max_dtls_payload_size()),
          recv_buffer_(std::make_shared<message_buffer_t>(get_max_dtls_payload_size(), 0)),
          remote_address_(_remote.address()),
          remote_port_(_remote.port()),
          tls_socket_(&socket_, socket_mutex_, psk_, pskid_, cipher_suite_) {
}

dtls_client_endpoint_impl::~dtls_client_endpoint_impl() {
    std::shared_ptr<endpoint_host> its_host = host_.lock();
    if (its_host) {
        its_host->release_port(local_.port(), false);
    }
}

bool dtls_client_endpoint_impl::is_local() const {
    return false;
}

void dtls_client_endpoint_impl::connect() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    // In case a client endpoint port was configured,
    // bind to it before connecting
    if (local_.port() != ILLEGAL_PORT) {
        boost::system::error_code its_bind_error;
        socket_.bind(local_, its_bind_error);
        if (its_bind_error) {
            VSOMEIP_WARNING<< "dcei::connect: "
            "Error binding socket: "
            << its_bind_error.message();
        }
    }

    socket_.async_connect(remote_,
                          std::bind(&dtls_client_endpoint_impl::connect_cbk_intern,
                                    std::dynamic_pointer_cast<dtls_client_endpoint_impl>(shared_from_this()),
                                    std::placeholders::_1));
}

void dtls_client_endpoint_impl::connect_cbk_intern(boost::system::error_code const &_error) {
    if (!_error) {
        tls_socket_.async_handshake(
                std::bind(&dtls_client_endpoint_impl::handshake_cbk,
                          std::dynamic_pointer_cast<dtls_client_endpoint_impl>(shared_from_this()),
                          std::placeholders::_1));
    } else {
        udp_client_endpoint_base_impl::connect_cbk(_error);
    }
}

void dtls_client_endpoint_impl::handshake_cbk(boost::system::error_code const &_error) {
    udp_client_endpoint_base_impl::connect_cbk(_error);

    if (!_error) {
        send_queued();
    }
}

void dtls_client_endpoint_impl::start() {
    boost::system::error_code its_error;
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        socket_.open(remote_.protocol(), its_error);
    }

    if (!its_error || its_error == boost::asio::error::already_open) {
        connect();
    } else {
        VSOMEIP_WARNING<< "dcei::connect: Error opening socket: "
                << its_error.message();
    }
}

void dtls_client_endpoint_impl::send_queued() {
    message_buffer_ptr_t its_buffer;

    if (!queue_.empty()) {
        its_buffer = queue_.front();
    } else {
        return;
    }

    tls_socket_.async_send(its_buffer,
                           std::bind(&udp_client_endpoint_base_impl::send_cbk,
                                     std::dynamic_pointer_cast<dtls_client_endpoint_impl>(shared_from_this()),
                                     std::placeholders::_1, std::placeholders::_2));
}

void dtls_client_endpoint_impl::receive() {
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        if (!socket_.is_open()) {
            return;
        }
    }

    tls_socket_.async_receive_from(recv_buffer_,
                                   remote_,
                                   std::bind(&dtls_client_endpoint_impl::receive_cbk,
                                             std::dynamic_pointer_cast<dtls_client_endpoint_impl>(shared_from_this()),
                                             std::placeholders::_1, std::placeholders::_2));
}

bool dtls_client_endpoint_impl::get_remote_address(
        boost::asio::ip::address &_address) const {
    if (remote_address_.is_unspecified()) {
        return false;
    }
    _address = remote_address_;
    return true;
}

unsigned short dtls_client_endpoint_impl::get_local_port() const {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    boost::system::error_code its_error;
    if (socket_.is_open()) {
        endpoint_type its_endpoint = socket_.local_endpoint(its_error);
        if (!its_error) {
            return its_endpoint.port();
        } else {
            VSOMEIP_WARNING<< "dtls_client_endpoint_impl::get_local_port() "
            << " couldn't get local_endpoint: "
            << its_error.message();
        }
    }
    return 0;
}

unsigned short dtls_client_endpoint_impl::get_remote_port() const {
    return remote_port_;
}

void dtls_client_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    if (_error == boost::asio::error::operation_aborted) {
        // endpoint was stopped
        return;
    }
    std::shared_ptr<endpoint_host> its_host = host_.lock();
    if (!_error && 0 < _bytes && its_host) {
        std::size_t remaining_bytes = _bytes;
        std::size_t i = 0;

        do {
            uint64_t read_message_size
                = utility::get_message_size(&(*recv_buffer_)[i], remaining_bytes);
            if (read_message_size > max_message_size_) {
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

                its_host->on_message(&(*recv_buffer_)[i], current_message_size,
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

}  // namespace vsomeip

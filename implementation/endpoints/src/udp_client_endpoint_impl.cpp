// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
      recv_buffer_(VSOMEIP_MAX_UDP_MESSAGE_SIZE, 0) {
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
	// In case a client endpoint port was configured,
	// bind to it before connecting
	if (local_.port() != ILLEGAL_PORT) {
		socket_.bind(local_);
	}

	socket_.async_connect(
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
    socket_.open(remote_.protocol(), its_error);
    if (!its_error || its_error == boost::asio::error::already_open) {
        connect();
    } else {
        VSOMEIP_WARNING << "udp_client_endpoint::connect: Error opening socket: "
                << its_error.message();
    }
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
    VSOMEIP_DEBUG << msg.str();
#endif
    socket_.async_send(
        boost::asio::buffer(*its_buffer),
        std::bind(
            &udp_client_endpoint_base_impl::send_cbk,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void udp_client_endpoint_impl::receive() {
    socket_.async_receive_from(
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
    _address = remote_.address();
    return true;
}

unsigned short udp_client_endpoint_impl::get_local_port() const {
    boost::system::error_code its_error;
    return socket_.local_endpoint(its_error).port();
}

unsigned short udp_client_endpoint_impl::get_remote_port() const {
    boost::system::error_code its_error;
    return socket_.remote_endpoint(its_error).port();
}

void udp_client_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    if (_error == boost::asio::error::operation_aborted) {
        // endpoint was stopped
        shutdown_and_close_socket();
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
        VSOMEIP_DEBUG << msg.str();
#endif
        uint32_t current_message_size
            = utility::get_message_size(&this->recv_buffer_[0],
                    (uint32_t) _bytes);
        if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                current_message_size <= _bytes) {
            its_host->on_message(&recv_buffer_[0], current_message_size, this);
        } else {
            VSOMEIP_ERROR << "Received a unreliable vSomeIP message with bad length field";
        }
    }
    if (!_error) {
        receive();
    } else {
        if (_error == boost::asio::error::connection_refused) {
            shutdown_and_close_socket();
        } else if (socket_.is_open()) {
            receive();
        }
    }
}

} // namespace vsomeip

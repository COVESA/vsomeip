// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
        std::shared_ptr< endpoint_host > _host, endpoint_type _remote,
        boost::asio::io_service &_io)
    : udp_client_endpoint_base_impl(_host, _remote, _io, VSOMEIP_MAX_UDP_MESSAGE_SIZE),
      recv_buffer_(VSOMEIP_MAX_UDP_MESSAGE_SIZE, 0),
      recv_buffer_size_(0) {
}

udp_client_endpoint_impl::~udp_client_endpoint_impl() {
}

bool udp_client_endpoint_impl::is_local() const {
    return false;
}

void udp_client_endpoint_impl::connect() {
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
    socket_.open(remote_.protocol());
    connect();
}

void udp_client_endpoint_impl::send_queued() {
    message_buffer_ptr_t its_buffer = queue_.front();
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
    if (recv_buffer_size_ == max_message_size_) {
        // Overrun -> Reset buffer
        recv_buffer_size_ = 0;
    }
    size_t buffer_size = max_message_size_ - recv_buffer_size_;
    socket_.async_receive_from(
        boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
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
  return socket_.local_endpoint().port();
}

unsigned short udp_client_endpoint_impl::get_remote_port() const {
  return socket_.remote_endpoint().port();
}

void udp_client_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
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
        recv_buffer_size_ += _bytes;
        uint32_t current_message_size
            = utility::get_message_size(&this->recv_buffer_[0],
                    (uint32_t) recv_buffer_size_);
        if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                current_message_size <= _bytes) {
            its_host->on_message(&recv_buffer_[0], current_message_size, this);
        } else {
            VSOMEIP_ERROR << "Received a unreliable vSomeIP message with bad length field";
        }
        recv_buffer_size_ = 0;
    }
    if (!_error) {
        receive();
    } else {
        if (socket_.is_open()) {
            receive();
        }
    }
}

} // namespace vsomeip

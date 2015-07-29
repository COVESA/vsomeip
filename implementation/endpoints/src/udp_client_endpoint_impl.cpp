// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/multicast.hpp>

#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/udp_client_endpoint_impl.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

udp_client_endpoint_impl::udp_client_endpoint_impl(
        std::shared_ptr< endpoint_host > _host, endpoint_type _remote,
        boost::asio::io_service &_io)
    : udp_client_endpoint_base_impl(_host, _remote, _io) {
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
    receive();
}

void udp_client_endpoint_impl::send_queued(message_buffer_ptr_t _buffer) {
#if 0
    std::stringstream msg;
    msg << "ucei<" << remote_.address() << ":"
        << std::dec << remote_.port()  << ">::sq: ";
    for (std::size_t i = 0; i < _buffer->size(); i++)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int)(*_buffer)[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    socket_.async_send(
        boost::asio::buffer(*_buffer),
        std::bind(
            &udp_client_endpoint_base_impl::send_cbk,
            shared_from_this(),
            _buffer,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
    receive();
}

void udp_client_endpoint_impl::receive() {
    packet_buffer_ptr_t its_buffer
        = std::make_shared< packet_buffer_t >();
    socket_.async_receive_from(
        boost::asio::buffer(*its_buffer),
        remote_,
        std::bind(
            &udp_client_endpoint_impl::receive_cbk,
            std::dynamic_pointer_cast<
                udp_client_endpoint_impl
            >(shared_from_this()),
            its_buffer,
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

void udp_client_endpoint_impl::join(const std::string &_address) {

    if (remote_.address().is_v4()) {
        try {
            socket_.set_option(
                boost::asio::ip::udp::socket::reuse_address(true));
            socket_.set_option(boost::asio::ip::multicast::join_group(
                boost::asio::ip::address::from_string(_address)));
        }
        catch (...) {
        }
    } else {
        // TODO: support multicast for IPv6
    }
}

void udp_client_endpoint_impl::leave(const std::string &_address) {
    if (remote_.address().is_v4()) {
        try {
            socket_.set_option(
                boost::asio::ip::udp::socket::reuse_address(true));
            socket_.set_option(boost::asio::ip::multicast::leave_group(
                boost::asio::ip::address::from_string(_address)));
        }
        catch (...) {
        }
    } else {
        // TODO: support multicast for IPv6
    }
}

void udp_client_endpoint_impl::receive_cbk(
        packet_buffer_ptr_t _buffer,
        boost::system::error_code const &_error, std::size_t _bytes) {
    std::shared_ptr<endpoint_host> its_host = host_.lock();
    if (!_error && 0 < _bytes && its_host) {
#if 0
        std::stringstream msg;
        msg << "ucei::rcb(" << _error.message() << "): ";
        for (std::size_t i = 0; i < _bytes; ++i)
            msg << std::hex << std::setw(2) << std::setfill('0')
                << (int)(*_buffer)[i] << " ";
        VSOMEIP_DEBUG << msg.str();
#endif
        this->message_.insert(this->message_.end(), _buffer->begin(),
                              _buffer->begin() + _bytes);

        bool has_full_message;
        do {
            uint32_t current_message_size
                = utility::get_message_size(this->message_);

            has_full_message = (current_message_size > 0
                && current_message_size <= this->message_.size());
            if (has_full_message) {
                its_host->on_message(&message_[0], current_message_size, this);
                this->message_.erase(this->message_.begin(),
                    this->message_.begin() + current_message_size);
            }
        } while (has_full_message);
    }

    receive();
}

} // namespace vsomeip

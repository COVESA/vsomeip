// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/write.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/local_client_endpoint_impl.hpp"

namespace vsomeip {

local_client_endpoint_impl::local_client_endpoint_impl(
        std::shared_ptr< endpoint_host > _host, endpoint_type _remote,
        boost::asio::io_service &_io)
    : local_client_endpoint_base_impl(_host, _remote, _io) {
    is_supporting_magic_cookies_ = false;
}

local_client_endpoint_impl::~local_client_endpoint_impl() {

}

bool local_client_endpoint_impl::is_local() const {
    return true;
}

void local_client_endpoint_impl::start() {
    connect();
}

void local_client_endpoint_impl::connect() {
    socket_.open(remote_.protocol());

    socket_.async_connect(
        remote_,
        std::bind(
            &local_client_endpoint_base_impl::connect_cbk,
            shared_from_this(),
            std::placeholders::_1
        )
    );
}

void local_client_endpoint_impl::receive() {
    packet_buffer_ptr_t its_buffer
        = std::make_shared< packet_buffer_t >();
    socket_.async_receive(
        boost::asio::buffer(*its_buffer),
        std::bind(
            &local_client_endpoint_impl::receive_cbk,
            std::dynamic_pointer_cast<
                local_client_endpoint_impl
            >(shared_from_this()),
            its_buffer,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_client_endpoint_impl::send_queued(message_buffer_ptr_t _buffer) {
#if 0
    std::stringstream msg;
    msg << "lce<" << this << ">::sq: ";
    for (std::size_t i = 0; i < _buffer->size(); i++)
        msg << std::setw(2) << std::setfill('0') << std::hex
            << (int)(*_buffer)[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif

    static byte_t its_start_tag[] = { 0x67, 0x37, 0x6D, 0x07 };
    static byte_t its_end_tag[] = { 0x07, 0x6D, 0x37, 0x67 };

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(
            its_start_tag,
            sizeof(its_start_tag)
        ),
        std::bind(
            &local_client_endpoint_impl::send_tag_cbk,
            std::dynamic_pointer_cast<
                local_client_endpoint_impl
            >(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*_buffer),
        std::bind(
            &client_endpoint_impl::send_cbk,
            this->shared_from_this(),
            _buffer,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(
            its_end_tag,
            sizeof(its_end_tag)
        ),
        std::bind(
            &local_client_endpoint_impl::send_tag_cbk,
            std::dynamic_pointer_cast<
                local_client_endpoint_impl
            >(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_client_endpoint_impl::send_magic_cookie() {
}

void local_client_endpoint_impl::send_tag_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
}

void local_client_endpoint_impl::receive_cbk(
        packet_buffer_ptr_t _buffer,
        boost::system::error_code const &_error, std::size_t _bytes) {
    VSOMEIP_ERROR << "Local endpoint received message ("
                  << _error.message() << ")";
}

} // namespace vsomeip

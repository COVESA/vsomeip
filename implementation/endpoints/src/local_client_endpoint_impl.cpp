// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/write.hpp>

#include <vsomeip/defines.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/local_client_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"

namespace vsomeip {

local_client_endpoint_impl::local_client_endpoint_impl(
        std::shared_ptr< endpoint_host > _host, endpoint_type _remote,
        boost::asio::io_service &_io, std::uint32_t _max_message_size)
    : local_client_endpoint_base_impl(_host, _remote, _io, _max_message_size) {
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

    boost::system::error_code error;
    error = socket_.connect(remote_, error);
    connect_cbk(error);
}

void local_client_endpoint_impl::receive() {
    receive_buffer_t its_buffer(VSOMEIP_MAX_LOCAL_MESSAGE_SIZE , 0);
    socket_.async_receive(
        boost::asio::buffer(its_buffer),
        std::bind(
            &local_client_endpoint_impl::receive_cbk,
            std::dynamic_pointer_cast<
                local_client_endpoint_impl
            >(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_client_endpoint_impl::send_queued() {
    static byte_t its_start_tag[] = { 0x67, 0x37, 0x6D, 0x07 };

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(
            its_start_tag,
            sizeof(its_start_tag)
        ),
        std::bind(
            &local_client_endpoint_impl::send_start_tag_cbk,
            std::dynamic_pointer_cast<
                local_client_endpoint_impl
            >(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_client_endpoint_impl::send_queued_data() {
    std::lock_guard<std::mutex> its_lock(mutex_);
    message_buffer_ptr_t its_buffer = queue_.front();
    #if 0
    std::stringstream msg;
    msg << "lce<" << this << ">::sq: ";
    for (std::size_t i = 0; i < its_buffer->size(); i++)
        msg << std::setw(2) << std::setfill('0') << std::hex
            << (int)(*its_buffer)[i] << " ";
    VSOMEIP_DEBUG << msg.str();
    #endif

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*its_buffer),
        std::bind(
            &local_client_endpoint_impl::send_queued_data_cbk,
            std::dynamic_pointer_cast<
                local_client_endpoint_impl
            >(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_client_endpoint_impl::send_end_tag() {
    static byte_t its_end_tag[] = { 0x07, 0x6D, 0x37, 0x67 };

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(
            its_end_tag,
            sizeof(its_end_tag)
        ),
        std::bind(
            &client_endpoint_impl::send_cbk,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_client_endpoint_impl::send_magic_cookie() {
}

void local_client_endpoint_impl::send_start_tag_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_bytes;
    if (_error)
        send_cbk(_error, 0);

    send_queued_data();
}

void local_client_endpoint_impl::send_queued_data_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_bytes;
    if (_error)
        send_cbk(_error, 0);

    send_end_tag();
}

void local_client_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_error;
    (void)_bytes;
    VSOMEIP_ERROR << "Local endpoint received message ("
                  << _error.message() << ")";
}

} // namespace vsomeip

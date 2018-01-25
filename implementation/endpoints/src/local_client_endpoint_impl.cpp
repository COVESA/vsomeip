// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include "../include/local_server_endpoint_impl.hpp"
#include "../../configuration/include/configuration.hpp"

// Credentials
#ifndef _WIN32
#include "../include/credentials.hpp"
#endif

namespace vsomeip {

local_client_endpoint_impl::local_client_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _remote,
        boost::asio::io_service &_io,
        std::uint32_t _max_message_size)
    : local_client_endpoint_base_impl(_host, _remote, _remote, _io, _max_message_size),
                                      // Using _remote for the local(!) endpoint is ok,
                                      // because we have no bind for local endpoints!
      recv_buffer_(1,0) {
    is_supporting_magic_cookies_ = false;
}

local_client_endpoint_impl::~local_client_endpoint_impl() {

}

bool local_client_endpoint_impl::is_local() const {
    return true;
}

void local_client_endpoint_impl::restart() {
    is_connected_ = false;
    {
        std::lock_guard<std::mutex> its_lock(mutex_);
        sending_blocked_ = false;
        queue_.clear();
    }
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        shutdown_and_close_socket_unlocked();
        socket_.reset(new socket_type(service_));
    }
    start_connect_timer();
}

void local_client_endpoint_impl::start() {
    connect();
}

void local_client_endpoint_impl::connect() {
    boost::system::error_code its_connect_error;
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        boost::system::error_code its_error;
        socket_->open(remote_.protocol(), its_error);

        if (!its_error || its_error == boost::asio::error::already_open) {
            socket_->set_option(boost::asio::socket_base::reuse_address(true), its_error);
            if (its_error) {
                VSOMEIP_WARNING << "local_client_endpoint_impl::connect: "
                        << "couldn't enable SO_REUSEADDR: " << its_error.message();
            }
            socket_->connect(remote_, its_connect_error);

// Credentials
#ifndef _WIN32
            if (!its_connect_error) {
                auto its_host = host_.lock();
                if (its_host) {
                    if (its_host->get_configuration()->is_security_enabled()) {
                        credentials::send_credentials(socket_->native(),
                                its_host->get_client());
                    }
                }
            }
#endif

        } else {
            VSOMEIP_WARNING << "local_client_endpoint::connect: Error opening socket: "
                    << its_error.message();
            return;
        }
    }
    // call connect_cbk asynchronously
    try {
        service_.post(
                std::bind(&client_endpoint_impl::connect_cbk, shared_from_this(),
                        its_connect_error));
    } catch (const std::exception &e) {
        VSOMEIP_ERROR << "local_client_endpoint_impl::connect: " << e.what();
    }
}

void local_client_endpoint_impl::receive() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if (socket_->is_open()) {
        socket_->async_receive(
            boost::asio::buffer(recv_buffer_),
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
}

void local_client_endpoint_impl::send_queued() {
    static byte_t its_start_tag[] = { 0x67, 0x37, 0x6D, 0x07 };
    static byte_t its_end_tag[] = { 0x07, 0x6D, 0x37, 0x67 };
    std::vector<boost::asio::const_buffer> bufs;

    message_buffer_ptr_t its_buffer;
    if(queue_.size()) {
        its_buffer = queue_.front();
    } else {
        return;
    }

#if 0
std::stringstream msg;
msg << "lce<" << this << ">::sq: ";
for (std::size_t i = 0; i < its_buffer->size(); i++)
    msg << std::setw(2) << std::setfill('0') << std::hex
        << (int)(*its_buffer)[i] << " ";
VSOMEIP_INFO << msg.str();
#endif

    bufs.push_back(boost::asio::buffer(its_start_tag));
    bufs.push_back(boost::asio::buffer(*its_buffer));
    bufs.push_back(boost::asio::buffer(its_end_tag));

    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        boost::asio::async_write(
            *socket_,
            bufs,
            std::bind(
                &client_endpoint_impl::send_cbk,
                std::dynamic_pointer_cast<
                    local_client_endpoint_impl
                >(shared_from_this()),
                std::placeholders::_1,
                std::placeholders::_2
            )
        );
    }
}

void local_client_endpoint_impl::send_magic_cookie() {
}

void local_client_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
    (void)_bytes;
    if (_error) {
        if (_error == boost::asio::error::operation_aborted) {
            // endpoint was stopped
            return;
        } else if (_error == boost::asio::error::connection_reset
                || _error == boost::asio::error::eof
                || _error == boost::asio::error::bad_descriptor) {
            VSOMEIP_TRACE << "local_client_endpoint:"
                    " connection_reseted/EOF/bad_descriptor";
        } else if (_error) {
            VSOMEIP_ERROR << "Local endpoint received message ("
                          << _error.message() << ")";
        }
        // The error handler is set only if the endpoint is hosted by the
        // routing manager. For the routing manager proxies, the corresponding
        // client endpoint (that connect to the same client) are removed
        // after the proxy has received the routing info.
        error_handler_t handler;
        {
            std::lock_guard<std::mutex> its_lock(error_handler_mutex_);
            handler = error_handler_;
        }
        if (handler)
            handler();
    } else {
        receive();
    }
}

bool local_client_endpoint_impl::get_remote_address(
        boost::asio::ip::address &_address) const {
    (void)_address;
    return false;
}

std::uint16_t local_client_endpoint_impl::get_remote_port() const {
    return 0;
}

void local_client_endpoint_impl::set_local_port() {
    // local_port_ is set to zero in ctor of client_endpoint_impl -> do nothing
}

} // namespace vsomeip

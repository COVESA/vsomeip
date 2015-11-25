// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <boost/asio/write.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/tcp_client_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/utility.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_client_endpoint_impl::tcp_client_endpoint_impl(
        std::shared_ptr< endpoint_host > _host, endpoint_type _remote,
        boost::asio::io_service &_io, std::uint32_t _max_message_size)
    : tcp_client_endpoint_base_impl(_host, _remote, _io, _max_message_size),
      recv_buffer_(_max_message_size, 0),
      recv_buffer_size_(0) {
    is_supporting_magic_cookies_ = true;
}

tcp_client_endpoint_impl::~tcp_client_endpoint_impl() {
}

bool tcp_client_endpoint_impl::is_local() const {
    return false;
}

void tcp_client_endpoint_impl::start() {
    connect();
}

void tcp_client_endpoint_impl::connect() {
    socket_.open(remote_.protocol());

    // Nagle algorithm off
    socket_.set_option(ip::tcp::no_delay(true));

    socket_.async_connect(
        remote_,
        std::bind(
            &tcp_client_endpoint_base_impl::connect_cbk,
            shared_from_this(),
            std::placeholders::_1
        )
    );
}

void tcp_client_endpoint_impl::receive() {
    if (recv_buffer_size_ == max_message_size_) {
        // Overrun -> Reset buffer
        recv_buffer_size_ = 0;
    }
    size_t buffer_size = max_message_size_ - recv_buffer_size_;
    socket_.async_receive(
        boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
        std::bind(
            &tcp_client_endpoint_impl::receive_cbk,
            std::dynamic_pointer_cast< tcp_client_endpoint_impl >(shared_from_this()),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void tcp_client_endpoint_impl::send_queued() {
    message_buffer_ptr_t its_buffer = queue_.front();

    if (has_enabled_magic_cookies_)
        send_magic_cookie(its_buffer);

#if 0
    std::stringstream msg;
    msg << "tcei<" << remote_.address() << ":"
        << std::dec << remote_.port()  << ">::sq: ";
    for (std::size_t i = 0; i < its_buffer->size(); i++)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int)(*its_buffer)[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*its_buffer),
        std::bind(
            &tcp_client_endpoint_base_impl::send_cbk,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

bool tcp_client_endpoint_impl::get_remote_address(
        boost::asio::ip::address &_address) const {
    _address = remote_.address();
    return true;
}

unsigned short tcp_client_endpoint_impl::get_local_port() const {
  return socket_.local_endpoint().port();
}

unsigned short tcp_client_endpoint_impl::get_remote_port() const {
  return socket_.remote_endpoint().port();
}

bool tcp_client_endpoint_impl::is_reliable() const {
  return true;
}

bool tcp_client_endpoint_impl::is_magic_cookie(size_t _offset) const {
    return (0 == std::memcmp(SERVICE_COOKIE, &recv_buffer_[_offset], sizeof(SERVICE_COOKIE)));
}

void tcp_client_endpoint_impl::send_magic_cookie(message_buffer_ptr_t &_buffer) {
    if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - _buffer->size() >=
        VSOMEIP_SOMEIP_HEADER_SIZE + VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE) {
        _buffer->insert(
            _buffer->begin(),
            CLIENT_COOKIE,
            CLIENT_COOKIE + sizeof(CLIENT_COOKIE)
        );
    } else {
        VSOMEIP_WARNING << "Packet full. Cannot insert magic cookie!";
    }
}

void tcp_client_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {
#if 0
    std::stringstream msg;
    msg << "cei::rcb (" << _error.message() << "): ";
    for (std::size_t i = 0; i < _bytes + recv_buffer_size_; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int) recv_buffer_[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    std::shared_ptr<endpoint_host> its_host = host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            recv_buffer_size_ += _bytes;

            size_t its_iteration_gap = 0;
            bool has_full_message;
            do {
                uint32_t current_message_size
                    = utility::get_message_size(&recv_buffer_[its_iteration_gap],
                            (uint32_t) recv_buffer_size_);
                has_full_message = (current_message_size > 0
                                 && current_message_size <= recv_buffer_size_);
                if (has_full_message) {
                    bool needs_forwarding(true);
                    if (is_magic_cookie(its_iteration_gap)) {
                        has_enabled_magic_cookies_ = true;
                    } else {
                        if (has_enabled_magic_cookies_) {
                            uint32_t its_offset = find_magic_cookie(&recv_buffer_[its_iteration_gap],
                                    (uint32_t) recv_buffer_size_);
                            if (its_offset < current_message_size) {
                                VSOMEIP_ERROR << "Message includes Magic Cookie. Ignoring it.";
                                current_message_size = its_offset;
                                needs_forwarding = false;
                            }
                        }
                    }
                    if (needs_forwarding) {
                        if (!has_enabled_magic_cookies_) {
                            its_host->on_message(&recv_buffer_[its_iteration_gap],
                                                 current_message_size, this);
                        } else {
                            // Only call on_message without a magic cookie in front of the buffer!
                            if (!is_magic_cookie(its_iteration_gap)) {
                                its_host->on_message(&recv_buffer_[its_iteration_gap],
                                                     current_message_size, this);
                            }
                        }
                    }
                    recv_buffer_size_ -= current_message_size;
                    its_iteration_gap += current_message_size;
                } else if (has_enabled_magic_cookies_ && recv_buffer_size_ > 0){
                    uint32_t its_offset = find_magic_cookie(&recv_buffer_[its_iteration_gap], recv_buffer_size_);
                    if (its_offset < recv_buffer_size_) {
                        recv_buffer_size_ -= its_offset;
                        its_iteration_gap += its_offset;
                        has_full_message = true; // trigger next loop
                    }
                } else if (current_message_size > max_message_size_) {
                    VSOMEIP_ERROR << "Message exceeds maximum message size. "
                                  << "Resetting receiver.";
                    recv_buffer_size_ = 0;
                }
            } while (has_full_message && recv_buffer_size_);
            if (its_iteration_gap) {
                // Copy incomplete message to front for next receive_cbk iteration
                for (size_t i = 0; i < recv_buffer_size_; ++i) {
                    recv_buffer_[i] = recv_buffer_[i + its_iteration_gap];
                }
            }
            restart();
        } else {
            if (socket_.is_open()) {
                receive();
            }
        }
    }
}

} // namespace vsomeip

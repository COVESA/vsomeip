// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <boost/asio/write.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/local_server_endpoint_impl.hpp"

#include "../../logging/include/logger.hpp"

namespace vsomeip {

local_server_endpoint_impl::local_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local, boost::asio::io_service &_io,
        std::uint32_t _max_message_size)
    : local_server_endpoint_base_impl(_host, _local, _io, _max_message_size),
      acceptor_(_io, _local) {
    is_supporting_magic_cookies_ = false;
}

local_server_endpoint_impl::~local_server_endpoint_impl() {
}

bool local_server_endpoint_impl::is_local() const {
    return true;
}

void local_server_endpoint_impl::start() {
    connection::ptr new_connection = connection::create(this, max_message_size_);

    acceptor_.async_accept(
        new_connection->get_socket(),
        std::bind(
            &local_server_endpoint_impl::accept_cbk,
            std::dynamic_pointer_cast<
                local_server_endpoint_impl
            >(shared_from_this()),
            new_connection,
            std::placeholders::_1
        )
    );
}

void local_server_endpoint_impl::stop() {
}

bool local_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target,
        const byte_t *_data, uint32_t _size, bool _flush) {
    (void)_target;
    (void)_data;
    (void)_size;
    (void)_flush;
    return false;
}

void local_server_endpoint_impl::send_queued(
        queue_iterator_type _queue_iterator) {
    auto connection_iterator = connections_.find(_queue_iterator->first);
    if (connection_iterator != connections_.end())
        connection_iterator->second->send_queued(_queue_iterator);
}

void local_server_endpoint_impl::receive() {
    // intentionally left empty
}

void local_server_endpoint_impl::restart() {
    current_->start();
}

local_server_endpoint_impl::endpoint_type
local_server_endpoint_impl::get_remote() const {
    return current_->get_socket().remote_endpoint();
}

bool local_server_endpoint_impl::get_multicast(
        service_t, event_t,
        local_server_endpoint_impl::endpoint_type &) const {
    return false;
}

void local_server_endpoint_impl::remove_connection(
        local_server_endpoint_impl::connection *_connection) {
    std::map< endpoint_type, connection::ptr >::iterator i
        = connections_.end();
    for (i = connections_.begin(); i != connections_.end(); i++) {
        if (i->second.get() == _connection)
            break;
    }

    if (i != connections_.end()) {
        connections_.erase(i);
    }
}

void local_server_endpoint_impl::accept_cbk(
        connection::ptr _connection, boost::system::error_code const &_error) {

    if (!_error) {
        socket_type &new_connection_socket = _connection->get_socket();
        endpoint_type remote = new_connection_socket.remote_endpoint();

        connections_[remote] = _connection;
        _connection->start();
    }

    start();
}

///////////////////////////////////////////////////////////////////////////////
// class local_service_impl::connection
///////////////////////////////////////////////////////////////////////////////

local_server_endpoint_impl::connection::connection(
        local_server_endpoint_impl *_server, std::uint32_t _max_message_size)
    : socket_(_server->service_), server_(_server),
      max_message_size_(_max_message_size + 8),
      recv_buffer_(max_message_size_, 0),
      recv_buffer_size_(0) {
}

local_server_endpoint_impl::connection::ptr
local_server_endpoint_impl::connection::create(
        local_server_endpoint_impl *_server, std::uint32_t _max_message_size) {
    return ptr(new connection(_server, _max_message_size));
}

local_server_endpoint_impl::socket_type &
local_server_endpoint_impl::connection::get_socket() {
    return socket_;
}

void local_server_endpoint_impl::connection::start() {
    if (recv_buffer_size_ == max_message_size_) {
        // Overrun -> Reset buffer
        recv_buffer_size_ = 0;
    }
    size_t buffer_size = max_message_size_ - recv_buffer_size_;
    socket_.async_receive(
        boost::asio::buffer(&recv_buffer_[recv_buffer_size_], buffer_size),
        std::bind(
            &local_server_endpoint_impl::connection::receive_cbk,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_server_endpoint_impl::connection::send_queued(
        queue_iterator_type _queue_iterator) {

    // TODO: We currently do _not_ use the send method of the local server
    // endpoints. If we ever need it, we need to add the "start tag", "data",
    // "end tag" sequence here.

    message_buffer_ptr_t its_buffer = _queue_iterator->second.front();
#if 0
        std::stringstream msg;
        msg << "lse::sq: ";
        for (std::size_t i = 0; i < its_buffer->size(); i++)
            msg << std::setw(2) << std::setfill('0') << std::hex
                << (int)(*its_buffer)[i] << " ";
        VSOMEIP_DEBUG << msg.str();
#endif
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*its_buffer),
        std::bind(
            &local_server_endpoint_base_impl::send_cbk,
            server_->shared_from_this(),
            _queue_iterator,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void local_server_endpoint_impl::connection::send_magic_cookie() {
}

void local_server_endpoint_impl::connection::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes) {

    std::shared_ptr<endpoint_host> its_host = server_->host_.lock();
    if (its_host) {
        std::size_t its_start;
        std::size_t its_end;
        std::size_t its_iteration_gap = 0;

        if (!_error && 0 < _bytes) {
    #if 0
            std::stringstream msg;
            msg << "lse::c<" << this << ">rcb: ";
            for (std::size_t i = 0; i < _bytes + recv_buffer_size_; i++)
                msg << std::setw(2) << std::setfill('0') << std::hex
                    << (int) (recv_buffer_[i]) << " ";
            VSOMEIP_DEBUG << msg.str();
    #endif

            recv_buffer_size_ += _bytes;

    #define MESSAGE_IS_EMPTY     std::size_t(-1)
    #define FOUND_MESSAGE        std::size_t(-2)

            do {
                its_start = 0 + its_iteration_gap;
                while (its_start + 3 < recv_buffer_size_ + its_iteration_gap &&
                    (recv_buffer_[its_start] != 0x67 ||
                    recv_buffer_[its_start+1] != 0x37 ||
                    recv_buffer_[its_start+2] != 0x6d ||
                    recv_buffer_[its_start+3] != 0x07)) {
                    its_start ++;
                }

                its_start = (its_start + 3 == recv_buffer_size_ + its_iteration_gap ?
                                 MESSAGE_IS_EMPTY : its_start+4);

                if (its_start != MESSAGE_IS_EMPTY) {
                    its_end = its_start;
                    while (its_end + 3 < recv_buffer_size_ + its_iteration_gap &&
                        (recv_buffer_[its_end] != 0x07 ||
                        recv_buffer_[its_end+1] != 0x6d ||
                        recv_buffer_[its_end+2] != 0x37 ||
                        recv_buffer_[its_end+3] != 0x67)) {
                        its_end ++;
                    }
                }

                if (its_start != MESSAGE_IS_EMPTY &&
                    its_end + 3 < recv_buffer_size_ + its_iteration_gap) {
                    its_host->on_message(&recv_buffer_[its_start],
                                         uint32_t(its_end - its_start), server_);

                    #if 0
                            std::stringstream local_msg;
                            local_msg << "lse::c<" << this << ">rcb::thunk: ";
                            for (std::size_t i = its_start; i < its_end; i++)
                                local_msg << std::setw(2) << std::setfill('0') << std::hex
                                    << (int) recv_buffer_[i] << " ";
                            VSOMEIP_DEBUG << local_msg.str();
                    #endif

                    recv_buffer_size_ -= (its_end + 4 - its_iteration_gap);
                    its_start = FOUND_MESSAGE;
                    its_iteration_gap = its_end + 4;
                } else {
                    if (its_start != MESSAGE_IS_EMPTY && its_iteration_gap) {
                        // Message not complete and not in front of the buffer!
                        // Copy last part to front for consume in future receive_cbk call!
                        for (size_t i = 0; i < recv_buffer_size_; ++i) {
                            recv_buffer_[i] = recv_buffer_[i + its_iteration_gap];
                        }
                    }
                }
            } while (recv_buffer_size_ > 0 && its_start == FOUND_MESSAGE);
        }

        if (_error == boost::asio::error::misc_errors::eof) {
            server_->remove_connection(this);
        } else {
            start();
        }
    }
}

} // namespace vsomeip

// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/local_acceptor_tcp_impl.hpp"
#include "../include/tcp_socket.hpp"
#include "../include/abstract_socket_factory.hpp"
#include "../include/local_socket_tcp_impl.hpp"

#include "../../configuration/include/configuration.hpp"

#include <boost/system/error_code.hpp>

#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {
local_acceptor_tcp_impl::local_acceptor_tcp_impl(boost::asio::io_context& _io, std::shared_ptr<configuration> _configuration) :
    io_(_io), acceptor_(abstract_socket_factory::get()->create_tcp_acceptor(_io)), configuration_(std::move(_configuration)) { }
local_acceptor_tcp_impl::~local_acceptor_tcp_impl() = default;

void local_acceptor_tcp_impl::init(boost::asio::ip::tcp::endpoint _local_ep, boost::system::error_code& _ec) {
    std::scoped_lock const lock{mtx_};
    local_ep_ = _local_ep;
    if (!acceptor_->is_open()) {
        acceptor_->open(local_ep_.protocol(), _ec);
        if (_ec) {
            VSOMEIP_ERROR << "lati::init: Error encountered when calling: open. Error: " << _ec.message() << " for socket: " << local_ep_
                          << ", mem: " << this;
            return;
        }
    }

#ifndef _WIN32
    acceptor_->set_option(boost::asio::socket_base::reuse_address(true), _ec);
    if (_ec) {
        VSOMEIP_ERROR << "lati::" << __func__ << ": could not setsockopt(SO_REUSEADDR), " << _ec.message() << ", " << local_ep_
                      << ", mem: " << this;
        return;
    }
#endif

#if defined(__linux__) || defined(ANDROID)
    if (!acceptor_->set_native_option_free_bind()) {
        VSOMEIP_ERROR << "lati::" << __func__ << ": could not setsockopt(IP_FREEBIND), errno " << errno << ", mem: " << this;
    }
#endif

    acceptor_->bind(local_ep_, _ec);
    if (_ec) {
        VSOMEIP_INFO << "lati::" << __func__ << ": could not bind, " << _ec.message() << ", " << local_ep_ << ", mem: " << this;
        // we do not close the socket, as this is an expected situation
        return;
    }

    acceptor_->listen(boost::asio::socket_base::max_listen_connections, _ec);
    if (_ec) {
        VSOMEIP_ERROR << "lati::" << __func__ << ": could not listen, " << _ec.message() << ", " << local_ep_ << ", mem: " << this;
        // if listen fails, we better also "revert" bind
        boost::system::error_code ec;
        acceptor_->close(ec);
        if (ec) {
            VSOMEIP_ERROR << "lati::" << __func__ << ": could not close socket, " << ec.message() << ", " << local_ep_ << ", mem: " << this;
        }
    }
}

void local_acceptor_tcp_impl::close(boost::system::error_code& _ec) {
    std::scoped_lock const lock{mtx_};
    if (acceptor_->is_open()) {
        VSOMEIP_INFO << "lati::" << __func__ << ": Closing the acceptor: " << local_ep_ << ", mem: " << this;
        acceptor_->close(_ec);
    }
}
void local_acceptor_tcp_impl::cancel(boost::system::error_code& _ec) {
    std::scoped_lock const lock{mtx_};
    if (acceptor_->is_open()) {
        VSOMEIP_INFO << "lati::" << __func__ << ": Cancel operations for the acceptor: " << local_ep_ << ", mem: " << this;
        acceptor_->cancel(_ec);
    }
}

void local_acceptor_tcp_impl::async_accept(connection_handler _handler) {
    std::scoped_lock const lock{mtx_};
    socket_ = abstract_socket_factory::get()->create_tcp_socket(io_);
    acceptor_->async_accept(*socket_, [weak_self = weak_from_this(), h = std::move(_handler)](auto const& _ec) {
        if (auto self = weak_self.lock(); self) {
            self->accept_cbk(_ec, std::move(h));
        }
    });
}

// NOSONAR(cpp:S5213) - Handler is already std::function from async_accept, making this a template provides no benefit
void local_acceptor_tcp_impl::accept_cbk(boost::system::error_code const& _ec, connection_handler _handler) {
    if (_ec) {
        _handler(_ec, nullptr);
        return;
    }
    std::unique_lock lock{mtx_};
    boost::system::error_code ec;
    auto remote = socket_->remote_endpoint(ec);
    if (ec) {
        VSOMEIP_WARNING << "lati::" << __func__ << ": could not read remote endpoint, "
                        << "error: " << ec.message();
        // invoke handler only without the lock (avoids the need of a recursive mutex)
        lock.unlock();
        _handler(ec, nullptr);
        return;
    }

    // transfer ownership of the socket member, subsequent async_accept calls will re-instantiate it
    auto socket = std::make_shared<local_socket_tcp_impl>(io_, std::move(socket_), local_ep_, std::move(remote), socket_role_e::RECEIVER);
    socket->set_socket_options(*configuration_);
    lock.unlock();
    // invoke handler only without the lock (avoids the need of a recursive mutex)
    _handler(_ec, std::move(socket));
}

port_t local_acceptor_tcp_impl::get_local_port() {
    std::scoped_lock const lock{mtx_};
    return local_ep_.port();
}
}

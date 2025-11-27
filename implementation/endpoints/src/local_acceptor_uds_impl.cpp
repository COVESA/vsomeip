// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__) || defined(__QNX__)

#include "../include/local_acceptor_uds_impl.hpp"
#include "../include/abstract_socket_factory.hpp"
#include "../include/local_socket_uds_impl.hpp"

#include "../../configuration/include/configuration.hpp"

#include <vsomeip/internal/logger.hpp>
#include <vsomeip/vsomeip_sec.h>

namespace vsomeip_v3 {
local_acceptor_uds_impl::local_acceptor_uds_impl(boost::asio::io_context& _io, endpoint _own_endpoint,
                                                 std::shared_ptr<configuration> _configuration) :
    io_(_io), acceptor_(std::make_unique<acceptor>(io_)), own_endpoint_(_own_endpoint), configuration_(std::move(_configuration)) { }

local_acceptor_uds_impl::~local_acceptor_uds_impl() = default;

void local_acceptor_uds_impl::init(boost::system::error_code& _ec, std::optional<int> _native_fd) {
    std::scoped_lock const lock{mtx_};
    if (_native_fd) {
        acceptor_->assign(own_endpoint_.protocol(), *_native_fd, _ec);
        if (_ec) {
            VSOMEIP_ERROR << "laui::" << __func__ << ": could not assign() acceptor " << own_endpoint_.path()
                          << " , with fd: " << *_native_fd << " due to err " << _ec.message() << ", mem: " << this;
            return;
        }
    } else {
        acceptor_->open(own_endpoint_.protocol(), _ec);
        if (_ec) {
            VSOMEIP_ERROR << "laui::" << __func__ << ": could not open() acceptor " << own_endpoint_.path() << " due to err "
                          << _ec.message() << ", mem: " << this;
            return;
        }
    }

    auto close = [this](char const* _method, boost::system::error_code const& _error_code) {
        VSOMEIP_ERROR << "laui::init: Error encountered when calling: " << _method << ". Error: " << _error_code.message()
                      << ", for socket: " << own_endpoint_.path() << ", mem: " << this;
        boost::system::error_code ec;
        acceptor_->close(ec);
        if (ec) {
            VSOMEIP_ERROR << "laui::init::close: Could not close socket for " << own_endpoint_.path() << ", mem: " << this;
        }
    };
    acceptor_->set_option(boost::asio::socket_base::reuse_address(true), _ec);
    if (_ec) {
        close("set_option(reuse_address)", _ec);
        return;
    }

    // cleanup leftover before bind()
    if (-1 == ::unlink(own_endpoint_.path().c_str()) && errno != ENOENT) {
        VSOMEIP_ERROR << "laui::" << __func__ << ": unlink(" << own_endpoint_.path() << ") failed due to errno " << errno
                      << ", mem: " << this;
    }

    acceptor_->bind(own_endpoint_, _ec);
    if (_ec) {
        close("bind", _ec);
        return;
    }

    acceptor_->listen(boost::asio::socket_base::max_listen_connections, _ec);
    if (_ec) {
        close("listen", _ec);
        return;
    }

#ifndef __QNX__
    if (chmod(own_endpoint_.path().c_str(), configuration_->get_permissions_uds()) == -1) {
        VSOMEIP_ERROR << __func__ << ": chmod: " << strerror(errno) << ", mem: " << this;
    }
#endif
}

void local_acceptor_uds_impl::close(boost::system::error_code& _ec) {
    std::scoped_lock const lock{mtx_};
    if (acceptor_->is_open()) {
        VSOMEIP_INFO << "laui::" << __func__ << ": Closing the acceptor: " << own_endpoint_.path() << ", mem: " << this;
        acceptor_->close(_ec);
    }
}
void local_acceptor_uds_impl::cancel(boost::system::error_code& _ec) {
    std::scoped_lock const lock{mtx_};
    if (acceptor_->is_open()) {
        VSOMEIP_INFO << "laui::" << __func__ << ": Cancel operations for the acceptor: " << own_endpoint_.path() << ", mem: " << this;
        acceptor_->cancel(_ec);
    }
}

void local_acceptor_uds_impl::async_accept(connection_handler _handler) {
    std::scoped_lock const lock{mtx_};
    socket_ = abstract_socket_factory::get()->create_uds_socket(io_);
    acceptor_->async_accept(*socket_, [weak_self = weak_from_this(), h = std::move(_handler)](auto const& _ec) {
        if (auto self = weak_self.lock(); self) {
            self->accept_cbk(_ec, std::move(h));
        }
    });
}

port_t local_acceptor_uds_impl::get_local_port() {
    return VSOMEIP_SEC_PORT_UNUSED;
}

// NOSONAR(cpp:S5213) - Handler is already std::function from async_accept, making this a template provides no benefit
void local_acceptor_uds_impl::accept_cbk(boost::system::error_code const& _ec, connection_handler _handler) {
    if (_ec) {
        _handler(_ec, nullptr);
        return;
    }

    std::unique_lock lock{mtx_};
    boost::system::error_code ec;
    auto remote = socket_->remote_endpoint(ec);
    if (ec) {
        VSOMEIP_WARNING << "laui::" << __func__ << ": could not read remote endpoint, "
                        << "error: " << ec.message() << ", mem: " << this;
        // invoke handler only without the lock (avoids the need of a recursive mutex)
        lock.unlock();
        _handler(ec, nullptr);
        return;
    }
    // transfer ownership of the socket member, subsequent async_accept calls will re-instantiate it
    auto socket = std::move(socket_);
    lock.unlock();
    // invoke handler only without the lock (avoids the need of a recursive mutex)
    _handler(_ec,
             std::make_shared<local_socket_uds_impl>(io_, std::move(socket), own_endpoint_, std::move(remote), socket_role_e::RECEIVER));
}

}
#endif

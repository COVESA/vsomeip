// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__) || defined(__QNX__)
#include "../include/local_socket_uds_impl.hpp"
#include "../include/abstract_socket_factory.hpp"
#include <vsomeip/internal/logger.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/write.hpp>

#include <sys/socket.h>
#include <sys/un.h>

#define VSOMEIP_LOG_PREFIX "lsui"

namespace {
std::string to_string(vsomeip_v3::local_socket_uds_impl::endpoint _own_endpoint, vsomeip_v3::local_socket_uds_impl::endpoint _peer_endpoint,
                      vsomeip_v3::local_socket_uds_impl const* _address, vsomeip_v3::socket_role_e _role) {
    std::stringstream s;
    s << "role: " << vsomeip_v3::to_string(_role) << ", own: " << _own_endpoint << ", peer: " << _peer_endpoint << ", mem: " << _address;
    return s.str();
}
}

namespace vsomeip_v3 {

local_socket_uds_impl::local_socket_uds_impl(boost::asio::io_context& _io, std::shared_ptr<socket_type> _socket, endpoint _own_endpoint,
                                             endpoint _peer_endpoint, socket_role_e _role) :
    socket_(std::move(_socket)), io_context_(_io), peer_endpoint_(std::move(_peer_endpoint)),
    name_(::to_string(_own_endpoint, peer_endpoint_, this, _role)) { }

local_socket_uds_impl::local_socket_uds_impl(boost::asio::io_context& _io, endpoint _own_endpoint, endpoint _peer_endpoint,
                                             socket_role_e _role) :
    local_socket_uds_impl(_io, abstract_socket_factory::get()->create_uds_socket(_io), std::move(_own_endpoint), std::move(_peer_endpoint),
                          _role) { }

void local_socket_uds_impl::stop([[maybe_unused]] bool _force) {
    VSOMEIP_INFO_P << name_;
    std::scoped_lock const lock{socket_mtx_};
    if (socket_->is_open()) {
        boost::system::error_code ec;
        socket_->close(ec);
        if (ec) {
            VSOMEIP_ERROR_P << ec.message() << ", " << name_;
        }
    } else {
        VSOMEIP_WARNING_P << "Socket was not open, " << name_;
    }
}

void local_socket_uds_impl::prepare_connect([[maybe_unused]] configuration const& _configuration, boost::system::error_code& _ec) {
    std::scoped_lock const lock{socket_mtx_};
    socket_->open(peer_endpoint_.protocol(), _ec);
    if (_ec) {
        if (_ec == boost::asio::error::already_open) {
            // not that any error above will be treated as a failure, but this one isn't really
            // --> reset
            _ec = boost::system::error_code();
        }
        return;
    }

    boost::system::error_code ec;
    socket_->set_reuse_address(ec);
    if (ec) {
        VSOMEIP_WARNING_P << "Could not setsockopt(SO_REUSEADDR), " << ec.message() << ", " << name_;
    }
}

void local_socket_uds_impl::async_connect(connect_handler _handler) {
    std::unique_lock lock{socket_mtx_};
    if (socket_->is_open()) {
        socket_->async_connect(peer_endpoint_, std::move(_handler));
    } else {
        boost::asio::post(io_context_, [h = std::move(_handler)] { h(boost::asio::error::fault); });
    }
}

void local_socket_uds_impl::async_receive(boost::asio::mutable_buffer _buffer, read_handler _r_handler) {
    std::unique_lock lock{socket_mtx_};
    if (socket_->is_open()) {
        socket_->async_receive(_buffer, std::move(_r_handler));
    } else {
        boost::asio::post(io_context_, [h = std::move(_r_handler)] { h(boost::asio::error::fault, 0); });
    }
}

void local_socket_uds_impl::async_send(std::vector<uint8_t> _data, write_handler _w_handle) {
    std::unique_lock lock{socket_mtx_};
    if (socket_->is_open()) {
        // ensure the memory is kept alive as long as the callback hasn't been invoked
        auto buffer = boost::asio::buffer(_data);
        socket_->async_write(buffer, [d = std::move(_data), handler = std::move(_w_handle)](auto const& _ec, size_t _bytes) {
            handler(_ec, _bytes, std::move(d));
        });
    } else {
        boost::asio::post(io_context_, [h = std::move(_w_handle), d = std::move(_data)] { h(boost::asio::error::fault, 0, std::move(d)); });
    }
}

std::string const& local_socket_uds_impl::to_string() const {
    return name_;
}

bool local_socket_uds_impl::update(vsomeip_sec_client_t& _client, [[maybe_unused]] configuration const& _configuration) {
    std::scoped_lock const lock{socket_mtx_};
    if (!socket_->get_peer_credentials(_client)) {
        VSOMEIP_ERROR_P << "Could not getsockopt(SO_PEERCRED), errno " << errno;
        return false;
    }
    return true;
}

port_t local_socket_uds_impl::own_port() const {
    return VSOMEIP_SEC_PORT_UNUSED;
}

boost::asio::ip::tcp::endpoint local_socket_uds_impl::peer_endpoint() const {
    return {};
}

}
#endif

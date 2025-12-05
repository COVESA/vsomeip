// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/abstract_socket_factory.hpp"
#include "../include/io_control_operation.hpp"
#include "../include/local_socket_tcp_impl.hpp"

#include "../../configuration/include/configuration.hpp"

#include "../../security/include/security.hpp"

#include <vsomeip/vsomeip_sec.h>
#include <boost/system/error_code.hpp>
#include <vsomeip/internal/logger.hpp>

#include <boost/asio/buffer.hpp>

#include <chrono>
#include <thread>

namespace {
std::string to_string(boost::asio::ip::tcp::endpoint _own, boost::asio::ip::tcp::endpoint _peer,
                      vsomeip_v3::local_socket_tcp_impl const* _address, vsomeip_v3::socket_role_e _role) {
    std::stringstream s;
    s << "role: " << vsomeip_v3::to_string(_role) << ", own: " << _own << ", peer: " << _peer << ", mem: " << _address;
    return s.str();
}
} // namespace
namespace vsomeip_v3 {

local_socket_tcp_impl::local_socket_tcp_impl(boost::asio::io_context& _io, std::shared_ptr<tcp_socket> _socket,
                                             boost::asio::ip::tcp::endpoint _own, boost::asio::ip::tcp::endpoint _peer,
                                             socket_role_e _role) :
    socket_(std::move(_socket)), role_(_role), io_context_(_io), peer_endpoint_(std::move(_peer)), own_endpoint_(std::move(_own)),
    name_(::to_string(own_endpoint_, peer_endpoint_, this, _role)) { }

local_socket_tcp_impl::local_socket_tcp_impl(boost::asio::io_context& _io, boost::asio::ip::tcp::endpoint _own,
                                             boost::asio::ip::tcp::endpoint _peer, socket_role_e _role) :
    local_socket_tcp_impl(_io, abstract_socket_factory::get()->create_tcp_socket(_io), std::move(_own), std::move(_peer), _role) { }

local_socket_tcp_impl::~local_socket_tcp_impl() {
    if (socket_->is_open()) {
        VSOMEIP_ERROR << "lsti::" << __func__ << ": socket was not closed before cleaning it up. This can cause boost to block on close"
                      << name_;
        local_socket_tcp_impl::stop(true);
    }
}
void local_socket_tcp_impl::stop(bool _force) {
    VSOMEIP_INFO << "lsti::" << __func__ << ": " << name_ << ", force: " << (_force ? "true" : "false");
#if defined(__linux__) || defined(__QNX__)
    boost::system::error_code its_error;
    io_control_operation<std::size_t> send_buffer_size_cmd(TIOCOUTQ);

    uint32_t retry_count(0);
    while (true) {
        {
            std::scoped_lock its_lock(socket_mtx_); // Do not block this mutex while waiting, to
                                                    // let other operations finish
            if (socket_ && socket_->is_open()) {
                socket_->io_control(send_buffer_size_cmd, its_error);
            }
        }

        if (its_error) {
            VSOMEIP_WARNING << "lsti::" << __func__ << ": fail to read send_buffer_size  "
                            << "(" << its_error.value() << "): " << its_error.message() << ", " << name_;
            break;
        }

        const auto send_buffer_size = send_buffer_size_cmd.get();
        if (send_buffer_size > 0) {
            if (_force) {
                VSOMEIP_WARNING << "lsti::" << __func__ << ": forced stop on socket, not waiting to send remaining " << send_buffer_size
                                << " bytes, " << name_;
                break;
            } else {
                VSOMEIP_WARNING << "lsti::" << __func__ << ": waiting[" << retry_count << "] on close to send " << send_buffer_size
                                << " bytes, " << name_;
                std::this_thread::sleep_for(std::chrono::milliseconds(VSOMEIP_TCP_CLOSE_SEND_BUFFER_CHECK_PERIOD));
            }
        } else {
            break;
        }
        ++retry_count;
        if (retry_count > VSOMEIP_TCP_CLOSE_SEND_BUFFER_RETRIES) {
            VSOMEIP_ERROR << "lsti::" << __func__ << ": max retries reached to send! will drop " << send_buffer_size << " bytes on close, "
                          << name_;
            break;
        }
    }
#endif
    std::scoped_lock const lock{socket_mtx_};
    if (socket_->is_open()) {
        boost::system::error_code ec;
        socket_->shutdown(tcp_socket::shutdown_both, ec);
        if (ec) {
            VSOMEIP_WARNING << "lsti::shutdown: " << ec.message() << ", " << name_;
        }
        socket_->close(ec);
        if (ec) {
            VSOMEIP_ERROR << "lsti::close: " << ec.message() << ", " << name_;
        }
    } else {
        VSOMEIP_WARNING << "lsti::" << __func__ << ": socket was not open, " << name_;
    }
}

void local_socket_tcp_impl::prepare_connect(configuration const& _configuration, boost::system::error_code& _ec) {
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
    set_socket_options(_configuration);
    socket_->bind(own_endpoint_, _ec);
}

// to be unaware of the protocol, no endpoint definition is passed in
void local_socket_tcp_impl::async_connect(connect_handler _handler) {
    std::unique_lock lock{socket_mtx_};
    if (socket_->is_open()) {
        socket_->async_connect(peer_endpoint_, std::move(_handler));
    } else {
        boost::asio::post(io_context_, [h = std::move(_handler)] { h(boost::asio::error::fault); });
    }
}
void local_socket_tcp_impl::async_receive(boost::asio::mutable_buffer _buffer, read_handler _r_handler) {
    std::unique_lock lock{socket_mtx_};
    if (socket_->is_open()) {

        socket_->async_receive(_buffer, [handler = std::move(_r_handler), weak_self = weak_from_this()](auto const& _ec, auto bytes) {
#if defined(__linux__)
            // yes, this needs to happen after every read
            if (!_ec) {
                if (auto self = weak_self.lock(); self) {
                    std::scoped_lock const inner_lock{self->socket_mtx_};
                    // set TCP QUICKACK
                    // necessary, because local connections are TCP RST'd, and in order to guarantee no
                    // loss of data, there is (active!) waiting for TCP ACKs - which relies on speedy delivery of TCP ACKs
                    if (!self->socket_->set_quick_ack()) {
                        VSOMEIP_WARNING << "ltsei::receive_cbk: could not setsockopt(TCP_QUICKACK), errno " << errno;
                    }
                }
            }
#endif
            handler(_ec, bytes);
        });
    } else {
        boost::asio::post(io_context_, [h = std::move(_r_handler)] { h(boost::asio::error::fault, 0); });
    }
}
void local_socket_tcp_impl::async_send(std::vector<uint8_t> _data, write_handler _w_handle) {
    std::unique_lock lock{socket_mtx_};
    if (socket_->is_open()) {
        // ensure the memory is kept alive as long as the callback hasn't been invoked
        auto buffer = boost::asio::buffer(_data);
        socket_->async_write({buffer}, [d = std::move(_data), handler = std::move(_w_handle)](auto const& _ec, size_t _bytes) {
            handler(_ec, _bytes, std::move(d));
        });
    } else {
        boost::asio::post(io_context_, [h = std::move(_w_handle), d = std::move(_data)] { h(boost::asio::error::fault, 0, std::move(d)); });
    }
}

std::string const& local_socket_tcp_impl::to_string() const {
    return name_;
}

bool local_socket_tcp_impl::update(vsomeip_sec_client_t& _client, configuration const& _configuration) {
    auto address = peer_endpoint_.address();
    auto port = peer_endpoint_.port();

    if (address.is_v4()) {
        _client.host = htonl(uint32_t(address.to_v4().to_uint()));
    }
    if (role_ == socket_role_e::SENDER) {
        // temporary hack, because this used to be called only for incoming (not outgoing!)
        // tcp connections, and security lib does not have configuration for _some_ outgoing
        // connections
        port += 1;
    }
    _client.port = htons(port);

    _configuration.get_security()->sync_client(&_client);
    return true;
}
port_t local_socket_tcp_impl::own_port() const {
    return own_endpoint_.port();
}
boost::asio::ip::tcp::endpoint local_socket_tcp_impl::peer_endpoint() const {
    return peer_endpoint_;
}

void local_socket_tcp_impl::set_socket_options(configuration const& _configuration) {
    boost::system::error_code ec;
    // force always TCP RST on close/shutdown, in order to:
    // 1) avoid issues with TIME_WAIT, which otherwise lasts for 120 secs with a
    // non-responding endpoint (see also 4396812d2)
    // 2) handle by default what needs to happen at suspend/shutdown
    socket_->set_option(boost::asio::socket_base::linger(true, 0), ec);
    if (ec) {
        VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(SO_LINGER), " << ec.message() << ", " << name_;
    }
    socket_->set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(SO_REUSEADDR), " << ec.message() << ", " << name_;
    }
    // Nagle algorithm off
    socket_->set_option(boost::asio::ip::tcp::no_delay(true), ec);
    if (ec) {
        VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(SO_TCP_NODELAY), " << ec.message() << ", " << name_;
    }

    // connection in the same host (and not across a host and guest or similar)
    // important, as we can (MUST!) be lax in the socket options - no keep alive necessary
    if (own_endpoint_.address() == peer_endpoint_.address()) {
        // disable keep alive
        socket_->set_option(boost::asio::socket_base::keep_alive(false), ec);
        if (ec) {
            VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(SO_KEEPALIVE) to false, " << ec.message() << ", " << name_;
        }
    } else {
        // enable keep alive
        socket_->set_option(boost::asio::socket_base::keep_alive(true), ec);
        if (ec) {
            VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(SO_KEEPALIVE) to true, " << ec.message() << ", " << name_;
        }

#if defined(__linux__) || defined(ANDROID)
        // set a user timeout
        // along the keep alives, this ensures connection closes if endpoint is unreachable
        if (!socket_->set_user_timeout(_configuration.get_local_tcp_user_timeout())) {
            VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(TCP_USER_TIMEOUT), errno " << errno << ", " << name_;
        }

        // override kernel settings
        // unfortunate, but there are plenty of custom keep-alive settings, and need to
        // enforce some sanity here
        if (!socket_->set_keepidle(_configuration.get_local_tcp_keepidle())) {
            VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(TCP_KEEPIDLE), errno " << errno << ", " << name_;
        }
        if (!socket_->set_keepintvl(_configuration.get_local_tcp_keepintvl())) {
            VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(TCP_KEEPINTVL), errno " << errno << ", " << name_;
        }
        if (!socket_->set_keepcnt(_configuration.get_local_tcp_keepcnt())) {
            VSOMEIP_WARNING << "lsti::" << __func__ << ": could not setsockopt(TCP_KEEPCNT), errno " << errno << ", " << name_;
        }
#endif
    }
}
} // namespace vsomeip_v3

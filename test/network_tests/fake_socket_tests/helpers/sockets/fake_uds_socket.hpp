// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../implementation/endpoints/include/uds_acceptor.hpp"
#include "../../../implementation/endpoints/include/uds_socket.hpp"

#include "fake_tcp_socket_handle.hpp"

#include <mutex>

namespace vsomeip_v3::testing {

struct fake_tcp_acceptor_handle;

class fake_uds_socket : public uds_socket {
public:
    explicit fake_uds_socket(std::shared_ptr<fake_tcp_socket_handle> _state) : state_(std::move(_state)) { }

private:
    [[nodiscard]] bool is_open() const { return state_->is_open(); }
    void open([[maybe_unused]] endpoint::protocol_type _type, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->open();
    }

    void close(boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->close();
    }

    void cancel(boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->cancel();
    }

    void set_reuse_address(boost::system::error_code& _ec) override { _ec = boost::system::error_code(); }
    [[nodiscard]] bool get_peer_credentials([[maybe_unused]] vsomeip_sec_client_t& _client) override {
        // TODO
        return true;
    }

    void async_connect(endpoint const& _ep, connect_handler _handler) override { state_->connect(_ep, std::move(_handler)); }
    void async_receive(boost::asio::mutable_buffer _buffer, rw_handler _handler) {
        state_->async_receive(std::move(_buffer), std::move(_handler));
    }
    void async_write(boost::asio::const_buffer const& _buffer, rw_handler _handler) { state_->write({_buffer}, std::move(_handler)); }

    friend struct fake_tcp_acceptor_handle;
    friend struct fake_uds_acceptor;
    std::shared_ptr<fake_tcp_socket_handle> state_;
};
struct fake_uds_acceptor : public uds_acceptor {
public:
    explicit fake_uds_acceptor(std::shared_ptr<fake_tcp_acceptor_handle> _state) : state_(std::move(_state)) { }

    ~fake_uds_acceptor() = default;

private:
    [[nodiscard]] bool is_open() const override { return state_->is_open(); }
    void assign([[maybe_unused]] endpoint::protocol_type _pt, [[maybe_unused]] int _fd,
                [[maybe_unused]] boost::system::error_code& _ec) override {
        throw std::logic_error("fake socket tests do not support assignment of fds");
    }
    void open([[maybe_unused]] endpoint::protocol_type _pt, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->open();
    }

    void bind(endpoint const& _ep, boost::system::error_code& _ec) override {
        if (!state_->bind(_ep)) {
            _ec = boost::asio::error::address_in_use;
            return;
        }
        _ec = boost::system::error_code();
    }
    void listen(int, boost::system::error_code& _ec) override { _ec = boost::system::error_code(); }
    [[nodiscard]] virtual bool unlink([[maybe_unused]] char const* _path) { return true; }

    void close(boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->close();
    }
    void cancel(boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->cancel();
    }

    void set_reuse_address(boost::system::error_code& _ec) override { _ec = boost::system::error_code(); }

    // In the fake, the remote_uds_ep_ is already set on the socket by the time the handler fires
    // (add_connection() runs synchronously before posting the handler), so reading it from
    // the socket handle is always valid.
    void async_accept(uds_socket& _socket, endpoint& _peer_ep, connect_handler _handler) override {
        auto* socket_impl = dynamic_cast<fake_uds_socket*>(&_socket);
        state_->async_accept(_socket, [socket_impl, &_peer_ep, h = std::move(_handler)](boost::system::error_code ec) mutable {
            if (!ec && socket_impl) {
                _peer_ep = socket_impl->state_->remote_uds_endpoint();
            }
            h(ec);
        });
    }
    std::shared_ptr<fake_tcp_acceptor_handle> state_;
};
}

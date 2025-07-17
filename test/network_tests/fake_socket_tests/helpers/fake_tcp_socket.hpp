// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TESTING_FAKE_TCP_SOCKET_HPP_
#define VSOMEIP_V3_TESTING_FAKE_TCP_SOCKET_HPP_

#include "../../../implementation/endpoints/include/tcp_socket.hpp"

#include "fake_tcp_socket_handle.hpp"

#include <mutex>

namespace vsomeip_v3::testing {

class fake_tcp_acceptor_handle;

/**
 * Test wrapper around the tcp_socket inteface that forwards each call
 * to the owned fake_tcp_socket_handle - except for socket_options which are
 * effectively ignored for now. Notice that the socket itself
 * might go out of scope while the socket_handler might outlive the socket,
 * because the socket_manager has a weak_reference to this handle that might be locked,
 * while the socket might go out of scope. For this reason the socket_handle
 * has no notion of the owning fake_tcp_socket class.
 **/
class fake_tcp_socket : public tcp_socket {
public:
    explicit fake_tcp_socket(std::shared_ptr<fake_tcp_socket_handle> _state) :
        state_(std::move(_state)) { }

private:
    [[nodiscard]] bool is_open() const { return state_->is_open(); }

    [[nodiscard]] virtual int native_handle() {
        // this function should not be called within a test execution. Otherwise a proper
        // indirection is missing. Because this function is only required to be called when using
        // native system calls, something that should be avoided when using fake sockets
        throw std::runtime_error("native_handle is not allowed to be called within a test. A "
                                 "proper abstraction is missing");
        return -1;
    }

    virtual void open(boost::asio::ip::tcp::endpoint::protocol_type _type,
                      boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        state_->open(_type);
    }

    virtual void bind(boost::asio::ip::tcp::endpoint const& _ep,
                      boost::system::error_code& _ec) override {
        if (!state_->bind(_ep)) {
            _ec = boost::asio::error::address_in_use;
            return;
        }
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
    }

    virtual void close(boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        state_->close();
    }

    virtual void cancel(boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        state_->cancel();
    }

    virtual void shutdown(boost::asio::ip::tcp::socket::shutdown_type,
                          boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        state_->shutdown();
    }

    virtual boost::asio::ip::tcp::endpoint
    local_endpoint(boost::system::error_code& _ec) const override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        return state_->local_endpoint();
    }

    virtual boost::asio::ip::tcp::endpoint
    remote_endpoint(boost::system::error_code& _ec) const override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        return state_->remote_endpoint();
    }

    virtual void set_option(boost::asio::ip::tcp::no_delay,
                            boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        set_no_delay_ = true;
    }

    virtual void set_option(boost::asio::ip::tcp::socket::keep_alive,
                            boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        set_keep_alive_ = true;
    }

    virtual void set_option(boost::asio::ip::tcp::socket::linger,
                            boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        set_linger_ = true;
    }

    virtual void set_option(boost::asio::ip::tcp::socket::reuse_address,
                            boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        set_reuse_address_ = true;
    }

#if defined(__linux__) || defined(ANDROID)
    [[nodiscard]] virtual bool set_user_timeout(unsigned int _timeout) {
        set_user_timeout_ = _timeout;
        return true;
    }
#endif

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
    [[nodiscard]] virtual bool bind_to_device(std::string const& _device) {
        bound_device_ = _device;
        return true;
    }

    [[nodiscard]] bool can_read_fd_flags() override { return true; }
#endif

    virtual void async_connect(boost::asio::ip::tcp::endpoint const& _ep,
                               connect_handler handler) override {
        state_->connect(_ep, std::move(handler));
    }

    virtual void async_receive(boost::asio::mutable_buffer _buffer, rw_handler _handler) override {
        state_->async_receive(std::move(_buffer), std::move(_handler));
    }

    virtual void async_write(std::vector<boost::asio::const_buffer> const& _buffer,
                             rw_handler _handler) override {
        state_->write(_buffer, std::move(_handler));
    }

    virtual void async_write(boost::asio::const_buffer const&, completion_condition,
                             rw_handler) override { }

    friend class fake_tcp_acceptor_handle;
    std::shared_ptr<fake_tcp_socket_handle> state_;
    bool set_no_delay_ {false};
    bool set_keep_alive_ {false};
    bool set_linger_ {false};
    bool set_reuse_address_ {false};
    std::optional<unsigned int> set_user_timeout_;
    std::optional<std::string> bound_device_;
};

/**
 * Test wrapper around the tcp_acceptor inteface that forwards each call
 * to the owned fake_tcp_acceptor_handle. Notice that the acceptor itself
 * might go out of scope while the acceptor_handler might outlive the acceptor,
 * because the socket_manager has a weak_reference to this handle that might be locked,
 * while the acceptor might go out of scope. For this reason the acceptor_handle
 * has no notion of the owning fake_tcp_acceptor class.
 **/
class fake_tcp_acceptor : public tcp_acceptor {
public:
    explicit fake_tcp_acceptor(std::shared_ptr<fake_tcp_acceptor_handle> _state) :
        state_(std::move(_state)) { }

    ~fake_tcp_acceptor() = default;

private:
    [[nodiscard]] virtual bool is_open() const override { return state_->is_open(); }
    [[nodiscard]] virtual int native_handle() override {
        // this function should not be called within a test execution. Otherwise a proper
        // indirection is missing. Because this function is only required to be called when using
        // native system calls, something that should be avoided when using fake sockets
        throw std::runtime_error("native_handle is not allowed to be called within a test. A "
                                 "proper abstraction is missing");
        return -1;
    }

    virtual void open(boost::asio::ip::tcp::endpoint::protocol_type,
                      boost::system::error_code& _ec) override {

        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        state_->open();
    }
    virtual void bind(boost::asio::ip::tcp::endpoint const& _ep,
                      boost::system::error_code& _ec) override {
        if (!state_->bind(_ep)) {
            _ec = boost::asio::error::address_in_use;
            return;
        }
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
    }

    virtual void close(boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
        state_->close();
    }
    virtual void set_option(boost::asio::ip::tcp::socket::reuse_address,
                            boost::system::error_code& _ec) override {

        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
    }

#if defined(__linux__) || defined(ANDROID)
    [[nodiscard]] virtual bool set_native_option_free_bind() override { return true; }
#endif

    virtual void listen(int, boost::system::error_code& _ec) override {
        _ec = boost::system::errc::make_error_code(boost::system::errc::success);
    }

    virtual void async_accept(tcp_socket& socket, connect_handler handler) override {
        state_->async_accept(socket, std::move(handler));
    }

    std::shared_ptr<fake_tcp_acceptor_handle> state_;
};

}
#endif

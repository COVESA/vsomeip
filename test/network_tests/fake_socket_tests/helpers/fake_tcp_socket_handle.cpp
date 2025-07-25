// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "fake_tcp_socket_handle.hpp"
#include "socket_manager.hpp"
#include "fake_tcp_socket.hpp"
#include "test_logging.hpp"

#include <span>
#include <thread>
#include <numeric>

namespace vsomeip_v3::testing {

char const* to_string(socket_role role) {
    switch (role) {
    case socket_role::sender:
        return "sender";
    case socket_role::receiver:
        return "receiver";
    default:
        return "unspecified";
    }
}

std::ostream& operator<<(std::ostream& o, socket_id const& _id) {
    return o << "{fd: " << _id.fd_ << ", role: " << to_string(_id.role_)
             << ", app: " << _id.app_name_ << "}";
}

fake_tcp_socket_handle::fake_tcp_socket_handle(boost::asio::io_context& _io) : io_(_io) { }

fake_tcp_socket_handle::~fake_tcp_socket_handle() {
    TEST_LOG << "[fake-socket] Deleting: " << socket_id_;
}

void fake_tcp_socket_handle::init(fd_t _fd, std::weak_ptr<socket_manager> _sm) {
    auto const lock = std::scoped_lock(mtx_);
    socket_id_.fd_ = _fd;
    socket_manager_ = _sm;
}

void fake_tcp_socket_handle::cancel() {
    close();
}

[[nodiscard]] bool fake_tcp_socket_handle::is_open() {
    auto const lock = std::scoped_lock(mtx_);
    return static_cast<bool>(protocol_type_);
}

void fake_tcp_socket_handle::open(boost::asio::ip::tcp::endpoint::protocol_type _type) {
    auto const lock = std::scoped_lock(mtx_);
    protocol_type_ = _type;
}

void fake_tcp_socket_handle::close() {
    TEST_LOG << "[fake-socket] calling close on: " << socket_id_;
    auto remote = [&]() -> std::shared_ptr<fake_tcp_socket_handle> {
        auto const lock = std::scoped_lock(mtx_);
        auto remote = connected_socket_.lock();
        connected_socket_ = {};
        protocol_type_ = std::nullopt;
        if (receptor_) {
            TEST_LOG << "[fake-socket] posting operation_aborted on: " << socket_id_;
            boost::asio::post(io_, [handler = std::move(receptor_->handler_)] {
                handler(boost::asio::error::operation_aborted, 0);
            });
            receptor_ = std::nullopt;
        }
        return remote;
    }();
    if (remote) {
        remote->inner_close();
    }
    auto block_time = [&] {
        auto const lock = std::scoped_lock(mtx_);
        return block_on_close_time_;
    }();
    if (block_time) {
        TEST_LOG << "[fake-socket] delaying close procesesing for: " << socket_id_
                 << " by: " << block_time->count() << "ms";
        std::this_thread::sleep_for(*block_time);
        TEST_LOG << "[fake-socket] continuing close procesesing for: " << socket_id_;
    }
}

void fake_tcp_socket_handle::shutdown() {
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] calling shutdown on: " << socket_id_;
    if (receptor_) {
        receptor_ = std::nullopt;
    }
}

[[nodiscard]] bool fake_tcp_socket_handle::bind(boost::asio::ip::tcp::endpoint const& _ep) {
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] calling bind on: " << socket_id_ << " with: " << _ep;
    local_ep_ = _ep;
    return true;
}

boost::asio::ip::tcp::endpoint fake_tcp_socket_handle::local_endpoint() {
    auto const lock = std::scoped_lock(mtx_);
    return local_ep_;
}

boost::asio::ip::tcp::endpoint fake_tcp_socket_handle::remote_endpoint() {
    auto const lock = std::scoped_lock(mtx_);
    return remote_ep_;
}

bool fake_tcp_socket_handle::disconnect(std::optional<boost::system::error_code> _ec) {
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] calling disconnect on: " << socket_id_
             << " with: " << (_ec ? _ec->message() : "nullopt");
    connected_socket_ = {};
    if (!_ec) {
        return true;
    }
    if (!receptor_) {
        TEST_LOG << "[fake-socket] Error on disconnect on: " << socket_id_ << " no receptor set";
        return false;
    }
    boost::asio::post(io_,
                      [ec = *_ec, handler = std::move(receptor_->handler_)] { handler(ec, 0); });
    receptor_ = std::nullopt;
    return true;
}
void fake_tcp_socket_handle::delay_processing(bool _delay) {
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] setting delay_processing: " << (_delay ? "true" : "false")
             << " on: " << socket_id_;
    delay_processing_ = _delay;
    update_reception();
}

void fake_tcp_socket_handle::block_on_close_for(
        std::optional<std::chrono::milliseconds> _block_time) {
    auto const lock = std::scoped_lock(mtx_);
    block_on_close_time_ = _block_time;
}

void fake_tcp_socket_handle::inner_close() {
    // called by the remote connected socket
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] calling inner_close on: " << socket_id_;
    if (receptor_) {
        // this has the potential to leak, as connected to sockets are not necessarily
        // managed, and could be implicitly deleted by the io_context in production
        // code, but within the test this "only" means that the io_context went
        // out of scope :/.
        boost::asio::post(io_, [handler = std::move(receptor_->handler_)] {
            handler(boost::asio::error::connection_reset, 0);
        });
        receptor_ = std::nullopt;
    }
    connected_socket_ = {};
}

void fake_tcp_socket_handle::connect(boost::asio::ip::tcp::endpoint const& _ep,
                                     connect_handler _handler) {
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();

    if (sm) {
        sm->connect(_ep, *this, [this, h = std::move(_handler)](auto ec) {
            boost::asio::post(io_, [handler = std::move(h), ec] { handler(ec); });
        });
        return;
    }
    boost::asio::post(io_, [handler = std::move(_handler)] {
        // TODO correct error code?
        handler(boost::system::errc::make_error_code(boost::system::errc::host_unreachable));
    });
}

void fake_tcp_socket_handle::clear_handler() {
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] Clearing the handler for: " << socket_id_;
    receptor_ = std::nullopt;
}

[[nodiscard]] bool fake_tcp_socket_handle::add_connection(fake_tcp_socket_handle& _connecting) {
    // to avoid race-conditions + dead-locks, ensure that both sockets are not altered for the time
    // being. (if one lock is acquired in the stack before acquiring the second lock later, the
    // inverse locking might lead to a dead-lock)
    auto const lock = std::scoped_lock(mtx_, _connecting.mtx_);
    if (auto connected = connected_socket_.lock(); connected) {
        return false;
    }

    connected_socket_ = _connecting.weak_from_this();
    remote_ep_ = _connecting.local_ep_;
    protocol_type_ = _connecting.protocol_type_;
    socket_id_.role_ = socket_role::receiver;
    local_ep_ = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), socket_id_.fd_);

    _connecting.connected_socket_ = weak_from_this();
    _connecting.remote_ep_ = local_ep_;
    _connecting.socket_id_.role_ = socket_role::sender;

    TEST_LOG << "[fake-socket] Established connection: " << _connecting.socket_id_ << " -> "
             << socket_id_;
    return true;
}

[[nodiscard]] bool fake_tcp_socket_handle::is_connected(std::weak_ptr<fake_tcp_socket_handle> _to) {
    auto to = _to.lock();
    if (!to) {
        return false;
    }
    auto const lock = std::scoped_lock(mtx_, to->mtx_);
    auto to_connected = to->connected_socket_.lock();
    if (!to_connected) {
        return false;
    }
    auto connected_socket = connected_socket_.lock();
    if (!connected_socket) {
        return false;
    }
    return to_connected.get() == this && connected_socket.get() == to.get();
}

void fake_tcp_socket_handle::write(std::vector<boost::asio::const_buffer> const& _buffer,
                                   rw_handler _handler) {
    auto receiver = [&]() -> std::shared_ptr<fake_tcp_socket_handle> {
        auto const lock = std::scoped_lock(mtx_);
        return connected_socket_.lock();
    }();

    if (receiver) {
        size_t size = receiver->consume(_buffer);
        boost::asio::post(io_, [size, handler = std::move(_handler)] {
            handler(boost::system::errc::make_error_code(boost::system::errc::success), size);
        });
        return;
    }
    boost::asio::post(io_, [handler = std::move(_handler)] {
        if (!handler) {
            return;
        }
        handler(boost::system::errc::make_error_code(boost::system::errc::broken_pipe), 0);
    });
}

void fake_tcp_socket_handle::async_receive(boost::asio::mutable_buffer _buffer,
                                           rw_handler _handler) {
    auto const lock = std::scoped_lock(mtx_);
    receptor_ = Receptor {std::move(_buffer), std::move(_handler)};
    update_reception();
}

size_t fake_tcp_socket_handle::consume(std::vector<boost::asio::const_buffer> const& _buffer) {
    size_t const incoming_size =
            std::accumulate(_buffer.begin(), _buffer.end(), 0,
                            [](auto last, auto const& bf) { return last + bf.size(); });
    auto const lock = std::scoped_lock(mtx_);
    input_data_.reserve(input_data_.size() + incoming_size);
    for (auto const& buffer : _buffer) {
        auto first = static_cast<const char*>(buffer.data());
        auto const last = first + buffer.size();
        for (; first != last; ++first) {
            input_data_.push_back(*first);
        }
    }

    update_reception();
    return incoming_size;
}

void fake_tcp_socket_handle::update_reception() {
    if (!receptor_ || delay_processing_) {
        return;
    }
    auto const len = std::min(receptor_->buffer_.size(), input_data_.size());
    if (len == 0) {
        return;
    }
    if (receptor_->buffer_.size() < input_data_.size()) {
        TEST_LOG << "[fake-socket] Input data too much for buffer, chopping input for: "
                 << socket_id_ << "(r: " << input_data_.size()
                 << " bytes, buffer_size: " << receptor_->buffer_.size() << " bytes)";
    }

    char* out = static_cast<char*>(receptor_->buffer_.data());
    char* end = out + len;
    for (auto it = input_data_.begin(); out != end; ++out) {
        *out = *it;
        ++it;
    }
    input_data_.erase(input_data_.begin(), input_data_.begin() + len);
    boost::asio::post(io_, [handler = std::move(receptor_->handler_), len] {
        if (!handler) {
            return;
        }
        handler(boost::system::errc::make_error_code(boost::system::errc::success), len);
    });
    receptor_ = std::nullopt;
}

void fake_tcp_socket_handle::set_app_name(std::string const& _name) {
    auto const lock = std::scoped_lock(mtx_);
    socket_id_.app_name_ = _name;
}

std::string fake_tcp_socket_handle::get_app_name() {
    auto const lock = std::scoped_lock(mtx_);
    return socket_id_.app_name_;
}

fd_t fake_tcp_socket_handle::fd() {
    auto const lock = std::scoped_lock(mtx_);
    return socket_id_.fd_;
}

fake_tcp_acceptor_handle::fake_tcp_acceptor_handle(boost::asio::io_context& _io) : io_(_io) { }

fake_tcp_acceptor_handle::~fake_tcp_acceptor_handle() {
    TEST_LOG << "[fake-acceptor] Deleting fd: " << fd_;
}

void fake_tcp_acceptor_handle::init(fd_t _fd, std::weak_ptr<socket_manager> _sm) {
    auto const lock = std::scoped_lock(mtx_);
    fd_ = _fd;
    socket_manager_ = _sm;
}

[[nodiscard]] bool fake_tcp_acceptor_handle::bind(boost::asio::ip::tcp::endpoint const& ep) {
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();
    if (!sm) {
        return false;
    }
    return sm->bind_acceptor(ep, weak_from_this());
}

void fake_tcp_acceptor_handle::open() {
    auto const lock = std::scoped_lock(mtx_);
    is_open_ = true;
}

void fake_tcp_acceptor_handle::close() {
    auto const lock = std::scoped_lock(mtx_);
    is_open_ = false;
    if (connection_) {
        connection_ = std::nullopt;
    }
}

[[nodiscard]] bool fake_tcp_acceptor_handle::is_open() {
    auto const lock = std::scoped_lock(mtx_);
    return is_open_;
}

void fake_tcp_acceptor_handle::async_accept(tcp_socket& _socket, connect_handler _handler) {
    auto const lock = std::scoped_lock(mtx_);
    auto* fake_socket = dynamic_cast<fake_tcp_socket*>(&_socket);
    if (!fake_socket) {
        boost::asio::post(io_, [handler = std::move(_handler)] {
            handler(boost::system::errc::make_error_code(boost::system::errc::invalid_argument));
        });
        return;
    }
    TEST_LOG << "[fake-acceptor] fd: " << fd_
             << ", is awaiting connections with fd: " << fake_socket->state_->fd();
    connection_ = connection {fake_socket->state_, std::move(_handler)};
    if (auto const sm = socket_manager_.lock(); sm) {
        sm->awaiting();
    }
}

[[nodiscard]] std::shared_ptr<fake_tcp_socket_handle>
fake_tcp_acceptor_handle::connect(fake_tcp_socket_handle& _state, connect_handler _handler) {
    // because the socket_handle will never call the acceptor there is no risk of a dead-lock,
    // in case the mutex of the acceptor is hold while invoking methods from a socket_handle.
    auto const lock = std::scoped_lock(mtx_);
    if (!connection_) {
        _handler(boost::system::errc::make_error_code(boost::system::errc::host_unreachable));
        return nullptr;
    }
    if (auto accepting_socket = connection_->socket_.lock(); accepting_socket) {
        if (!accepting_socket->add_connection(_state)) {
            _handler(boost::system::errc::make_error_code(boost::system::errc::host_unreachable));
            return nullptr;
        }
        boost::asio::post(io_, [handler = std::move(connection_->handler_)] {
            handler(boost::system::errc::make_error_code(boost::system::errc::success));
        });
        _handler(boost::system::errc::make_error_code(boost::system::errc::success));
        connection_ = std::nullopt;
        return accepting_socket;
    }
    _handler(boost::system::errc::make_error_code(boost::system::errc::host_unreachable));
    return nullptr;
}

void fake_tcp_acceptor_handle::set_app_name(std::string const& _name) {
    auto const lock = std::scoped_lock(mtx_);
    app_name_ = _name;
}

std::string fake_tcp_acceptor_handle::get_app_name() {
    auto const lock = std::scoped_lock(mtx_);
    return app_name_;
}

[[nodiscard]] bool fake_tcp_acceptor_handle::is_awaiting_connection() {
    auto const lock = std::scoped_lock(mtx_);
    return static_cast<bool>(connection_);
}
}

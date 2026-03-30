// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "fake_tcp_socket_handle.hpp"
#include "../socket_manager.hpp"
#include "fake_tcp_socket.hpp"
#include "fake_uds_socket.hpp"
#include "../test_logging.hpp"

#include <thread>
#include <numeric>

namespace vsomeip_v3::testing {

static char const* to_string(socket_role role) {
    switch (role) {
    case socket_role::client:
        return "client";
    case socket_role::server:
        return "server";
    default:
        return "unspecified";
    }
}
static char const* to_string(socket_type _type) {
    switch (_type) {
    case socket_type::tcp:
        return "tcp";
    case socket_type::uds:
        return "uds";
    default:
        return "unspecified";
    }
}

std::ostream& operator<<(std::ostream& o, socket_id const& _id) {
    return o << "{fd: " << _id.fd_ << ", type: " << to_string(_id.type_) << ", role: " << to_string(_id.role_) << ", app: " << _id.app_name_
             << "}";
}

fake_tcp_socket_handle::fake_tcp_socket_handle(boost::asio::io_context& _io) : io_(_io) { }

fake_tcp_socket_handle::~fake_tcp_socket_handle() {
    TEST_LOG << "[fake-socket] Deleting: " << socket_id_;
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();
    if (!sm) {
        return;
    }
    sm->remove(socket_id_.fd_);
}

void fake_tcp_socket_handle::init(fd_t _fd, socket_type _type, std::weak_ptr<socket_manager> _sm) {
    auto const lock = std::scoped_lock(mtx_);
    socket_id_.fd_ = _fd;
    socket_id_.type_ = _type;
    socket_manager_ = _sm;
}

void fake_tcp_socket_handle::cancel() {
    TEST_LOG << "[fake-socket] calling cancel on: " << socket_id_;
    auto remote = [&]() -> std::shared_ptr<fake_tcp_socket_handle> {
        auto const lock = std::scoped_lock(mtx_);
        auto remote = connected_socket_.lock();
        connected_socket_ = {};
        is_open_ = false;
        if (receptor_) {
            TEST_LOG << "[fake-socket] posting operation_aborted on: " << socket_id_;
            boost::asio::post(io_, [handler = std::move(receptor_->handler_)] { handler(boost::asio::error::operation_aborted, 0); });
            receptor_ = std::nullopt;
        }
        return remote;
    }();
    if (remote) {
        remote->inner_close();
        auto sm = [this] {
            auto const lock = std::scoped_lock(mtx_);
            return socket_manager_.lock();
        }();
        sm->close_connection(get_app_name(), remote->get_app_name(), socket_id_.role_);
    }
}

[[nodiscard]] bool fake_tcp_socket_handle::is_open() {
    auto const lock = std::scoped_lock(mtx_);
    return is_open_;
}

void fake_tcp_socket_handle::open() {
    auto const lock = std::scoped_lock(mtx_);
    is_open_ = true;
}

void fake_tcp_socket_handle::close() {
    cancel();
    TEST_LOG << "[fake-socket] calling close on: " << socket_id_;
    auto block_time = [&] {
        auto const lock = std::scoped_lock(mtx_);
        return block_on_close_time_;
    }();
    if (block_time) {
        TEST_LOG << "[fake-socket] delaying close processing for: " << socket_id_ << " by: " << block_time->count() << "ms";
        std::this_thread::sleep_for(*block_time);
        TEST_LOG << "[fake-socket] continuing close processing for: " << socket_id_;
    }
}

[[nodiscard]] bool fake_tcp_socket_handle::bind(boost::asio::ip::tcp::endpoint const& _ep) {
    TEST_LOG << "[fake-socket] calling bind on: " << socket_id_ << " with: " << _ep;
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();
    if (sm) {
        if (sm->bind_socket(*this, _ep, socket_id_.fd_)) {
            auto const lock = std::scoped_lock(mtx_);
            local_ep_ = _ep;
            return true;
        }
    }
    return false;
}

boost::asio::ip::tcp::endpoint fake_tcp_socket_handle::local_endpoint() {
    auto const lock = std::scoped_lock(mtx_);
    return local_ep_;
}

boost::asio::ip::tcp::endpoint fake_tcp_socket_handle::remote_endpoint() {
    auto const lock = std::scoped_lock(mtx_);
    return remote_ep_;
}

uds_endpoint fake_tcp_socket_handle::remote_uds_endpoint() {
    auto const lock = std::scoped_lock(mtx_);
    return remote_uds_ep_;
}

void fake_tcp_socket_handle::disconnect(std::optional<boost::system::error_code> _ec) {
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] calling disconnect on: " << socket_id_ << " with: " << (_ec ? _ec->message() : "nullopt");
    connected_socket_ = {};
    if (!_ec) {
        return;
    }
    if (!receptor_) {
        TEST_LOG << "[fake-socket] Error on disconnect on: " << socket_id_ << " no receptor set, stashing the error: " << _ec->message();
        stashed_ec_ = _ec;
        return;
    }
    boost::asio::post(io_, [ec = *_ec, handler = std::move(receptor_->handler_)] { handler(ec, 0); });
    receptor_ = std::nullopt;
    return;
}
void fake_tcp_socket_handle::delay_processing(bool _delay) {
    auto const lock = std::scoped_lock(mtx_);
    if (delay_processing_ != _delay) {
        TEST_LOG << "[fake-socket] setting delay_processing: " << (_delay ? "true" : "false") << " on: " << socket_id_;
        delay_processing_ = _delay;
        update_reception();
    }
}

void fake_tcp_socket_handle::set_ignore_nothing_to_read_from(bool _ignore) {
    auto const lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] setting ignore_nothing_to_read_from: " << (_ignore ? "true" : "false") << " on: " << socket_id_;
    ignore_nothing_to_read_from_ = _ignore;
    if (auto remote = connected_socket_.lock(); !remote && !ignore_nothing_to_read_from_ && receptor_) {
        TEST_LOG << "[fake-socket] Error on: " << socket_id_ << ", no connection to read from";
        boost::asio::post(io_, [handler = std::move(receptor_->handler_)] { handler(boost::asio::error::connection_reset, 0); });
        // safe to do under the lock, because the handler is already empty and will be destructed after being called on one of the io
        // threads.
        receptor_ = std::nullopt;
        return;
    }
}

void fake_tcp_socket_handle::block_on_close_for(std::optional<std::chrono::milliseconds> _block_time) {
    auto const lock = std::scoped_lock(mtx_);
    block_on_close_time_ = _block_time;
}

void fake_tcp_socket_handle::inner_close() {
    // called by the remote connected socket
    auto lock = std::scoped_lock(mtx_);
    TEST_LOG << "[fake-socket] calling inner_close on: " << socket_id_;
    // the connected_socket_ can be reset even when ignoring the call,
    // as the inner_close call implies the other socket tries to clean itself up
    // (any usage through connected_socket_ could only work via a race condition
    // and should be avoided).
    connected_socket_ = {};
    if (ignore_inner_close_) {
        TEST_LOG << "[fake-socket] inner_close has no effect on: " << socket_id_;
        return;
    }
    if (receptor_) {
        // this has the potential to leak, as connected to sockets are not necessarily
        // managed, and could be implicitly deleted by the io_context in production
        // code, but within the test this "only" means that the io_context went
        // out of scope :/.
        boost::asio::post(io_, [handler = std::move(receptor_->handler_)] { handler(boost::asio::error::connection_reset, 0); });
        receptor_ = std::nullopt;
    } else {
        TEST_LOG << "[fake-socket] WARNING: calling inner_close on: " << socket_id_ << " could not forward the connection_reset";
    }
}

void fake_tcp_socket_handle::connect(boost::asio::ip::tcp::endpoint const& _ep, connect_handler _handler) {
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();

    if (sm) {
        sm->connect(_ep, *this,
                    [this, h = std::move(_handler)](auto ec) { boost::asio::post(io_, [handler = std::move(h), ec] { handler(ec); }); });
        return;
    }
    boost::asio::post(
            io_, [handler = std::move(_handler)] { handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable)); });
}
void fake_tcp_socket_handle::connect(uds_endpoint const& _ep, connect_handler _handler) {
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();

    if (sm) {
        sm->connect(_ep, *this,
                    [this, h = std::move(_handler)](auto ec) { boost::asio::post(io_, [handler = std::move(h), ec] { handler(ec); }); });
        return;
    }
    boost::asio::post(
            io_, [handler = std::move(_handler)] { handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable)); });
}

void fake_tcp_socket_handle::clear_handler() {
    rw_handler callback{}; // This is needed to avoid lock order inversion with the close on dtor
    {
        auto const lock = std::scoped_lock(mtx_);
        TEST_LOG << "[fake-socket] Clearing the handler for: " << socket_id_;
        if (receptor_) {
            callback = std::move(receptor_->handler_);
        }
        receptor_ = std::nullopt;
    }
}

[[nodiscard]] bool fake_tcp_socket_handle::add_connection(fake_tcp_socket_handle& _connecting) {
    // to avoid race-conditions + dead-locks, ensure that both sockets are not altered for the time
    // being. (if one lock is acquired in the stack before acquiring the second lock later, the
    // inverse locking might lead to a dead-lock)
    auto const lock = std::scoped_lock(mtx_, _connecting.mtx_);
    if (auto connected = connected_socket_.lock(); connected) {
        return false;
    }

    connected_socket_ = std::dynamic_pointer_cast<fake_tcp_socket_handle>(_connecting.shared_from_this());
    remote_ep_ = _connecting.local_ep_;
    remote_uds_ep_ = _connecting.local_uds_ep_;
    is_open_ = true;
    socket_id_.role_ = socket_role::server;
    local_ep_ = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), socket_id_.fd_);

    _connecting.connected_socket_ = std::dynamic_pointer_cast<fake_tcp_socket_handle>(shared_from_this());
    _connecting.remote_ep_ = local_ep_;
    _connecting.remote_uds_ep_ = local_uds_ep_;
    _connecting.socket_id_.role_ = socket_role::client;

    TEST_LOG << "[fake-socket] Established connection: " << _connecting.socket_id_ << " -> " << socket_id_;
    return true;
}

[[nodiscard]] bool fake_tcp_socket_handle::is_connected(std::weak_ptr<fake_tcp_socket_handle> _to) {
    // trick from
    // https://stackoverflow.com/questions/12301916/how-can-you-efficiently-check-whether-two-stdweak-ptr-pointers-are-pointing-to
    // absolutely do not want to `_to.lock()` so that we do cannot be the ones destroying `_to` when it falls out of scope
    // that avoids deadlocks
    auto const lock = std::scoped_lock(mtx_);
    return !connected_socket_.owner_before(_to) && !_to.owner_before(connected_socket_);
}

void fake_tcp_socket_handle::write(std::vector<boost::asio::const_buffer> const& _buffer, rw_handler _handler) {
    auto receiver = [&]() -> std::shared_ptr<fake_tcp_socket_handle> {
        auto const lock = std::scoped_lock(mtx_);
        return connected_socket_.lock();
    }();

    if (receiver) {
        size_t size = receiver->consume(_buffer);
        boost::asio::post(io_, [size, handler = std::move(_handler)] { handler(boost::system::error_code(), size); });
        return;
    }

    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();

    if (sm && sm->ignore_broken_pipe(*this)) {
        return;
    }

    boost::asio::post(
            io_, [handler = std::move(_handler)] { handler(boost::asio::error::make_error_code(boost::asio::error::broken_pipe), 0); });
}

void fake_tcp_socket_handle::async_receive(boost::asio::mutable_buffer _buffer, rw_handler _handler) {
    auto const lock = std::scoped_lock(mtx_);
    if (stashed_ec_) {
        TEST_LOG << "[fake-socket] Injecting on: " << socket_id_ << ", the stashed error: " << stashed_ec_->message();
        boost::asio::post(io_, [ec = *stashed_ec_, handler = std::move(_handler)] { handler(ec, 0); });
        stashed_ec_ = std::nullopt;
        return;
    }
    if (auto remote = connected_socket_.lock(); !remote && pipe_->size() == 0 && !ignore_nothing_to_read_from_) {
        TEST_LOG << "[fake-socket] Error on: " << socket_id_ << ", no connection to read from";
        boost::asio::post(io_, [handler = std::move(_handler)] { handler(boost::asio::error::connection_reset, 0); });
        return;
    }

    receptor_ = Receptor{std::move(_buffer), std::move(_handler)};
    update_reception();
}

size_t fake_tcp_socket_handle::consume(std::vector<boost::asio::const_buffer> const& _buffer, bool force_reception) {
    size_t const incoming_size =
            std::accumulate(_buffer.begin(), _buffer.end(), size_t{0}, [](size_t last, auto const& bf) { return last + bf.size(); });
    std::unique_lock<std::mutex> lock(mtx_);
    std::vector<unsigned char> input;
    input.reserve(incoming_size);
    for (auto const& buffer : _buffer) {
        auto first = static_cast<const char*>(buffer.data());
        auto const last = first + buffer.size();
        for (; first != last; ++first) {
            input.push_back(static_cast<unsigned char>(*first));
        }
    }

    size_t current_size{0};
    std::vector<unsigned char> raw_message;
    while (input.size() > 0) {
        command_message message;
        if (auto parsed_bytes = parse(input, message)) {
            TEST_LOG << "[fake-socket] " << socket_id_ << " received command_message: " << message;
            bool block_message{false};
            if (!force_reception && command_handler_) {
                auto copy_handler = command_handler_;
                lock.unlock();
                block_message = copy_handler(message);
                lock.lock();
            }
            if (block_message) {
                TEST_LOG << "[fake-socket] " << socket_id_ << " dropping message " << message;
            } else {
                received_command_record_.record(message.id_);
                current_size += parsed_bytes;
                raw_message.reserve(current_size);
                std::copy(input.begin(), input.begin() + parsed_bytes, std::back_inserter(raw_message));
            }
            input.erase(input.begin(), input.begin() + parsed_bytes);
            input.shrink_to_fit();
        } else {
            TEST_LOG << "[fake-socket] Error: unable to parse input. Size of the input: " << input.size();
            break;
        }
    }

    pipe_->add_data(raw_message);
    update_reception();
    return incoming_size;
}

void fake_tcp_socket_handle::update_reception() {
    if (!receptor_ || delay_processing_) {
        return;
    }
    auto bytes = pipe_->fetch_data(receptor_->buffer_);
    if (bytes == 0) {
        return;
    }
    boost::asio::post(io_, [handler = std::move(receptor_->handler_), bytes] { handler(boost::system::error_code(), bytes); });
    receptor_ = std::nullopt;
}

void fake_tcp_socket_handle::set_app_name(std::string const& _name) {
    auto const lock = std::scoped_lock(mtx_);
    socket_id_.app_name_ = _name;
}

std::string fake_tcp_socket_handle::get_app_name() const {
    auto const lock = std::scoped_lock(mtx_);
    return socket_id_.app_name_;
}

void fake_tcp_socket_handle::ignore_inner_close() {
    auto const lock = std::scoped_lock(mtx_);
    ignore_inner_close_ = true;
}
void fake_tcp_socket_handle::ignore_nothing_to_read_from(bool _ignore) {
    auto const lock = std::scoped_lock(mtx_);
    ignore_nothing_to_read_from_ = _ignore;
}

fd_t fake_tcp_socket_handle::fd() {
    auto const lock = std::scoped_lock(mtx_);
    return socket_id_.fd_;
}

void fake_tcp_socket_handle::set_vsomeip_command_handler(vsomeip_command_handler const& _handler) {
    auto const lock = std::scoped_lock(mtx_);
    command_handler_ = _handler;
}

void fake_tcp_socket_handle::replace_pipe(std::shared_ptr<data_pipe> _pipe) {
    auto const lock = std::scoped_lock(mtx_);
    _pipe->init([weak_self = weak_from_this(), this] {
        if (auto self = weak_self.lock(); self) {
            auto const lock = std::scoped_lock(mtx_);
            update_reception();
        }
    });
    pipe_->exchange_queues(*_pipe);
    pipe_ = _pipe;
    // wake any pending receptor that may now be satisfiable with transferred data
    update_reception();
}

void fake_tcp_socket_handle::delayed_consume(std::vector<boost::asio::const_buffer> const& _buffer) {
    boost::asio::post(io_, [this, _buffer] { consume(_buffer, true); });
}

fake_tcp_acceptor_handle::fake_tcp_acceptor_handle(boost::asio::io_context& _io) : io_(_io) { }

fake_tcp_acceptor_handle::~fake_tcp_acceptor_handle() {
    TEST_LOG << "[fake-acceptor] Deleting fd: " << fd_;
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();
    if (!sm) {
        return;
    }
    if (type_ == socket_type::tcp) {
        sm->remove_acceptor(fd_, endpoint_);
    } else {
        sm->remove_acceptor(fd_, uds_ep_);
    }
}

void fake_tcp_acceptor_handle::init(fd_t _fd, socket_type _type, std::weak_ptr<socket_manager> _sm) {
    fd_ = _fd;
    type_ = _type;
    socket_manager_ = _sm;
}

[[nodiscard]] bool fake_tcp_acceptor_handle::bind(boost::asio::ip::tcp::endpoint const& _ep) {
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();
    if (!sm) {
        return false;
    }
    auto result = sm->bind_acceptor(_ep, weak_from_this());
    if (result) {
        auto const lock = std::scoped_lock(mtx_);
        endpoint_ = _ep;
    }
    return result;
}

[[nodiscard]] bool fake_tcp_acceptor_handle::bind(uds_endpoint const& _ep) {
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();
    if (!sm) {
        return false;
    }
    auto result = sm->bind_acceptor(_ep, weak_from_this());
    if (result) {
        auto const lock = std::scoped_lock(mtx_);
        uds_ep_ = _ep;
    }
    return result;
}

void fake_tcp_acceptor_handle::open() {
    auto const lock = std::scoped_lock(mtx_);
    is_open_ = true;
}

void fake_tcp_acceptor_handle::close() {
    connect_handler handler{};
    {
        auto lock = std::scoped_lock(mtx_);
        is_open_ = false;
        if (connection_) {
            handler = std::move(connection_->handler_);
            connection_ = std::nullopt;
        }
    }
}
void fake_tcp_acceptor_handle::cancel() {
    connect_handler handler{};
    {
        auto lock = std::scoped_lock(mtx_);
        if (connection_) {
            handler = std::move(connection_->handler_);
            connection_ = std::nullopt;
        }
    }
}

[[nodiscard]] bool fake_tcp_acceptor_handle::is_open() {
    auto const lock = std::scoped_lock(mtx_);
    return is_open_;
}

void fake_tcp_acceptor_handle::async_accept(tcp_socket& _socket, connect_handler _handler) {
    auto* fake_socket = dynamic_cast<fake_tcp_socket*>(&_socket);
    if (!fake_socket) {
        boost::asio::post(io_, [handler = std::move(_handler)] {
            handler(boost::asio::error::make_error_code(boost::asio::error::invalid_argument));
        });
        return;
    }

    accept(fake_socket->state_, std::move(_handler));
}

void fake_tcp_acceptor_handle::async_accept(uds_socket& _socket, connect_handler _handler) {
    auto* fake_socket = dynamic_cast<fake_uds_socket*>(&_socket);
    if (!fake_socket) {
        boost::asio::post(io_, [handler = std::move(_handler)] {
            handler(boost::asio::error::make_error_code(boost::asio::error::invalid_argument));
        });
        return;
    }
    accept(fake_socket->state_, std::move(_handler));
}
void fake_tcp_acceptor_handle::accept(std::shared_ptr<fake_tcp_socket_handle> _accepting, connect_handler _handler) {
    connect_handler handler{};
    {
        auto lock = std::scoped_lock(mtx_);
        if (connection_) {
            handler = std::move(connection_->handler_);
            connection_ = std::nullopt;
        }

        TEST_LOG << "[fake-acceptor] fd: " << fd_ << ", is awaiting connections with fd: " << _accepting->fd();
        connection_ = connection{_accepting, std::move(_handler)};
        if (auto const sm = socket_manager_.lock(); sm) {
            sm->awaiting();
        }
    }
}

[[nodiscard]] std::shared_ptr<fake_tcp_socket_handle> fake_tcp_acceptor_handle::connect(fake_tcp_socket_handle& _state,
                                                                                        connect_handler _handler) {
    // because the socket_handle will never call the acceptor there is no risk of a dead-lock,
    // in case the mutex of the acceptor is hold while invoking methods from a socket_handle.
    auto const lock = std::scoped_lock(mtx_);
    if (!connection_) {
        _handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable));
        return nullptr;
    }
    if (auto accepting_socket = connection_->socket_.lock(); accepting_socket) {
        if (!accepting_socket->add_connection(_state)) {
            _handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable));
            return nullptr;
        }
        boost::asio::post(io_, [handler = std::move(connection_->handler_)] { handler(boost::system::error_code()); });
        _handler(boost::system::error_code());
        connection_ = std::nullopt;
        return accepting_socket;
    }
    _handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable));
    return nullptr;
}
void fake_tcp_acceptor_handle::clear_handler() {
    connect_handler handler;
    {
        auto const lock = std::scoped_lock(mtx_);
        if (connection_) {
            TEST_LOG << "[fake-acceptor] fd: " << fd_ << ", deleting the accept handler";
            handler = std::move(connection_->handler_);
            connection_ = std::nullopt;
        }
    }
}

void fake_tcp_acceptor_handle::set_app_name(std::string const& _name) {
    auto const lock = std::scoped_lock(mtx_);
    app_name_ = _name;
}

std::string fake_tcp_acceptor_handle::get_app_name() const {
    auto const lock = std::scoped_lock(mtx_);
    return app_name_;
}

[[nodiscard]] bool fake_tcp_acceptor_handle::is_awaiting_connection() {
    auto const lock = std::scoped_lock(mtx_);
    return static_cast<bool>(connection_);
}
}

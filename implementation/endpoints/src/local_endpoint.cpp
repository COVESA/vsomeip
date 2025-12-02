// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/local_endpoint.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/local_socket.hpp"

#include "../../configuration/include/configuration.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../../security/include/policy_manager_impl.hpp"
#include "../../utility/include/is_value.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/internal/logger.hpp>
#include <vsomeip/vsomeip_sec.h>

#include <sstream>
#include <iomanip>

namespace vsomeip_v3 {

char const* local_endpoint::to_string(local_endpoint::state_e _state) {
    switch (_state) {
    case state_e::INIT:
        return "INIT";
    case state_e::CONNECTING:
        return "CONNECTING";
    case state_e::CONNECTED:
        return "CONNECTED";
    case state_e::STOPPED:
        return "STOPPED";
    case state_e::FAILED:
        return "FAILED";
    }
    return "UNKNOWN";
}
std::ostream& operator<<(std::ostream& _out, local_endpoint::state_e _state) {
    return _out << local_endpoint::to_string(_state);
}

std::shared_ptr<local_endpoint> local_endpoint::create_server_ep(local_endpoint_context const& _context, local_endpoint_params _params,
                                                                 std::shared_ptr<local_receive_buffer> _receive_buffer,
                                                                 bool _is_routing_endpoint) {
    auto p = std::make_shared<local_endpoint>(hidden{}, _context, std::move(_params), std::move(_receive_buffer), _is_routing_endpoint,
                                              state_e::CONNECTED);
    if (!p->is_allowed()) {
        p->stop(false);
        return nullptr;
    }
    return p;
}

std::shared_ptr<local_endpoint> local_endpoint::create_client_ep(local_endpoint_context const& _context, local_endpoint_params _params) {
    auto buffer = std::make_shared<local_receive_buffer>(_context.configuration_->get_max_message_size_local(),
                                                         _context.configuration_->get_buffer_shrink_threshold());
    auto p = std::make_shared<local_endpoint>(hidden{}, _context, std::move(_params), std::move(buffer), false, state_e::INIT);
    return p;
}

local_endpoint::local_endpoint([[maybe_unused]] hidden, local_endpoint_context const& _context, local_endpoint_params _params,
                               std::shared_ptr<local_receive_buffer> _receive_buffer, bool _is_routing_endpoint, state_e _initial_state) :
    is_routing_endpoint_(_is_routing_endpoint), state_(_initial_state), peer_(_params.peer_),
    max_connection_attempts_(MAX_RECONNECTS_LOCAL), max_message_size_(_context.configuration_->get_max_message_size_local()),
    queue_limit_(_context.configuration_->get_endpoint_queue_limit_local()), receive_buffer_(std::move(_receive_buffer)), io_(_context.io_),
    socket_(std::move(_params.socket_)), configuration_(_context.configuration_), routing_host_(_context.routing_host_),
    endpoint_host_(_context.endpoint_host_) { }

local_endpoint::~local_endpoint() {
    if (state_ != state_e::STOPPED) {
        // should have been stopped gracefully before
        VSOMEIP_ERROR << "le::" << __func__ << ": Enforcing stop on object clean-up, socket: " << socket_->to_string();
        // stop is a virtual function, explain sonarQube that we would really like to call it from this very class.
        local_endpoint::stop(true);
    }
}

void local_endpoint::start() {
    std::unique_lock lock{mutex_};
    if (state_ == state_e::INIT) {
        connect_unlock();
    } else if (state_ == state_e::CONNECTED) {
        send_unlock();
        // process is not allowed to be called with a lock hold, as process
        // might dispatch to a callback
        lock.unlock();
        if (!process(0)) {
            escalate();
            return;
        }
    } else {
        VSOMEIP_ERROR << "le::" << __func__ << ": Unexpected state when trying to start: " << status_unlock();
    }
}

void local_endpoint::stop(bool _due_to_error) {
    std::scoped_lock const lock{mutex_};
    stop_unlocked(_due_to_error);
}

void local_endpoint::escalate() {
    std::unique_lock lock{mutex_};
    if (is_value(state_).any_of(state_e::FAILED, state_e::STOPPED)) {
        return;
    }
    set_state_unlocked(state_e::FAILED);

    // Note:
    // There are two competing problems:
    // 1. We should avoid locking any mutex when invoking the error handler,
    //    as this has the potential of a lock inversion
    // 2. The error handler needs to be called asap, to avoid cleaning up a "wrong"
    //    endpoint, because the current error handlers are not guaranteed to identify this
    //    endpoint uniquely, but only by the client identifier.
    //
    // While "1." can not be avoided, "2." is accidental complexity imposed on this class.
    // In the future "2." should be rethought allowing to call "post"
    // here. This would greatly ease reasoning about locking the class mutex, across
    // functions.
    error_handler_t h = error_handler_;
    lock.unlock();
    if (h) {
        h();
    }
}

void local_endpoint::restart([[maybe_unused]] bool _force) {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}

bool local_endpoint::send(byte_t const* _data, uint32_t _size) {
    std::scoped_lock const lock{mutex_};
    if (std::numeric_limits<size_t>::max() - _size < send_queue_.size()) {
        VSOMEIP_ERROR << "le::" << __func__ << ": Dropping message of size: " << _size << ", to avoid buffer overflow, " << status_unlock();
        return false;
    }
    // Note: _size + send_queue_.size() could oveflow,
    // Note 2:
    // 1. Assume: If send_queue_ is not greater then the queue_limit_ after the n-th call,
    // then the next check ensures that the send_queue_.size() will be not greater then the queue_limit_
    // after the n+1-th call
    // 2. send_queue_.size() starts with 0 -> queue_limit_ is not smaller then send_queue_.size() after n = 0
    // 1. + 2. => the next line can never cause trouble
    if (queue_limit_ != QUEUE_SIZE_UNLIMITED && queue_limit_ - send_queue_.size() < _size) {
        VSOMEIP_ERROR << "le::" << __func__ << ": Dropping message, because the queue limit (" << queue_limit_
                      << ") would be exceeded with the message size: " << _size << ", " << status_unlock();
        return false;
    }
    if (max_message_size_ != MESSAGE_SIZE_UNLIMITED && max_message_size_ < _size) {
        VSOMEIP_ERROR << "le::" << __func__ << ": Dropping message, because the message size (" << _size << ") exceeded the limit ("
                      << max_message_size_ << "), " << status_unlock();
        return false;
    }
    send_queue_.insert(send_queue_.end(), _data, _data + _size);
    send_unlock();
    return true;
}

void local_endpoint::connect_unlock() {
    if (state_ != state_e::INIT) {
        return;
    }
    auto config = configuration_.lock();
    if (!config) {
        return;
    }
    set_state_unlocked(state_e::CONNECTING);
    boost::system::error_code ec;
    socket_->prepare_connect(*config, ec);
    if (ec) {
        VSOMEIP_WARNING << "le::" << __func__ << " handling error: " << ec.message() << ", " << socket_->to_string();
        // post to ensure same call semantics as if enqueued as a reaction. This avoids unlocking the mutex (as connect_cbk needs to be able
        // to call escalate)
        boost::asio::post(io_, [weak_self = weak_from_this(), ec] {
            if (auto self = weak_self.lock(); self) {
                self->connect_cbk(ec);
            }
        });
        return;
    }
    if (!connecting_timebox_) {
        connecting_timebox_ =
                timer::create(io_, std::chrono::milliseconds(VSOMEIP_DEFAULT_CONNECTING_TIMEOUT), [weak_self = weak_from_this()] {
                    if (auto self = weak_self.lock(); self) {
                        self->connect_cbk(boost::asio::error::timed_out);
                    }
                    return false;
                });
    }
    connecting_timebox_->start();
    socket_->async_connect([weak_self = weak_from_this()](auto const& _ec) {
        if (auto self = weak_self.lock(); self) {
            self->connect_cbk(_ec);
        }
    });
}

void local_endpoint::stop_unlocked(bool _due_to_error) {
    if (state_ == state_e::STOPPED) {
        return;
    }
    if (connect_debounce_) {
        connect_debounce_->stop();
    }
    if (connecting_timebox_) {
        connecting_timebox_->stop();
    }
    socket_->stop(state_ == state_e::FAILED || _due_to_error);
    set_state_unlocked(state_e::STOPPED);
}

void local_endpoint::set_state_unlocked(state_e _state) {
    if (state_ == _state) {
        return;
    }
    VSOMEIP_INFO << "le::" << __func__ << ": change state: " << state_ << " -> " << _state << ", " << status_unlock();
    state_ = _state;
}

void local_endpoint::receive_unlock() {
    socket_->async_receive(
            receive_buffer_->buffer(),
            [weak_self = weak_from_this(),
             buffer_cp = receive_buffer_ /*ensure memory remains alive until the cbk is invoked*/](auto const& _ec, size_t _bytes) {
                if (auto self = weak_self.lock(); self) {
                    self->receive_cbk(_ec, _bytes);
                }
            });
}
void local_endpoint::send_unlock() {
    if (state_ == state_e::CONNECTED && !is_sending_) {
        send_buffer_unlock();
    }
}

void local_endpoint::send_buffer_unlock() {
    if (send_queue_.empty()) {
        is_sending_ = false;
        return;
    }
    is_sending_ = true;
    socket_->async_send(std::move(send_queue_), [weak_self = weak_from_this()](auto const& _ec, size_t _bytes, auto _buffer) {
        if (auto self = weak_self.lock(); self) {
            self->send_cbk(_ec, _bytes, std::move(_buffer));
        }
    });
    send_queue_ = {};
}

void local_endpoint::connect_cbk(boost::system::error_code const& _ec) {
    // first lock to ensure that if there is a close race with the timer
    // that the successful connect is not enqueued after the timer due to waiting
    // for the timer lock
    std::unique_lock lock{mutex_};
    if (connecting_timebox_) {
        connecting_timebox_->stop();
    }
    if (state_ != state_e::CONNECTING) {
        return;
    }
    if (!_ec) {
        if (!is_allowed()) {
            lock.unlock();
            escalate();
            return;
        }
        set_state_unlocked(state_e::CONNECTED);
        send_unlock();
        receive_unlock();
        // ensure external code is not called under the lock (and be consistent with on_disconnect)
        boost::asio::post(io_, [weak_eph = endpoint_host_, self = shared_from_this()] {
            if (auto eph = weak_eph.lock(); eph) {
                eph->on_connect(self);
            }
        });
        return;
    }
    if (_ec == boost::asio::error::operation_aborted) {
        VSOMEIP_WARNING << "le::" << __func__ << ": socket stopped, state: " << state_;
        socket_->stop(true);
        set_state_unlocked(state_e::STOPPED);
        return;
    }
    VSOMEIP_WARNING << "le::" << __func__ << ": error: " << _ec.message() << ", " << status_unlock();
    socket_->stop(true);
    // posting here avoids the need to unlock/lock the mutex when calling into the callback
    boost::asio::post(io_, [weak_eph = endpoint_host_, self = shared_from_this()] {
        if (auto eph = weak_eph.lock(); eph) {
            eph->on_disconnect(self);
        }
    });
    if (max_connection_attempts_ == MAX_RECONNECTS_UNLIMITED || max_connection_attempts_ >= ++reconnect_counter_) {
        set_state_unlocked(state_e::INIT);
        if (!connect_debounce_) {
            connect_debounce_ =
                    timer::create(io_, std::chrono::milliseconds(VSOMEIP_DEFAULT_CONNECT_TIMEOUT), [weak_self = weak_from_this()] {
                        if (auto self = weak_self.lock(); self) {
                            std::scoped_lock const inner_lock{self->mutex_};
                            self->connect_unlock();
                        }
                        return false;
                    });
        }
        connect_debounce_->start();
    } else {
        VSOMEIP_WARNING << "le::" << __func__ << ": escalating the error: " << _ec.message() << ", " << status_unlock();
        lock.unlock();
        escalate();
    }
}

std::string local_endpoint::status() const {
    std::scoped_lock const lock{mutex_};
    return status_unlock();
}
std::string local_endpoint::status_unlock() const {
    std::stringstream s;
    s << "client: " << std::hex << std::setfill('0') << std::setw(4) << peer_ << ", connection : " << socket_->to_string()
      << ", send_queue: " << send_queue_.size() << ", receive_buffer: " << *receive_buffer_
      << ", is_sending: " << (is_sending_ ? "true" : "false") << ", state: " << state_;
    return s.str();
}

void local_endpoint::send_cbk(boost::system::error_code const& _ec, [[maybe_unused]] size_t _bytes, std::vector<uint8_t> _send_buffer) {
    if (!_ec) {
        std::scoped_lock const lock{mutex_};
        if (state_ == state_e::CONNECTED) {
            send_buffer_unlock();
        }
        return;
    }
    if (_send_buffer.size() > 0) {
        VSOMEIP_WARNING << "le::" << __func__ << " error: " << _ec.message() << ", " << _send_buffer.size() << " bytes are dropped, "
                        << status();
    }

    if (_ec != boost::asio::error::operation_aborted) {
        VSOMEIP_WARNING << "le::" << __func__ << " escalating the error: " << _ec.message() << ", " << status();
        escalate();
        return;
    }
    VSOMEIP_WARNING << "le::" << __func__ << " not dealing with the error: " << _ec.message() << ", " << status();
}

[[nodiscard]] bool local_endpoint::process(size_t _new_bytes) {
    std::unique_lock lock{mutex_};
    if (_new_bytes > 0 && !receive_buffer_->bump_end(_new_bytes)) {
        VSOMEIP_ERROR << "le::" << __func__ << ": inconsistent buffer handling, trying add the read of: " << _new_bytes
                      << " bytes, escalating on " << status_unlock();
        return false;
    }
    next_message_result result;
    auto routing = routing_host_.lock();
    if (!routing) {
        return false;
    }
    auto const endpoint = socket_->peer_endpoint();
    while (receive_buffer_->next_message(result)) {
        lock.unlock(); // fine to unlock, because the caller needs to return, before we would schedule another read
        routing->on_message(result.message_data_, result.message_size_, nullptr, false, peer_, &sec_client_, endpoint.address(),
                            endpoint.port());
        lock.lock(); // because next_message changes internal state -> lock
    }
    if (result.error_) {
        VSOMEIP_ERROR << "le::" << __func__ << ": received parsing error, socket > " << status_unlock();
        return false;
    }
    receive_buffer_->shift_front();
    receive_unlock();
    return true;
}

void local_endpoint::receive_cbk(boost::system::error_code const& _ec, size_t _bytes) {
    if (_ec) {
        VSOMEIP_WARNING << "le::" << __func__ << " escalating the error: " << _ec.message() << " socket > " << status();
        if (_ec != boost::asio::error::operation_aborted) {
            escalate();
        }
        return;
    }
    if (!process(_bytes)) {
        escalate();
    }
}

bool local_endpoint::is_allowed() {
    auto end = [&] {
        socket_->stop(true);
        return false;
    };
    auto config = configuration_.lock();
    if (!config) {
        return end();
    }
    if (!socket_->update(sec_client_, *config)) {
        VSOMEIP_WARNING << "le::" << __func__ << ": escalating after a failed sec_client update, socket > " << status_unlock();
        return end();
    }
    auto ep = endpoint_host_.lock();
    if (!ep) {
        return end();
    }
    if (config->is_security_enabled()) {
        if (!config->check_routing_credentials(peer_, &sec_client_)) {
            VSOMEIP_WARNING << "le::" << __func__
                            << ": vSomeIP Security: Rejecting new connection with routing "
                               "manager client ID 0x"
                            << std::hex << peer_ << " uid/gid= " << std::dec << sec_client_.user << "/" << sec_client_.group
                            << " because passed credentials do not match with routing manager "
                               "credentials! "
                            << status_unlock();
            return end();
        }

        if (!is_routing_endpoint_) {
            if (!config->get_policy_manager()->check_credentials(peer_, &sec_client_)) {
                VSOMEIP_WARNING << "le::" << __func__ << ": vSomeIP Security: Client 0x" << std::hex << ep->get_client()
                                << " received client credentials from client 0x" << peer_
                                << " which violates the security policy : uid/gid=" << std::dec << sec_client_.user << "/"
                                << sec_client_.group;
                return end();
            }
        }
    } else {
        config->get_policy_manager()->store_client_to_sec_client_mapping(peer_, &sec_client_);
        config->get_policy_manager()->store_sec_client_to_client_mapping(&sec_client_, peer_);
    }
    return true;
}

void local_endpoint::prepare_stop([[maybe_unused]] const prepare_stop_handler_t& _handler, [[maybe_unused]] service_t _service) {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}

bool local_endpoint::is_established() const {
    std::scoped_lock const lock{mutex_};
    return state_ == state_e::CONNECTED;
}
bool local_endpoint::is_established_or_connected() const {
    std::scoped_lock const lock{mutex_};
    return state_ == state_e::CONNECTED;
}

bool local_endpoint::send_to([[maybe_unused]] std::shared_ptr<endpoint_definition> const _target, [[maybe_unused]] const byte_t* _data,
                             [[maybe_unused]] uint32_t _size) {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
    return false;
}
bool local_endpoint::send_error([[maybe_unused]] const std::shared_ptr<endpoint_definition> _target, [[maybe_unused]] const byte_t* _data,
                                [[maybe_unused]] uint32_t _size) {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
    return false;
}
void local_endpoint::receive() {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}

void local_endpoint::add_default_target([[maybe_unused]] service_t _service, [[maybe_unused]] const std::string& _address,
                                        [[maybe_unused]] uint16_t _port) {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}
void local_endpoint::remove_default_target([[maybe_unused]] service_t _service) {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}
void local_endpoint::remove_stop_handler([[maybe_unused]] service_t _service) {
    VSOMEIP_ERROR << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}
void local_endpoint::set_established([[maybe_unused]] bool _established) {
    VSOMEIP_WARNING << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}
void local_endpoint::set_connected([[maybe_unused]] bool _connected) {
    VSOMEIP_WARNING << "le::" << __func__ << ", no impl given for: " << socket_->to_string();
}

bool local_endpoint::is_reliable() const {
    return false;
}
bool local_endpoint::is_local() const {
    return true;
}

void local_endpoint::register_error_handler(const error_handler_t& _error) {
    std::scoped_lock const lock{mutex_};
    error_handler_ = _error;
}

void local_endpoint::print_status() {
    VSOMEIP_INFO << "le::" << __func__ << ": " << status();
}

size_t local_endpoint::get_queue_size() const {
    std::scoped_lock const lock{mutex_};
    return send_queue_.size();
}

std::uint16_t local_endpoint::get_local_port() const {
    return socket_->own_port();
}

client_t local_endpoint::connected_client() const {
    return peer_;
}

std::string const& local_endpoint::name() const {
    return socket_->to_string();
}
}

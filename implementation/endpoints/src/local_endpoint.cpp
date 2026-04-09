// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/local_endpoint.hpp"
#include "../include/local_socket.hpp"

#include "../../configuration/include/configuration.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../../security/include/policy_manager_impl.hpp"
#include "../../utility/include/is_value.hpp"
#include "../../utility/include/utility.hpp"
#include "../../protocol/include/assign_client_ack_command.hpp"
#include "../../protocol/include/logging.hpp"
#include "logger_ext.hpp"

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/error.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/vsomeip_sec.h>

#include <sstream>
#include <iomanip>

#define VSOMEIP_LOG_PREFIX "le"

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
                                                                 std::shared_ptr<local_receive_buffer> _receive_buffer) {
    auto p = std::make_shared<local_endpoint>(hidden{}, _context, std::move(_params), std::move(_receive_buffer), state_e::CONNECTED);
    if (!p->is_allowed()) {
        p->stop(false);
        return nullptr;
    }
    return p;
}

std::shared_ptr<local_endpoint> local_endpoint::create_client_ep(local_endpoint_context const& _context, local_endpoint_params _params) {
    auto buffer = std::make_shared<local_receive_buffer>(_context.configuration_->get_max_message_size_local(),
                                                         _context.configuration_->get_buffer_shrink_threshold());
    auto p = std::make_shared<local_endpoint>(hidden{}, _context, std::move(_params), std::move(buffer), state_e::INIT);
    return p;
}

local_endpoint::local_endpoint([[maybe_unused]] hidden, local_endpoint_context const& _context, local_endpoint_params _params,
                               std::shared_ptr<local_receive_buffer> _receive_buffer, state_e _initial_state) :
    state_(_initial_state), own_(_params.own_),
    peer_data_({_params.peer_, std::move(_params.env_), {}, std::move(_params.routing_address_), _params.routing_port_}),
    max_connection_attempts_(MAX_RECONNECTS_LOCAL), max_message_size_(_context.configuration_->get_max_message_size_local()),
    queue_limit_(_context.configuration_->get_endpoint_queue_limit_local()), receive_buffer_(std::move(_receive_buffer)), io_(_context.io_),
    socket_(std::move(_params.socket_)), configuration_(_context.configuration_), routing_host_(_context.routing_host_) { }

local_endpoint::~local_endpoint() {
    if (state_ != state_e::STOPPED) {
        // should have been stopped gracefully before
        VSOMEIP_ERROR_P << "Enforcing stop on object clean-up, socket: " << socket_->to_string();
        // stop is a virtual function, explain sonarQube that we would really like to call it from this very class.
        local_endpoint::stop(true);
    }
}

void local_endpoint::start() {
    std::unique_lock lock{mutex_};
    is_started_ = true;
    if (state_ == state_e::INIT) {
        connect_unlock();
    } else if (state_ == state_e::CONNECTED) {
        // Posting the actual start action here ensures that
        // every public method can be called without re-entering any other callback.
        boost::asio::post(io_, [this, weak_self = weak_from_this()] {
            if (auto self = weak_self.lock(); self) {
                std::unique_lock inner_lock{mutex_};
                // ensure that we weren't stopped in between
                if (state_ != state_e::CONNECTED) {
                    VSOMEIP_INFO_P << "post: Not starting to process due to state: " << status_unlock();
                    return;
                }
                if (!process(0, inner_lock)) {
                    escalate_internal(inner_lock);
                    return;
                }
                // only now we allow sending
                send_unlock();
            }
        });
    } else {
        VSOMEIP_ERROR_P << "Unexpected state when trying to start: " << status_unlock();
    }
}

void local_endpoint::stop(bool _due_to_error) {
    std::unique_lock lock{mutex_};
    stop_internal(lock, _due_to_error);
}

void local_endpoint::escalate() {
    std::unique_lock lock{mutex_};
    escalate_internal(lock);
}

void local_endpoint::escalate_internal(std::unique_lock<std::mutex>& _lock) {
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
    if (h) {
        _lock.unlock();
        h();
        _lock.lock();
    }
}

bool local_endpoint::send(byte_t const* _data, uint32_t _size) {
    std::scoped_lock const lock{mutex_};
    if (std::numeric_limits<size_t>::max() - _size < send_queue_.size()) {
        VSOMEIP_ERROR_P << "Dropping message type: " << protocol::read_command_id(_data, _size) << " and size: " << _size
                        << ", to avoid buffer overflow, " << status_unlock();
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
        VSOMEIP_ERROR_P << "Dropping message of type: " << protocol::read_command_id(_data, _size) << ", because the queue limit ("
                        << queue_limit_ << ") would be exceeded with the message size: " << _size << ", " << status_unlock();
        return false;
    }
    if (max_message_size_ != MESSAGE_SIZE_UNLIMITED && max_message_size_ < _size) {
        VSOMEIP_ERROR_P << "Dropping message of type: " << protocol::read_command_id(_data, _size) << " because the message size (" << _size
                        << ") exceeded the limit (" << max_message_size_ << "), " << status_unlock();
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
        VSOMEIP_WARNING_P << "Handling error: " << ec.message() << ", " << socket_->to_string();
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

void local_endpoint::stop_internal(std::unique_lock<std::mutex>& lock, bool _due_to_error) {
    if (state_ == state_e::STOPPED) {
        return;
    }
    if (connect_debounce_) {
        connect_debounce_->stop();
    }
    if (connecting_timebox_) {
        connecting_timebox_->stop();
    }

    uint32_t retry_count(0);
    while (true) {
        if (is_sending_) {
            if (_due_to_error || state_ == state_e::FAILED) {
                VSOMEIP_WARNING_P << "Stop due to error, will lose data, " << status_unlock();
                break;
            } else {
                VSOMEIP_WARNING_P << "Waiting [" << retry_count << "] to complete send, " << status_unlock();

                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(VSOMEIP_LOCAL_CLOSE_SEND_BUFFER_CHECK_PERIOD));
                lock.lock();
            }
        } else {
            break;
        }
        ++retry_count;
        if (retry_count > VSOMEIP_LOCAL_CLOSE_SEND_BUFFER_RETRIES) {
            VSOMEIP_WARNING_P << "Max retries reached to send! will lose data, " << status_unlock();
            break;
        }
    }

    socket_->stop(state_ == state_e::FAILED || _due_to_error);
    set_state_unlocked(state_e::STOPPED);
}

void local_endpoint::set_state_unlocked(state_e _state) {
    if (state_ == _state) {
        return;
    }
    VSOMEIP_INFO_P << "change state: " << state_ << " -> " << _state << ", " << status_unlock();
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
    if (is_started_ && state_ == state_e::CONNECTED && !is_sending_) {
        send_buffer_unlock();
    } else if (state_ == state_e::STOPPED || state_ == state_e::FAILED) {
        // any `send` in this state is "interesting", so log it
        VSOMEIP_WARNING_P << "Cannot send, " << status_unlock();
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
        set_state_unlocked(state_e::CONNECTED);
        if (peer_data_.id_ == VSOMEIP_ROUTING_CLIENT) {
            if (!assignment_timebox_) {
                assignment_timebox_ = timer::create(io_, std::chrono::seconds(3), [weak_self = weak_from_this()] {
                    if (auto self = weak_self.lock(); self) {
                        self->assignment_timeout();
                    }
                    return false;
                });
            }
            assignment_timebox_->start();
        } else {
            if (!is_allowed()) {
                escalate_internal(lock);
                return;
            }
        }
        send_unlock();
        receive_unlock();
        return;
    }
    if (_ec == boost::asio::error::operation_aborted) {
        VSOMEIP_WARNING_P << "error: " << _ec.message() << ", socket stopped, state: " << status_unlock();
        escalate_internal(lock);
        return;
    }
    VSOMEIP_WARNING_P << "Error: " << _ec.message() << ", " << status_unlock();
    socket_->stop(true);
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
        VSOMEIP_WARNING_P << "Escalating the error: " << _ec.message() << ", " << status_unlock();
        escalate_internal(lock);
    }
}

std::string local_endpoint::status() const {
    std::scoped_lock const lock{mutex_};
    return status_unlock();
}
std::string local_endpoint::status_unlock() const {
    std::stringstream s;
    s << "Client: " << hex4(peer_data_.id_) << ", connection : " << socket_->to_string() << ", send_queue: " << send_queue_.size()
      << ", receive_buffer: " << *receive_buffer_ << ", is_sending: " << (is_sending_ ? "true" : "false") << ", state: " << state_;
    return s.str();
}

void local_endpoint::send_cbk(boost::system::error_code const& _ec, [[maybe_unused]] size_t _bytes, std::vector<uint8_t> _send_buffer) {
    if (!_ec) {
        std::scoped_lock const lock{mutex_};
        if (state_ == state_e::CONNECTED) {
            send_buffer_unlock();
        } else {
            VSOMEIP_WARNING_P << "Unexpected state " << status_unlock();
        }

        return;
    }
    if (_send_buffer.size() > 0) {
        VSOMEIP_WARNING_P << "Error: " << _ec.message() << ", " << _send_buffer.size() << " bytes are dropped, " << status();
    }

    if (_ec != boost::asio::error::operation_aborted) {
        VSOMEIP_WARNING_P << "Escalating the error: " << _ec.message() << ", " << status();
        escalate();
        return;
    }
    VSOMEIP_WARNING_P << "Not dealing with the error: " << _ec.message() << ", " << status();
}

[[nodiscard]] bool local_endpoint::process(size_t _new_bytes, std::unique_lock<std::mutex>& _lock) {
    if (_new_bytes > 0 && !receive_buffer_->bump_end(_new_bytes)) {
        VSOMEIP_ERROR_P << "Inconsistent buffer handling, trying add the read of: " << _new_bytes << " bytes, escalating on "
                        << status_unlock();
        return false;
    }
    next_message_result result;
    auto routing = routing_host_.lock();
    if (!routing) {
        return false;
    }

    // avoid checking every incoming messages, if a client id was already received
    if (own_ == VSOMEIP_CLIENT_UNSET) {
        while (receive_buffer_->next_message(result)) {
            if (auto const id = protocol::read_client_id(result.message_data_, result.message_size_); id != VSOMEIP_CLIENT_UNSET) {
                own_ = id;
                if (assignment_timebox_) {
                    assignment_timebox_->stop();
                }
            }
            _lock.unlock(); // fine to unlock, because the caller needs to return, before we would schedule another read
            routing->on_message(result.message_data_, result.message_size_, peer_data_);
            _lock.lock(); // because next_message changes internal state -> lock
        }
    } else {
        while (receive_buffer_->next_message(result)) {
            _lock.unlock(); // fine to unlock, because the caller needs to return, before we would schedule another read
            routing->on_message(result.message_data_, result.message_size_, peer_data_);
            _lock.lock(); // because next_message changes internal state -> lock
        }
    }
    if (result.error_) {
        VSOMEIP_ERROR_P << "Received parsing error, socket > " << status_unlock();
        return false;
    }
    receive_buffer_->shift_front();
    receive_unlock();
    return true;
}

void local_endpoint::assignment_timeout() {
    std::unique_lock lock{mutex_};
    if (own_ == VSOMEIP_CLIENT_UNSET) {
        VSOMEIP_ERROR_P << " ";
        escalate_internal(lock);
        return;
    }
}

void local_endpoint::receive_cbk(boost::system::error_code const& _ec, size_t _bytes) {
    if (_ec) {
        VSOMEIP_WARNING_P << "Escalating the error: " << _ec.message() << " socket > " << status();
        if (_ec != boost::asio::error::operation_aborted) {
            escalate();
        }
        return;
    }
    if (std::unique_lock lock{mutex_}; !process(_bytes, lock)) {
        escalate_internal(lock);
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
    if (!socket_->update(peer_data_.sec_client_, *config)) {
        VSOMEIP_WARNING_P << "Escalating after a failed sec_client update, socket > " << status_unlock();
        return end();
    }
    if (config->is_security_enabled()) {
        if (!config->check_routing_credentials(peer_data_.id_, &peer_data_.sec_client_)) {
            VSOMEIP_WARNING_P << "vSomeIP Security: Rejecting new connection with routing manager client ID 0x" << hex4(peer_data_.id_)
                              << " uid/gid= " << peer_data_.sec_client_.user << "/" << peer_data_.sec_client_.group
                              << " because passed credentials do not match with routing manager credentials! " << status_unlock();
            return end();
        }

        if (!config->get_policy_manager()->check_credentials(peer_data_.id_, &peer_data_.sec_client_)) {
            VSOMEIP_WARNING_P << "vSomeIP Security: Client 0x" << hex4(own_) << " received client credentials from client 0x"
                              << hex4(peer_data_.id_) << " which violates the security policy : uid/gid=" << peer_data_.sec_client_.user
                              << "/" << peer_data_.sec_client_.group;
            return end();
        }
    } else {
        config->get_policy_manager()->store_client_to_sec_client_mapping(peer_data_.id_, &peer_data_.sec_client_);
        config->get_policy_manager()->store_sec_client_to_client_mapping(&peer_data_.sec_client_, peer_data_.id_);
    }

    // For UDS clients that advertised a routing address/port in assign_client_command,
    // verify the claimed port falls within the range configured for their uid/gid.
    // Without this check, any local process could claim an arbitrary TCP endpoint
    // and have the routing manager treat it as authoritative.
    if (config->is_uds_preferred() && socket_->own_port() == VSOMEIP_SEC_PORT_UNUSED && peer_data_.id_ == VSOMEIP_ROUTING_CLIENT) {
        auto const allowed_ranges = config->get_routing_guest_ports(peer_data_.sec_client_.user, peer_data_.sec_client_.group);
        auto const its_port = peer_endpoint().port();
        bool const port_allowed = std::any_of(allowed_ranges.begin(), allowed_ranges.end(),
                                              [&](auto const& r) { return its_port >= r.first && its_port <= r.second; });
        if (!port_allowed) {
            VSOMEIP_ERROR_P << "vSomeIP Security: Rejecting claimed routing port " << its_port
                            << " from uid/gid=" << peer_data_.sec_client_.user << "/" << peer_data_.sec_client_.group
                            << " (not in allowed range for this uid/gid), client: " << hex4(peer_data_.id_);
            return false;
        }
    }
    return true;
}

void local_endpoint::flush_queue() {

    std::unique_lock lock{mutex_};

    boost::system::error_code its_error;
    uint32_t retry_count(0);
    while (true) {
        size_t send_buffer_size = socket_->get_send_buffer_size(its_error);
        if (its_error) {
            VSOMEIP_WARNING_P << "Fail to read send_buffer_size (" << its_error.value() << "): " << its_error.message() << ", "
                              << status_unlock();
            break;
        }

        if (is_sending_ || !send_queue_.empty() || send_buffer_size > 0) {
            VSOMEIP_WARNING_P << "Waiting[" << retry_count << "] on close to send remaining data, " << status_unlock()
                              << " and send_buffer_size: " << send_buffer_size;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(VSOMEIP_LOCAL_CLOSE_SEND_BUFFER_CHECK_PERIOD));
            lock.lock();
            ++retry_count;
        } else {
            break;
        }
        if (retry_count > VSOMEIP_LOCAL_CLOSE_SEND_BUFFER_RETRIES) {
            VSOMEIP_WARNING_P << "Max retries reached to send! Will drop remaining data on close, " << status_unlock()
                              << " and send_buffer_size: " << send_buffer_size;
            ;
            break;
        }
    }
}

void local_endpoint::trigger_error() {
    // post to uphold the interface contract of the local_endpoint:
    // No public API call will lead to a callback invocation
    boost::asio::post(io_, [weak_self = weak_from_this()] {
        if (auto self = weak_self.lock(); self) {
            self->escalate();
        }
    });
}

void local_endpoint::register_error_handler(const error_handler_t& _error) {
    std::scoped_lock const lock{mutex_};
    error_handler_ = _error;
}

void local_endpoint::print_status() {
    VSOMEIP_INFO_P << status();
}

size_t local_endpoint::get_queue_size() const {
    std::scoped_lock const lock{mutex_};
    return send_queue_.size();
}

std::string local_endpoint::get_env() const {
    return peer_data_.env_;
}

vsomeip_sec_client_t local_endpoint::get_sec_client() const {
    return peer_data_.sec_client_;
}

std::uint16_t local_endpoint::get_local_port() const {
    return socket_->own_port();
}
boost::asio::ip::tcp::endpoint local_endpoint::peer_endpoint() const {
    if (!peer_data_.routing_address_.is_unspecified()) {
        return {peer_data_.routing_address_, peer_data_.routing_port_};
    }
    return socket_->peer_endpoint();
}

client_t local_endpoint::connected_client() const {
    return peer_data_.id_;
}

std::string const& local_endpoint::name() const {
    return socket_->to_string();
}
}

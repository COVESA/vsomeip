// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "app_connection.hpp"
#include "fake_tcp_socket_handle.hpp"
#include "test_logging.hpp"

#define LOCAL_LOG TEST_LOG << "[app_connection] "

namespace vsomeip_v3::testing {

app_connection::app_connection(std::string const& _name) : name_(_name) { }

void app_connection::set_sockets(std::weak_ptr<fake_tcp_socket_handle> _from, std::weak_ptr<fake_tcp_socket_handle> _to) {
    {
        std::unique_lock lock{mtx_};
        from_ = _from;
        to_ = _to;
        ++socket_count_;
        apply_options(std::move(lock));
    }
    cv_.notify_all();
}

bool app_connection::inject_command(std::vector<unsigned char> _payload) const {
    auto [from, to] = promoted();
    if (to) {
        std::vector<boost::asio::const_buffer> buffer;
        buffer.push_back(boost::asio::buffer(_payload));
        to->consume(buffer, true);
        return true;
    } else {
        LOCAL_LOG << "[Error] command injection failed for " << name_;
        return false;
    }
}

void app_connection::clear_command_record() const {
    auto [from, to] = promoted();
    if (from) {
        from->received_command_record_.clear();
    }
    if (to) {
        to->received_command_record_.clear();
    }
}

bool app_connection::delay_message_processing(bool _delay) {
    std::unique_lock lock{mtx_};
    // since this is a directed connection, there is only one
    // socket for this option
    to_options_.delay_message_processing_ = _delay;
    return apply_options(std::move(lock));
}

[[nodiscard]] bool app_connection::set_ignore_inner_close(bool _from, bool _to) {
    std::unique_lock lock{mtx_};
    from_options_.ignore_inner_close_ = _from;
    to_options_.ignore_inner_close_ = _to;
    return apply_options(std::move(lock));
}

void app_connection::set_custom_command_handler(vsomeip_command_handler _handler, socket_role _sender) {
    std::unique_lock lock{mtx_};
    if (_sender == socket_role::unspecified || _sender == socket_role::sender) {
        to_options_.handler_ = _handler;
    }
    if (_sender == socket_role::unspecified || _sender == socket_role::receiver) {
        from_options_.handler_ = _handler;
    }
    apply_options(std::move(lock));
}

bool app_connection::block_on_close_for(std::optional<std::chrono::milliseconds> _from_block_time,
                                        std::optional<std::chrono::milliseconds> _to_block_time) {
    std::unique_lock lock{mtx_};
    from_options_.block_on_close_time_ = _from_block_time;
    to_options_.block_on_close_time_ = _to_block_time;
    return apply_options(std::move(lock));
}

bool app_connection::wait_for_connection(std::chrono::milliseconds _timeout) const {
    // keep the shared_ptr out of the predicate to ensure that it is cleaned-up
    // without a lock
    std::shared_ptr<fake_tcp_socket_handle> from;
    auto lock = std::unique_lock(mtx_);
    return cv_.wait_for(lock, _timeout, [&from, this] {
        from = from_.lock();
        if (!from) {
            return false;
        }
        return from->is_connected(to_);
    });
}

[[nodiscard]] bool app_connection::wait_for_command(protocol::id_e _id, std::chrono::milliseconds _timeout) const {
    auto [_, to] = promoted();
    if (to) {
        return to->received_command_record_.wait_for_any(_id, _timeout);
    }
    return false;
}

[[nodiscard]] bool app_connection::disconnect(std::optional<boost::system::error_code> _from_error,
                                              std::optional<boost::system::error_code> _to_error, socket_role _side_to_disconnect) {
    auto [from, to] = promoted();
    auto disconnect = [this](auto& _ptr, auto const& _err) {
        if (_ptr) {
            _ptr->disconnect(_err);
            return true;
        } else if (_err) {
            LOCAL_LOG << "The error code: \"" << _err->message() << "\" could not be injected into: " << name_;
            return false;
        }
        return true;
    };

    bool result = true;

    switch (_side_to_disconnect) {
    case socket_role::receiver:
        result = disconnect(to, _to_error);
        break;
    case socket_role::sender:
        result = disconnect(from, _from_error);
        break;
    case socket_role::unspecified:
    default:
        result = disconnect(from, _from_error) && disconnect(to, _to_error);
        break;
    }

    return result;
}

size_t app_connection::count() const {
    std::scoped_lock lock{mtx_};
    return socket_count_;
}

bool app_connection::apply_options(std::unique_lock<std::mutex> _lock) {
    auto from_opt = from_options_;
    auto to_opt = to_options_;
    _lock.unlock();

    auto apply_option = [this](auto& ptr, auto const& opt) {
        if (ptr) {
            ptr->block_on_close_for(opt.block_on_close_time_);
            ptr->delay_processing(opt.delay_message_processing_);
            if (opt.ignore_inner_close_) {
                ptr->ignore_inner_close();
            }
            if (opt.handler_) {
                ptr->set_vsomeip_command_handler(opt.handler_);
            }
            return true;
        }
        LOCAL_LOG << __func__ << ": Failed, no socket to apply options to on: " << name_;
        return false;
    };

    auto [from, to] = promoted();
    return apply_option(from, from_opt) && apply_option(to, to_opt);
}

std::pair<std::shared_ptr<fake_tcp_socket_handle>, std::shared_ptr<fake_tcp_socket_handle>> app_connection::promoted() const {
    std::scoped_lock lock{mtx_};
    return {from_.lock(), to_.lock()};
}

}

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "app_connection.hpp"
#include "data_pipe.hpp"
#include "sockets/fake_socket_handle.hpp"
#include "sockets/fake_tcp_socket_handle.hpp"
#include "test_logging.hpp"

#define LOCAL_LOG TEST_LOG << "[app_connection] "

namespace vsomeip_v3::testing {

app_connection::app_connection(std::string const& _name) : name_(_name) { }

void app_connection::set_sockets(std::weak_ptr<fake_tcp_socket_handle> _client, std::weak_ptr<fake_tcp_socket_handle> _server) {
    {
        std::unique_lock lock{mtx_};
        // ensure that the queues are separated and not shared across sockets
        if (auto client = client_.lock(); client) {
            client->replace_pipe(std::make_shared<data_pipe>());
        }
        if (auto server = server_.lock(); server) {
            server->replace_pipe(std::make_shared<data_pipe>());
        }
        client_ = _client;
        server_ = _server;
        ++socket_count_;
        apply_options(std::move(lock));
    }
    cv_.notify_all();
}

void app_connection::notify() {
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

bool app_connection::setup_data_pipe(std::shared_ptr<data_pipe> const& _pipe, socket_role _applied_on) {
    if (_applied_on == socket_role::unspecified) {
        // a data pipe can only be applied to a single socket
        return false;
    }
    std::unique_lock lock{mtx_};
    if (_applied_on == socket_role::client) {
        client_options_.data_pipe_ = _pipe;
    }
    if (_applied_on == socket_role::server) {
        server_options_.data_pipe_ = _pipe;
    }
    // if we can already apply the option, we are replacing
    // the data pipe potentially while "in action". This might
    // lead to data loss on the socket -> don't do it.
    return !apply_options(std::move(lock));
}

bool app_connection::delay_message_processing(bool _delay, socket_role _role) {
    std::unique_lock lock{mtx_};
    if (_role == socket_role::unspecified || _role == socket_role::client) {
        client_options_.delay_message_processing_ = _delay;
    }
    if (_role == socket_role::unspecified || _role == socket_role::server) {
        server_options_.delay_message_processing_ = _delay;
    }
    return apply_options(std::move(lock));
}

[[nodiscard]] bool app_connection::set_ignore_inner_close(bool _client, bool _server) {
    std::unique_lock lock{mtx_};
    client_options_.ignore_inner_close_ = _client;
    server_options_.ignore_inner_close_ = _server;
    return apply_options(std::move(lock));
}

void app_connection::set_ignore_nothing_to_read_from(socket_role _role, bool _ignore) {
    std::unique_lock lock{mtx_};
    if (_role == socket_role::unspecified || _role == socket_role::client) {
        client_options_.ignore_nothing_to_read_from_ = _ignore;
    }
    if (_role == socket_role::unspecified || _role == socket_role::server) {
        server_options_.ignore_nothing_to_read_from_ = _ignore;
    }
    apply_options(std::move(lock));
}

void app_connection::set_custom_command_handler(vsomeip_command_handler _handler, socket_role _sender) {
    std::unique_lock lock{mtx_};
    if (_sender == socket_role::unspecified || _sender == socket_role::client) {
        // the custom_command_handler is set on the receiver..
        server_options_.handler_ = _handler;
    }
    if (_sender == socket_role::unspecified || _sender == socket_role::server) {
        client_options_.handler_ = _handler;
    }
    apply_options(std::move(lock));
}

bool app_connection::block_on_close_for(std::optional<std::chrono::milliseconds> _client_block_time,
                                        std::optional<std::chrono::milliseconds> _server_block_time) {
    std::unique_lock lock{mtx_};
    client_options_.block_on_close_time_ = _client_block_time;
    server_options_.block_on_close_time_ = _server_block_time;
    return apply_options(std::move(lock));
}

bool app_connection::wait_for_connection(std::chrono::milliseconds _timeout) const {
    // keep the shared_ptr out of the predicate to ensure that it is cleaned-up
    // without a lock
    std::shared_ptr<fake_tcp_socket_handle> from;
    auto lock = std::unique_lock(mtx_);
    return cv_.wait_for(lock, _timeout, [&from, this] {
        from = client_.lock();
        if (!from) {
            return false;
        }
        return from->is_connected(server_);
    });
}

[[nodiscard]] bool app_connection::wait_for_connection_drop(std::chrono::milliseconds _timeout) const {
    std::shared_ptr<fake_tcp_socket_handle> server;
    auto lock = std::unique_lock(mtx_);
    return cv_.wait_for(lock, _timeout, [this, &server] {
        server = server_.lock();
        return socket_count_ > 0 && (!server || !server->is_connected(client_));
    });
}

[[nodiscard]] bool app_connection::wait_for_command(protocol::id_e _id, socket_role _waiting, std::chrono::milliseconds _timeout) const {
    auto [client, server] = promoted();
    if (socket_role::client == _waiting && client) {
        return client->received_command_record_.wait_for_any(_id, _timeout);
    }
    if (socket_role::server == _waiting && server) {
        return server->received_command_record_.wait_for_any(_id, _timeout);
    }
    LOCAL_LOG << "No connection to await a command on";
    return false;
}

[[nodiscard]] bool app_connection::wait_for_last_command(protocol::id_e _id, socket_role _waiting,
                                                         std::chrono::milliseconds _timeout) const {
    auto [client, server] = promoted();
    if (socket_role::client == _waiting && client) {
        return client->received_command_record_.wait_for_last(_id, _timeout);
    }
    if (socket_role::server == _waiting && server) {
        return server->received_command_record_.wait_for_last(_id, _timeout);
    }
    LOCAL_LOG << "No connection to await a command on";
    return false;
}

[[nodiscard]] bool app_connection::disconnect(std::optional<boost::system::error_code> _client_error,
                                              std::optional<boost::system::error_code> _server_error, socket_role _side_to_disconnect) {
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
    case socket_role::server:
        result = disconnect(to, _server_error);
        break;
    case socket_role::client:
        result = disconnect(from, _client_error);
        break;
    case socket_role::unspecified:
    default:
        result = disconnect(from, _client_error) && disconnect(to, _server_error);
        break;
    }

    return result;
}

size_t app_connection::count() const {
    std::scoped_lock lock{mtx_};
    return socket_count_;
}

std::optional<socket_type> app_connection::get_socket_type() const {
    std::scoped_lock lock{mtx_};
    if (auto client = client_.lock(); client) {
        return client->get_socket_id().type_;
    }
    return std::nullopt;
}

bool app_connection::apply_options(std::unique_lock<std::mutex> _lock) {
    auto from_opt = client_options_;
    auto to_opt = server_options_;
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
            if (opt.data_pipe_) {
                ptr->replace_pipe(opt.data_pipe_);
            }
            ptr->ignore_nothing_to_read_from(opt.ignore_nothing_to_read_from_);
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
    return {client_.lock(), server_.lock()};
}

}

// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "socket_manager.hpp"

#include "app_connection.hpp"
#include "test_logging.hpp"

std::string connection_name(std::string const& _client, std::string const& _server) {
    return _client + "_to_" + _server;
}

#define LOCAL_LOG TEST_LOG << "[socket-manager] "

namespace vsomeip_v3::testing {
socket_manager::~socket_manager() = default;

void socket_manager::add(std::string const& app) {
    auto const lock = std::scoped_lock(mtx_);
    name_to_context_[app] = nullptr;
}

void socket_manager::clear_handler(std::string const& app) {
    std::vector<std::shared_ptr<fake_socket_handle>> handle_to_clear;
    std::vector<std::shared_ptr<fake_tcp_acceptor_handle>> acceptor_handle_to_clear;
    {
        auto const lock = std::scoped_lock(mtx_);
        if (auto const it = timers_.find(app); it != timers_.end()) {
            it->second->cancel();
        }
        timers_.erase(app);

        auto const it_context = name_to_context_.find(app);
        if (it_context == name_to_context_.end()) {
            return;
        }
        LOCAL_LOG << "clearing handler for io_context: " << it_context->second;
        auto* io = it_context->second;
        context_to_name_.erase(io);
        name_to_context_.erase(app);

        auto const it_fds = context_to_fd_.find(io);
        if (it_fds == context_to_fd_.end()) {
            return;
        }
        for (auto fd : it_fds->second) {
            if (auto const it_handle = fd_to_handle_.find(fd); it_handle != fd_to_handle_.end()) {
                if (auto handle = it_handle->second.lock(); handle) {
                    handle_to_clear.push_back(handle);
                }
            } else if (auto const it_acc = fd_to_acceptor_states_.find(fd); it_acc != fd_to_acceptor_states_.end()) {
                if (auto handle = it_acc->second.lock(); handle) {
                    acceptor_handle_to_clear.push_back(handle);
                }
            }
        }
        context_to_fd_.erase(io);
    }
    for (auto& handle : handle_to_clear) {
        handle->clear_handler();
    }
    for (auto& handle : acceptor_handle_to_clear) {
        handle->clear_handler();
    }
}

void socket_manager::add_socket(std::weak_ptr<fake_socket_handle> _state, boost::asio::io_context* _io, socket_type _type) {
    if (auto const state = _state.lock(); state) {
        auto fd = next_fd_++;
        if (next_fd_.load() < fd) {
            throw std::runtime_error("Exhausted fake file descriptors");
        }
        state->init(fd, _type, weak_from_this());
        auto app_name = [&]() -> std::string {
            auto const lock = std::scoped_lock(mtx_);
            fd_to_handle_[fd] = _state;
            try_add(_io, fd, "socket");
            if (auto const it_name = context_to_name_.find(_io); it_name != context_to_name_.end()) {
                return it_name->second;
            }
            return "";
        }();
        state->set_app_name(app_name);
    }
}

void socket_manager::remove(fd_t fd) {
    auto const lock = std::scoped_lock(mtx_);
    fd_to_handle_.erase(fd);

    for (auto it = multicast_to_fds_.begin(); it != multicast_to_fds_.end();) {
        auto& [address, fds] = *it;
        if (auto it_fds = fds.find(fd); it_fds != fds.end()) {
            fds.erase(it_fds);
            if (fds.empty()) {
                it = multicast_to_fds_.erase(it);
                continue;
            }
        }
        ++it;
    }

    auto it_udp = std::find_if(endpoint_udp_to_fd_.begin(), endpoint_udp_to_fd_.end(), [fd](const auto& _fd) { return _fd.second == fd; });
    if (it_udp != endpoint_udp_to_fd_.end()) {
        endpoint_udp_to_fd_.erase(it_udp);
    }

    auto it_tcp = std::find_if(endpoint_tcp_to_fd_.begin(), endpoint_tcp_to_fd_.end(), [fd](const auto& _fd) { return _fd.second == fd; });
    if (it_tcp != endpoint_tcp_to_fd_.end()) {
        endpoint_tcp_to_fd_.erase(it_tcp);
    }
}

void socket_manager::add_acceptor(std::weak_ptr<fake_tcp_acceptor_handle> _state, boost::asio::io_context* _io, socket_type _type) {
    if (auto const state = _state.lock(); state) {
        auto fd = next_fd_++;
        if (next_fd_.load() < fd) {
            throw std::runtime_error("Exhausted fake file descriptors");
        }
        state->init(fd, _type, weak_from_this());
        auto app_name = [&]() -> std::string {
            auto const lock = std::scoped_lock(mtx_);
            fd_to_acceptor_states_[fd] = _state;
            try_add(_io, fd, "acceptor");
            if (auto const it_name = context_to_name_.find(_io); it_name != context_to_name_.end()) {
                return it_name->second;
            }
            return "";
        }();
        state->set_app_name(app_name);
    }
}

/**
 * Notice that the d'tor will be called when boost::asio::io_context is destroyed,
 * as a consequence of cleaning up all handlers. Upon destruction of this handler,
 * the d'tor will ensure that the stored handlers are deleted, that would otherwise
 * have been destroyed during the destruction of io_context in the production code.
 * Note that this is necessary as long as the receive sockets/connections are
 * managed by themself.
 */
struct io_stop_spy {
    io_stop_spy(std::weak_ptr<socket_manager> _sm, std::string _app_name) : sm_(std::move(_sm)), app_name_(std::move(_app_name)) { }
    io_stop_spy(io_stop_spy const&) = delete;
    io_stop_spy& operator=(io_stop_spy const&) = delete;

    ~io_stop_spy() {
        if (auto sm = sm_.lock(); sm) {
            LOCAL_LOG << "io_spy deleted, calling clear handler for: " << app_name_;
            sm->clear_handler(app_name_);
        }
    }
    std::weak_ptr<socket_manager> sm_;
    std::string app_name_;
};

void socket_manager::try_add(boost::asio::io_context* _io, fd_t _fd, char const* _type) {
    context_to_fd_[_io].push_back(_fd);
    if (auto const it = context_to_name_.find(_io); it == context_to_name_.end()) {
        for (auto& pair : name_to_context_) {
            if (!pair.second) {
                LOCAL_LOG << "connected: \"" << pair.first << "\" with io: " << _io;
                pair.second = _io;
                assignment_cv_.notify_all();
                context_to_name_[_io] = pair.first;
                LOCAL_LOG << "added fake fd: " << _fd << " (" << _type << ") to client: " << pair.first;

                auto [it, _] = timers_.emplace(pair.first, std::make_unique<boost::asio::steady_timer>(*_io));
                // ensure the timer does not really expire by waiting for the maximum time.
                // This leads to a handler destruction in the clean-up of the io_context, which
                // we need to know about to clean-up the handlers we stored in the fake_sockets,
                // belonging to this very io_context
                it->second->expires_at(boost::asio::steady_timer::time_point::max());
                it->second->async_wait([spy = std::make_unique<io_stop_spy>(weak_from_this(), pair.first)](auto ec) {
                    LOCAL_LOG << "[ERROR] io_spy timer expired. Reporting ec: " << ec.message() << " for app: " << spy->app_name_;
                });
            }
        }
    } else {
        LOCAL_LOG << "added fake fd: " << _fd << " (" << _type << ") to client: " << it->second << " with context: " << _io;
    }
}

std::shared_ptr<app_connection> socket_manager::get_or_create_connection(std::string const& _client, std::string const& _server) {
    auto const name = connection_name(_client, _server);
    auto const lock = std::scoped_lock(mtx_);
    if (auto it = connections_.find(name); it != connections_.end()) {
        return it->second;
    }
    auto [it, _] = connections_.emplace(name, std::make_shared<app_connection>(name));
    return it->second;
}

void socket_manager::remove_acceptor(fd_t _fd, boost::asio::ip::tcp::endpoint _ep) {
    auto const lock = std::scoped_lock(mtx_);
    fd_to_acceptor_states_.erase(_fd);
    ep_to_acceptor_states_.erase(_ep);
}
void socket_manager::remove_acceptor(fd_t _fd, uds_endpoint _ep) {
    auto const lock = std::scoped_lock(mtx_);
    fd_to_acceptor_states_.erase(_fd);
    uds_to_acceptor_states_.erase(_ep);
}

[[nodiscard]] bool socket_manager::bind_acceptor(boost::asio::ip::tcp::endpoint const& _ep,
                                                 std::weak_ptr<fake_tcp_acceptor_handle> _state) {
    auto const lock = std::scoped_lock(mtx_);
    if (auto const it = ep_to_acceptor_states_.find(_ep); it != ep_to_acceptor_states_.end()) {
        return false;
    }
    ep_to_acceptor_states_[_ep] = _state;
    return true;
}
[[nodiscard]] bool socket_manager::bind_acceptor(uds_endpoint const& _ep, std::weak_ptr<fake_tcp_acceptor_handle> _state) {
    auto const lock = std::scoped_lock(mtx_);
    if (auto const it = uds_to_acceptor_states_.find(_ep); it != uds_to_acceptor_states_.end()) {
        return false;
    }
    uds_to_acceptor_states_[_ep] = _state;
    return true;
}

[[nodiscard]] bool socket_manager::bind_socket(fake_tcp_socket_handle const& _handle, boost::asio::ip::tcp::endpoint const& _ep, fd_t _fd) {
    auto const lock = std::scoped_lock(mtx_);
    if (fail_on_bind_.count(_handle.get_app_name()) == 0) {
        endpoint_tcp_to_fd_[_ep] = _fd;
        return true;
    }

    return false;
}

[[nodiscard]] bool socket_manager::bind_socket(fake_udp_socket_handle const& _handle, boost::asio::ip::udp::endpoint const& _ep, fd_t _fd) {
    std::shared_ptr<fake_udp_socket_handle> udp_handle;
    bool pending_delay{false};
    {
        auto const lock = std::scoped_lock(mtx_);
        if (fail_on_bind_.count(_handle.get_app_name()) != 0) {
            return false;
        }
        endpoint_udp_to_fd_[_ep] = _fd;
        if (auto const it = udp_sending_delay_.find(_ep); it != udp_sending_delay_.end()) {
            pending_delay = it->second;
            if (auto const h_it = fd_to_handle_.find(_fd); h_it != fd_to_handle_.end()) {
                udp_handle = std::dynamic_pointer_cast<fake_udp_socket_handle>(h_it->second.lock());
            }
        }
    }
    if (pending_delay && !udp_handle) {
        LOCAL_LOG << "invalid handler for fd: " << _fd << ", endpoint: " << _ep;
        return false;
    }
    if (udp_handle) {
        LOCAL_LOG << "applying pending sending delay (" << pending_delay << ") on bind for sender endpoint: " << _ep;
        udp_handle->delay_message_processing(pending_delay);
    }
    return true;
}
void socket_manager::connect(uds_endpoint const& _ep, fake_tcp_socket_handle& _connecting, connect_handler _handler) {
    // while the acceptor is only assigned when we find the "right" acceptor, scoped_acc will be set
    // as soon as a weak_ptr is promoted to guarantee that the d'tor is only called without locking the mutex
    std::shared_ptr<fake_tcp_acceptor_handle> acceptor, scoped_ac;
    [&]() {
        auto const lock = std::scoped_lock(mtx_);
        auto const it = uds_to_acceptor_states_.find(_ep);
        if (it == uds_to_acceptor_states_.end()) {
            _handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable));
            return;
        }
        scoped_ac = it->second.lock();
        if (!scoped_ac) {
            _handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable));
            return;
        }
        auto acc_name = scoped_ac->get_app_name();
        if (auto const it = app_to_next_connection_errors_.find(acc_name); it != app_to_next_connection_errors_.end()) {
            if (auto& err = it->second; !err.empty()) {
                _handler(*err.begin());
                err.erase(err.begin());
                return;
            }
        }
        if (auto const it = app_name_to_ignore_connections_count_.find(acc_name); it != app_name_to_ignore_connections_count_.end()) {
            if (it->second != 0) {
                --(it->second);
                // ignore the handler
                return;
            }
        }
        if (connections_to_ignore_.count(acc_name) > 0) {
            return;
        }
        acceptor = scoped_ac;
    }();
    if (!acceptor) {
        return;
    }

    auto accepting = acceptor->connect(_connecting, std::move(_handler));
    if (!accepting) {
        // handler has been moved out
        return;
    }
    auto c_name = _connecting.get_app_name();
    auto s_name = accepting->get_app_name();
    if (!c_name.empty() && !s_name.empty()) {
        auto cn = connection_name(c_name, s_name);
        auto connection = get_or_create_connection(c_name, s_name);
        connection->set_sockets(std::dynamic_pointer_cast<fake_tcp_socket_handle>(_connecting.shared_from_this()), accepting);

        auto const lock = std::scoped_lock(mtx_);
        LOCAL_LOG << "added: " << cn << " to the known connections";
    } else {
        LOCAL_LOG << "Error: socket encountered without set app name! "
                  << "c_name: " << c_name << ", s_name: " << s_name;
    }
}

void socket_manager::connect(boost::asio::ip::tcp::endpoint const& _ep, fake_tcp_socket_handle& _connecting, connect_handler _handler) {
    // while the acceptor is only assigned when we find the "right" acceptor, scoped_acc will be set
    // as soon as a weak_ptr is promoted to guarantee that the d'tor is only called without locking the mutex
    std::shared_ptr<fake_tcp_acceptor_handle> acceptor, scoped_ac;
    [&]() {
        auto const lock = std::scoped_lock(mtx_);
        auto const it = ep_to_acceptor_states_.find(_ep);
        if (it == ep_to_acceptor_states_.end()) {
            _handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable));
            return;
        }
        scoped_ac = it->second.lock();
        if (!scoped_ac) {
            _handler(boost::asio::error::make_error_code(boost::asio::error::host_unreachable));
            return;
        }
        auto acc_name = scoped_ac->get_app_name();
        if (auto const it = app_to_next_connection_errors_.find(acc_name); it != app_to_next_connection_errors_.end()) {
            if (auto& err = it->second; !err.empty()) {
                _handler(*err.begin());
                err.erase(err.begin());
                return;
            }
        }
        if (auto const it = app_name_to_ignore_connections_count_.find(acc_name); it != app_name_to_ignore_connections_count_.end()) {
            if (it->second != 0) {
                --(it->second);
                // ignore the handler
                return;
            }
        }
        if (connections_to_ignore_.count(acc_name) > 0) {
            return;
        }
        acceptor = scoped_ac;
    }();
    if (!acceptor) {
        return;
    }

    auto accepting = acceptor->connect(_connecting, std::move(_handler));
    if (!accepting) {
        // handler has been moved out
        return;
    }
    auto c_name = _connecting.get_app_name();
    auto s_name = accepting->get_app_name();
    if (!c_name.empty() && !s_name.empty()) {
        auto cn = connection_name(c_name, s_name);
        auto connection = get_or_create_connection(c_name, s_name);
        connection->set_sockets(std::dynamic_pointer_cast<fake_tcp_socket_handle>(_connecting.shared_from_this()), accepting);
        LOCAL_LOG << "added: " << cn << " to the known connections";
    } else {
        LOCAL_LOG << "Error: socket encountered without set app name! "
                  << "c_name: " << c_name << ", s_name: " << s_name;
    }
}

[[nodiscard]] bool socket_manager::disconnect(std::string const& _client_name, std::optional<boost::system::error_code> _client_error,
                                              std::string const& _server_name, std::optional<boost::system::error_code> _server_error,
                                              socket_role _side_to_disconnect) {
    auto connection = get_or_create_connection(_client_name, _server_name);
    return connection->disconnect(_client_error, _server_error, _side_to_disconnect);
}

[[nodiscard]] bool socket_manager::await_assignment(std::string const& _app, std::chrono::milliseconds _timeout) {

    auto lock = std::unique_lock(mtx_);
    return assignment_cv_.wait_for(lock, _timeout, [&, this] {
        auto const it = name_to_context_.find(_app);
        return it != name_to_context_.end() && it->second != nullptr;
    });
}
void socket_manager::fail_on_bind(std::string const& _app, bool fail) {
    auto lock = std::unique_lock(mtx_);
    if (fail) {
        fail_on_bind_.insert(_app);
    } else {
        fail_on_bind_.erase(_app);
    }
}

[[nodiscard]] bool socket_manager::await_connectable(std::string const& _app, std::chrono::milliseconds _timeout) {
    // fake_tcp_acceptor_handle need to outlive mtx_ so declare it before
    std::vector<std::shared_ptr<fake_tcp_acceptor_handle>> handles_to_hold;
    auto lock = std::unique_lock(mtx_);
    return connectable_cv_.wait_for(lock, _timeout, [&, this] {
        // tcp endpoints should be sufficient
        // Potential point of problem in mixed mode
        for (auto const& ep_ac : ep_to_acceptor_states_) {
            if (auto const acceptor = ep_ac.second.lock()) {
                handles_to_hold.push_back(acceptor);
                if (acceptor->get_app_name() == _app) {
                    return acceptor->is_awaiting_connection();
                }
            }
        }
        return false;
    });
}

[[nodiscard]] bool socket_manager::await_connection(std::string const& _client, std::string const& _server,
                                                    std::chrono::milliseconds _timeout) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->wait_for_connection(_timeout);
}

void socket_manager::awaiting() {
    connectable_cv_.notify_all();
}

size_t socket_manager::count_established_connections(std::string const& _client, std::string const& _server) {
    return get_or_create_connection(_client, _server)->count();
}

[[nodiscard]] std::optional<socket_type> socket_manager::get_connection_socket_type(std::string const& _client,
                                                                                    std::string const& _server) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->get_socket_type();
}

void socket_manager::report_on_connect(std::string const& _app_name, std::vector<boost::system::error_code> _next_errors) {
    auto const lock = std::scoped_lock(mtx_);
    auto& errors = app_to_next_connection_errors_[_app_name];
    errors.reserve(errors.size() + _next_errors.size());
    std::move(_next_errors.begin(), _next_errors.end(), std::back_inserter(errors));
}

void socket_manager::ignore_connections(std::string const& _app_name, size_t _number_of_ignored_connections) {
    auto const lock = std::scoped_lock(mtx_);
    app_name_to_ignore_connections_count_[_app_name] = _number_of_ignored_connections;
}

void socket_manager::set_ignore_connections(std::string const& _app_name, bool _ignore_connections) {
    auto const lock = std::scoped_lock(mtx_);
    if (_ignore_connections) {
        connections_to_ignore_.insert(_app_name);
    } else {
        connections_to_ignore_.erase(_app_name);
    }
}
[[nodiscard]] bool socket_manager::delay_message_processing(std::string const& _client, std::string const& _server, bool _delay,
                                                            socket_role _role) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->delay_message_processing(_delay, _role);
}
[[nodiscard]] bool socket_manager::delay_boardnet_sending(boost::asio::ip::udp::endpoint const& _ep, bool _delay) {
    std::shared_ptr<fake_udp_socket_handle> udp_handle;
    {
        std::scoped_lock lock(mtx_);
        udp_sending_delay_[_ep] = _delay;
        auto const ep_it = endpoint_udp_to_fd_.find(_ep);
        if (ep_it == endpoint_udp_to_fd_.end()) {
            LOCAL_LOG << "sender endpoint not bound yet, delay will be applied on bind: " << _ep;
            return true;
        }
        auto const h_it = fd_to_handle_.find(ep_it->second);
        if (h_it == fd_to_handle_.end()) {
            return false;
        }
        udp_handle = std::dynamic_pointer_cast<fake_udp_socket_handle>(h_it->second.lock());
    }
    if (!udp_handle) {
        return false;
    }
    LOCAL_LOG << "delaying sending on UDP socket (sender endpoint): " << _ep;
    udp_handle->delay_message_processing(_delay);
    return true;
}

[[nodiscard]] bool socket_manager::set_ignore_inner_close(std::string const& _client, bool _ignore_in_client, std::string const& _server,
                                                          bool _ignore_in_server) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->set_ignore_inner_close(_ignore_in_client, _ignore_in_server);
}

void socket_manager::set_ignore_nothing_to_read_from(std::string const& _client, std::string const& _server, socket_role _role,
                                                     bool _ignore) {
    auto connection = get_or_create_connection(_client, _server);
    connection->set_ignore_nothing_to_read_from(_role, _ignore);
}

[[nodiscard]] bool socket_manager::block_on_close_for(std::string const& _client,
                                                      std::optional<std::chrono::milliseconds> _client_block_time,
                                                      std::string const& _server,
                                                      std::optional<std::chrono::milliseconds> _server_block_time) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->block_on_close_for(_client_block_time, _server_block_time);
}

void socket_manager::clear_command_record(std::string const& _client, std::string const& _server) {
    auto connection = get_or_create_connection(_client, _server);
    connection->clear_command_record();
}

bool socket_manager::setup_data_pipe(std::string const& _client, std::string const& _server, socket_role _applied_on,
                                     std::shared_ptr<data_pipe> const& _pipe) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->setup_data_pipe(_pipe, _applied_on);
}

[[nodiscard]] bool socket_manager::wait_for_command(std::string const& _client, std::string const& _server, protocol::id_e _id,
                                                    socket_role _waiting, std::chrono::milliseconds _timeout) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->wait_for_command(_id, _waiting, _timeout);
}

[[nodiscard]] bool socket_manager::wait_for_last_command(std::string const& _client, std::string const& _server, socket_role _waiting,
                                                         protocol::id_e _id, std::chrono::milliseconds _timeout) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->wait_for_last_command(_id, _waiting, _timeout);
}

[[nodiscard]] bool socket_manager::wait_for_connection_drop(std::string const& _client, std::string const& _server,
                                                            std::chrono::milliseconds _timeout) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->wait_for_connection_drop(_timeout);
}

void socket_manager::set_ignore_broken_pipe(std::string const& _app_name, bool _set) {
    auto const lock = std::scoped_lock(mtx_);
    if (_set) {
        ignore_broken_pipe_.insert(_app_name);
    } else {
        ignore_broken_pipe_.erase(_app_name);
    }
}

bool socket_manager::ignore_broken_pipe(fake_tcp_socket_handle const& _handle) {
    auto const lock = std::scoped_lock(mtx_);
    auto app_name = _handle.get_app_name();
    auto const it = ignore_broken_pipe_.find(app_name);
    if (it == ignore_broken_pipe_.end()) {
        return false;
    }

    LOCAL_LOG << "ignoring broken_pipe for app: " << app_name;
    return true;
}

std::future<protocol::id_e> socket_manager::drop_command_once(std::string const& _from, std::string const& _to, protocol::id_e _id) {
    std::shared_ptr<std::promise<protocol::id_e>> blocked_promise{std::make_shared<std::promise<protocol::id_e>>()};
    auto fut = blocked_promise->get_future();
    set_custom_command_handler(_from, _to, [this, blocked_promise, _id, from = _from, to = _to](command_message const& _cmd) {
        if (_cmd.id_ == _id) {
            blocked_promise->set_value(_id);
            // reset handler
            set_custom_command_handler(from, to, nullptr);
            return true;
        }
        return false;
    });

    return fut;
}

bool socket_manager::inject_command(std::string const& _client, std::string const& _server, std::vector<unsigned char>& _payload) {
    auto connection = get_or_create_connection(_client, _server);
    return connection->inject_command(_payload);
}

void socket_manager::set_custom_command_handler(std::string const& _client, std::string const& _server,
                                                vsomeip_command_handler const& _handler, socket_role _sender) {
    auto connection = get_or_create_connection(_client, _server);
    connection->set_custom_command_handler(_handler, _sender);
}
void socket_manager::close_connection(std::string const& _one, std::string const& _two, socket_role _closing) {
    auto connection = _closing == socket_role::server ? get_or_create_connection(_two, _one) : get_or_create_connection(_one, _two);
    connection->notify();
}

void socket_manager::join_multicast_group(boost::asio::ip::address _multicast, fd_t _fd, std::string _app_name) {
    auto const lock = std::scoped_lock(mtx_);
    if (auto it_router = ignore_all_multicast_joins_.find(_app_name); it_router != ignore_all_multicast_joins_.end()) {
        return;
    }

    if (auto it_address = multicast_to_fds_.find(_multicast); it_address != multicast_to_fds_.end()) {
        if (auto handle = it_address->second.find(_fd); handle == it_address->second.end()) {
            it_address->second.insert(_fd);
        }
    } else {
        multicast_to_fds_[_multicast].insert(_fd);
    }
}

void socket_manager::leave_multicast_group(boost::asio::ip::address _multicast, fd_t _fd) {
    auto const lock = std::scoped_lock(mtx_);
    if (auto it_address = multicast_to_fds_.find(_multicast); it_address != multicast_to_fds_.end()) {
        if (auto handle = it_address->second.find(_fd); handle != it_address->second.end()) {
            it_address->second.erase(_fd);
        }
        if (it_address->second.empty()) {
            multicast_to_fds_.erase(it_address);
        }
    }
}

void socket_manager::send_someip(boost::asio::const_buffer const& _buffer, boost::asio::ip::udp::endpoint _src,
                                 boost::asio::ip::udp::endpoint _dst) {
    if (_dst.address().is_multicast()) {
        LOCAL_LOG << " trying to send multicast message from " << _src << " to " << _dst;
        std::vector<std::shared_ptr<fake_socket_handle>> multicast_group;
        {
            auto lock = std::unique_lock<std::mutex>(mtx_);
            if (auto it_multicast = multicast_to_fds_.find(_dst.address()); it_multicast != multicast_to_fds_.end()) {
                for (auto const& fd : it_multicast->second) {
                    if (auto handle_it = fd_to_handle_.find(fd); handle_it != fd_to_handle_.end()) {
                        if (auto shared_handle = handle_it->second.lock(); shared_handle) {
                            multicast_group.push_back(shared_handle);
                        }
                    }
                }
            }
        }

        for (auto const& handle : multicast_group) {
            if (auto udp_handle = std::dynamic_pointer_cast<fake_udp_socket_handle>(handle); udp_handle) {
                udp_handle->consume(_buffer, _src, _dst);
            }
        }
    } else {
        LOCAL_LOG << " trying to send unicast message from " << _src << " to " << _dst;
        std::shared_ptr<fake_socket_handle> shared_handle;
        auto lock = std::unique_lock<std::mutex>(mtx_);
        fd_t fd{0};
        if (auto endpoint_it = endpoint_udp_to_fd_.find(_dst); endpoint_it != endpoint_udp_to_fd_.end()) {
            fd = endpoint_it->second;
        }
        if (auto handle = fd_to_handle_.find(fd); handle != fd_to_handle_.end()) {
            if (shared_handle = handle->second.lock(); shared_handle) {
                if (auto udp_handle = std::dynamic_pointer_cast<fake_udp_socket_handle>(shared_handle); udp_handle) {
                    udp_handle->consume(_buffer, _src, _dst);
                }
            }
        }
    }
}

void socket_manager::ignore_router_all_multicast_joins(std::string _router, bool _ignore) {
    auto const lock = std::scoped_lock(mtx_);
    auto router_it = ignore_all_multicast_joins_.find(_router);
    if (router_it != ignore_all_multicast_joins_.end() && !_ignore) {
        ignore_all_multicast_joins_.erase(_router);
    } else if (_ignore) {
        ignore_all_multicast_joins_.insert(_router);
    }
}

[[nodiscard]] bool socket_manager::wait_for_sd_message(boost::asio::ip::udp::endpoint const& _ep, someip_sd_record_message _message,
                                                       std::chrono::milliseconds _timeout) {
    std::shared_ptr<fake_udp_socket_handle> udp_handle;
    {
        std::scoped_lock lock(mtx_);
        auto const ep_it = endpoint_udp_to_fd_.find(_ep);
        if (ep_it == endpoint_udp_to_fd_.end()) {
            LOCAL_LOG << "Sender endpoint not yet bound" << _ep;
            return false;
        }
        auto const h_it = fd_to_handle_.find(ep_it->second);
        if (h_it == fd_to_handle_.end()) {
            return false;
        }
        udp_handle = std::dynamic_pointer_cast<fake_udp_socket_handle>(h_it->second.lock());
    }
    if (!udp_handle) {
        return false;
    }

    return udp_handle->received_sd_record_.wait_for_any(_message, _timeout);
}

void socket_manager::clear_sd_message_record(boost::asio::ip::udp::endpoint const& _ep) {
    std::shared_ptr<fake_udp_socket_handle> udp_handle;
    {
        std::scoped_lock lock(mtx_);
        auto const ep_it = endpoint_udp_to_fd_.find(_ep);
        if (ep_it == endpoint_udp_to_fd_.end()) {
            LOCAL_LOG << "Sender endpoint not yet bound" << _ep;
            return;
        }
        auto const h_it = fd_to_handle_.find(ep_it->second);
        if (h_it == fd_to_handle_.end()) {
            return;
        }
        udp_handle = std::dynamic_pointer_cast<fake_udp_socket_handle>(h_it->second.lock());
    }
    if (!udp_handle) {
        return;
    }

    udp_handle->received_sd_record_.clear();
}
}

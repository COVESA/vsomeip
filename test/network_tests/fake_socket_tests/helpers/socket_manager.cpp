// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "socket_manager.hpp"
#include "test_logging.hpp"

std::string connection_name(std::string const& _from, std::string const& _to) {
    return _from + "_to_" + _to;
}

#define LOCAL_LOG TEST_LOG << "[socket-manager] "

namespace vsomeip_v3::testing {

void socket_manager::add(std::string const& app) {
    auto const lock = std::scoped_lock(mtx_);
    name_to_context_[app] = nullptr;
}

void socket_manager::clear_handler(std::string const& app) {
    auto const lock = std::scoped_lock(mtx_);
    auto const it_context = name_to_context_.find(app);
    if (it_context == name_to_context_.end()) {
        return;
    }
    auto const it_fds = context_to_fd_.find(it_context->second);
    if (it_fds == context_to_fd_.end()) {
        return;
    }
    for (auto fd : it_fds->second) {
        auto const it_handle = fd_to_handle_.find(fd);
        if (it_handle != fd_to_handle_.end()) {
            if (auto handle = it_handle->second.lock(); handle) {
                handle->clear_handler();
            }
        }
    }
    if (auto const it = timers_.find(app); it != timers_.end()) {
        it->second->cancel();
    }
    timers_.erase(app);
}

void socket_manager::add_socket(std::weak_ptr<fake_tcp_socket_handle> _state,
                                boost::asio::io_context* _io) {
    if (auto const state = _state.lock(); state) {
        auto fd = next_fd_++;
        if (next_fd_.load() < fd) {
            throw std::runtime_error("Exhausted fake file descriptors");
        }
        state->init(fd, weak_from_this());
        auto app_name = [&]() -> std::string {
            auto const lock = std::scoped_lock(mtx_);
            fd_to_handle_[fd] = _state;
            try_add(_io, fd, "socket");
            if (auto const it_name = context_to_name_.find(_io);
                it_name != context_to_name_.end()) {
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
}

void socket_manager::add_acceptor(std::weak_ptr<fake_tcp_acceptor_handle> _state,
                                  boost::asio::io_context* _io) {
    if (auto const state = _state.lock(); state) {
        auto fd = next_fd_++;
        if (next_fd_.load() < fd) {
            throw std::runtime_error("Exhausted fake file descriptors");
        }
        state->init(fd, weak_from_this());
        auto app_name = [&]() -> std::string {
            auto const lock = std::scoped_lock(mtx_);
            fd_to_acceptor_states_[fd] = _state;
            try_add(_io, fd, "acceptor");
            if (auto const it_name = context_to_name_.find(_io);
                it_name != context_to_name_.end()) {
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
    io_stop_spy(std::weak_ptr<socket_manager> _sm, std::string _app_name) :
        sm_(std::move(_sm)), app_name_(std::move(_app_name)) { }
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
                pair.second = _io;
                assignment_cv_.notify_all();
                context_to_name_[_io] = pair.first;
                LOCAL_LOG << "added fake fd: " << _fd << " (" << _type
                          << ") to client: " << pair.first;

                auto [it, _] = timers_.emplace(pair.first,
                                               std::make_unique<boost::asio::steady_timer>(*_io));
                // ensure the timer does not really expire by waiting for the maximum time.
                // This leads to a handler destruction in the clean-up of the io_context, which
                // we need to know about to clean-up the handlers we stored in the fake_sockets,
                // belonging to this very io_context
                it->second->expires_at(boost::asio::steady_timer::time_point::max());
                it->second->async_wait([spy = std::make_unique<io_stop_spy>(weak_from_this(),
                                                                            pair.first)](auto ec) {
                    LOCAL_LOG << "[ERROR] io_spy timer expired. Reporting ec: " << ec.message()
                              << " for app: " << spy->app_name_;
                });
            }
        }
    } else {
        LOCAL_LOG << "added fake fd: " << _fd << " (" << _type << ") to client: " << it->second;
    }
}

void socket_manager::remove_acceptor(fd_t fd) {
    auto const lock = std::scoped_lock(mtx_);
    fd_to_acceptor_states_.erase(fd);
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

void socket_manager::connect(boost::asio::ip::tcp::endpoint const& _ep,
                             fake_tcp_socket_handle& _connecting, connect_handler _handler) {
    auto acceptor = [&]() -> std::shared_ptr<fake_tcp_acceptor_handle> {
        auto const lock = std::scoped_lock(mtx_);
        auto const it = ep_to_acceptor_states_.find(_ep);
        if (it == ep_to_acceptor_states_.end()) {
            _handler(boost::system::errc::make_error_code(boost::system::errc::host_unreachable));
            return nullptr;
        }
        auto acc = it->second.lock();
        if (!acc) {
            _handler(boost::system::errc::make_error_code(boost::system::errc::host_unreachable));
            return nullptr;
        }
        auto acc_name = acc->get_app_name();
        if (auto const it = app_to_next_connection_errors_.find(acc_name);
            it != app_to_next_connection_errors_.end()) {
            if (auto& err = it->second; !err.empty()) {
                _handler(*err.begin());
                err.erase(err.begin());
                return nullptr;
            }
        }
        if (auto const it = app_name_to_ignore_connections_count_.find(acc_name);
            it != app_name_to_ignore_connections_count_.end()) {
            if (it->second != 0) {
                --(it->second);
                // ignore the handler
                return nullptr;
            }
        }
        if (connections_to_ignore_.count(acc_name) > 0) {
            return nullptr;
        }
        return acc;
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
        auto const lock = std::scoped_lock(mtx_);
        app_names_to_connection[cn] = std::pair(_connecting.weak_from_this(), accepting);
        ++connection_name_to_connection_count_[cn];
        connection_cv_.notify_all();
        LOCAL_LOG << "added: " << cn << " to the known connections";
    } else {
        LOCAL_LOG << "Error: socket encountered without set app name! "
                  << "c_name: " << c_name << ", s_name: " << s_name;
    }
}

[[nodiscard]] bool socket_manager::disconnect(std::string const& _from_name,
                                              std::optional<boost::system::error_code> _from_error,
                                              std::string const& _to_name,
                                              std::optional<boost::system::error_code> _to_error) {
    auto [weak_from, weak_to] = [&]() -> std::pair<std::weak_ptr<fake_tcp_socket_handle>,
                                                   std::weak_ptr<fake_tcp_socket_handle>> {
        auto const lock = std::scoped_lock(mtx_);
        auto const cn = connection_name(_from_name, _to_name);
        auto const it_connection = app_names_to_connection.find(cn);
        if (it_connection == app_names_to_connection.end()) {
            return {{}, {}};
        }
        return it_connection->second;
    }();

    bool result = true;
    if (auto from = weak_from.lock(); from) {
        result &= from->disconnect(_from_error);
    } else if (_from_error) {
        LOCAL_LOG << "The error code: \"" << _from_error->message()
                  << "\" could not be injected into \"" << _from_name << "\"";
        // if the error could not be injected -> error
        result = false;
    }
    if (auto to = weak_to.lock(); to) {
        result &= to->disconnect(_to_error);
    } else if (_to_error) {
        LOCAL_LOG << "The error code: \"" << _to_error->message()
                  << "\" could not be injected into \"" << _to_name << "\"";
        result = false;
    }
    return result;
}

[[nodiscard]] bool socket_manager::await_assignment(std::string const& _app,
                                                    std::chrono::milliseconds _timeout) {

    auto lock = std::unique_lock(mtx_);
    return assignment_cv_.wait_for(lock, _timeout, [&, this] {
        auto const it = name_to_context_.find(_app);
        return it != name_to_context_.end() && it->second != nullptr;
    });
}

[[nodiscard]] bool socket_manager::await_connectable(std::string const& _app,
                                                     std::chrono::milliseconds _timeout) {
    auto lock = std::unique_lock(mtx_);
    return connectable_cv_.wait_for(lock, _timeout, [&, this] {
        for (auto const& ep_ac : ep_to_acceptor_states_) {
            if (auto const acceptor = ep_ac.second.lock(); acceptor) {
                if (acceptor->get_app_name() == _app) {
                    return acceptor->is_awaiting_connection();
                }
            }
        }
        return false;
    });
}

[[nodiscard]] bool socket_manager::await_connection(std::string const& _from,
                                                    std::string const& _to,
                                                    std::chrono::milliseconds _timeout) {
    auto lock = std::unique_lock(mtx_);
    auto const cn = connection_name(_from, _to);
    return connection_cv_.wait_for(lock, _timeout, [&, this] {
        auto const it = app_names_to_connection.find(cn);
        if (it == app_names_to_connection.end()) {
            return false;
        }
        auto const from = it->second.first.lock();
        if (!from) {
            return false;
        }
        return from->is_connected(it->second.second);
    });
}

void socket_manager::awaiting() {
    connectable_cv_.notify_all();
}

size_t socket_manager::count_established_connections(std::string const& _from,
                                                     std::string const& _to) {
    auto const lock = std::scoped_lock(mtx_);
    auto const it = connection_name_to_connection_count_.find(connection_name(_from, _to));
    return it == connection_name_to_connection_count_.end() ? 0 : it->second;
}

void socket_manager::report_on_connect(std::string const& _app_name,
                                       std::vector<boost::system::error_code> _next_errors) {
    auto const lock = std::scoped_lock(mtx_);
    auto& errors = app_to_next_connection_errors_[_app_name];
    errors.reserve(errors.size() + _next_errors.size());
    std::move(_next_errors.begin(), _next_errors.end(), std::back_inserter(errors));
}

void socket_manager::ignore_connections(std::string const& _app_name,
                                        size_t _number_of_ignored_connections) {
    auto const lock = std::scoped_lock(mtx_);
    app_name_to_ignore_connections_count_[_app_name] = _number_of_ignored_connections;
}

void socket_manager::set_ignore_connections(std::string const& _app_name,
                                            bool _ignore_connections) {
    auto const lock = std::scoped_lock(mtx_);
    if (_ignore_connections) {
        connections_to_ignore_.insert(_app_name);
    } else {
        connections_to_ignore_.erase(_app_name);
    }
}
[[nodiscard]] bool socket_manager::delay_message_processing(std::string const& _from,
                                                            std::string const& _to, bool _delay) {
    auto weak_to = [&]() -> std::weak_ptr<fake_tcp_socket_handle> {
        auto const lock = std::scoped_lock(mtx_);
        auto const cn = connection_name(_from, _to);
        auto const it_connection = app_names_to_connection.find(cn);
        if (it_connection == app_names_to_connection.end()) {
            return {};
        }
        return it_connection->second.second;
    }();
    if (auto to = weak_to.lock(); to) {
        to->delay_processing(_delay);
        return true;
    }
    return false;
}

[[nodiscard]] bool socket_manager::block_on_close_for(
        std::string const& _from, std::optional<std::chrono::milliseconds> _from_block_time,
        std::string const& _to, std::optional<std::chrono::milliseconds> _to_block_time) {
    auto const lock = std::scoped_lock(mtx_);
    auto const cn = connection_name(_from, _to);
    auto const it_connection = app_names_to_connection.find(cn);
    if (it_connection == app_names_to_connection.end()) {
        return false;
    }
    bool ret = true;
    if (auto from = it_connection->second.first.lock(); from) {
        from->block_on_close_for(_from_block_time);
    } else {
        ret = false;
    }
    if (auto to = it_connection->second.second.lock(); to) {
        to->block_on_close_for(_to_block_time);
    } else {
        ret = false;
    }
    return ret;
}
}

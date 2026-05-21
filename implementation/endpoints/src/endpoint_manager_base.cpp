// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/endpoint_manager_base.hpp"

#include <boost/asio/local/stream_protocol.hpp>

#include "logger_ext.hpp"
#include "../include/local_server.hpp"
#include "../include/local_endpoint.hpp"
#include "../include/local_acceptor_tcp_impl.hpp"
#include "../include/local_acceptor_uds_impl.hpp"
#include "../include/local_socket_tcp_impl.hpp"
#include "../include/local_socket_uds_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../protocol/include/assign_client_command.hpp"
#include "../../protocol/include/config_command.hpp"
#include "../../routing/include/routing_manager_base.hpp"
#include "../../utility/include/utility.hpp"

#include <iomanip>
#include <thread>

#define VSOMEIP_LOG_PREFIX "emb"

namespace vsomeip_v3 {

static constexpr uint32_t invalid_client_token_ = std::numeric_limits<uint32_t>::max();

endpoint_manager_base::endpoint_manager_base(local_endpoint_manager_host& _host, boost::asio::io_context& _io,
                                             const std::shared_ptr<configuration>& _configuration, std::string _name,
                                             std::string _client_host) :
    host_(_host), io_(_io), configuration_(_configuration), is_local_routing_(configuration_->is_local_routing()),
    is_uds_preferred_(configuration_->is_uds_preferred()), local_port_(ILLEGAL_PORT), name_(std::move(_name)),
    client_host_(std::move(_client_host)) { }

void endpoint_manager_base::init(std::shared_ptr<routing_host> const& _local_message_handler) {
    std::scoped_lock its_lock(mtx_);
    local_message_handler_ = _local_message_handler;
}

void endpoint_manager_base::start() {
    std::scoped_lock its_lock(mtx_);
    is_started_ = true;
}

void endpoint_manager_base::stop() {
    std::scoped_lock its_lock(mtx_);
    if (!is_started_) {
        return;
    }
    is_started_ = false;
    ++lc_token_;
    VSOMEIP_INFO_P << "Start endpoint flushing for client 0x" << hex4(get_client_id());
    for (auto const& [_, ep] : local_server_endpoints_) {
        ep->start_flushing();
    }
    for (auto const& [_, ep] : local_client_endpoints_) {
        ep->start_flushing();
    }
    for (auto const& [_, ep] : pending_server_endpoints_) {
        ep->stop(true); // never started -> nothing to flush
    }
    pending_server_endpoints_.clear();
}

[[nodiscard]] bool endpoint_manager_base::await_stopped(std::chrono::milliseconds _timeout) {
    std::unique_lock its_lock{mtx_};
    return cv_.wait_for(its_lock, _timeout, [this] { return local_server_endpoints_.empty() && local_client_endpoints_.empty(); });
}

void endpoint_manager_base::remove_provider_endpoint(client_t _client, bool _remove_due_to_error) {
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client) << ", error " << _remove_due_to_error;
    std::scoped_lock lock{mtx_};
    remove_local_server_endpoint_unlocked(_client, _remove_due_to_error);
    if (!is_started_) {
        cv_.notify_one();
    }
}
void endpoint_manager_base::remove_consumer_endpoint(client_t _client, bool _remove_due_to_error) {
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client) << ", error " << _remove_due_to_error;
    std::scoped_lock lock{mtx_};
    remove_local_client_endpoint_unlocked(_client, _remove_due_to_error);
    if (!is_started_) {
        cv_.notify_one();
    }
}

void endpoint_manager_base::clear_provider_endpoints() {
    std::scoped_lock lock{mtx_};
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id());
    for (auto const& [id, ep] : local_server_endpoints_) {
        ep->stop(true);
        bump_provider_token(id);
    }
    for (auto const& [id, ep] : pending_server_endpoints_) {
        ep->stop(true); // never "started", but the socket needs to be stopped anyhow
    }
    local_server_endpoints_.clear();
    pending_server_endpoints_.clear();
}

void endpoint_manager_base::clear_consumer_endpoints() {
    std::scoped_lock lock{mtx_};
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id());
    for (auto const& [id, ep] : local_client_endpoints_) {
        ep->stop(true);
    }
    local_client_endpoints_.clear();
}

void endpoint_manager_base::stop_all_endpoints() {
    std::scoped_lock lock{mtx_};
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id());
    for (auto const& [id, ep] : local_client_endpoints_) {
        ep->stop(true);
    }
    for (auto const& [id, ep] : local_server_endpoints_) {
        ep->stop(true);
    }
    for (auto const& [id, ep] : pending_server_endpoints_) {
        ep->stop(true);
    }
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_or_create_local_client(client_t _client) {
    std::shared_ptr<local_endpoint> its_endpoint{nullptr};
    {
        std::scoped_lock its_lock{mtx_};
        its_endpoint = find_local_client_unlocked(_client);
        if (!its_endpoint) {
            if (!is_started_) {
                return nullptr;
            }
            VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client);
            its_endpoint = create_local_client_unlocked(_client);

            if (its_endpoint) {
                its_endpoint->start();
            } else {
                VSOMEIP_ERROR_P << "Couldn't find or create endpoint, self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client);
            }
        }
    }
    return its_endpoint;
}
std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_client(client_t _client) {
    std::scoped_lock its_lock(mtx_);
    return find_local_client_unlocked(_client);
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_server_endpoint(client_t _client) const {
    std::scoped_lock lock{mtx_};
    if (auto const it = local_server_endpoints_.find(_client); it != local_server_endpoints_.end()) {
        return it->second;
    }
    return nullptr;
}

void endpoint_manager_base::add_local_server_endpoint(std::shared_ptr<local_endpoint> _connection, uint32_t _token) {
    auto const its_client = _connection->connected_client();
    std::scoped_lock const its_endpoint_lock{mtx_};
    if (_token != lc_token_) {
        _connection->stop(true); // force a stop, as we can not accept this connection from a previous lifecycle
        return;
    }
    add_local_server_endpoint_unlocked(its_client, _connection);
}

void endpoint_manager_base::add_local_server_endpoint_unlocked(client_t _client, const std::shared_ptr<local_endpoint>& _connection) {
    if (auto const it = local_server_endpoints_.find(_client); it != local_server_endpoints_.end()) {
        VSOMEIP_WARNING_P << "Already existing endpoint found for client 0x" << hex4(_client)
                          << ". Enforcing a clean-up of endpoint:" << it->second->name()
                          << " but queuing endpoint: " << _connection->name();
        if (auto const it2 = pending_server_endpoints_.find(_client); it2 != pending_server_endpoints_.end()) {
            VSOMEIP_WARNING_P << "Replacing existing pending for client 0x" << hex4(_client) << ", connection: " << it2->second->name();
            it2->second->stop(true);
        }
        pending_server_endpoints_[_client] = _connection;
        it->second->trigger_error();
        return;
    }
    host_.register_error_handler(_client, _connection);
    local_server_endpoints_[_client] = _connection;
    _connection->start(provider_tokens_[_client]);
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client) << ", connection > " << _connection->name();
}
std::shared_ptr<local_acceptor> endpoint_manager_base::create_uds_local_acceptor(const std::string& _path, client_t _client) {
    std::shared_ptr<local_acceptor> uds_acceptor;
#if defined(__linux__) || defined(__QNX__)
    try {
        uint32_t its_current_wait_time{0};
        while (!uds_acceptor) {
            // Create a fresh acceptor object on every attempt so that a previously
            // opened-but-not-bound socket does not carry over stale state.
            auto tmp = std::make_shared<local_acceptor_uds_impl>(io_, boost::asio::local::stream_protocol::endpoint(_path), configuration_);
            boost::system::error_code its_error;
            tmp->init(its_error, std::nullopt);
            if (its_error) {
                its_current_wait_time += IPC_PORT_WAIT_TIME;
                if (its_current_wait_time > IPC_PORT_MAX_WAIT_TIME) {
                    VSOMEIP_ERROR_P << "Local UDS server endpoint initialization failed. Client 0x" << hex4(_client) << " Path: " << _path
                                    << " Reason: " << its_error.message();
                    break;
                }
                VSOMEIP_WARNING_P << "Local UDS server endpoint initialization failed, retrying in " << IPC_PORT_WAIT_TIME
                                  << "ms. Client 0x" << hex4(_client) << " Path: " << _path << " Reason: " << its_error.message();
                std::this_thread::sleep_for(std::chrono::milliseconds(IPC_PORT_WAIT_TIME));
            } else {
                uds_acceptor = tmp;
                VSOMEIP_INFO << "Listening @ " << _path;
            }
        }
    } catch (const std::exception& e) {
        VSOMEIP_ERROR_P << "Caught exception: " << e.what();
    }
#endif
    return uds_acceptor;
}

std::shared_ptr<local_acceptor> endpoint_manager_base::create_tcp_local_acceptor(client_t _client) {
    std::shared_ptr<local_acceptor> tcp_acceptor;
    try {
        port_t its_port;
        std::set<port_t> its_used_ports;
        auto its_address = configuration_->get_routing_guest_address();
        uint32_t its_current_wait_time{0};

        auto its_tmp = std::make_shared<local_acceptor_tcp_impl>(io_, configuration_);
        auto bind_fail_count_ = 0;
        while (get_local_server_port(its_port, its_used_ports) && !tcp_acceptor) {
            boost::system::error_code its_error;
            auto local_ep = boost::asio::ip::tcp::endpoint(its_address, its_port);
            its_tmp->init(local_ep, its_error);
            if (!its_error) {
                VSOMEIP_INFO << "Listening @ " << its_address.to_string() << ":" << its_port;
                local_port_ = port_t(its_port + 1);
                VSOMEIP_INFO << "Connecting to other clients from " << its_address.to_string() << ":" << local_port_;

                host_.set_port(local_port_);

                tcp_acceptor = its_tmp;
            } else {
                if (its_error == boost::asio::error::address_in_use) {
                    its_used_ports.insert(its_port);
                    if (++bind_fail_count_ % 10 == 0) {
                        VSOMEIP_INFO_P << "Could not bind (x" << bind_fail_count_ << "), " << its_error.message() << ", " << local_ep
                                       << ", mem: " << its_tmp.get();
                    }
                } else {
                    its_current_wait_time += IPC_PORT_WAIT_TIME;
                    if (its_current_wait_time > IPC_PORT_MAX_WAIT_TIME)
                        break;

                    std::this_thread::sleep_for(std::chrono::milliseconds(IPC_PORT_WAIT_TIME));
                }
            }
        }

        if (tcp_acceptor && _client != VSOMEIP_ROUTING_CLIENT) {
            VSOMEIP_INFO << "Adds guest for client 0x" << hex4(_client) << " with address " << its_address.to_string() << " and port "
                         << its_port;
        } else {
            VSOMEIP_ERROR_P << "Local TCP server endpoint initialization failed. Client 0x" << hex4(_client)
                            << " Reason: No local port available!";
        }
    } catch (const std::exception& e) {
        VSOMEIP_ERROR_P << "Caught exception: " << e.what();
    }
    return tcp_acceptor;
}

std::shared_ptr<local_server> endpoint_manager_base::create_local_server(transport_protocol_e _transport_protocol) {
    std::stringstream its_path;
    its_path << utility::get_base_path(configuration_->get_network()) << std::hex << get_client_id();
    const client_t its_client = get_client_id();
    VSOMEIP_INFO << "Creating local server endpoint for client 0x" << hex4(its_client) << " with transport type "
                 << (_transport_protocol == transport_protocol_e::UDS ? "UDS" : "TCP") << ".";

    std::shared_ptr<local_acceptor> its_acceptor;

    if (_transport_protocol == transport_protocol_e::UDS) {
        its_acceptor = create_uds_local_acceptor(its_path.str(), its_client);
    } else {
        its_acceptor = create_tcp_local_acceptor(its_client);
    }

    if (its_acceptor) {
        auto token = [this] {
            std::scoped_lock lock{mtx_};
            return lc_token_;
        }();
        return std::make_shared<local_server>(
                io_, std::move(its_acceptor), configuration_, local_message_handler_,
                [weak_self = weak_from_this(), token](auto _ep) {
                    if (auto self = weak_self.lock(); self) {
                        self->add_local_server_endpoint(std::move(_ep), token);
                    }
                },
                false, get_client_env());
    }
    return nullptr;
}

client_t endpoint_manager_base::get_client_id() const {
    return host_.get_client_id();
}

std::string endpoint_manager_base::get_client_env() const {
    return client_host_;
}

void endpoint_manager_base::log_client_states() const {
    std::vector<std::pair<client_t, size_t>> its_client_queue_sizes;
    std::stringstream its_log;

    {
        std::scoped_lock its_lock(mtx_);
        for (const auto& [id, ep] : local_client_endpoints_) {
            size_t its_queue_size = ep->get_queue_size();
            if (its_queue_size > VSOMEIP_DEFAULT_QUEUE_WARN_SIZE) {
                its_client_queue_sizes.push_back(std::make_pair(id, its_queue_size));
            }
        }
    }

    std::sort(its_client_queue_sizes.begin(), its_client_queue_sizes.end(),
              [](const std::pair<client_t, size_t>& _a, const std::pair<client_t, size_t>& _b) { return (_a.second > _b.second); });

    // NOTE: limit is important, do *NOT* want an arbitrarily big string!
    size_t its_max(std::min(size_t(10), its_client_queue_sizes.size()));
    its_log << std::setfill('0');
    for (size_t i = 0; i < its_max; i++) {
        its_log << hex4(its_client_queue_sizes[i].first) << ":" << its_client_queue_sizes[i].second;
        if (i < its_max - 1)
            its_log << ", ";
    }

    if (its_log.str().length() > 0)
        VSOMEIP_WARNING << "ICQ: " << its_client_queue_sizes.size() << " [" << its_log.str() << "]";
}

std::shared_ptr<local_endpoint> endpoint_manager_base::create_local_client_endpoint(client_t _client, client_t _own_id,
                                                                                    boost::asio::ip::address const& _remote_address,
                                                                                    port_t _remote_port, bool _is_guest) {
    std::shared_ptr<local_endpoint> its_endpoint;
    boost::asio::ip::address const its_local_address = configuration_->get_routing_guest_address();
    bool const same_address = _is_guest && its_local_address == _remote_address;

    local_endpoint_context const context{io_, configuration_, local_message_handler_};

#if defined(__linux__) || defined(__QNX__)
    if (is_local_routing_ || (is_uds_preferred_ && same_address)) {
        std::stringstream its_path;
        its_path << utility::get_base_path(configuration_->get_network()) << std::hex << _client;
        its_endpoint = local_endpoint::create_client_ep(
                context,
                local_endpoint_params{_client, _own_id, "",
                                      std::make_shared<local_socket_uds_impl>(io_, boost::asio::local::stream_protocol::endpoint(""),
                                                                              boost::asio::local::stream_protocol::endpoint(its_path.str()),
                                                                              socket_role_e::CLIENT)});
        VSOMEIP_INFO << "Client [" << hex4(_own_id) << "] is connecting to [" << hex4(_client) << "] at " << its_path.str()
                     << " endpoint > " << its_endpoint;
    } else {
#else
    {
#endif

        if (_is_guest) {
            try {
                its_endpoint = local_endpoint::create_client_ep(
                        context,
                        local_endpoint_params{_client, _own_id, "",
                                              std::make_shared<local_socket_tcp_impl>(
                                                      io_, boost::asio::ip::tcp::endpoint(its_local_address, local_port_),
                                                      boost::asio::ip::tcp::endpoint(_remote_address, _remote_port), socket_role_e::CLIENT),
                                              _remote_address, _remote_port});

                VSOMEIP_INFO << "Client [" << hex4(_own_id) << "] @ " << its_local_address.to_string() << ":" << local_port_
                             << " is connecting to [" << hex4(_client) << "] @ " << _remote_address.to_string() << ":" << _remote_port
                             << " endpoint > " << its_endpoint;

            } catch (...) { }
        } else {
            VSOMEIP_ERROR_P << "self 0x" << hex4(_own_id) << " cannot get guest address of client 0x" << hex4(_client);
        }
    }

    if (its_endpoint) {
        // need to send some initial info, and it must be done before the _caller_ code sends something else
        protocol::config_command config_command;
        config_command.set_client(_own_id);
        config_command.insert("hostname", get_client_env());
        auto id_str = std::string(sizeof(_client), '0');
        std::memcpy(id_str.data(), &_client, sizeof(_client));
        config_command.insert("expected_id", std::move(id_str));

        std::vector<byte_t> config_buffer;
        config_command.serialize(config_buffer);

        its_endpoint->send(&config_buffer[0], static_cast<uint32_t>(config_buffer.size()));

        host_.register_error_handler(_client, its_endpoint);
    } else {
        VSOMEIP_WARNING_P << "0x" << hex4(_own_id) << " not connected. Ignoring client assignment";
    }
    return its_endpoint;
}

std::shared_ptr<local_endpoint> endpoint_manager_base::create_routing_client() {
    auto its_endpoint = create_local_client_endpoint(VSOMEIP_ROUTING_CLIENT, VSOMEIP_CLIENT_UNSET,
                                                     configuration_->get_routing_host_address(), configuration_->get_routing_host_port(),
                                                     !configuration_->get_routing_host_address().is_unspecified());
    if (its_endpoint) {
        protocol::assign_client_command assign_command;
        assign_command.set_client(get_client_id());
        assign_command.set_name(name_);

        assign_command.set_address(configuration_->get_routing_guest_address());
        assign_command.set_port(local_port_);

        std::vector<byte_t> assign_buffer;
        assign_command.serialize(assign_buffer);

        its_endpoint->send(&assign_buffer[0], static_cast<uint32_t>(assign_buffer.size()));
    }
    return its_endpoint;
}

std::shared_ptr<local_endpoint> endpoint_manager_base::create_local_client_unlocked(client_t _client) {
    boost::asio::ip::address its_remote_address;
    port_t its_remote_port;
    bool is_guest = host_.get_connection_param(_client, its_remote_address, its_remote_port);
    auto its_endpoint = create_local_client_endpoint(_client, get_client_id(), its_remote_address, its_remote_port, is_guest);
    if (its_endpoint) {
        local_client_endpoints_[_client] = its_endpoint;
    }
    return its_endpoint;
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_client_unlocked(client_t _client) {
    std::shared_ptr<local_endpoint> its_endpoint;
    if (auto found_endpoint = local_client_endpoints_.find(_client); found_endpoint != local_client_endpoints_.end()) {
        its_endpoint = found_endpoint->second;
    }
    return its_endpoint;
}

bool endpoint_manager_base::get_local_server_port(port_t& _port, const std::set<port_t>& _used_ports) const {

#define SERVER_PORT_OFFSET 2

#ifdef _WIN32
    uid_t its_uid{ANY_UID};
    gid_t its_gid{ANY_GID};
#else
    uid_t its_uid{getuid()};
    gid_t its_gid{getgid()};
#endif

    auto its_port_ranges = configuration_->get_routing_guest_ports(its_uid, its_gid);

    if (its_port_ranges.empty()) {
        VSOMEIP_WARNING_P << "No configured port ranges for uid/gid=" << its_uid << '/' << its_gid;
    }

    for (const auto& [begin, end] : its_port_ranges) {
        for (int r = begin; r < end; r += SERVER_PORT_OFFSET) {

            if (!_used_ports.contains(port_t(r)) && r != configuration_->get_routing_host_port()) {

                _port = port_t(r);
                return true;
            }
        }
    }

    return false;
}

void endpoint_manager_base::print_status() const {
    std::scoped_lock const its_lock(mtx_);
    VSOMEIP_INFO << "status local client endpoints: " << local_client_endpoints_.size();
    for (const auto& [_, ep] : local_client_endpoints_) {
        ep->print_status();
    }
    VSOMEIP_INFO << "status local server endpoints: " << local_server_endpoints_.size();
    for (const auto& [_, ep] : local_server_endpoints_) {
        ep->print_status();
    }
    VSOMEIP_INFO << "status pending local server endpoints: " << pending_server_endpoints_.size();
}

uint32_t endpoint_manager_base::provider_connection_token(client_t _client) const {
    std::scoped_lock const its_lock(mtx_);
    auto const it = provider_tokens_.find(_client);
    return it == provider_tokens_.end() ? invalid_client_token_ : it->second;
}

void endpoint_manager_base::remove_local_client_endpoint_unlocked(client_t _client, bool _remove_due_to_error) {
    if (auto const it = local_client_endpoints_.find(_client); it != local_client_endpoints_.end()) {
        it->second->stop(_remove_due_to_error);
        VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << " is closing connection to server 0x" << hex4(_client) << " endpoint > "
                       << it->second->name();
        local_client_endpoints_.erase(it);
    }
}

void endpoint_manager_base::remove_local_server_endpoint_unlocked(client_t _client, bool _remove_due_to_error) {
    if (auto const it = local_server_endpoints_.find(_client); it != local_server_endpoints_.end()) {
        it->second->stop(_remove_due_to_error);
        VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << " is closing connection to client 0x" << hex4(_client) << " endpoint > "
                       << it->second->name();
        local_server_endpoints_.erase(it);
        bump_provider_token(_client);
    }
    if (auto const it = pending_server_endpoints_.find(_client); it != pending_server_endpoints_.end()) {
        add_local_server_endpoint_unlocked(_client, it->second);
        // safe to still use the iterator, because the adding of the endpoint can no longer fail
        // (because of the locked mtx), therefore the pending set remains untouched
        pending_server_endpoints_.erase(it);
    }
}

uint32_t endpoint_manager_base::bump_provider_token(client_t _client) {
    auto& token = provider_tokens_[_client];
    ++token;
    // "invalid_client_token_" has the special meaning of "no token found for this client". Therefore it should not used as an "actual"
    // token
    if (token == invalid_client_token_) {
        ++token;
    }
    return token;
}

} // namespace vsomeip_v3

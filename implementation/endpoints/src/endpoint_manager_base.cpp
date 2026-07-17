// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/endpoint_manager_base.hpp"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include "logger_ext.hpp"
#include "../include/local_server.hpp"
#include "../include/local_endpoint.hpp"
#include "../include/local_acceptor_tcp_impl.hpp"
#include "../include/local_acceptor_uds_impl.hpp"
#include "../include/local_socket_tcp_impl.hpp"
#include "../include/local_socket_uds_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../protocol/include/command_types.hpp"
#include "../../protocol/include/serialize.hpp"
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

void endpoint_manager_base::force_stop() {
    std::scoped_lock its_lock(mtx_);
    is_started_ = false;
    if (stop_done_trigger_) {
        stop_done_trigger_ = {};
    }

    clear_provider_endpoints(its_lock);
}

async::hook endpoint_manager_base::stop() {
    std::scoped_lock its_lock(mtx_);
    assert(!stop_done_trigger_);
    stop_done_trigger_ = async::trigger(io_);
    if (!is_started_) {
        stop_done_trigger_.fire();
        auto hook = stop_done_trigger_.get_hook();
        stop_done_trigger_ = {};
        return hook;
    }
    is_started_ = false;
    ++lc_token_;
    VSOMEIP_INFO_P << "Start endpoint flushing for client 0x" << hex4(get_client_id());
    for (auto const& [_, ep] : local_server_endpoints_) {
        ep->start_flushing();
    }
    for (auto const& [_, ep] : pending_server_endpoints_) {
        ep->stop(true); // never started -> nothing to flush
    }
    pending_server_endpoints_.clear();

    auto ret = stop_done_trigger_.get_hook();
    if (local_server_endpoints_.empty()) {
        stop_done_trigger_.fire();
        stop_done_trigger_ = {};
    }

    return ret;
}

void endpoint_manager_base::remove_provider_endpoint(client_t _client, bool _remove_due_to_error) {
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client) << ", error " << _remove_due_to_error;
    std::scoped_lock lock{mtx_};
    remove_local_server_endpoint_unlocked(_client, _remove_due_to_error);
    if (stop_done_trigger_ && local_server_endpoints_.empty()) {
        stop_done_trigger_.fire();
        stop_done_trigger_ = {};
    }
}

void endpoint_manager_base::clear_provider_endpoints() {
    std::scoped_lock lock{mtx_};
    clear_provider_endpoints(lock);
}
void endpoint_manager_base::clear_provider_endpoints([[maybe_unused]] std::scoped_lock<std::mutex> const& _lock) {
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

void endpoint_manager_base::stop_all_endpoints() {
    std::scoped_lock lock{mtx_};
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id());
    for (auto const& [id, ep] : local_server_endpoints_) {
        ep->stop(true);
    }
    for (auto const& [id, ep] : pending_server_endpoints_) {
        ep->stop(true);
    }
}

std::shared_ptr<local_endpoint> endpoint_manager_base::create_consumer_endpoint(client_t _client, client_t _own_id,
                                                                                boost::asio::ip::address _remote_address,
                                                                                port_t _remote_port) {
    return create_local_client_endpoint(_client, _own_id, _remote_address, _remote_port, true);
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_server_endpoint(client_t _client) const {
    std::scoped_lock lock{mtx_};
    if (auto const it = local_server_endpoints_.find(_client); it != local_server_endpoints_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_server_endpoint_by_peer(const boost::asio::ip::address& _peer_address,
                                                                                          port_t _peer_port) const {
    std::scoped_lock lock{mtx_};
    for (auto const& [its_client, its_endpoint] : local_server_endpoints_) {
        if (its_endpoint) {
            auto const its_peer = its_endpoint->peer_endpoint();
            if (its_peer.port() == _peer_port && its_peer.address() == _peer_address) {
                return its_endpoint;
            }
        }
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
    host_.register_error_handler(_client, _connection, connection_role_e::provider);
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
        auto id_str = std::string(sizeof(_client), '0');
        std::memcpy(id_str.data(), &_client, sizeof(_client));
        auto client_env = get_client_env();
        its_endpoint->send(protocol::create_config_cmd(_own_id, {{"hostname", client_env}, {"expected_id", id_str}}));

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
        auto guest_addr = configuration_->get_routing_guest_address();
        bool has_addr = !guest_addr.is_unspecified();
        its_endpoint->send(protocol::create_assign_client_cmd(
                get_client_id(), name_, has_addr ? guest_addr.to_v4().to_bytes() : std::array<uint8_t, 4>{}, local_port_, has_addr));
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

            if (_used_ports.count(port_t(r)) == 0 && r != configuration_->get_routing_host_port()) {

                _port = port_t(r);
                return true;
            }
        }
    }

    return false;
}

void endpoint_manager_base::print_status() const {
    std::scoped_lock const its_lock(mtx_);
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

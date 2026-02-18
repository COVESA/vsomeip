// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/endpoint_manager_base.hpp"

#include <boost/asio/local/stream_protocol.hpp>
#include <vsomeip/internal/logger.hpp>
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

endpoint_manager_base::endpoint_manager_base(routing_manager_base* const _rm, boost::asio::io_context& _io,
                                             const std::shared_ptr<configuration>& _configuration) :
    rm_(_rm), io_(_io), configuration_(_configuration), local_port_(ILLEGAL_PORT) {

    is_local_routing_ = configuration_->is_local_routing();
}

void endpoint_manager_base::init(std::shared_ptr<routing_host> const& _local_message_handler) {
    std::scoped_lock its_lock(mtx_);
    local_message_handler_ = _local_message_handler;
}

std::shared_ptr<local_endpoint> endpoint_manager_base::create_local_client(client_t _client) {
    std::scoped_lock its_lock(mtx_);
    return create_local_client_unlocked(_client);
}

void endpoint_manager_base::remove_local(const client_t _client, bool _remove_due_to_error) {
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client) << ", error " << _remove_due_to_error;
    std::scoped_lock lock{mtx_};
    remove_local_client_endpoint_unlocked(_client, _remove_due_to_error);
    remove_local_server_endpoint_unlocked(_client, _remove_due_to_error);
}

void endpoint_manager_base::clear_local_endpoints() {
    std::scoped_lock lock{mtx_};
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id());
    for (auto const& [id, ep] : local_client_endpoints_) {
        ep->register_error_handler(nullptr);
        ep->stop(false);
    }
    for (auto const& [id, ep] : local_server_endpoints_) {
        ep->register_error_handler(nullptr);
        ep->stop(false);
    }
    for (auto const& [id, ep] : pending_server_endpoints_) {
        ep->stop(true); // never "started", but the socket needs to be stopped anyhow
    }
    local_client_endpoints_.clear();
    local_server_endpoints_.clear();
    pending_server_endpoints_.clear();
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_or_create_local_client(client_t _client) {
    std::shared_ptr<local_endpoint> its_endpoint{nullptr};
    {
        std::scoped_lock its_lock{mtx_};
        its_endpoint = find_local_client_unlocked(_client);
        if (!its_endpoint) {
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

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_client(service_t _service, instance_t _instance) {
    return find_local_client(rm_->find_local_client(_service, _instance));
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_server_endpoint(client_t _client) const {
    std::scoped_lock lock{mtx_};
    if (auto const it = local_server_endpoints_.find(_client); it != local_server_endpoints_.end()) {
        return it->second;
    }
    return nullptr;
}

void endpoint_manager_base::add_local_server_endpoint(std::shared_ptr<local_endpoint> _connection) {
    auto const its_client = _connection->connected_client();
    std::scoped_lock const its_endpoint_lock{mtx_};
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
    rm_->register_client_error_handler(_client, _connection);
    local_server_endpoints_[_client] = _connection;
    _connection->start();
    VSOMEIP_INFO_P << "self 0x" << hex4(rm_->get_client()) << ", client 0x" << hex4(_client) << ", connection > " << _connection->name();
}

std::unordered_set<client_t> endpoint_manager_base::get_connected_clients() const {
    std::scoped_lock its_lock(mtx_);
    std::unordered_set<client_t> clients;
    for (const auto& [id, _] : local_client_endpoints_) {
        clients.insert(id);
    }
    return clients;
}

std::shared_ptr<local_server> endpoint_manager_base::create_local_server() {
    std::stringstream its_path;
    its_path << utility::get_base_path(configuration_->get_network()) << std::hex << get_client_id();
    const client_t its_client = get_client_id();

    std::shared_ptr<local_acceptor> its_acceptor;
#if defined(__linux__) || defined(__QNX__)
    if (is_local_routing_) {
        try {

            auto tmp = std::make_shared<local_acceptor_uds_impl>(io_, boost::asio::local::stream_protocol::endpoint(its_path.str()),
                                                                 configuration_);
            boost::system::error_code its_error;
            tmp->init(its_error, std::nullopt);
            if (its_error) {
                VSOMEIP_ERROR_P << "Local UDS server endpoint initialization failed. Client 0x" << hex4(its_client)
                                << " Path: " << its_path.str() << " Reason: " << its_error.message();

            } else {
                its_acceptor = tmp;
                VSOMEIP_INFO_P << "Listening @ " << its_path.str();
            }
        } catch (const std::exception& e) {
            VSOMEIP_ERROR_P << "Caught exception: " << e.what();
        }
    } else {
#else
    {
#endif
        try {
            port_t its_port;
            std::set<port_t> its_used_ports;
            auto its_address = configuration_->get_routing_guest_address();
            uint32_t its_current_wait_time{0};

            auto its_tmp = std::make_shared<local_acceptor_tcp_impl>(io_, configuration_);
            while (get_local_server_port(its_port, its_used_ports) && !its_acceptor) {
                boost::system::error_code its_error;
                its_tmp->init(boost::asio::ip::tcp::endpoint(its_address, its_port), its_error);
                if (!its_error) {
                    VSOMEIP_INFO_P << "Listening @ " << its_address.to_string() << ":" << std::dec << its_port;
                    local_port_ = port_t(its_port + 1);
                    VSOMEIP_INFO_P << "Connecting to other clients from " << its_address.to_string() << ":" << std::dec << local_port_;

                    rm_->set_sec_client_port(local_port_);

                    its_acceptor = its_tmp;
                } else {
                    if (its_error == boost::asio::error::address_in_use) {
                        its_used_ports.insert(its_port);
                    } else {
                        its_current_wait_time += LOCAL_TCP_PORT_WAIT_TIME;
                        if (its_current_wait_time > LOCAL_TCP_PORT_MAX_WAIT_TIME)
                            break;

                        std::this_thread::sleep_for(std::chrono::milliseconds(LOCAL_TCP_PORT_WAIT_TIME));
                    }
                }
            }

            if (its_acceptor) {
                rm_->add_guest(its_client, its_address, its_port);
            } else {
                VSOMEIP_ERROR_P << "Local TCP server endpoint initialization failed. Client 0x" << hex4(its_client)
                                << " Reason: No local port available!";
            }
        } catch (const std::exception& e) {
            VSOMEIP_ERROR_P << "Caught exception: " << e.what();
        }
    }

    if (its_acceptor) {
        return std::make_shared<local_server>(
                io_, std::move(its_acceptor), configuration_, local_message_handler_,
                [weak_self = weak_from_this()](auto _ep) {
                    if (auto self = weak_self.lock(); self) {
                        self->add_local_server_endpoint(std::move(_ep));
                    }
                },
                false, get_client_env());
    }
    return nullptr;
}

client_t endpoint_manager_base::get_client_id() const {
    return rm_->get_client();
}

std::string endpoint_manager_base::get_client_env() const {
    return rm_->get_client_host();
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
        its_log << hex4(its_client_queue_sizes[i].first) << ":" << std::dec << its_client_queue_sizes[i].second;
        if (i < its_max - 1)
            its_log << ", ";
    }

    if (its_log.str().length() > 0)
        VSOMEIP_WARNING << "ICQ: " << its_client_queue_sizes.size() << " [" << its_log.str() << "]";
}

std::shared_ptr<local_endpoint> endpoint_manager_base::create_local_client_unlocked(client_t _client) {

    std::stringstream its_path;
    its_path << utility::get_base_path(configuration_->get_network()) << std::hex << _client;
    std::shared_ptr<local_endpoint> its_endpoint;

#if defined(__linux__) || defined(__QNX__)
    if (is_local_routing_) {
        its_endpoint = local_endpoint::create_client_ep(
                local_endpoint_context{io_, configuration_, local_message_handler_},
                local_endpoint_params{_client, _client == VSOMEIP_ROUTING_CLIENT ? client_t{VSOMEIP_CLIENT_UNSET} : get_client_id(),
                                      std::make_shared<local_socket_uds_impl>(io_, boost::asio::local::stream_protocol::endpoint(""),
                                                                              boost::asio::local::stream_protocol::endpoint(its_path.str()),
                                                                              socket_role_e::CLIENT)});
        VSOMEIP_INFO << "Client [" << hex4(get_client_id()) << "] is connecting to [" << hex4(_client) << "] at " << its_path.str()
                     << " endpoint > " << its_endpoint;
    } else {
#else
    {
#endif
        boost::asio::ip::address its_local_address, its_remote_address;
        port_t its_remote_port;

        bool is_guest = rm_->get_guest(_client, its_remote_address, its_remote_port);
        if (is_guest) {
            try {
                its_local_address = configuration_->get_routing_guest_address();

                its_endpoint = local_endpoint::create_client_ep(
                        local_endpoint_context{io_, configuration_, local_message_handler_},
                        local_endpoint_params{
                                _client, _client == VSOMEIP_ROUTING_CLIENT ? client_t{VSOMEIP_CLIENT_UNSET} : get_client_id(),

                                std::make_shared<local_socket_tcp_impl>(io_, boost::asio::ip::tcp::endpoint(its_local_address, local_port_),
                                                                        boost::asio::ip::tcp::endpoint(its_remote_address, its_remote_port),
                                                                        socket_role_e::CLIENT)});

                VSOMEIP_INFO << "Client [" << hex4(get_client_id()) << "] @ " << its_local_address.to_string() << ":" << std::dec
                             << local_port_ << " is connecting to [" << hex4(_client) << "] @ " << its_remote_address.to_string() << ":"
                             << std::dec << its_remote_port << " endpoint > " << its_endpoint;

            } catch (...) { }
        } else {
            VSOMEIP_ERROR_P << "self 0x" << hex4(get_client_id()) << " cannot get guest address of client 0x" << hex4(_client);
        }
    }

    if (its_endpoint) {
        // need to send some initial info, and it must be done before the _caller_ code sends something else
        protocol::config_command config_command;
        client_t const id_to_be_sent = _client == VSOMEIP_ROUTING_CLIENT ? VSOMEIP_CLIENT_UNSET : get_client_id();
        config_command.set_client(id_to_be_sent);
        config_command.insert("hostname", get_client_env());
        auto id_str = std::string(sizeof(_client), '0');
        std::memcpy(id_str.data(), &_client, sizeof(_client));
        config_command.insert("expected_id", std::move(id_str));

        std::vector<byte_t> config_buffer;
        protocol::error_e err;
        config_command.serialize(config_buffer, err);

        if (err == protocol::error_e::ERROR_OK) {
            its_endpoint->send(&config_buffer[0], static_cast<uint32_t>(config_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "config_command creation failed , self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client)
                            << ", err " << static_cast<int>(err);
            return nullptr;
        }

        if (_client == VSOMEIP_ROUTING_CLIENT) {
            protocol::assign_client_command assign_command;
            assign_command.set_client(get_client_id());
            assign_command.set_name(rm_->get_name());

            std::vector<byte_t> assign_buffer;
            assign_command.serialize(assign_buffer, err);

            if (err == protocol::error_e::ERROR_OK) {
                its_endpoint->send(&assign_buffer[0], static_cast<uint32_t>(assign_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "assign_client_command creation failed , self 0x" << hex4(get_client_id()) << ", client 0x"
                                << hex4(_client) << ", err " << static_cast<int>(err);
                return nullptr;
            }
        } else {
            // Messages sent to the VSOMEIP_ROUTING_CLIENT are meant to be routed to
            // external devices. Therefore, its local endpoint must not be found by
            // a call to find_local_client. Thus it must not be inserted to the list of local
            // clients.
            local_client_endpoints_[_client] = its_endpoint;
        }
        rm_->register_client_error_handler(_client, its_endpoint);
    } else {
        VSOMEIP_WARNING_P << "0x" << hex4(get_client_id()) << " not connected. Ignoring client assignment";
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
        VSOMEIP_WARNING_P << "No configured port ranges for uid/gid=" << std::dec << its_uid << '/' << its_gid;
    }

    for (const auto& [begin, end] : its_port_ranges) {
        for (int r = begin; r < end; r += SERVER_PORT_OFFSET) {

            if (_used_ports.find(port_t(r)) == _used_ports.end() && r != configuration_->get_routing_host_port()) {

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

void endpoint_manager_base::remove_local_client_endpoint_unlocked(client_t _client, bool _remove_due_to_error) {
    if (auto const it = local_client_endpoints_.find(_client); it != local_client_endpoints_.end()) {
        it->second->register_error_handler(nullptr);
        it->second->stop(_remove_due_to_error);
        VSOMEIP_INFO_P << "self 0x" << hex4(rm_->get_client()) << " is closing connection to server 0x" << hex4(_client) << " endpoint > "
                       << it->second->name();
        local_client_endpoints_.erase(it);
    }
}

void endpoint_manager_base::remove_local_server_endpoint_unlocked(client_t _client, bool _remove_due_to_error) {
    if (auto const it = local_server_endpoints_.find(_client); it != local_server_endpoints_.end()) {
        it->second->register_error_handler(nullptr);
        it->second->stop(_remove_due_to_error);
        VSOMEIP_INFO_P << "self 0x" << hex4(rm_->get_client()) << " is closing connection to client 0x" << hex4(_client) << " endpoint > "
                       << it->second->name();
        local_server_endpoints_.erase(it);
    }
    if (auto const it = pending_server_endpoints_.find(_client); it != pending_server_endpoints_.end()) {
        add_local_server_endpoint_unlocked(_client, it->second);
        // safe to still use the iterator, because the adding of the endpoint can no longer fail
        // (because of the locked mtx), therefore the pending set remains untouched
        pending_server_endpoints_.erase(it);
    }
}

void endpoint_manager_base::flush_local_endpoint_queues() const {
    auto eps = [this] {
        std::scoped_lock its_lock(mtx_);
        VSOMEIP_INFO_P << "Start endpoints flush for client 0x" << hex4(get_client_id());
        std::vector<std::shared_ptr<local_endpoint>> collected_eps;
        collected_eps.reserve(local_server_endpoints_.size() + local_client_endpoints_.size());
        for (auto const& [_, ep] : local_server_endpoints_) {
            collected_eps.push_back(ep);
        }
        for (auto const& [_, ep] : local_client_endpoints_) {
            collected_eps.push_back(ep);
        }
        return collected_eps;
    }();
    // Note: The flushing effectively blocks. Therefore no mutex is allowed to be locked, to avoid stale mates
    for (auto const& ep : eps) {
        ep->flush_queue();
    }
    VSOMEIP_INFO_P << "Finished endpoints flush for client 0x" << hex4(get_client_id());
}

} // namespace vsomeip_v3

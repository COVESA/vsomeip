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

namespace vsomeip_v3 {

endpoint_manager_base::endpoint_manager_base(routing_manager_base* const _rm, boost::asio::io_context& _io,
                                             const std::shared_ptr<configuration>& _configuration) :
    rm_(_rm), io_(_io), configuration_(_configuration), local_port_(ILLEGAL_PORT) {

    is_local_routing_ = configuration_->is_local_routing();
}

std::shared_ptr<local_endpoint> endpoint_manager_base::create_local(client_t _client) {
    std::scoped_lock its_lock(local_endpoint_mutex_);
    return create_local_unlocked(_client);
}

void endpoint_manager_base::remove_local(const client_t _client, bool _remove_due_to_error) {
    VSOMEIP_INFO << "emb::" << __func__ << ": self " << hex4(get_client_id()) << ", client " << hex4(_client) << ", error "
                 << _remove_due_to_error;
    std::shared_ptr<local_endpoint> its_endpoint{find_local(_client)};
    if (its_endpoint) {
        its_endpoint->register_error_handler(nullptr);
        its_endpoint->stop(_remove_due_to_error);
        VSOMEIP_INFO << "Client [" << hex4(get_client_id()) << "] is closing connection to [" << hex4(_client) << "]"
                     << " endpoint > " << its_endpoint;
        std::scoped_lock its_lock(local_endpoint_mutex_);
        local_endpoints_.erase(_client);
    }
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_or_create_local(client_t _client) {
    std::shared_ptr<local_endpoint> its_endpoint{nullptr};
    {
        std::scoped_lock its_lock{local_endpoint_mutex_};
        its_endpoint = find_local_unlocked(_client);
        if (!its_endpoint) {
            VSOMEIP_INFO << "emb::" << __func__ << ": self " << hex4(get_client_id()) << ", client " << hex4(_client);
            its_endpoint = create_local_unlocked(_client);

            if (its_endpoint) {
                its_endpoint->start();

            } else {
                VSOMEIP_ERROR << "emb::" << __func__ << ": couldn't find or create endpoint, self " << hex4(get_client_id()) << ", client "
                              << hex4(_client);
            }
        }
    }
    return its_endpoint;
}
std::shared_ptr<local_endpoint> endpoint_manager_base::find_local(client_t _client) {
    std::scoped_lock its_lock(local_endpoint_mutex_);
    return find_local_unlocked(_client);
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local(service_t _service, instance_t _instance) {
    return find_local(rm_->find_local_client(_service, _instance));
}

std::unordered_set<client_t> endpoint_manager_base::get_connected_clients() const {
    std::scoped_lock its_lock(local_endpoint_mutex_);
    std::unordered_set<client_t> clients;
    for (const auto& its_client : local_endpoints_) {
        clients.insert(its_client.first);
    }
    return clients;
}

std::shared_ptr<local_server> endpoint_manager_base::create_local_server(const std::shared_ptr<routing_host>& _routing_host) {
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
                VSOMEIP_ERROR << "emb::" << __func__ << ": Local UDS server endpoint initialization failed. Client " << hex4(its_client)
                              << " Path: " << its_path.str() << " Reason: " << its_error.message();

            } else {
                its_acceptor = tmp;
                VSOMEIP_INFO << "emb::" << __func__ << ": Listening @ " << its_path.str();
            }
        } catch (const std::exception& e) {
            VSOMEIP_ERROR << "emb::" << __func__ << ": Caught exception: " << e.what();
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
                    VSOMEIP_INFO << "emb::" << __func__ << ": Listening @ " << its_address.to_string() << ":" << std::dec << its_port;

                    if (rm_->is_routing_manager()) {
                        local_port_ = port_t(configuration_->get_routing_host_port() + 1);
                    } else {
                        local_port_ = port_t(its_port + 1);
                    }
                    VSOMEIP_INFO << "emb::" << __func__ << ": Connecting to other clients from " << its_address.to_string() << ":"
                                 << std::dec << local_port_;

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
                VSOMEIP_ERROR << "emb::" << __func__ << ": Local TCP server endpoint initialization failed. "
                              << "Client " << hex4(its_client) << " Reason: No local port available!";
            }
        } catch (const std::exception& e) {
            VSOMEIP_ERROR << "emb::" << __func__ << ": Caught exception: " << e.what();
        }
    }

    if (its_acceptor) {
        return std::make_shared<local_server>(io_, std::move(its_acceptor), configuration_, _routing_host, false);
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
        std::scoped_lock its_lock(local_endpoint_mutex_);
        for (const auto& e : local_endpoints_) {
            size_t its_queue_size = e.second->get_queue_size();
            if (its_queue_size > VSOMEIP_DEFAULT_QUEUE_WARN_SIZE) {
                its_client_queue_sizes.push_back(std::make_pair(e.first, its_queue_size));
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

std::shared_ptr<local_endpoint> endpoint_manager_base::create_local_unlocked(client_t _client) {

    std::stringstream its_path;
    its_path << utility::get_base_path(configuration_->get_network()) << std::hex << _client;
    std::shared_ptr<local_endpoint> its_endpoint;

#if defined(__linux__) || defined(__QNX__)
    if (is_local_routing_) {
        its_endpoint = local_endpoint::create_client_ep(
                local_endpoint_context{io_, configuration_, rm_->weak_from_this()},
                local_endpoint_params{_client, _client == VSOMEIP_ROUTING_CLIENT ? client_t{VSOMEIP_CLIENT_UNSET} : get_client_id(),
                                      std::make_shared<local_socket_uds_impl>(io_, boost::asio::local::stream_protocol::endpoint(""),
                                                                              boost::asio::local::stream_protocol::endpoint(its_path.str()),
                                                                              socket_role_e::SENDER)});
        VSOMEIP_INFO << "Client [" << std::hex << get_client_id() << "] is connecting to [" << std::hex << _client << "] at "
                     << its_path.str() << " endpoint > " << its_endpoint;
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
                        local_endpoint_context{io_, configuration_, rm_->weak_from_this()},
                        local_endpoint_params{
                                _client, _client == VSOMEIP_ROUTING_CLIENT ? client_t{VSOMEIP_CLIENT_UNSET} : get_client_id(),

                                std::make_shared<local_socket_tcp_impl>(io_, boost::asio::ip::tcp::endpoint(its_local_address, local_port_),
                                                                        boost::asio::ip::tcp::endpoint(its_remote_address, its_remote_port),
                                                                        socket_role_e::SENDER)});

                VSOMEIP_INFO << "Client [" << hex4(get_client_id()) << "] @ " << its_local_address.to_string() << ":" << std::dec
                             << local_port_ << " is connecting to [" << hex4(_client) << "] @ " << its_remote_address.to_string() << ":"
                             << std::dec << its_remote_port << " endpoint > " << its_endpoint;

            } catch (...) { }
        } else {
            VSOMEIP_ERROR << __func__ << ": self " << hex4(get_client_id()) << " cannot get guest address of client [" << _client << "]";
        }
    }

    if (its_endpoint) {
        // need to send some initial info, and it must be done before the _caller_ code sends something else
        protocol::config_command config_command;
        client_t const id_to_be_sent = [this, &_client]() -> client_t {
            if (_client == VSOMEIP_ROUTING_CLIENT) {
                return VSOMEIP_CLIENT_UNSET;
            }
            // bloody hell of a work around. Router as a client only works with tcp, because the client is not known and it defaults to
            // the router, but uds + internal security needs to know the client in case we are talking to something else then the router
            // in case this is the "sender" towards the router, the client_id is rubbish, but that's fine at this moment
            if (rm_->is_routing_manager() && !is_local_routing_) {
                return VSOMEIP_ROUTING_CLIENT;
            }
            return get_client_id();
        }();

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
            VSOMEIP_ERROR << "emb::" << __func__ << ": config_command creation failed , self " << hex4(get_client_id()) << ", client "
                          << hex4(_client) << ", err " << static_cast<int>(err);
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
                VSOMEIP_ERROR << "emb::" << __func__ << ": assign_client_command creation failed , self " << hex4(get_client_id())
                              << ", client " << hex4(_client) << ", err " << static_cast<int>(err);
                return nullptr;
            }
        } else {
            // Messages sent to the VSOMEIP_ROUTING_CLIENT are meant to be routed to
            // external devices. Therefore, its local endpoint must not be found by
            // a call to find_local. Thus it must not be inserted to the list of local
            // clients.
            local_endpoints_[_client] = its_endpoint;
        }
        rm_->register_client_error_handler(_client, its_endpoint);
    } else {
        VSOMEIP_WARNING << __func__ << ": (" << std::hex << get_client_id() << ") not connected. Ignoring client assignment";
    }
    return its_endpoint;
}

std::shared_ptr<local_endpoint> endpoint_manager_base::find_local_unlocked(client_t _client) {
    std::shared_ptr<local_endpoint> its_endpoint;
    auto found_endpoint = local_endpoints_.find(_client);
    if (found_endpoint != local_endpoints_.end()) {
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
        VSOMEIP_WARNING << __func__ << ": No configured port ranges for uid/gid=" << std::dec << its_uid << '/' << its_gid;
    }

    for (const auto& its_range : its_port_ranges) {
        for (int r = its_range.first; r < its_range.second; r += SERVER_PORT_OFFSET) {

            if (_used_ports.find(port_t(r)) == _used_ports.end() && r != configuration_->get_routing_host_port()) {

                _port = port_t(r);
                return true;
            }
        }
    }

    return false;
}

void endpoint_manager_base::print_status() const {
    std::scoped_lock const its_lock(local_endpoint_mutex_);

    VSOMEIP_INFO << "status local client endpoints: " << std::dec << local_endpoints_.size();
    for (const auto& [key, ep] : local_endpoints_) {
        ep->print_status();
    }
}

void endpoint_manager_base::flush_local_endpoint_queues() const {
    auto eps = [this] {
        std::scoped_lock its_lock(local_endpoint_mutex_);
        VSOMEIP_INFO << "emb::flush_local_endpoint_queues: Start endpoints flush for client " << hex4(get_client_id());
        return local_endpoints_;
    }();
    // Note: The flushing effectively blocks. Therefore no mutex is allowed to be locked, to avoid stale mates
    for (auto const& [id, ep] : eps) {
        ep->flush_queue();
    }
    VSOMEIP_INFO << "emb::flush_local_endpoint_queues: Finished endpoints flush for client " << hex4(get_client_id());
}

} // namespace vsomeip_v3

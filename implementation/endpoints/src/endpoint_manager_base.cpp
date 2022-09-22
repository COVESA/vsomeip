// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/endpoint_manager_base.hpp"

#include <vsomeip/internal/logger.hpp>
#include "../../utility/include/utility.hpp"
#include "../../routing/include/routing_manager_base.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../include/local_client_endpoint_impl.hpp"
#include "../include/local_server_endpoint_impl.hpp"

#include <iomanip>

namespace vsomeip_v3 {

endpoint_manager_base::endpoint_manager_base(routing_manager_base* const _rm,
                                             boost::asio::io_service& _io,
                                             const std::shared_ptr<configuration>& _configuration) :
    rm_(_rm),
    io_(_io),
    configuration_(_configuration){

}

std::shared_ptr<endpoint> endpoint_manager_base::create_local(client_t _client) {
    std::lock_guard<std::mutex> its_lock(local_endpoint_mutex_);
    return create_local_unlocked(_client);
}

void endpoint_manager_base::remove_local(client_t _client) {
    std::shared_ptr<endpoint> its_endpoint(find_local(_client));
    if (its_endpoint) {
        its_endpoint->register_error_handler(nullptr);
        its_endpoint->stop();
        VSOMEIP_INFO << "Client [" << std::hex << rm_->get_client() << "] is closing connection to ["
                      << std::hex << _client << "]";
        std::lock_guard<std::mutex> its_lock(local_endpoint_mutex_);
        local_endpoints_.erase(_client);
    }
}

std::shared_ptr<endpoint> endpoint_manager_base::find_or_create_local(client_t _client) {
    std::lock_guard<std::mutex> its_lock(local_endpoint_mutex_);
    std::shared_ptr<endpoint> its_endpoint(find_local_unlocked(_client));
    if (!its_endpoint) {
        its_endpoint = create_local_unlocked(_client);
        its_endpoint->start();
    }
    return (its_endpoint);
}

std::shared_ptr<endpoint> endpoint_manager_base::find_local(client_t _client) {
    std::lock_guard<std::mutex> its_lock(local_endpoint_mutex_);
    return find_local_unlocked(_client);
}

std::shared_ptr<endpoint> endpoint_manager_base::find_local(service_t _service,
        instance_t _instance) {
    return find_local(rm_->find_local_client(_service, _instance));
}


std::unordered_set<client_t> endpoint_manager_base::get_connected_clients() const {
    std::lock_guard<std::mutex> its_lock(local_endpoint_mutex_);
    std::unordered_set<client_t> clients;
    for (const auto& its_client : local_endpoints_) {
        clients.insert(its_client.first);
    }
    return clients;
}

std::shared_ptr<local_server_endpoint_impl> endpoint_manager_base::create_local_server(
        const std::shared_ptr<routing_host>& _routing_host) {
    std::shared_ptr<local_server_endpoint_impl> its_server_endpoint;
    std::stringstream its_path;
    its_path << utility::get_base_path(configuration_) << std::hex << rm_->get_client();
    const client_t its_client = rm_->get_client();
#ifdef _WIN32
    ::_unlink(its_path.str().c_str());
    int port = VSOMEIP_INTERNAL_BASE_PORT + its_client;
#else
    if (-1 == ::unlink(its_path.str().c_str()) && errno != ENOENT) {
        VSOMEIP_ERROR << "endpoint_manager_base::init_receiver unlink failed ("
                << its_path.str() << "): "<< std::strerror(errno);
    }
#endif
    try {
        its_server_endpoint = std::make_shared<local_server_endpoint_impl>(
                shared_from_this(), _routing_host,
#ifdef _WIN32
                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
#else
                boost::asio::local::stream_protocol_ext::endpoint(its_path.str()),
#endif
                io_,
                configuration_, false);
#ifdef _WIN32
        VSOMEIP_INFO << "Listening at " << port;
#else
        VSOMEIP_INFO << "Listening at " << its_path.str();
#endif
    } catch (const std::exception &e) {
        VSOMEIP_ERROR << "Local server endpoint creation failed. Client ID: "
                << std::hex << std::setw(4) << std::setfill('0') << its_client
#ifdef _WIN32
                << " Port: " << std::dec << port
#else
                << " Path: " << its_path.str()
#endif
                << " Reason: " << e.what();
    }
    return its_server_endpoint;
}

void endpoint_manager_base::on_connect(std::shared_ptr<endpoint> _endpoint) {
    rm_->on_connect(_endpoint);
}

void endpoint_manager_base::on_disconnect(std::shared_ptr<endpoint> _endpoint) {
    rm_->on_disconnect(_endpoint);
}

void endpoint_manager_base::on_error(
        const byte_t *_data, length_t _length, endpoint* const _receiver,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {
    (void)_data;
    (void)_length;
    (void)_receiver;
    (void)_remote_address;
    (void)_remote_port;
    // intentionally left blank
}

void endpoint_manager_base::release_port(uint16_t _port, bool _reliable) {
    (void)_port;
    (void)_reliable;
    // intentionally left blank
}

client_t endpoint_manager_base::get_client() const {
    return rm_->get_client();
}

std::map<client_t, std::shared_ptr<endpoint>>
endpoint_manager_base::get_local_endpoints() const {
    std::lock_guard<std::mutex> its_lock(local_endpoint_mutex_);
    return local_endpoints_;
}

void
endpoint_manager_base::log_client_states() const {
    std::vector<std::pair<client_t, size_t> > its_client_queue_sizes;
    std::stringstream its_log;

    {
        std::lock_guard<std::mutex> its_lock(local_endpoint_mutex_);
        for (const auto& e : local_endpoints_) {
            size_t its_queue_size = e.second->get_queue_size();
            if (its_queue_size > VSOMEIP_DEFAULT_QUEUE_WARN_SIZE) {
                its_client_queue_sizes.push_back(
                        std::make_pair(e.first, its_queue_size));
            }
        }
    }

    std::sort(its_client_queue_sizes.begin(), its_client_queue_sizes.end(),
            [](const std::pair<client_t, size_t> &_a,
               const std::pair<client_t, size_t> &_b) {
        return (_a.second > _b.second);
    });

    size_t its_max(std::min(size_t(10), its_client_queue_sizes.size()));
    for (size_t i = 0; i < its_max; i++) {
        its_log << std::hex << std::setw(4) << std::setfill('0')
                << its_client_queue_sizes[i].first << ":"
                << std::dec << its_client_queue_sizes[i].second;
        if (i < its_max-1)
            its_log << ", ";
    }

    if (its_log.str().length() > 0)
        VSOMEIP_WARNING << "ICQ: [" << its_log.str() << "]";
}

std::shared_ptr<endpoint> endpoint_manager_base::create_local_unlocked(client_t _client) {
    std::stringstream its_path;
    its_path << utility::get_base_path(configuration_) << std::hex << _client;
    std::shared_ptr<local_client_endpoint_impl> its_endpoint;

#ifdef _WIN32
    boost::asio::ip::address address = boost::asio::ip::address::from_string("127.0.0.1");
    int port = VSOMEIP_INTERNAL_BASE_PORT + _client;
    VSOMEIP_INFO << "Connecting to ["
        << std::hex << _client << "] at " << port;
#else
    VSOMEIP_INFO << "Client [" << std::hex << rm_->get_client() << "] is connecting to ["
            << std::hex << _client << "] at " << its_path.str();
#endif
    its_endpoint = std::make_shared<local_client_endpoint_impl>(
            shared_from_this(), rm_->shared_from_this(),
#ifdef _WIN32
            boost::asio::ip::tcp::endpoint(address, port)
#else
            boost::asio::local::stream_protocol::endpoint(its_path.str())
#endif
            , io_, configuration_);

    // Messages sent to the VSOMEIP_ROUTING_CLIENT are meant to be routed to
    // external devices. Therefore, its local endpoint must not be found by
    // a call to find_local. Thus it must not be inserted to the list of local
    // clients.
    if (_client != VSOMEIP_ROUTING_CLIENT) {
        local_endpoints_[_client] = its_endpoint;
    }
    rm_->register_client_error_handler(_client, its_endpoint);

    return its_endpoint;
}

std::shared_ptr<endpoint> endpoint_manager_base::find_local_unlocked(client_t _client) {
    std::shared_ptr<endpoint> its_endpoint;
    auto found_endpoint = local_endpoints_.find(_client);
    if (found_endpoint != local_endpoints_.end()) {
        its_endpoint = found_endpoint->second;
    }
    return (its_endpoint);
}

} // namespace vsomeip_v3

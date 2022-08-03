// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/endpoint_manager_impl.hpp"

#include <vsomeip/internal/logger.hpp>

#include "../include/udp_client_endpoint_impl.hpp"
#include "../include/udp_server_endpoint_impl.hpp"
#include "../include/tcp_client_endpoint_impl.hpp"
#include "../include/tcp_server_endpoint_impl.hpp"
#include "../include/local_server_endpoint_impl.hpp"
#include "../include/virtual_server_endpoint_impl.hpp"
#include "../include/endpoint_definition.hpp"
#include "../../routing/include/routing_manager_base.hpp"
#include "../../routing/include/routing_manager_impl.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../../security/include/security.hpp"
#include "../../utility/include/utility.hpp"
#include "../../utility/include/byteorder.hpp"


#include <forward_list>
#include <iomanip>

#ifndef WITHOUT_SYSTEMD
#include <systemd/sd-daemon.h>
#endif
#define SD_LISTEN_FDS_START 3

namespace vsomeip_v3 {

endpoint_manager_impl::endpoint_manager_impl(
        routing_manager_base* const _rm, boost::asio::io_service& _io,
        const std::shared_ptr<configuration>& _configuration) :
        endpoint_manager_base(_rm, _io, _configuration) {
}

std::shared_ptr<endpoint> endpoint_manager_impl::find_or_create_remote_client(
        service_t _service, instance_t _instance, bool _reliable) {
    std::shared_ptr<endpoint> its_endpoint;
    bool start_endpoint(false);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        its_endpoint = find_remote_client(_service, _instance, _reliable);
        if (!its_endpoint) {
            its_endpoint = create_remote_client(_service, _instance, _reliable);
            start_endpoint = true;
        }
    }
    if (start_endpoint && its_endpoint
            && configuration_->is_someip(_service, _instance)) {
        its_endpoint->start();
    }
    return its_endpoint;
}

void endpoint_manager_impl::find_or_create_remote_client(
        service_t _service, instance_t _instance) {
    std::shared_ptr<endpoint> its_reliable_endpoint;
    std::shared_ptr<endpoint> its_unreliable_endpoint;
    bool start_reliable_endpoint(false);
    bool start_unreliable_endpoint(false);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        its_reliable_endpoint = find_remote_client(_service, _instance, true);
        if (!its_reliable_endpoint) {
            its_reliable_endpoint = create_remote_client(_service, _instance, true);
            start_reliable_endpoint = true;
        }
        its_unreliable_endpoint = find_remote_client(_service, _instance, false);
        if (!its_unreliable_endpoint) {
            its_unreliable_endpoint = create_remote_client(_service, _instance, false);
            start_unreliable_endpoint = true;
        }
    }
    const bool is_someip = configuration_->is_someip(_service, _instance);
    if (start_reliable_endpoint && its_reliable_endpoint && is_someip) {
        its_reliable_endpoint->start();
    }
    if (start_unreliable_endpoint && its_unreliable_endpoint && is_someip) {
        its_unreliable_endpoint->start();
    }
}

void endpoint_manager_impl::is_remote_service_known(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor,
        const boost::asio::ip::address &_reliable_address,
        uint16_t _reliable_port, bool* _reliable_known,
        const boost::asio::ip::address &_unreliable_address,
        uint16_t _unreliable_port, bool* _unreliable_known) const {

    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto found_service = remote_service_info_.find(_service);
    if (found_service != remote_service_info_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            std::shared_ptr<endpoint_definition> its_definition;
            if (_reliable_port != ILLEGAL_PORT) {
                auto found_reliable = found_instance->second.find(true);
                if (found_reliable != found_instance->second.end()) {
                    its_definition = found_reliable->second;
                    if (its_definition->get_address() == _reliable_address
                            && its_definition->get_port() == _reliable_port) {
                        *_reliable_known = true;
                    } else {
                        VSOMEIP_WARNING << "Reliable service endpoint has changed: ["
                            << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                            << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                            << std::dec << static_cast<std::uint32_t>(_major) << "."
                            << std::dec << _minor << "] old: "
                            << its_definition->get_address().to_string() << ":"
                            << its_definition->get_port() << " new: "
                            << _reliable_address.to_string() << ":"
                            << _reliable_port;
                    }
                }
            }
            if (_unreliable_port != ILLEGAL_PORT) {
                auto found_unreliable = found_instance->second.find(false);
                if (found_unreliable != found_instance->second.end()) {
                    its_definition = found_unreliable->second;
                    if (its_definition->get_address() == _unreliable_address
                            && its_definition->get_port() == _unreliable_port) {
                        *_unreliable_known = true;
                    } else {
                        VSOMEIP_WARNING << "Unreliable service endpoint has changed: ["
                            << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                            << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                            << std::dec << static_cast<std::uint32_t>(_major) << "."
                            << std::dec << _minor << "] old: "
                            << its_definition->get_address().to_string() << ":"
                            << its_definition->get_port() << " new: "
                            << _unreliable_address.to_string() << ":"
                            << _unreliable_port;
                    }
                }
            }
        }
    }
}

void endpoint_manager_impl::add_remote_service_info(
        service_t _service, instance_t _instance,
        const std::shared_ptr<endpoint_definition>& _ep_definition) {

    std::shared_ptr<serviceinfo> its_info;
    std::shared_ptr<endpoint> its_endpoint;
    bool must_report(false);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        remote_service_info_[_service][_instance][_ep_definition->is_reliable()] =
            _ep_definition;

        if (_ep_definition->is_reliable()) {
            its_endpoint = find_remote_client(_service, _instance, true);
            must_report = (its_endpoint && its_endpoint->is_established_or_connected());
            if (must_report)
                its_info = rm_->find_service(_service, _instance);
        }
    }

    if (must_report)
        static_cast<routing_manager_impl*>(rm_)->service_endpoint_connected(
                _service, _instance, its_info->get_major(), its_info->get_minor(),
                its_endpoint, false);
}

void endpoint_manager_impl::add_remote_service_info(
        service_t _service, instance_t _instance,
        const std::shared_ptr<endpoint_definition>& _ep_definition_reliable,
        const std::shared_ptr<endpoint_definition>& _ep_definition_unreliable) {

    std::shared_ptr<serviceinfo> its_info;
    std::shared_ptr<endpoint> its_reliable, its_unreliable;
    bool must_report(false);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        remote_service_info_[_service][_instance][false] = _ep_definition_unreliable;
        remote_service_info_[_service][_instance][true] = _ep_definition_reliable;

        its_unreliable = find_remote_client(_service, _instance, false);
        its_reliable = find_remote_client(_service, _instance, true);

        must_report = (its_unreliable && its_unreliable->is_established_or_connected()
                && its_reliable && its_reliable->is_established_or_connected());

        if (must_report)
            its_info = rm_->find_service(_service, _instance);
    }

    if (must_report) {
        static_cast<routing_manager_impl*>(rm_)->service_endpoint_connected(
                _service, _instance, its_info->get_major(), its_info->get_minor(),
                its_unreliable, false);
        static_cast<routing_manager_impl*>(rm_)->service_endpoint_connected(
                _service, _instance, its_info->get_major(), its_info->get_minor(),
                its_reliable, false);
    }
}

void endpoint_manager_impl::clear_remote_service_info(service_t _service,
                                                      instance_t _instance,
                                                      bool _reliable) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    const auto found_service = remote_service_info_.find(_service);
    if (found_service != remote_service_info_.end()) {
        const auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            if (found_instance->second.erase(_reliable)) {
                if (!found_instance->second.size()) {
                    found_service->second.erase(found_instance);
                    if (!found_service->second.size()) {
                        remote_service_info_.erase(found_service);
                    }
                }
            }
        }
    }
}

std::shared_ptr<endpoint> endpoint_manager_impl::create_server_endpoint(
        uint16_t _port, bool _reliable, bool _start) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    std::shared_ptr<endpoint> its_endpoint;
    try {
        boost::asio::ip::address its_unicast = configuration_->get_unicast_address();
        const std::string its_unicast_str = its_unicast.to_string();
        if (_start) {
            if (_reliable) {
                its_endpoint = std::make_shared<tcp_server_endpoint_impl>(
                        shared_from_this(),
                        rm_->shared_from_this(),
                        boost::asio::ip::tcp::endpoint(its_unicast, _port),
                        io_,
                        configuration_);
                if (configuration_->has_enabled_magic_cookies(
                        its_unicast_str, _port) ||
                        configuration_->has_enabled_magic_cookies(
                                "local", _port)) {
                    its_endpoint->enable_magic_cookies();
                }
            } else {
                its_endpoint = std::make_shared<udp_server_endpoint_impl>(
                        shared_from_this(),
                        rm_->shared_from_this(),
                        boost::asio::ip::udp::endpoint(its_unicast, _port),
                        io_,
                        configuration_);
            }

        } else {
            its_endpoint = std::make_shared<virtual_server_endpoint_impl>(
                                its_unicast_str, _port, _reliable, io_);
        }

        if (its_endpoint) {
            server_endpoints_[_port][_reliable] = its_endpoint;
            its_endpoint->start();
        }
    } catch (const std::exception &e) {
        VSOMEIP_ERROR << __func__
                << " Server endpoint creation failed."
                << " Reason: "<< e.what()
                << " Port: " << _port
                << " (reliable="
                << (_reliable ? "reliable" : "unreliable")
                << ")";
    }

    return (its_endpoint);
}

std::shared_ptr<endpoint> endpoint_manager_impl::find_server_endpoint(
        uint16_t _port, bool _reliable) const {
    std::shared_ptr<endpoint> its_endpoint;
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto found_port = server_endpoints_.find(_port);
    if (found_port != server_endpoints_.end()) {
        auto found_endpoint = found_port->second.find(_reliable);
        if (found_endpoint != found_port->second.end()) {
            its_endpoint = found_endpoint->second;
        }
    }
    return (its_endpoint);
}

std::shared_ptr<endpoint> endpoint_manager_impl::find_or_create_server_endpoint(
        uint16_t _port, bool _reliable, bool _start,  service_t _service,
        instance_t _instance, bool &_is_found, bool _is_multicast) {
    std::shared_ptr<endpoint> its_endpoint = find_server_endpoint(_port,
            _reliable);
    _is_found = false;
    if (!its_endpoint) {
        its_endpoint = create_server_endpoint(_port, _reliable, _start);
    } else {
        _is_found = true;
    }
    if (its_endpoint) {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        if (!_is_multicast) {
            service_instances_[_service][its_endpoint.get()] =  _instance;
        }
        its_endpoint->increment_use_count();
    }
    return (its_endpoint);
}

bool endpoint_manager_impl::remove_server_endpoint(uint16_t _port, bool _reliable) {
    bool ret = false;
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto found_port = server_endpoints_.find(_port);
    if (found_port != server_endpoints_.end()) {
        auto found_reliable = found_port->second.find(_reliable);
        if (found_reliable != found_port->second.end()) {
            if (found_reliable->second->get_use_count() == 0 &&
                    found_port->second.erase(_reliable)) {
                ret = true;
                if (found_port->second.empty()) {
                    server_endpoints_.erase(found_port);
                }
            }
        }
    }
    return ret;
}

void endpoint_manager_impl::clear_client_endpoints(service_t _service, instance_t _instance,
        bool _reliable) {
    std::shared_ptr<endpoint> endpoint_to_delete;
    bool other_services_reachable_through_endpoint(false);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        // Clear client endpoints for remote services (generic and specific ones)
        const auto found_service = remote_services_.find(_service);
        if (found_service != remote_services_.end()) {
            const auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                const auto found_reliability = found_instance->second.find(_reliable);
                if (found_reliability != found_instance->second.end()) {
                    service_instances_[_service].erase(found_reliability->second.get());
                    endpoint_to_delete = found_reliability->second;

                    found_instance->second.erase(found_reliability);
                    if (found_instance->second.empty()) {
                        found_service->second.erase(found_instance);
                        if (found_service->second.empty()) {
                            remote_services_.erase(found_service);
                        }
                    }
                }
            }
        }

        // Only stop and delete the endpoint if none of the services
        // reachable through it is online anymore.
        if (endpoint_to_delete) {
            for (const auto& service : remote_services_) {
                for (const auto& instance : service.second) {
                    const auto found_reliability = instance.second.find(_reliable);
                    if (found_reliability != instance.second.end()
                            && found_reliability->second == endpoint_to_delete) {
                        other_services_reachable_through_endpoint = true;
                        break;
                    }
                }
                if (other_services_reachable_through_endpoint) { break; }
            }

            if (!other_services_reachable_through_endpoint) {
                std::uint16_t its_port(0);
                boost::asio::ip::address its_address;
                if (_reliable) {
                    std::shared_ptr<tcp_client_endpoint_impl> ep =
                            std::dynamic_pointer_cast<tcp_client_endpoint_impl>(endpoint_to_delete);
                    if (ep) {
                        its_port = ep->get_remote_port();
                        ep->get_remote_address(its_address);
                    }
                } else {
                    std::shared_ptr<udp_client_endpoint_impl> ep =
                            std::dynamic_pointer_cast<udp_client_endpoint_impl>(endpoint_to_delete);
                    if (ep) {
                        its_port = ep->get_remote_port();
                        ep->get_remote_address(its_address);
                    }
                }
                const auto found_ip = client_endpoints_by_ip_.find(its_address);
                if (found_ip != client_endpoints_by_ip_.end()) {
                    const auto found_port = found_ip->second.find(its_port);
                    if (found_port != found_ip->second.end()) {
                        const auto found_reliable = found_port->second.find(_reliable);
                        if (found_reliable != found_port->second.end()) {
                            if (found_reliable->second == endpoint_to_delete) {
                                found_port->second.erase(_reliable);
                                // delete if necessary
                                if (!found_port->second.size()) {
                                    found_ip->second.erase(found_port);
                                    if (!found_ip->second.size()) {
                                        client_endpoints_by_ip_.erase(found_ip);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!other_services_reachable_through_endpoint && endpoint_to_delete) {
        endpoint_to_delete->stop();
    }
}

void endpoint_manager_impl::find_or_create_multicast_endpoint(
        service_t _service, instance_t _instance,
        const boost::asio::ip::address &_sender,
        const boost::asio::ip::address &_address, uint16_t _port) {
    bool multicast_known(false);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        const auto found_service = multicast_info.find(_service);
        if (found_service != multicast_info.end()) {
            const auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                const auto& endpoint_def = found_instance->second;
                if (endpoint_def->get_address() == _address &&
                        endpoint_def->get_port() == _port) {
                    // Multicast info and endpoint already created before
                    // This can happen when more than one client subscribe on the same instance!
                    multicast_known = true;
                }
            }
        }
    }
    const bool is_someip = configuration_->is_someip(_service, _instance);
    bool _is_found(false);
    // Create multicast endpoint & join multicase group
    std::shared_ptr<endpoint> its_endpoint = find_or_create_server_endpoint(
            _port, false, is_someip, _service, _instance, _is_found, true);
    if (!_is_found) {
        // Only save multicast info if we created a new endpoint
        // to be able to delete the new endpoint
        // as soon as the instance stops offering its service
        std::shared_ptr<endpoint_definition> endpoint_def =
                endpoint_definition::get(_address, _port, false, _service, _instance);
        multicast_info[_service][_instance] = endpoint_def;
    }

    if (its_endpoint) {
        if (!multicast_known) {
            std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
            service_instances_multicast_[_service][_sender] = _instance;
        }
        dynamic_cast<udp_server_endpoint_impl*>(its_endpoint.get())->join_unlocked(
                _address.to_string());
    } else {
        VSOMEIP_ERROR <<"Could not find/create multicast endpoint!";
    }
}

void endpoint_manager_impl::clear_multicast_endpoints(service_t _service, instance_t _instance) {
    std::shared_ptr<endpoint> multicast_endpoint;
    std::string address;

    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        // Clear multicast info and endpoint and multicast instance (remote service)
        if (multicast_info.find(_service) != multicast_info.end()) {
            if (multicast_info[_service].find(_instance) != multicast_info[_service].end()) {
                address = multicast_info[_service][_instance]->get_address().to_string();
                uint16_t port = multicast_info[_service][_instance]->get_port();
                auto found_port = server_endpoints_.find(port);
                if (found_port != server_endpoints_.end()) {
                    auto found_unreliable = found_port->second.find(false);
                    if (found_unreliable != found_port->second.end()) {
                        multicast_endpoint = found_unreliable->second;
                        server_endpoints_[port].erase(false);
                    }
                    if (found_port->second.find(true) == found_port->second.end()) {
                        server_endpoints_.erase(port);
                    }
                }
                multicast_info[_service].erase(_instance);
                if (0 >= multicast_info[_service].size()) {
                    multicast_info.erase(_service);
                }
                (void)remove_instance_multicast(_service, _instance);
            }
        }
    }
    if (multicast_endpoint) {
        dynamic_cast<udp_server_endpoint_impl*>(
                multicast_endpoint.get())->leave(address);

        multicast_endpoint->stop();
    }
}

bool endpoint_manager_impl::supports_selective(service_t _service,
                                               instance_t _instance) const {
    bool supports_selective(false);
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    const auto its_service = remote_service_info_.find(_service);
    if (its_service != remote_service_info_.end()) {
        const auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            for (const auto& its_reliable : its_instance->second) {
                supports_selective |= configuration_->
                        supports_selective_broadcasts(
                                its_reliable.second->get_address());
            }
        }
    }
    return supports_selective;
}

void endpoint_manager_impl::print_status() const {
    // local client endpoints
    {
        std::map<client_t, std::shared_ptr<endpoint>> lces = get_local_endpoints();
        VSOMEIP_INFO << "status local client endpoints: " << std::dec << lces.size();
        for (const auto& lce : lces) {
            lce.second->print_status();
        }
    }

    // udp and tcp client endpoints
    {
        client_endpoints_by_ip_t client_endpoints_by_ip;
        server_endpoints_t server_endpoints;
        {
            std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
            client_endpoints_by_ip = client_endpoints_by_ip_;
            server_endpoints = server_endpoints_;
        }
        VSOMEIP_INFO << "status start remote client endpoints:";
        std::uint32_t num_remote_client_endpoints(0);
        // normal endpoints
        for (const auto &a : client_endpoints_by_ip) {
            for (const auto& p : a.second) {
                for (const auto& ru : p.second) {
                    ru.second->print_status();
                    num_remote_client_endpoints++;
                }
            }
        }
        VSOMEIP_INFO << "status end remote client endpoints: " << std::dec
                << num_remote_client_endpoints;

        VSOMEIP_INFO << "status start server endpoints:";
        std::uint32_t num_server_endpoints(1);
        // local server endpoints
        static_cast<routing_manager_impl*>(rm_)->print_stub_status();

        // server endpoints
        for (const auto& p : server_endpoints) {
            for (const auto& ru : p.second ) {
                ru.second->print_status();
                num_server_endpoints++;
            }
        }
        VSOMEIP_INFO << "status end server endpoints:"
                << std::dec << num_server_endpoints;
    }
}

std::shared_ptr<local_server_endpoint_impl>
endpoint_manager_impl::create_local_server(
        bool* _is_socket_activated,
        const std::shared_ptr<routing_host>& _routing_host) {
    std::shared_ptr<local_server_endpoint_impl> its_endpoint;
    std::stringstream its_endpoint_path_ss;
    its_endpoint_path_ss << utility::get_base_path(configuration_) << VSOMEIP_ROUTING_CLIENT;
    const std::string its_endpoint_path = its_endpoint_path_ss.str();
    client_t routing_host_id = configuration_->get_id(configuration_->get_routing_host());
    if (security::get()->is_enabled() && get_client() != routing_host_id) {
        VSOMEIP_ERROR << "endpoint_manager_impl::create_local_server: "
                << std::hex << "Client " << get_client() << " isn't allowed"
                << " to create the routing endpoint as its not configured as the routing master!";
        return its_endpoint;
    }
    uint32_t native_socket_fd = 0;
    int32_t num_fd = 0;
#ifndef WITHOUT_SYSTEMD
    num_fd = sd_listen_fds(0);
#endif
    if (num_fd > 1) {
        VSOMEIP_ERROR <<  "Too many file descriptors received by systemd socket activation! num_fd: " << num_fd;
    } else if (num_fd == 1) {
        native_socket_fd = SD_LISTEN_FDS_START + 0;
        VSOMEIP_INFO <<  "Using native socket created by systemd socket activation! fd: " << native_socket_fd;
        #ifndef _WIN32
            try {
                its_endpoint =
                        std::make_shared <local_server_endpoint_impl>(
                                shared_from_this(), _routing_host,
                                boost::asio::local::stream_protocol_ext::endpoint(its_endpoint_path),
                                io_,
                                native_socket_fd,
                                configuration_, true);
            } catch (const std::exception &e) {
                VSOMEIP_ERROR << "Server endpoint creation failed. Client ID: "
                        << std::hex << std::setw(4) << std::setfill('0')
                        << VSOMEIP_ROUTING_CLIENT << ": " << e.what();
            }
        #endif
        *_is_socket_activated = true;
    } else {
        #if _WIN32
            ::_unlink(its_endpoint_path.c_str());
            int port = VSOMEIP_INTERNAL_BASE_PORT;
            VSOMEIP_INFO << "Routing endpoint at " << port;
        #else
            if (-1 == ::unlink(its_endpoint_path.c_str()) && errno != ENOENT) {
                VSOMEIP_ERROR << "endpoint_manager_impl::create_local_server unlink failed ("
                        << its_endpoint_path << "): "<< std::strerror(errno);
            }
            VSOMEIP_INFO << __func__ << " Routing endpoint at " << its_endpoint_path;
        #endif

        try {
            its_endpoint =
                    std::make_shared <local_server_endpoint_impl>(
                            shared_from_this(), _routing_host,
                            #ifdef _WIN32
                                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
                            #else
                                boost::asio::local::stream_protocol_ext::endpoint(its_endpoint_path),
                            #endif
                                io_,
                                configuration_, true);
        } catch (const std::exception &e) {
            VSOMEIP_ERROR << "Server endpoint creation failed. Client ID: "
                    << std::hex << std::setw(4) << std::setfill('0')
                    << VSOMEIP_ROUTING_CLIENT << ": " << e.what();
        }
        *_is_socket_activated = false;
    }
    return its_endpoint;
}

instance_t endpoint_manager_impl::find_instance(
        service_t _service, endpoint* const _endpoint) const {
    instance_t its_instance(0xFFFF);
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto found_service = service_instances_.find(_service);
    if (found_service != service_instances_.end()) {
        auto found_endpoint = found_service->second.find(_endpoint);
        if (found_endpoint != found_service->second.end()) {
            its_instance = found_endpoint->second;
        }
    }
    return its_instance;
}

instance_t endpoint_manager_impl::find_instance_multicast(
        service_t _service, const boost::asio::ip::address &_sender) const {
    instance_t its_instance(0xFFFF);
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto found_service = service_instances_multicast_.find(_service);
    if (found_service != service_instances_multicast_.end()) {
        auto found_sender = found_service->second.find(_sender);
        if (found_sender != found_service->second.end()) {
            its_instance = found_sender->second;
        }
    }
    return its_instance;
}

bool endpoint_manager_impl::remove_instance(service_t _service,
                                            endpoint* const _endpoint) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto found_service = service_instances_.find(_service);
    if (found_service != service_instances_.end()) {
        if (found_service->second.erase(_endpoint)) {
            if (!found_service->second.size()) {
                service_instances_.erase(found_service);
            }
        }
    }
    _endpoint->decrement_use_count();
    return (_endpoint->get_use_count() == 0);
}

bool endpoint_manager_impl::remove_instance_multicast(service_t _service,
        instance_t _instance) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto found_service = service_instances_multicast_.find(_service);
    if (found_service != service_instances_multicast_.end()) {
        for (auto &its_sender : found_service->second) {
            if (its_sender.second == _instance) {
                if (found_service->second.erase(its_sender.first)) {
                    if (!found_service->second.size()) {
                        service_instances_multicast_.erase(_service);
                    }
                }
                return (true);
            }
        }
    }
    return (false);
}

void endpoint_manager_impl::on_connect(std::shared_ptr<endpoint> _endpoint) {
    // Is called when endpoint->connect succeeded!
    struct service_info {
        service_t service_id_;
        instance_t instance_id_;
        major_version_t major_;
        minor_version_t minor_;
        std::shared_ptr<endpoint> endpoint_;
        bool service_is_unreliable_only_;
    };

    // Set to state CONNECTED as connection is not yet fully established in remote side POV
    // but endpoint is ready to send / receive. Set to ESTABLISHED after timer expires
    // to prevent inserting subscriptions twice or send out subscription before remote side
    // is finished with TCP 3 way handshake
    _endpoint->set_connected(true);

    std::forward_list<struct service_info> services_to_report_;
    {
        const bool endpoint_is_reliable = _endpoint->is_reliable();
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        for (auto &its_service : remote_services_) {
            for (auto &its_instance : its_service.second) {
                auto found_endpoint = its_instance.second.find(endpoint_is_reliable);
                if (found_endpoint != its_instance.second.end()) {
                    if (found_endpoint->second == _endpoint) {
                        std::shared_ptr<serviceinfo> its_info(
                                rm_->find_service(its_service.first,
                                        its_instance.first));
                        if (!its_info) {
                            _endpoint->set_established(true);
                            return;
                        }
                        // only report services offered via TCP+UDP when both
                        // endpoints are connected
                        const auto its_other_endpoint = its_info->get_endpoint(
                                !endpoint_is_reliable);

                        if (!its_other_endpoint || (its_other_endpoint
                             && its_other_endpoint->is_established_or_connected())) {
                            services_to_report_.push_front(
                                        { its_service.first,
                                                its_instance.first,
                                                its_info->get_major(),
                                                its_info->get_minor(),
                                                _endpoint,
                                                (!endpoint_is_reliable &&
                                                        !its_other_endpoint)});
                        }
                    }
                }
            }
        }
    }
    for (const auto &s : services_to_report_) {
        static_cast<routing_manager_impl*>(rm_)->service_endpoint_connected(
                s.service_id_, s.instance_id_, s.major_, s.minor_, s.endpoint_,
                s.service_is_unreliable_only_);
    }
    if (services_to_report_.empty()) {
        _endpoint->set_established(true);
    }
}

void endpoint_manager_impl::on_disconnect(std::shared_ptr<endpoint> _endpoint) {
    // Is called when endpoint->connect fails!
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    for (auto &its_service : remote_services_) {
        for (auto &its_instance : its_service.second) {
            const bool is_reliable = _endpoint->is_reliable();
            auto found_endpoint = its_instance.second.find(is_reliable);
            if (found_endpoint != its_instance.second.end()) {
                if (found_endpoint->second == _endpoint) {
                    std::shared_ptr<serviceinfo> its_info(
                            rm_->find_service(its_service.first,
                                    its_instance.first));
                    if(!its_info){
                        return;
                    }
                    if (!is_reliable) {
                        static_cast<routing_manager_impl*>(rm_)->on_availability(
                                its_service.first, its_instance.first,
                                false, its_info->get_major(),
                                its_info->get_minor());
                    }
                    static_cast<routing_manager_impl*>(rm_)->service_endpoint_disconnected(
                            its_service.first, its_instance.first,
                            its_info->get_major(),
                            its_info->get_minor(), _endpoint);
                }
            }
        }
    }
}

void endpoint_manager_impl::on_error(
        const byte_t *_data, length_t _length, endpoint* const _receiver,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {
    instance_t its_instance = 0;
    if (_length >= VSOMEIP_SERVICE_POS_MAX) {
        service_t its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        its_instance = find_instance(its_service, _receiver);
    }
    static_cast<routing_manager_impl*>(rm_)->send_error(
            return_code_e::E_MALFORMED_MESSAGE, _data, _length, its_instance,
            _receiver->is_reliable(), _receiver, _remote_address, _remote_port);
}

void endpoint_manager_impl::release_port(uint16_t _port, bool _reliable) {
    std::lock_guard<std::mutex> its_lock(used_client_ports_mutex_);
    used_client_ports_[_reliable].erase(_port);
}

std::shared_ptr<endpoint> endpoint_manager_impl::find_remote_client(
        service_t _service, instance_t _instance, bool _reliable) {
    std::shared_ptr<endpoint> its_endpoint;
    auto found_service = remote_services_.find(_service);
    if (found_service != remote_services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_reliability = found_instance->second.find(_reliable);
            if (found_reliability != found_instance->second.end()) {
                its_endpoint = found_reliability->second;
            }
        }
    }
    if (its_endpoint) {
        return its_endpoint;
    }

    // If another service is hosted on the same server_endpoint
    // reuse the existing client_endpoint.
    auto found_service_info = remote_service_info_.find(_service);
    if(found_service_info != remote_service_info_.end()) {
        auto found_instance = found_service_info->second.find(_instance);
        if(found_instance != found_service_info->second.end()) {
            auto found_reliable = found_instance->second.find(_reliable);
            if(found_reliable != found_instance->second.end()) {
                std::shared_ptr<endpoint_definition> its_ep_def =
                        found_reliable->second;
                auto found_address = client_endpoints_by_ip_.find(
                        its_ep_def->get_address());
                if(found_address != client_endpoints_by_ip_.end()) {
                    auto found_port = found_address->second.find(
                            its_ep_def->get_remote_port());
                    if(found_port != found_address->second.end()) {
                        auto found_reliable2 = found_port->second.find(
                                _reliable);
                        if(found_reliable2 != found_port->second.end()) {
                            its_endpoint = found_reliable2->second;
                            // store the endpoint under this service/instance id
                            // as well - needed for later cleanup
                            remote_services_[_service][_instance][_reliable] =
                                    its_endpoint;
                            service_instances_[_service][its_endpoint.get()] = _instance;
                            // add endpoint to serviceinfo object
                            auto found_service_info = rm_->find_service(_service,_instance);
                            if (found_service_info) {
                                found_service_info->set_endpoint(its_endpoint, _reliable);
                            }
                        }
                    }
                }
            }
        }
    }
    return its_endpoint;
}

std::shared_ptr<endpoint> endpoint_manager_impl::create_remote_client(
        service_t _service, instance_t _instance, bool _reliable) {
    std::shared_ptr<endpoint> its_endpoint;
    std::shared_ptr<endpoint_definition> its_endpoint_def;
    uint16_t its_local_port;
    uint16_t its_remote_port = ILLEGAL_PORT;

    auto found_service = remote_service_info_.find(_service);
    if (found_service != remote_service_info_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_reliability = found_instance->second.find(_reliable);
            if (found_reliability != found_instance->second.end()) {
                its_endpoint_def = found_reliability->second;
                its_remote_port = its_endpoint_def->get_port();
            }
        }
    }

    if( its_remote_port != ILLEGAL_PORT) {
        // if client port range for remote service port range is configured
        // and remote port is in range, determine unused client port
        std::unique_lock<std::mutex> its_lock(used_client_ports_mutex_);
        if (configuration_->get_client_port(_service, _instance, its_remote_port, _reliable,
                used_client_ports_, its_local_port)) {
            if(its_endpoint_def) {
                its_endpoint = create_client_endpoint(
                        its_endpoint_def->get_address(),
                        its_local_port,
                        its_endpoint_def->get_port(),
                        _reliable);
            }

            if (its_endpoint) {
                used_client_ports_[_reliable].insert(its_local_port);
                its_lock.unlock();
                service_instances_[_service][its_endpoint.get()] = _instance;
                remote_services_[_service][_instance][_reliable] = its_endpoint;

                client_endpoints_by_ip_[its_endpoint_def->get_address()]
                                       [its_endpoint_def->get_port()]
                                       [_reliable] = its_endpoint;
                // Set the basic route to the service in the service info
                auto found_service_info = rm_->find_service(_service, _instance);
                if (found_service_info) {
                    found_service_info->set_endpoint(its_endpoint, _reliable);
                }
            }
        }
    }
    return its_endpoint;
}

std::shared_ptr<endpoint> endpoint_manager_impl::create_client_endpoint(
        const boost::asio::ip::address &_address,
        uint16_t _local_port, uint16_t _remote_port,
        bool _reliable) {

    std::shared_ptr<endpoint> its_endpoint;
    boost::asio::ip::address its_unicast = configuration_->get_unicast_address();

    try {
        if (_reliable) {
            its_endpoint = std::make_shared<tcp_client_endpoint_impl>(
                    shared_from_this(),
                    rm_->shared_from_this(),
                    boost::asio::ip::tcp::endpoint(its_unicast, _local_port),
                    boost::asio::ip::tcp::endpoint(_address, _remote_port),
                    io_,
                    configuration_);

            if (configuration_->has_enabled_magic_cookies(_address.to_string(),
                    _remote_port)) {
                its_endpoint->enable_magic_cookies();
            }
        } else {
            its_endpoint = std::make_shared<udp_client_endpoint_impl>(
                    shared_from_this(),
                    rm_->shared_from_this(),
                    boost::asio::ip::udp::endpoint(its_unicast, _local_port),
                    boost::asio::ip::udp::endpoint(_address, _remote_port),
                    io_,
                    configuration_);
        }
    } catch (...) {
        VSOMEIP_ERROR << __func__ << " Client endpoint creation failed";
    }

    return (its_endpoint);
}

void
endpoint_manager_impl::log_client_states() const {
    std::stringstream its_log;
    client_endpoints_by_ip_t its_client_endpoints;
    std::vector<
        std::pair<
            std::tuple<boost::asio::ip::address, uint16_t, bool>,
            size_t
        >
    > its_client_queue_sizes;

    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        its_client_endpoints = client_endpoints_by_ip_;
    }

    for (const auto& its_address : its_client_endpoints) {
        for (const auto& its_port : its_address.second) {
            for (const auto& its_reliability : its_port.second) {
                size_t its_queue_size = its_reliability.second->get_queue_size();
                if (its_queue_size > VSOMEIP_DEFAULT_QUEUE_WARN_SIZE)
                    its_client_queue_sizes.push_back(
                        std::make_pair(
                            std::make_tuple(
                                its_address.first,
                                its_port.first,
                                its_reliability.first),
                            its_queue_size));
            }
        }
    }

    std::sort(its_client_queue_sizes.begin(), its_client_queue_sizes.end(),
                [](const std::pair<
                        std::tuple<boost::asio::ip::address, uint16_t, bool>,
                        size_t> &_a,
                   const std::pair<
                       std::tuple<boost::asio::ip::address, uint16_t, bool>,
                       size_t> &_b) {
            return (_a.second > _b.second);
        });

    size_t its_max(std::min(size_t(5), its_client_queue_sizes.size()));
    for (size_t i = 0; i < its_max; i++) {
        its_log << std::hex << std::setw(4) << std::setfill('0')
                << std::get<0>(its_client_queue_sizes[i].first).to_string()
                << ":" << std::dec << std::get<1>(its_client_queue_sizes[i].first)
                << "(" << (std::get<2>(its_client_queue_sizes[i].first) ? "tcp" : "udp") << "):"
                << std::dec << its_client_queue_sizes[i].second;
        if (i < its_max-1)
            its_log << ", ";
    }

    if (its_log.str().length() > 0)
        VSOMEIP_INFO << "ECQ: [" << its_log.str() << "]";
}

void
endpoint_manager_impl::log_server_states() const {
    std::stringstream its_log;
    server_endpoints_t its_server_endpoints;
    std::vector<
        std::pair<
            std::pair<uint16_t, bool>,
            size_t
        >
    > its_client_queue_sizes;

    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        its_server_endpoints = server_endpoints_;
    }

    for (const auto& its_port : its_server_endpoints) {
        for (const auto& its_reliability : its_port.second) {
            size_t its_queue_size = its_reliability.second->get_queue_size();
            if (its_queue_size > VSOMEIP_DEFAULT_QUEUE_WARN_SIZE)
                its_client_queue_sizes.push_back(
                    std::make_pair(
                        std::make_pair(
                            its_port.first,
                            its_reliability.first),
                        its_queue_size));
        }
    }

    std::sort(its_client_queue_sizes.begin(), its_client_queue_sizes.end(),
                [](const std::pair<std::pair<uint16_t, bool>, size_t> &_a,
                   const std::pair<std::pair<uint16_t, bool>, size_t> &_b) {
            return (_a.second > _b.second);
        });

    size_t its_max(std::min(size_t(5), its_client_queue_sizes.size()));
    for (size_t i = 0; i < its_max; i++) {
        its_log << std::dec << its_client_queue_sizes[i].first.first
                << "(" << (its_client_queue_sizes[i].first.second ? "tcp" : "udp") << "):"
                << std::dec << its_client_queue_sizes[i].second;
        if (i < its_max-1)
            its_log << ", ";
    }

    if (its_log.str().length() > 0)
        VSOMEIP_INFO << "ESQ: [" << its_log.str() << "]";
}

} // namespace vsomeip_v3

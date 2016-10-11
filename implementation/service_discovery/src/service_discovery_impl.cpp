// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include <forward_list>

#include "../include/constants.hpp"
#include "../include/defines.hpp"
#include "../include/deserializer.hpp"
#include "../include/enumeration_types.hpp"
#include "../include/eventgroupentry_impl.hpp"
#include "../include/ipv4_option_impl.hpp"
#include "../include/ipv6_option_impl.hpp"
#include "../include/message_impl.hpp"
#include "../include/request.hpp"
#include "../include/runtime.hpp"
#include "../include/service_discovery_fsm.hpp"
#include "../include/service_discovery_host.hpp"
#include "../include/service_discovery_impl.hpp"
#include "../include/serviceentry_impl.hpp"
#include "../include/subscription.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/endpoint.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/serializer.hpp"
#include "../../routing/include/eventgroupinfo.hpp"
#include "../../routing/include/serviceinfo.hpp"

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(service_discovery_host *_host)
        : io_(_host->get_io()),
          host_(_host),
          serializer_(std::make_shared<serializer>()),
          deserializer_(std::make_shared<deserializer>()),
          ttl_timer_(_host->get_io()),
          smallest_ttl_(DEFAULT_TTL),
          subscription_expiration_timer_(_host->get_io()) {
    std::chrono::seconds smallest_ttl(DEFAULT_TTL);
    smallest_ttl_ = std::chrono::duration_cast<std::chrono::milliseconds>(smallest_ttl);

    // TODO: cleanup start condition!
    next_subscription_expiration_ = std::chrono::steady_clock::now() + std::chrono::hours(24);
}

service_discovery_impl::~service_discovery_impl() {
}

std::shared_ptr<configuration> service_discovery_impl::get_configuration() const {
    return host_->get_configuration();
}

boost::asio::io_service & service_discovery_impl::get_io() {
    return io_;
}

void service_discovery_impl::init() {
    runtime_ = runtime::get();
    default_ = std::make_shared<service_discovery_fsm>(shared_from_this());

    std::shared_ptr < configuration > its_configuration =
            host_->get_configuration();
    if (its_configuration) {
        unicast_ = its_configuration->get_unicast_address();

        port_ = its_configuration->get_sd_port();
        reliable_ = (its_configuration->get_sd_protocol()
                == "tcp");
        max_message_size_ = (reliable_ ? VSOMEIP_MAX_TCP_SD_PAYLOAD :
                VSOMEIP_MAX_UDP_SD_PAYLOAD);

        serializer_->create_data(
                reliable_ ?
                        VSOMEIP_MAX_TCP_MESSAGE_SIZE :
                        VSOMEIP_MAX_UDP_MESSAGE_SIZE);

        endpoint_ = host_->create_service_discovery_endpoint(
                its_configuration->get_sd_multicast(), port_, reliable_);

        ttl_ = its_configuration->get_sd_ttl();
    } else {
        VSOMEIP_ERROR << "SD: no configuration found!";
    }
}

void service_discovery_impl::start() {
    default_->start();
    for (auto &its_group : additional_) {
        its_group.second->start();
    }

    default_->process(ev_none());
    for (auto &its_group : additional_) {
        its_group.second->process(ev_none());
    }
}

void service_discovery_impl::stop() {
}

void service_discovery_impl::request_service(service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        ttl_t _ttl) {
    bool is_new_request(true);
    {
        std::lock_guard<std::mutex> its_lock(requested_mutex_);
        auto find_service = requested_.find(_service);
        if (find_service != requested_.end()) {
            auto find_instance = find_service->second.find(_instance);
            if (find_instance != find_service->second.end()) {
                is_new_request = false;
                // TODO: check version and report errors
            } else {
                find_service->second[_instance] = std::make_shared < request
                        > (_major, _minor, _ttl);
            }
        } else {
            requested_[_service][_instance] = std::make_shared < request
                    > (_major, _minor, _ttl);
        }
    }
    if (is_new_request) {
        default_->process(ev_request_service());
    }
}

void service_discovery_impl::release_service(service_t _service,
        instance_t _instance) {
    auto find_service = requested_.find(_service);
    if (find_service != requested_.end()) {
        find_service->second.erase(_instance);
    }
}

std::shared_ptr<request>
service_discovery_impl::find_request(service_t _service, instance_t _instance) {
    auto find_service = requested_.find(_service);
    if (find_service != requested_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
            return find_instance->second;
        }
    }
    return nullptr;
}

void service_discovery_impl::subscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl, client_t _client,
        subscription_type_e _subscription_type) {
    uint8_t subscribe_count(0);
    {
        std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
        auto found_service = subscribed_.find(_service);
        if (found_service != subscribed_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    auto found_client = found_eventgroup->second.find(_client);
                    if (found_client != found_eventgroup->second.end()) {
                        if (found_client->second->get_major() == _major) {
                            found_client->second->set_ttl(_ttl);
                            found_client->second->set_expiration(std::chrono::steady_clock::now()
                                + std::chrono::seconds(_ttl));
                        } else {
                            VSOMEIP_ERROR
                                    << "Subscriptions to different versions of the same "
                                            "service instance are not supported!";
                        }
                        return;
                    }
                }
            }
        }

        const uint8_t max_parallel_subscriptions = 16; // 4Bit Counter field
        subscribe_count = static_cast<uint8_t>(subscribed_[_service][_instance][_eventgroup].size());
        if (subscribe_count >= max_parallel_subscriptions) {
            VSOMEIP_WARNING << "Too many parallel subscriptions (max.16) on same event group: "
                            << std::hex << _eventgroup << std::dec;
            return;
        }
    }

    std::shared_ptr < endpoint > its_unreliable;
    std::shared_ptr < endpoint > its_reliable;
    bool has_address(false);
    boost::asio::ip::address its_address;

    get_subscription_endpoints(_subscription_type, its_unreliable, its_reliable,
            &its_address, &has_address, _service, _instance, _client);

    std::shared_ptr<runtime> its_runtime = runtime_.lock();
    if (!its_runtime) {
        return;
    }
    std::shared_ptr<message_impl> its_message = its_runtime->create_message();
    {
        std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
        // New subscription
        std::shared_ptr < subscription > its_subscription = std::make_shared
                < subscription > (_major, _ttl, its_reliable, its_unreliable,
                        _subscription_type, subscribe_count,
                        std::chrono::steady_clock::time_point() + std::chrono::seconds(_ttl));
        subscribed_[_service][_instance][_eventgroup][_client] = its_subscription;
        if (has_address) {

            if (_client != VSOMEIP_ROUTING_CLIENT) {
                if (its_subscription->get_endpoint(true) &&
                        !host_->has_identified(_client, _service, _instance, true)) {
                    return;
                }
                if (its_subscription->get_endpoint(false) &&
                        !host_->has_identified(_client, _service, _instance, false)) {
                    return;
                }
            }

            if (its_subscription->get_endpoint(true)
                    && its_subscription->get_endpoint(true)->is_connected()) {
                insert_subscription(its_message,
                        _service, _instance,
                        _eventgroup,
                        its_subscription, true, true);
            } else {
                // don't insert reliable endpoint option if the
                // TCP client endpoint is not yet connected
                insert_subscription(its_message,
                        _service, _instance,
                        _eventgroup,
                        its_subscription, false, true);
                its_subscription->set_tcp_connection_established(false);
            }
            if(0 < its_message->get_entries().size()) {
                its_subscription->set_acknowledged(false);
            }
        }
    }
    if(has_address && 0 < its_message->get_entries().size()) {
        serialize_and_send(its_message, its_address);
    }
}

void service_discovery_impl::get_subscription_endpoints(
        subscription_type_e _subscription_type,
        std::shared_ptr<endpoint>& _unreliable,
        std::shared_ptr<endpoint>& _reliable, boost::asio::ip::address* _address,
        bool* _has_address,
        service_t _service, instance_t _instance, client_t _client) const {
    switch (_subscription_type) {
        case subscription_type_e::SU_RELIABLE_AND_UNRELIABLE:
            _reliable = host_->find_or_create_remote_client(_service, _instance,
                    true, _client);
            _unreliable = host_->find_or_create_remote_client(_service,
                    _instance, false, _client);
            if (_unreliable) {
                *_has_address = _unreliable->get_remote_address(*_address);
            }
            if (_reliable) {
                *_has_address = *_has_address
                        || _reliable->get_remote_address(*_address);
            }
            break;
        case subscription_type_e::SU_PREFER_UNRELIABLE:
            _unreliable = host_->find_or_create_remote_client(_service,
                    _instance, false, _client);
            if (_unreliable) {
                *_has_address = _unreliable->get_remote_address(*_address);
            } else {
                _reliable = host_->find_or_create_remote_client(_service,
                        _instance, true, _client);
                if (_reliable) {
                    *_has_address = _reliable->get_remote_address(*_address);
                }
            }
            break;
        case subscription_type_e::SU_PREFER_RELIABLE:
            _reliable = host_->find_or_create_remote_client(_service,
                        _instance, true, _client);
            if (_reliable) {
                *_has_address = _reliable->get_remote_address(*_address);
            } else {
            _unreliable = host_->find_or_create_remote_client(_service,
                    _instance, false, _client);
                if (_unreliable) {
                    *_has_address = _unreliable->get_remote_address(*_address);
                }
            }
            break;
        case subscription_type_e::SU_UNRELIABLE:
            _unreliable = host_->find_or_create_remote_client(_service,
                    _instance,
                    false, _client);
            if (_unreliable) {
                *_has_address = _unreliable->get_remote_address(*_address);
            }
            break;
        case subscription_type_e::SU_RELIABLE:
            _reliable = host_->find_or_create_remote_client(_service, _instance,
            true, _client);
            if (_reliable) {
                *_has_address = _reliable->get_remote_address(*_address);
            }
    }
}

void service_discovery_impl::unsubscribe(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, client_t _client) {
    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if(!its_runtime) {
        return;
    }
    std::shared_ptr < message_impl > its_message = its_runtime->create_message();
    boost::asio::ip::address its_address;
    bool has_address(false);
    {
        std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
        std::shared_ptr < subscription > its_subscription;
        auto found_service = subscribed_.find(_service);
        if (found_service != subscribed_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    auto found_client = found_eventgroup->second.find(_client);
                    if (found_client != found_eventgroup->second.end()) {
                        its_subscription = found_client->second;
                        its_subscription->set_ttl(0);
                        found_eventgroup->second.erase(_client);
                        auto endpoint = its_subscription->get_endpoint(false);
                        if (endpoint) {
                            has_address = endpoint->get_remote_address(its_address);
                        } else {
                            endpoint = its_subscription->get_endpoint(true);
                            if (endpoint) {
                                has_address = endpoint->get_remote_address(its_address);
                            } else {
                                return;
                            }
                        }
                        insert_subscription(its_message, _service, _instance, _eventgroup,
                                            its_subscription, true, true);
                    }
                }
            }
        }
    }
    if (has_address && 0 < its_message->get_entries().size()) {
        serialize_and_send(its_message, its_address);
    }
}

void service_discovery_impl::unsubscribe_all(service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            for (auto &its_eventgroup : found_instance->second) {
                for (auto its_client : its_eventgroup.second) {
                    its_client.second->set_acknowledged(true);
                    its_client.second->set_endpoint(nullptr, true);
                    its_client.second->set_endpoint(nullptr, false);
                }
            }
        }
    }
}

std::pair<session_t, bool> service_discovery_impl::get_session(
        const boost::asio::ip::address &_address) {
    std::pair<session_t, bool> its_session;
    auto found_session = sessions_sent_.find(_address);
    if (found_session == sessions_sent_.end()) {
        its_session = sessions_sent_[_address] = { 1, true };
    } else {
        its_session = found_session->second;
    }
    return its_session;
}

void service_discovery_impl::increment_session(
        const boost::asio::ip::address &_address) {
    auto found_session = sessions_sent_.find(_address);
    if (found_session != sessions_sent_.end()) {
        found_session->second.first++;
        if (found_session->second.first == 0) {
            found_session->second = { 1, false };
        }
    }
}

bool service_discovery_impl::is_reboot(
        const boost::asio::ip::address &_sender,
        const boost::asio::ip::address &_destination,
        bool _reboot_flag, session_t _session) {
    bool result(false);

    auto its_last_session = sessions_received_.find(_sender);
    bool is_multicast = _destination.is_multicast();

    session_t its_unicast_id = (is_multicast ? 0 : _session);
    session_t its_multicast_id = (is_multicast ? _session : 0);

    if (its_last_session == sessions_received_.end()) {
        sessions_received_[_sender]
            = std::make_tuple(its_multicast_id, its_unicast_id, _reboot_flag);
    } else {
        // Reboot detection: Either the flag has changed from false to true,
        // or the session identifier overrun while the flag is true
        if (its_last_session != sessions_received_.end()) {
            if (!std::get<2>(its_last_session->second) && _reboot_flag) {
                result = true;
            } else {
                session_t its_last_id = (is_multicast ?
                                         std::get<0>(its_last_session->second) :
                                         std::get<1>(its_last_session->second));

                if (std::get<2>(its_last_session->second) && _reboot_flag &&
                        its_last_id >= _session) {
                    result = true;
                }
            }
        }

        if (result == false) {
            // no reboot -> update session
            if (is_multicast) {
                std::get<0>(its_last_session->second) = its_multicast_id;
            } else {
                std::get<1>(its_last_session->second) = its_unicast_id;
            }
        } else {
            // reboot -> reset the session
            sessions_received_.erase(_sender);
        }
    }

    return result;
}

template<class Option, typename AddressType>
std::shared_ptr<option_impl> service_discovery_impl::find_existing_option(
    std::shared_ptr<message_impl> &_message,
    AddressType _address,
    uint16_t _port, layer_four_protocol_e _protocol,
    option_type_e _option_type) {
    if (_message->get_options().size() > 0) {
        std::uint16_t option_length(0x0);
        if(_option_type == option_type_e::IP4_ENDPOINT ||
           _option_type == option_type_e::IP4_MULTICAST) {
            option_length = 0x9;
        } else if(_option_type == option_type_e::IP6_ENDPOINT ||
                  _option_type == option_type_e::IP6_MULTICAST) {
            option_length = 0x15;
        } else { // unsupported option type
            return nullptr;
        }

        bool is_multicast(false);
        if(_option_type == option_type_e::IP4_MULTICAST ||
           _option_type == option_type_e::IP6_MULTICAST) {
            is_multicast = true;
        }

        std::vector<std::shared_ptr<option_impl>> its_options =
                _message->get_options();
        for (const std::shared_ptr<option_impl>& opt : its_options) {
            if (opt->get_length() == option_length &&
                opt->get_type() == _option_type &&
                std::static_pointer_cast<ip_option_impl>(opt)->get_layer_four_protocol() == _protocol &&
                std::static_pointer_cast<ip_option_impl>(opt)->get_port() == _port &&
                std::static_pointer_cast<Option>(opt)->is_multicast() == is_multicast &&
                std::static_pointer_cast<Option>(opt)->get_address() == _address) {
                return opt;
            }
        }
    }
    return nullptr;
}
template<class Option, typename AddressType>
bool service_discovery_impl::check_message_for_ip_option_and_assign_existing(
        std::shared_ptr<message_impl> &_message,
        std::shared_ptr<entry_impl> _entry, AddressType _address,
        uint16_t _port, layer_four_protocol_e _protocol,
        option_type_e _option_type) {

    std::shared_ptr<option_impl> its_option
        = find_existing_option<Option, AddressType>(_message, _address, _port, _protocol, _option_type);
    if (its_option) {
        _entry->assign_option(its_option);
        return true;
    }
    return false;
}

template<class Option, typename AddressType>
void service_discovery_impl::assign_ip_option_to_entry(
        std::shared_ptr<Option> _option, AddressType _address,
        uint16_t _port, layer_four_protocol_e _protocol,
        std::shared_ptr<entry_impl> _entry) {
    if (_option) {
        _option->set_address(_address);
        _option->set_port(_port);
        _option->set_layer_four_protocol(_protocol);
        _entry->assign_option(_option);
    }
}

void service_discovery_impl::insert_option(
        std::shared_ptr<message_impl> &_message,
        std::shared_ptr<entry_impl> _entry,
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _is_reliable) {
    layer_four_protocol_e its_protocol =
            _is_reliable ? layer_four_protocol_e::TCP :
                    layer_four_protocol_e::UDP;
    bool entry_assigned(false);

    if (unicast_ == _address) {
        if (unicast_.is_v4()) {
            ipv4_address_t its_address = unicast_.to_v4().to_bytes();
            entry_assigned = check_message_for_ip_option_and_assign_existing<
                    ipv4_option_impl, ipv4_address_t>(_message, _entry,
                    its_address, _port, its_protocol,
                    option_type_e::IP4_ENDPOINT);
            if(!entry_assigned) {
                std::shared_ptr < ipv4_option_impl > its_option =
                        _message->create_ipv4_option(false);
                assign_ip_option_to_entry<ipv4_option_impl, ipv4_address_t>(
                        its_option, its_address, _port, its_protocol, _entry);
            }
        } else {
            ipv6_address_t its_address = unicast_.to_v6().to_bytes();
            entry_assigned = check_message_for_ip_option_and_assign_existing<
                    ipv6_option_impl, ipv6_address_t>(_message, _entry,
                    its_address, _port, its_protocol,
                    option_type_e::IP6_ENDPOINT);
            if(!entry_assigned) {
                std::shared_ptr < ipv6_option_impl > its_option =
                        _message->create_ipv6_option(false);
                assign_ip_option_to_entry<ipv6_option_impl, ipv6_address_t>(
                        its_option, its_address, _port, its_protocol, _entry);
            }
        }
    } else {
        if (_address.is_v4()) {
            ipv4_address_t its_address = _address.to_v4().to_bytes();
            entry_assigned = check_message_for_ip_option_and_assign_existing<
                    ipv4_option_impl, ipv4_address_t>(_message, _entry,
                    its_address, _port, its_protocol,
                    option_type_e::IP4_MULTICAST);
            if(!entry_assigned) {
                std::shared_ptr < ipv4_option_impl > its_option =
                        _message->create_ipv4_option(true);
                assign_ip_option_to_entry<ipv4_option_impl, ipv4_address_t>(
                        its_option, its_address, _port, its_protocol, _entry);
            }
        } else {
            ipv6_address_t its_address = _address.to_v6().to_bytes();
            entry_assigned = check_message_for_ip_option_and_assign_existing<
                    ipv6_option_impl, ipv6_address_t>(_message, _entry,
                    its_address, _port, its_protocol,
                    option_type_e::IP6_MULTICAST);
            if(!entry_assigned) {
                std::shared_ptr < ipv6_option_impl > its_option =
                        _message->create_ipv6_option(true);
                assign_ip_option_to_entry<ipv6_option_impl, ipv6_address_t>(
                        its_option, its_address, _port, its_protocol, _entry);
            }
        }
    }
}

void service_discovery_impl::insert_find_entries(
        std::shared_ptr<message_impl> &_message, requests_t &_requests,
        uint32_t _start, uint32_t &_size, bool &_done) {
    std::lock_guard<std::mutex> its_lock(requested_mutex_);
    uint32_t its_size(0);
    uint32_t i = 0;

    _done = true;
    for (auto its_service : _requests) {
        for (auto its_instance : its_service.second) {
            auto its_request = its_instance.second;
            uint8_t its_sent_counter = its_request->get_sent_counter();
            if (its_sent_counter != default_->get_repetition_max() + 1) {
                if (i >= _start) {
                    if (its_size + VSOMEIP_SOMEIP_SD_ENTRY_SIZE <= max_message_size_) {
                        std::shared_ptr < serviceentry_impl > its_entry =
                                _message->create_service_entry();
                        if (its_entry) {
                            its_entry->set_type(entry_type_e::FIND_SERVICE);
                            its_entry->set_service(its_service.first);
                            its_entry->set_instance(its_instance.first);
                            its_entry->set_major_version(its_request->get_major());
                            its_entry->set_minor_version(its_request->get_minor());
                            its_entry->set_ttl(its_request->get_ttl());
                            its_size += VSOMEIP_SOMEIP_SD_ENTRY_SIZE;
                            its_sent_counter++;

                            its_request->set_sent_counter(its_sent_counter);
                        } else {
                            VSOMEIP_ERROR << "Failed to create service entry!";
                        }
                    } else {
                        _done = false;
                        _size = its_size;
                        return;
                    }
                }
            }
            i++;
        }
    }
    _size = its_size;
}

void service_discovery_impl::insert_offer_entries(
        std::shared_ptr<message_impl> &_message, services_t &_services,
        uint32_t &_start, uint32_t _size, bool &_done) {
    uint32_t i = 0;
    uint32_t its_size(_size);
    for (auto its_service : _services) {
        for (auto its_instance : its_service.second) {
            // Only insert services with configured endpoint(s)
            if (its_instance.second->get_endpoint(false)
                    || its_instance.second->get_endpoint(true)) {
                if (i >= _start) {
                    if (!insert_offer_service(_message, its_service.first,
                            its_instance.first, its_instance.second, its_size)) {
                        _start = i;
                        _done = false;
                        return;
                    }
                }
            }
            i++;
        }
    }
    _start = i;
    _done = true;
}

void service_discovery_impl::insert_subscription(
        std::shared_ptr<message_impl> &_message, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<subscription> &_subscription,
        bool _insert_reliable, bool _insert_unreliable) {
    if((_insert_reliable && !_insert_unreliable && !_subscription->get_endpoint(true)) ||
       (_insert_unreliable && !_insert_reliable && !_subscription->get_endpoint(false))) {
        // don't create an eventgroup entry if there isn't an endpoint option
        // to insert
        return;
    }
    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_counter(_subscription->get_counter());
    its_entry->set_major_version(_subscription->get_major());
    its_entry->set_ttl(_subscription->get_ttl());
    std::shared_ptr < endpoint > its_endpoint;
    if (_insert_reliable) {
        its_endpoint = _subscription->get_endpoint(true);
        if (its_endpoint) {
            insert_option(_message, its_entry, unicast_,
                    its_endpoint->get_local_port(), true);
        }
    }
    if (_insert_unreliable) {
        its_endpoint = _subscription->get_endpoint(false);
        if (its_endpoint) {
            insert_option(_message, its_entry, unicast_,
                    its_endpoint->get_local_port(), false);
        }
    }
}

void service_discovery_impl::insert_nack_subscription_on_resubscribe(std::shared_ptr<message_impl> &_message,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            std::shared_ptr<subscription> &_subscription) {

    // SIP_SD_844:
    // This method is used for not acknowledged subscriptions on renew subscription
    // Two entries: Stop subscribe & subscribe within one SD-Message
    // One option: Both entries reference it

    std::shared_ptr < eventgroupentry_impl > its_stop_entry =
            _message->create_eventgroup_entry();
    its_stop_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP);
    its_stop_entry->set_service(_service);
    its_stop_entry->set_instance(_instance);
    its_stop_entry->set_eventgroup(_eventgroup);
    its_stop_entry->set_counter(_subscription->get_counter());
    its_stop_entry->set_major_version(_subscription->get_major());
    its_stop_entry->set_ttl(0);

    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_counter(_subscription->get_counter());
    its_entry->set_major_version(_subscription->get_major());
    its_entry->set_ttl(_subscription->get_ttl());

    std::shared_ptr < endpoint > its_endpoint;
    its_endpoint = _subscription->get_endpoint(true);
    if (its_endpoint) {
        insert_option(_message, its_stop_entry, unicast_,
                its_endpoint->get_local_port(), true);
        insert_option(_message, its_entry, unicast_,
                its_endpoint->get_local_port(), true);
    }
    its_endpoint = _subscription->get_endpoint(false);
    if (its_endpoint) {
        insert_option(_message, its_stop_entry, unicast_,
                its_endpoint->get_local_port(), false);
        insert_option(_message, its_entry, unicast_,
                its_endpoint->get_local_port(), false);
    }
}

void service_discovery_impl::insert_subscription_ack(
        std::shared_ptr<message_impl> &_message, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<eventgroupinfo> &_info, ttl_t _ttl, uint8_t _counter, major_version_t _major, uint16_t _reserved) {
    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP_ACK);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_major_version(_major);
    its_entry->set_reserved(_reserved);
    its_entry->set_counter(_counter);
    // SWS_SD_00315
    its_entry->set_ttl(_ttl);

    boost::asio::ip::address its_address;
    uint16_t its_port;
    if (_info->get_multicast(its_address, its_port)) {
        insert_option(_message, its_entry, its_address, its_port, false);
    }
}

void service_discovery_impl::insert_subscription_nack(
        std::shared_ptr<message_impl> &_message, service_t _service,
                instance_t _instance, eventgroup_t _eventgroup,
                uint8_t _counter, major_version_t _major, uint16_t _reserved) {
    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    // SWS_SD_00316 and SWS_SD_00385
    its_entry->set_type(entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_major_version(_major);
    its_entry->set_reserved(_reserved);
    its_entry->set_counter(_counter);
    // SWS_SD_00432
    its_entry->set_ttl(0x0);
}

bool service_discovery_impl::send(bool _is_announcing, bool _is_find) {
    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (its_runtime) {
        std::vector< std::shared_ptr< message_impl > > its_messages;
        std::shared_ptr < message_impl > its_message;

        uint32_t its_remaining(max_message_size_);

        if (_is_find || !_is_announcing) {
            uint32_t its_start(0);
            uint32_t its_size(0);
            bool is_done(false);
            while (!is_done) {
                its_message = its_runtime->create_message();
                its_messages.push_back(its_message);

                insert_find_entries(its_message, requested_, its_start, its_size, is_done);
                its_start += its_size / VSOMEIP_SOMEIP_SD_ENTRY_SIZE;
            };
            its_remaining -= its_size;
        } else {
            its_message = its_runtime->create_message();
            its_messages.push_back(its_message);
        }

        if (!_is_find) {
            services_t its_offers = host_->get_offered_services();

            uint32_t its_start(0);
            bool is_done(false);
            while (!is_done) {
                insert_offer_entries(its_message, its_offers, its_start, its_remaining, is_done);
                if (!is_done) {
                    its_remaining = max_message_size_;
                    its_message = its_runtime->create_message();
                    its_messages.push_back(its_message);
                }
            }
        }

        // Serialize and send
        bool has_sent(false);
        {
            // lock serialize mutex here as well even if we don't use the
            // serializer as it's used to guard access to the sessions_sent_ map
            std::lock_guard<std::mutex> its_lock(serialize_mutex_);
            for (auto m : its_messages) {
                if (m->get_entries().size() > 0) {
                    std::pair<session_t, bool> its_session = get_session(unicast_);
                    m->set_session(its_session.first);
                    m->set_reboot_flag(its_session.second);
                    if (host_->send(VSOMEIP_SD_CLIENT, m, true)) {
                        increment_session(unicast_);
                    }
                    has_sent = true;
                }
            }
        }
        return has_sent;
    }
    return false;
}

// Interface endpoint_host
void service_discovery_impl::on_message(const byte_t *_data, length_t _length,
        const boost::asio::ip::address &_sender,
        const boost::asio::ip::address &_destination) {
#if 0
    std::stringstream msg;
    msg << "sdi::on_message: ";
    for (length_t i = 0; i < _length; ++i)
    msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    deserializer_->set_data(_data, _length);
    std::shared_ptr < message_impl
            > its_message(deserializer_->deserialize_sd_message());
    if (its_message) {
        // ignore all messages which are sent with invalid header fields
        if(!check_static_header_fields(its_message)) {
            return;
        }
        // Expire all subscriptions / services in case of reboot
        if (is_reboot(_sender, _destination,
                its_message->get_reboot_flag(), its_message->get_session())) {
            host_->expire_subscriptions(_sender);
            host_->expire_services(_sender);
        }

        std::chrono::milliseconds expired = stop_ttl_timer();
        smallest_ttl_ = host_->update_routing_info(expired);

        std::vector < std::shared_ptr<option_impl> > its_options =
                its_message->get_options();

        std::shared_ptr<runtime> its_runtime = runtime_.lock();
        if (!its_runtime) {
            return;
        }

        std::shared_ptr < message_impl > its_message_response
            = its_runtime->create_message();
        std::vector <accepted_subscriber_t> accepted_subscribers;

        for (auto its_entry : its_message->get_entries()) {
            if (its_entry->is_service_entry()) {
                std::shared_ptr < serviceentry_impl > its_service_entry =
                        std::dynamic_pointer_cast < serviceentry_impl
                                > (its_entry);
                bool its_unicast_flag = its_message->get_unicast_flag();
                process_serviceentry(its_service_entry, its_options, its_unicast_flag );
            } else {
                std::shared_ptr < eventgroupentry_impl > its_eventgroup_entry =
                        std::dynamic_pointer_cast < eventgroupentry_impl
                                > (its_entry);
                process_eventgroupentry( its_eventgroup_entry, its_options, its_message_response, accepted_subscribers);
            }
        }

        //send ACK / NACK if present
        if( 0 < its_message_response->get_entries().size() && its_message_response ) {
            serialize_and_send(its_message_response, _sender);
        }

        for( const auto &a : accepted_subscribers) {
            host_->on_subscribe(a.service_id, a.instance_id, a.eventgroup_, a.subscriber, a.target, a.its_expiration);
        }
        accepted_subscribers.clear();
        start_ttl_timer();
    } else {
        VSOMEIP_ERROR << "service_discovery_impl::on_message: deserialization error.";
        return;
    }
}

void service_discovery_impl::on_offer_change() {
    default_->process(ev_offer_change());
}

// Entry processing
void service_discovery_impl::process_serviceentry(
        std::shared_ptr<serviceentry_impl> &_entry,
        const std::vector<std::shared_ptr<option_impl> > &_options,
        bool _unicast_flag) {

    // Read service info from entry
    entry_type_e its_type = _entry->get_type();
    service_t its_service = _entry->get_service();
    instance_t its_instance = _entry->get_instance();
    major_version_t its_major = _entry->get_major_version();
    minor_version_t its_minor = _entry->get_minor_version();
    ttl_t its_ttl = _entry->get_ttl();

    // Read address info from options
    boost::asio::ip::address its_reliable_address;
    uint16_t its_reliable_port(ILLEGAL_PORT);

    boost::asio::ip::address its_unreliable_address;
    uint16_t its_unreliable_port(ILLEGAL_PORT);

    for (auto i : { 1, 2 }) {
        for (auto its_index : _entry->get_options(uint8_t(i))) {
            if( _options.size() > its_index ) {
                std::shared_ptr < option_impl > its_option = _options[its_index];

                switch (its_option->get_type()) {
                case option_type_e::IP4_ENDPOINT: {
                    std::shared_ptr < ipv4_option_impl > its_ipv4_option =
                            std::dynamic_pointer_cast < ipv4_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v4 its_ipv4_address(
                            its_ipv4_option->get_address());

                    if (its_ipv4_option->get_layer_four_protocol()
                            == layer_four_protocol_e::UDP) {


                        its_unreliable_address = its_ipv4_address;
                        its_unreliable_port = its_ipv4_option->get_port();
                    } else {
                        its_reliable_address = its_ipv4_address;
                        its_reliable_port = its_ipv4_option->get_port();
                    }
                    break;
                }
                case option_type_e::IP6_ENDPOINT: {
                    std::shared_ptr < ipv6_option_impl > its_ipv6_option =
                            std::dynamic_pointer_cast < ipv6_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v6 its_ipv6_address(
                            its_ipv6_option->get_address());

                    if (its_ipv6_option->get_layer_four_protocol()
                            == layer_four_protocol_e::UDP) {
                        its_unreliable_address = its_ipv6_address;
                        its_unreliable_port = its_ipv6_option->get_port();
                    } else {
                        its_reliable_address = its_ipv6_address;
                        its_reliable_port = its_ipv6_option->get_port();
                    }
                    break;
                }
                case option_type_e::IP4_MULTICAST:
                case option_type_e::IP6_MULTICAST:
                    break;
                case option_type_e::CONFIGURATION:
                    break;
                case option_type_e::UNKNOWN:
                default:
                    VSOMEIP_ERROR << "Unsupported service option";
                    break;
                }
            }
        }
    }

    if (0 < its_ttl) {
        switch(its_type) {
            case entry_type_e::FIND_SERVICE:
                process_findservice_serviceentry(its_service, its_instance,
                                                 its_major, its_minor, _unicast_flag);
                break;
            case entry_type_e::OFFER_SERVICE:
                process_offerservice_serviceentry(its_service, its_instance,
                        its_major, its_minor, its_ttl,
                        its_reliable_address, its_reliable_port,
                        its_unreliable_address, its_unreliable_port);
                break;
            case entry_type_e::UNKNOWN:
            default:
                VSOMEIP_ERROR << "Unsupported serviceentry type";
        }

    } else {
        std::shared_ptr<request> its_request = find_request(its_service, its_instance);
        if (its_request)
            its_request->set_sent_counter((uint8_t) (default_->get_repetition_max() + 1));

        unsubscribe_all(its_service, its_instance);
        host_->del_routing_info(its_service, its_instance,
                                (its_reliable_port != ILLEGAL_PORT),
                                (its_unreliable_port != ILLEGAL_PORT));
    }
}

void service_discovery_impl::process_offerservice_serviceentry(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, ttl_t _ttl,
        const boost::asio::ip::address &_reliable_address,
        uint16_t _reliable_port,
        const boost::asio::ip::address &_unreliable_address,
        uint16_t _unreliable_port) {
    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (!its_runtime)
        return;

    std::shared_ptr<request> its_request = find_request(_service, _instance);
    if (its_request)
        its_request->set_sent_counter((uint8_t) (default_->get_repetition_max() + 1));

    smallest_ttl_ = host_->add_routing_info(_service, _instance,
                            _major, _minor, _ttl,
                            _reliable_address, _reliable_port,
                            _unreliable_address, _unreliable_port);

    std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            if (0 < found_instance->second.size()) {
                std::shared_ptr<message_impl> its_message
                    = its_runtime->create_message();
                for (auto its_eventgroup : found_instance->second) {
                    for (auto its_client : its_eventgroup.second) {
                        std::shared_ptr<subscription> its_subscription(its_client.second);
                        std::shared_ptr<endpoint> its_unreliable;
                        std::shared_ptr<endpoint> its_reliable;
                        bool has_address(false);
                        boost::asio::ip::address its_address;
                        get_subscription_endpoints(
                                its_client.second->get_subscription_type(),
                                its_unreliable, its_reliable, &its_address,
                                &has_address, _service, _instance,
                                its_client.first);
                        its_subscription->set_endpoint(its_reliable, true);
                        its_subscription->set_endpoint(its_unreliable, false);
                        if (its_client.first != VSOMEIP_ROUTING_CLIENT) {
                            if (its_client.second->get_endpoint(true) &&
                                    !host_->has_identified(its_client.first, _service,
                                            _instance, true)) {
                                continue;
                            }
                            if (its_client.second->get_endpoint(false) &&
                                    !host_->has_identified(its_client.first, _service,
                                            _instance, false)) {
                                continue;
                            }
                        }
                        if (its_subscription->is_acknowledged()) {
                            if (its_subscription->get_endpoint(true)
                                    && its_subscription->get_endpoint(true)->is_connected()) {
                                insert_subscription(its_message,
                                        _service, _instance,
                                        its_eventgroup.first,
                                        its_subscription, true, true);
                            } else {
                                // don't insert reliable endpoint option if the
                                // TCP client endpoint is not yet connected
                                insert_subscription(its_message,
                                        _service, _instance,
                                        its_eventgroup.first,
                                        its_subscription, false, true);
                                its_client.second->set_tcp_connection_established(false);
                            }

                            its_subscription->set_acknowledged(false);
                        } else {
                            insert_nack_subscription_on_resubscribe(its_message,
                                    _service, _instance, its_eventgroup.first,
                                    its_subscription);
                        }
                    }
                }

                if (0 < its_message->get_entries().size()) {
                    std::shared_ptr<endpoint_definition> its_target;
                    std::pair<session_t, bool> its_session;
                    if (_reliable_port != ILLEGAL_PORT) {
                        its_target = endpoint_definition::get(
                                _reliable_address, port_, reliable_);
                        its_session = get_session(_reliable_address);
                    } else if (_unreliable_port != ILLEGAL_PORT) {
                        its_target = endpoint_definition::get(
                                _unreliable_address, port_, reliable_);
                        its_session = get_session(_unreliable_address);
                    }

                    if (its_target) {
                        std::lock_guard<std::mutex> its_lock(serialize_mutex_);
                        its_message->set_session(its_session.first);
                        its_message->set_reboot_flag(its_session.second);
                        serializer_->serialize(its_message.get());
                        if (host_->send_to(its_target,
                                           serializer_->get_data(),
                                           serializer_->get_size(), port_)) {
                            increment_session(its_target->get_address());
                        }
                        serializer_->reset();
                    }
                }
            }
        }
    }
}

void service_discovery_impl::process_findservice_serviceentry(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, bool _unicast_flag) {
    services_t offered_services = host_->get_offered_services();
    auto found_service = offered_services.find(_service);
    if (found_service != offered_services.end()) {
        if (_instance != ANY_INSTANCE) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                std::shared_ptr<serviceinfo> its_info = found_instance->second;

                ev_find_service find_event(its_info, _service,
                                           _instance, _major, _minor, _unicast_flag );
                default_->process(find_event);
            }
        } else {
            // send back all available instances
            for (const auto &found_instance : found_service->second) {

                ev_find_service find_event(found_instance.second, _service,
                                           _instance, _major, _minor, _unicast_flag );
                default_->process(find_event);
            }
        }
    }
}

void service_discovery_impl::send_unicast_offer_service(
        const std::shared_ptr<const serviceinfo> &_info, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    if (_major == ANY_MAJOR || _major == _info->get_major()) {
        if (_minor == 0xFFFFFFFF || _minor <= _info->get_minor()) {
            if (_info->get_endpoint(false) || _info->get_endpoint(true)) {
                std::shared_ptr<runtime> its_runtime = runtime_.lock();
                if (!its_runtime) {
                    return;
                }
                std::shared_ptr<message_impl> its_message =
                        its_runtime->create_message();
                uint32_t its_size(max_message_size_);
                insert_offer_service(its_message, _service, _instance, _info,
                        its_size);

                serialize_and_send(its_message, get_current_remote_address());
            }
        }
    }
}

void service_discovery_impl::send_multicast_offer_service(
        const std::shared_ptr<const serviceinfo> &_info, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    if (_major == ANY_MAJOR || _major == _info->get_major()) {
        if (_minor == 0xFFFFFFFF || _minor <= _info->get_minor()) {
            if (_info->get_endpoint(false) || _info->get_endpoint(true)) {
                std::shared_ptr<runtime> its_runtime = runtime_.lock();
                if (!its_runtime) {
                    return;
                }
                std::shared_ptr<message_impl> its_message =
                        its_runtime->create_message();

                uint32_t its_size(max_message_size_);
                insert_offer_service(its_message, _service, _instance, _info,
                        its_size);

                if (its_message->get_entries().size() > 0) {
                    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
                    std::pair<session_t, bool> its_session = get_session(
                            unicast_);
                    its_message->set_session(its_session.first);
                    its_message->set_reboot_flag(its_session.second);
                    if (host_->send(VSOMEIP_SD_CLIENT, its_message, true)) {
                        increment_session(unicast_);
                    }
                }
            }
        }
    }
}

void service_discovery_impl::on_reliable_endpoint_connected(
        service_t _service, instance_t _instance,
        const std::shared_ptr<const vsomeip::endpoint> &_endpoint) {
    std::shared_ptr<runtime> its_runtime = runtime_.lock();
    if (!its_runtime) {
        return;
    }

    // send out subscriptions for services where the tcp connection
    // wasn't established at time of subscription

    std::shared_ptr<message_impl> its_message(its_runtime->create_message());
    bool has_address(false);
    boost::asio::ip::address its_address;

    {
        std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
        auto found_service = subscribed_.find(_service);
        if (found_service != subscribed_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                if(0 < found_instance->second.size()) {

                    for(const auto &its_eventgroup : found_instance->second) {
                        for(const auto &its_client : its_eventgroup.second) {
                            if (its_client.first != VSOMEIP_ROUTING_CLIENT) {
                                if (its_client.second->get_endpoint(true) &&
                                        !host_->has_identified(its_client.first, _service,
                                            _instance, true)) {
                                    continue;
                                }
                            }
                            std::shared_ptr<subscription> its_subscription(its_client.second);
                            if(its_subscription && !its_subscription->is_tcp_connection_established()) {
                                const std::shared_ptr<const endpoint> its_endpoint(
                                        its_subscription->get_endpoint(true));
                                if(its_endpoint && its_endpoint->is_connected()) {
                                    if(its_endpoint.get() == _endpoint.get()) {
                                        // mark as established
                                        its_subscription->set_tcp_connection_established(true);

                                        std::shared_ptr<endpoint> its_unreliable;
                                        std::shared_ptr<endpoint> its_reliable;
                                        get_subscription_endpoints(
                                                its_subscription->get_subscription_type(),
                                                its_unreliable, its_reliable, &its_address,
                                                &has_address, _service, _instance,
                                                its_client.first);
                                        its_subscription->set_endpoint(its_reliable, true);
                                        its_subscription->set_endpoint(its_unreliable, false);
                                        // only insert reliable subscriptions as unreliable
                                        // ones are immediately sent out
                                        insert_subscription(its_message, _service,
                                                _instance, its_eventgroup.first,
                                                its_subscription, true, false);
                                        its_subscription->set_acknowledged(false);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (has_address && 0 < its_message->get_entries().size()) {
        serialize_and_send(its_message, its_address);
    }
}

bool service_discovery_impl::insert_offer_service(
        std::shared_ptr < message_impl > _message, service_t _service,
        instance_t _instance, const std::shared_ptr<const serviceinfo> &_info,
        uint32_t &_size) {

    std::shared_ptr < endpoint > its_reliable = _info->get_endpoint(true);
    std::shared_ptr < endpoint > its_unreliable = _info->get_endpoint(false);

    uint32_t its_size = VSOMEIP_SOMEIP_SD_ENTRY_SIZE;
    if (its_reliable) {
        uint32_t its_endpoint_size(0);
        if (unicast_.is_v4()) {
            if (!find_existing_option<ipv4_option_impl, ipv4_address_t>(_message,
                    unicast_.to_v4().to_bytes(), its_reliable->get_local_port(),
                    layer_four_protocol_e::TCP, option_type_e::IP4_ENDPOINT)) {
                its_endpoint_size = VSOMEIP_SOMEIP_SD_IPV4_OPTION_SIZE;
            }
        } else {
            if (!find_existing_option<ipv6_option_impl, ipv6_address_t>(_message,
                    unicast_.to_v6().to_bytes(), its_reliable->get_local_port(),
                    layer_four_protocol_e::TCP, option_type_e::IP6_ENDPOINT)) {
                its_endpoint_size = VSOMEIP_SOMEIP_SD_IPV6_OPTION_SIZE;
            }
        }
        its_size += its_endpoint_size;
    }
    if (its_unreliable) {
        uint32_t its_endpoint_size(0);
        if (unicast_.is_v4()) {
            if (!find_existing_option<ipv4_option_impl, ipv4_address_t>(_message,
                    unicast_.to_v4().to_bytes(), its_unreliable->get_local_port(),
                    layer_four_protocol_e::UDP, option_type_e::IP4_ENDPOINT)) {
                its_endpoint_size = VSOMEIP_SOMEIP_SD_IPV4_OPTION_SIZE;
            }
        } else {
            if (!find_existing_option<ipv6_option_impl, ipv6_address_t>(_message,
                    unicast_.to_v6().to_bytes(), its_reliable->get_local_port(),
                    layer_four_protocol_e::UDP, option_type_e::IP6_ENDPOINT)) {
                its_endpoint_size = VSOMEIP_SOMEIP_SD_IPV6_OPTION_SIZE;
            }
        }
        its_size += its_endpoint_size;
    }

    if (its_size <= _size) {
        _size -= its_size;

        std::shared_ptr < serviceentry_impl > its_entry =
                _message->create_service_entry();
        if (its_entry) {
            its_entry->set_type(entry_type_e::OFFER_SERVICE);
            its_entry->set_service(_service);
            its_entry->set_instance(_instance);
            its_entry->set_major_version(_info->get_major());
            its_entry->set_minor_version(_info->get_minor());

            ttl_t its_ttl = _info->get_ttl();
            if (its_ttl > 0)
                its_ttl = ttl_;
            its_entry->set_ttl(its_ttl);

            if (its_reliable) {
                insert_option(_message, its_entry, unicast_,
                        its_reliable->get_local_port(), true);
                if (0 == _info->get_ttl()) {
                    host_->del_routing_info(_service,
                            _instance, true, false);
                }
            }

            if (its_unreliable) {
                insert_option(_message, its_entry, unicast_,
                        its_unreliable->get_local_port(), false);
                if (0 == _info->get_ttl()) {
                    host_->del_routing_info(_service,
                            _instance, false, true);
                }
            }
            // This would be a clean solution but does _not_ work with the ANDi tool
            //unsubscribe_all(_service, _instance);
        } else {
            VSOMEIP_ERROR << "Failed to create service entry.";
        }
        return true;
    }

    return false;
}

void service_discovery_impl::process_eventgroupentry(
        std::shared_ptr<eventgroupentry_impl> &_entry,
        const std::vector<std::shared_ptr<option_impl> > &_options,
        std::shared_ptr < message_impl > &its_message_response,
        std::vector <accepted_subscriber_t> &accepted_subscribers) {
    service_t its_service = _entry->get_service();
    instance_t its_instance = _entry->get_instance();
    eventgroup_t its_eventgroup = _entry->get_eventgroup();
    entry_type_e its_type = _entry->get_type();
    major_version_t its_major = _entry->get_major_version();
    ttl_t its_ttl = _entry->get_ttl();
    uint16_t its_reserved = _entry->get_reserved();
    uint8_t its_counter = _entry->get_counter();

    if (_entry->get_owning_message()->get_return_code() != return_code) {
        VSOMEIP_ERROR << "Invalid return code in SD header";
        if(its_ttl > 0) {
            insert_subscription_nack(its_message_response, its_service, its_instance,
                                     its_eventgroup, its_counter, its_major, its_reserved);
        }
        return;
    }

    if(its_type == entry_type_e::SUBSCRIBE_EVENTGROUP) {
        if (_entry->get_num_options(1) == 0
                && _entry->get_num_options(2) == 0) {
            VSOMEIP_ERROR << "Invalid number of options in SubscribeEventGroup entry";
            if(its_ttl > 0) {
                insert_subscription_nack(its_message_response, its_service, its_instance,
                                         its_eventgroup, its_counter, its_major, its_reserved);
            }
            return;
        }
        if(_entry->get_owning_message()->get_options_length() < 12) {
            VSOMEIP_ERROR << "Invalid options length in SD message";
            if(its_ttl > 0) {
                insert_subscription_nack(its_message_response, its_service, its_instance,
                                         its_eventgroup, its_counter, its_major, its_reserved);
            }
            return;
        }
        if (_options.size()
                 // cast is needed in order to get unsigned type since int will be promoted
                 // by the + operator on 16 bit or higher machines. 
                 < static_cast<std::vector<std::shared_ptr<option_impl>>::size_type>(
                     (_entry->get_num_options(1)) + (_entry->get_num_options(2)))) {
            VSOMEIP_ERROR << "Fewer options in SD message than "
                             "referenced in EventGroup entry or malformed option received";
            if(its_ttl > 0) {
                insert_subscription_nack(its_message_response, its_service, its_instance,
                                         its_eventgroup, its_counter, its_major, its_reserved);
            }
            return;
        }
        if(_entry->get_owning_message()->get_someip_length() < _entry->get_owning_message()->get_length()
                && its_ttl > 0) {
            VSOMEIP_ERROR  << std::dec << "SomeIP length field in SubscribeEventGroup message header: ["
                                << _entry->get_owning_message()->get_someip_length()
                                << "] bytes, is shorter than length of deserialized message: ["
                                << (uint32_t) _entry->get_owning_message()->get_length() << "] bytes.";
            return;
        }
    }

    boost::asio::ip::address its_first_address;
    uint16_t its_first_port(ILLEGAL_PORT);
    bool is_first_reliable(false);
    boost::asio::ip::address its_second_address;
    uint16_t its_second_port(ILLEGAL_PORT);
    bool is_second_reliable(false);

    for (auto i : { 1, 2 }) {
        for (auto its_index : _entry->get_options(uint8_t(i))) {
            std::shared_ptr < option_impl > its_option;
            try {
                its_option = _options.at(its_index);
            } catch(const std::out_of_range& e) {
#ifdef WIN32
                e; // silence MSVC warining C4101
#endif
                VSOMEIP_ERROR << "Fewer options in SD message than "
                                 "referenced in EventGroup entry for "
                                 "option run number: " << i;
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type && its_ttl > 0) {
                    insert_subscription_nack(its_message_response, its_service, its_instance,
                                             its_eventgroup, its_counter, its_major, its_reserved);
                }
                return;
            }
            switch (its_option->get_type()) {
            case option_type_e::IP4_ENDPOINT: {
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
                    std::shared_ptr < ipv4_option_impl > its_ipv4_option =
                            std::dynamic_pointer_cast < ipv4_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v4 its_ipv4_address(
                            its_ipv4_option->get_address());
                    if (!check_layer_four_protocol(its_ipv4_option)) {
                        if( its_ttl > 0) {
                            insert_subscription_nack(its_message_response, its_service, its_instance,
                                                     its_eventgroup, its_counter, its_major, its_reserved);
                        }
                        return;
                    }

                    if (its_first_port == ILLEGAL_PORT) {
                        its_first_address = its_ipv4_address;
                        its_first_port = its_ipv4_option->get_port();
                        is_first_reliable = (its_ipv4_option->get_layer_four_protocol()
                                             == layer_four_protocol_e::TCP);

                        if(!check_ipv4_address(its_first_address)
                                || 0 == its_first_port) {
                            if(its_ttl > 0) {
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                         its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            VSOMEIP_ERROR << "Invalid port or IP address in first IPv4 endpoint option specified!";
                            return;
                        }
                    } else
                    if (its_second_port == ILLEGAL_PORT) {
                        its_second_address = its_ipv4_address;
                        its_second_port = its_ipv4_option->get_port();
                        is_second_reliable = (its_ipv4_option->get_layer_four_protocol()
                                              == layer_four_protocol_e::TCP);

                        if(!check_ipv4_address(its_second_address)
                                || 0 == its_second_port) {
                            if(its_ttl > 0) {
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                         its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            VSOMEIP_ERROR << "Invalid port or IP address in second IPv4 endpoint option specified!";
                            return;
                        }
                    } else {
                        // TODO: error message, too many endpoint options!
                    }
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv4 Endpoint)";
                }
                break;
            }
            case option_type_e::IP6_ENDPOINT: {
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
                    std::shared_ptr < ipv6_option_impl > its_ipv6_option =
                            std::dynamic_pointer_cast < ipv6_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v6 its_ipv6_address(
                            its_ipv6_option->get_address());
                    if (!check_layer_four_protocol(its_ipv6_option)) {
                        if(its_ttl > 0) {
                            insert_subscription_nack(its_message_response, its_service, its_instance,
                                                     its_eventgroup, its_counter, its_major, its_reserved);
                        }
                        VSOMEIP_ERROR << "Invalid layer 4 protocol type in IPv6 endpoint option specified!";
                        return;
                    }

                    if (its_first_port == ILLEGAL_PORT) {
                        its_first_address = its_ipv6_address;
                        its_first_port = its_ipv6_option->get_port();
                        is_first_reliable = (its_ipv6_option->get_layer_four_protocol()
                                             == layer_four_protocol_e::TCP);
                    } else
                    if (its_second_port == ILLEGAL_PORT) {
                        its_second_address = its_ipv6_address;
                        its_second_port = its_ipv6_option->get_port();
                        is_second_reliable = (its_ipv6_option->get_layer_four_protocol()
                                              == layer_four_protocol_e::TCP);
                    } else {
                        // TODO: error message, too many endpoint options!
                    }
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv6 Endpoint)";
                }
                break;
            }
            case option_type_e::IP4_MULTICAST:
                if (entry_type_e::SUBSCRIBE_EVENTGROUP_ACK == its_type) {
                    std::shared_ptr < ipv4_option_impl > its_ipv4_option =
                            std::dynamic_pointer_cast < ipv4_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v4 its_ipv4_address(
                            its_ipv4_option->get_address());

                    if (its_first_port == ILLEGAL_PORT) {
                        its_first_address = its_ipv4_address;
                        its_first_port = its_ipv4_option->get_port();
                    } else
                    if (its_second_port == ILLEGAL_PORT) {
                        its_second_address = its_ipv4_address;
                        its_second_port = its_ipv4_option->get_port();
                    } else {
                        // TODO: error message, too many endpoint options!
                    }
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv4 Multicast)";
                }
                break;
            case option_type_e::IP6_MULTICAST:
                if (entry_type_e::SUBSCRIBE_EVENTGROUP_ACK == its_type) {
                    std::shared_ptr < ipv6_option_impl > its_ipv6_option =
                            std::dynamic_pointer_cast < ipv6_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v6 its_ipv6_address(
                            its_ipv6_option->get_address());

                    if (its_first_port == ILLEGAL_PORT) {
                        its_first_address = its_ipv6_address;
                        its_first_port = its_ipv6_option->get_port();
                    } else
                    if (its_second_port == ILLEGAL_PORT) {
                        its_second_address = its_ipv6_address;
                        its_second_port = its_ipv6_option->get_port();
                    } else {
                        // TODO: error message, too many endpoint options!
                    }
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv6 Multicast)";
                }
                break;
            case option_type_e::CONFIGURATION: {
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
                }
                break;
            }
            case option_type_e::UNKNOWN:
            default:
                VSOMEIP_WARNING << "Unsupported eventgroup option";
                if(its_ttl > 0) {
                    insert_subscription_nack(its_message_response, its_service, its_instance,
                                             its_eventgroup, its_counter, its_major, its_reserved);
                }
                break;
            }
        }
    }

    if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
        handle_eventgroup_subscription(its_service, its_instance,
                its_eventgroup, its_major, its_ttl, its_counter, its_reserved,
                its_first_address, its_first_port, is_first_reliable,
                its_second_address, its_second_port, is_second_reliable, its_message_response, accepted_subscribers);
    } else {
        if( entry_type_e::SUBSCRIBE_EVENTGROUP_ACK == its_type) { //this type is used for ACK and NACK messages
            if(its_ttl > 0) {
                handle_eventgroup_subscription_ack(its_service, its_instance,
                        its_eventgroup, its_major, its_ttl, its_counter,
                        its_first_address, its_first_port);
            } else {
                handle_eventgroup_subscription_nack(its_service, its_instance, its_eventgroup, its_counter);
            }
        }
    }
}

void service_discovery_impl::handle_eventgroup_subscription(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        ttl_t _ttl, uint8_t _counter, uint16_t _reserved,
        const boost::asio::ip::address &_first_address, uint16_t _first_port, bool _is_first_reliable,
        const boost::asio::ip::address &_second_address, uint16_t _second_port, bool _is_second_reliable,
        std::shared_ptr < message_impl > &its_message,
        std::vector <accepted_subscriber_t> &accepted_subscribers) {

    if (its_message) {
        std::shared_ptr < eventgroupinfo > its_info = host_->find_eventgroup(
                _service, _instance, _eventgroup);

        bool is_nack(false);
        std::shared_ptr < endpoint_definition > its_first_subscriber,
            its_second_subscriber;
        std::shared_ptr < endpoint_definition > its_first_target,
            its_second_target;

        // Could not find eventgroup or wrong version
        if (!its_info || _major != its_info->get_major()) {
            // Create a temporary info object with TTL=0 --> send NACK
            if( its_info && (_major != its_info->get_major())) {
                VSOMEIP_ERROR << "Requested major version:[" << (uint32_t) _major
                        << "] in subscription to service:[" << _service
                        << "] instance:[" << _instance
                        << "] eventgroup:[" << _eventgroup
                        << "], does not match with services major version:[" << (uint32_t) its_info->get_major()
                        << "]";
            } else {
                VSOMEIP_ERROR << "Requested eventgroup:[" << _eventgroup
                        << "] not found for subscription to service:["
                        << _service << "] instance:[" << _instance << "]";
            }
            its_info = std::make_shared < eventgroupinfo > (_major, 0);
            if(_ttl > 0) {
                insert_subscription_nack(its_message, _service, _instance,
                        _eventgroup, _counter, _major, _reserved);
            }
            return;
        } else {
            boost::asio::ip::address its_first_address, its_second_address;
            uint16_t its_first_port, its_second_port;
            if (ILLEGAL_PORT != _first_port) {
                its_first_subscriber = endpoint_definition::get(
                        _first_address, _first_port, _is_first_reliable);
                if (!_is_first_reliable &&
                    its_info->get_multicast(its_first_address, its_first_port)) { // udp multicast
                    its_first_target = endpoint_definition::get(
                        its_first_address, its_first_port, false);
                } else if(_is_first_reliable) { // tcp unicast
                    its_first_target = its_first_subscriber;
                    // check if TCP connection is established by client
                    if( !is_tcp_connected(_service, _instance, its_first_target) && _ttl > 0) {
                        insert_subscription_nack(its_message, _service, _instance,
                            _eventgroup, _counter, _major, _reserved);
                        VSOMEIP_ERROR << "TCP connection to target1: [" << its_first_target->get_address().to_string()
                                << ":" << its_first_target->get_port()
                                << "] not established for subscription to service:[" << _service
                                << "] instance:[" << _instance
                                << "] eventgroup:[" << _eventgroup << "]";
                        return;
                    }
                } else { // udp unicast
                    its_first_target = its_first_subscriber;
                }
            }
            if (ILLEGAL_PORT != _second_port) {
                its_second_subscriber = endpoint_definition::get(
                        _second_address, _second_port, _is_second_reliable);
                if (!_is_second_reliable &&
                    its_info->get_multicast(its_second_address, its_second_port)) { // udp multicast
                    its_second_target = endpoint_definition::get(
                        its_second_address, its_second_port, false);
                } else if (_is_second_reliable) { // tcp unicast
                    its_second_target = its_second_subscriber;
                    // check if TCP connection is established by client
                    if( !is_tcp_connected(_service, _instance, its_second_target) && _ttl > 0) {
                        insert_subscription_nack(its_message, _service, _instance,
                            _eventgroup, _counter, _major, _reserved);
                        VSOMEIP_ERROR << "TCP connection to target2 : [" << its_second_target->get_address().to_string()
                                << ":" << its_second_target->get_port()
                                << "] not established for subscription to service:[" << _service
                                << "] instance:[" << _instance
                                << "] eventgroup:[" << _eventgroup << "]";
                        return;
                    }
                } else { // udp unicast
                    its_second_target = its_second_subscriber;
                }
            }
        }

        if (_ttl == 0) { // --> unsubscribe
            if (its_first_subscriber) {
                host_->on_unsubscribe(_service, _instance, _eventgroup, its_first_subscriber);
            }
            if (its_second_subscriber) {
                host_->on_unsubscribe(_service, _instance, _eventgroup, its_second_subscriber);
            }
            return;
        }

       std::chrono::steady_clock::time_point its_expiration
            = std::chrono::steady_clock::now() + std::chrono::seconds(_ttl);

        if (its_first_target) {
            if(!host_->on_subscribe_accepted(_service, _instance, _eventgroup,
                    its_first_subscriber, its_expiration)) {
                is_nack = true;
                insert_subscription_nack(its_message, _service, _instance, _eventgroup,
                        _counter, _major, _reserved);
            }
        }
        if (its_second_subscriber) {
            if(!host_->on_subscribe_accepted(_service, _instance, _eventgroup,
                    its_second_subscriber, its_expiration)) {
                is_nack = true;
                insert_subscription_nack(its_message, _service, _instance, _eventgroup,
                        _counter, _major, _reserved);
            }
        }

        if (!is_nack)
        {
            insert_subscription_ack(its_message, _service, _instance, _eventgroup,
                    its_info, _ttl, _counter, _major, _reserved);

            if (its_expiration < next_subscription_expiration_) {
                stop_subscription_expiration_timer();
                next_subscription_expiration_ = its_expiration;
                start_subscription_expiration_timer();
            }

            if (its_first_target && its_first_subscriber) {
                accepted_subscriber_t subscriber_;
                subscriber_.service_id = _service;
                subscriber_.instance_id = _instance;
                subscriber_.eventgroup_ = _eventgroup;
                subscriber_.subscriber = its_first_subscriber;
                subscriber_.target = its_first_target;
                subscriber_.its_expiration = its_expiration;

                accepted_subscribers.push_back(subscriber_);
            }
            if (its_second_target && its_second_subscriber) {
                accepted_subscriber_t subscriber_;
                subscriber_.service_id = _service;
                subscriber_.instance_id = _instance;
                subscriber_.eventgroup_ = _eventgroup;
                subscriber_.subscriber = its_second_subscriber;
                subscriber_.target = its_second_target;
                subscriber_.its_expiration = its_expiration;

                accepted_subscribers.push_back(subscriber_);
            }
        }
    }
}

void service_discovery_impl::handle_eventgroup_subscription_nack(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, uint8_t _counter) {
    client_t nackedClient = 0;
    std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                for (auto client : found_eventgroup->second) {
                    if (client.second->get_counter() == _counter) {
                        // Deliver nack
                        nackedClient = client.first;
                        host_->on_subscribe_nack(client.first, _service, _instance, _eventgroup);
                        break;
                    }
                }
                // Remove nacked subscription
                found_eventgroup->second.erase(nackedClient);
            }
        }
    }
}

void service_discovery_impl::handle_eventgroup_subscription_ack(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        major_version_t _major, ttl_t _ttl, uint8_t _counter,
        const boost::asio::ip::address &_address, uint16_t _port) {
    (void)_major;
    (void)_ttl;
    std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                for (auto its_client : found_eventgroup->second) {
                    if (its_client.second->get_counter() == _counter) {
                        its_client.second->set_acknowledged(true);
                    }
                    if (_address.is_multicast()) {
                        host_->on_subscribe_ack(_service, _instance, _address,
                                _port);
                    }
                    host_->on_subscribe_ack(its_client.first, _service,
                            _instance, _eventgroup);
                }
            }
        }
    }
}

bool service_discovery_impl::is_tcp_connected(service_t _service,
         instance_t _instance,
         std::shared_ptr<vsomeip::endpoint_definition> its_endpoint) {
    bool is_connected = false;
    services_t offered_services = host_->get_offered_services();
    auto found_service = offered_services.find(_service);
    if (found_service != offered_services.end()) {
        if (_instance != ANY_INSTANCE) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                std::shared_ptr<serviceinfo> its_info = found_instance->second;
                if(its_info) {
                    //get reliable server endpoint
                    auto its_reliable_endpoint = its_info->get_endpoint(true);
                    if(its_reliable_endpoint) {
                        std::shared_ptr<tcp_server_endpoint_impl> its_ptr(std::static_pointer_cast<tcp_server_endpoint_impl>(its_reliable_endpoint));
                        if( !its_ptr->is_established(its_endpoint)) {
                        }
                        else {
                            is_connected = true;
                        }
                    }
                }
            }
        }
    }
    return is_connected;
}

void service_discovery_impl::serialize_and_send(
        std::shared_ptr<message_impl> _message,
        const boost::asio::ip::address &_address) {
    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    std::pair<session_t, bool> its_session = get_session(_address);
    _message->set_session(its_session.first);
    _message->set_reboot_flag(its_session.second);
    if(!serializer_->serialize(_message.get())) {
        VSOMEIP_ERROR << "service_discovery_impl::serialize_and_send: serialization error.";
        return;
    }
    if (host_->send_to(endpoint_definition::get(_address, port_, reliable_),
            serializer_->get_data(), serializer_->get_size(), port_)) {
        increment_session(_address);
    }
    serializer_->reset();
}

void service_discovery_impl::start_ttl_timer() {
    ttl_timer_.expires_from_now(std::chrono::milliseconds(smallest_ttl_));
    ttl_timer_.async_wait(
            std::bind(&service_discovery_impl::check_ttl, shared_from_this(),
                      std::placeholders::_1));
}

std::chrono::milliseconds service_discovery_impl::stop_ttl_timer() {
    std::chrono::milliseconds remaining = std::chrono::duration_cast<
                                std::chrono::milliseconds
                            >(ttl_timer_.expires_from_now());
    ttl_timer_.cancel();
    return (smallest_ttl_ - remaining);
}

void service_discovery_impl::check_ttl(const boost::system::error_code &_error) {
    if (!_error) {
        smallest_ttl_ = host_->update_routing_info(smallest_ttl_);
        start_ttl_timer();
    }
}

boost::asio::ip::address service_discovery_impl::get_current_remote_address() const {
    if (reliable_) {
        return std::static_pointer_cast<tcp_server_endpoint_impl>(endpoint_)->get_remote().address();
    } else {
        return std::static_pointer_cast<udp_server_endpoint_impl>(endpoint_)->get_remote().address();
    }
}

bool service_discovery_impl::check_static_header_fields(
        const std::shared_ptr<const message> &_message) const {
    if(_message->get_protocol_version() != protocol_version) {
        VSOMEIP_ERROR << "Invalid protocol version in SD header";
        return false;
    }
    if(_message->get_interface_version() != interface_version) {
        VSOMEIP_ERROR << "Invalid interface version in SD header";
        return false;
    }
    if(_message->get_message_type() != message_type) {
        VSOMEIP_ERROR << "Invalid message type in SD header";
        return false;
    }
    if(_message->get_return_code() > return_code_e::E_OK
            && _message->get_return_code()< return_code_e::E_UNKNOWN) {
        VSOMEIP_ERROR << "Invalid return code in SD header";
        return false;
    }
    return true;
}

bool service_discovery_impl::check_layer_four_protocol(
        const std::shared_ptr<const ip_option_impl> _ip_option) const {
    if (_ip_option->get_layer_four_protocol() == layer_four_protocol_e::UNKNOWN) {
        VSOMEIP_ERROR << "Invalid layer 4 protocol in IP endpoint option";
        return false;
    }
    return true;
}

void service_discovery_impl::send_subscriptions(service_t _service, instance_t _instance,
        client_t _client, bool _reliable) {
    std::shared_ptr<runtime> its_runtime = runtime_.lock();
    if (!its_runtime) {
        return;
    }
    std::forward_list<std::pair<std::shared_ptr<message_impl>,
                                const boost::asio::ip::address>> subscription_messages;
    {
        std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
        auto found_service = subscribed_.find(_service);
        if (found_service != subscribed_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                for (auto found_eventgroup : found_instance->second) {
                    auto found_client = found_eventgroup.second.find(_client);
                    if (found_client != found_eventgroup.second.end()) {
                        std::shared_ptr<endpoint> its_unreliable;
                        std::shared_ptr<endpoint> its_reliable;
                        bool has_address(false);
                        boost::asio::ip::address its_address;
                        get_subscription_endpoints(
                                found_client->second->get_subscription_type(),
                                its_unreliable, its_reliable, &its_address,
                                &has_address, _service, _instance,
                                found_client->first);
                        std::shared_ptr<endpoint> endpoint;
                        if (_reliable) {
                            endpoint = its_reliable;
                            found_client->second->set_endpoint(its_reliable, true);
                        } else {
                            endpoint = its_unreliable;
                            found_client->second->set_endpoint(its_unreliable, false);
                        }
                        if (endpoint) {
                            endpoint->get_remote_address(its_address);
                            std::shared_ptr<message_impl> its_message
                                = its_runtime->create_message();

                            if(_reliable) {
                                if(endpoint->is_connected()) {
                                    insert_subscription(its_message, _service,
                                            _instance, found_eventgroup.first,
                                            found_client->second, _reliable, !_reliable);
                                    found_client->second->set_tcp_connection_established(true);
                                } else {
                                    // don't insert reliable endpoint option if the
                                    // TCP client endpoint is not yet connected
                                    found_client->second->set_tcp_connection_established(false);
                                }
                            } else {
                                insert_subscription(its_message, _service,
                                        _instance, found_eventgroup.first,
                                        found_client->second, _reliable, !_reliable);
                            }
                            if(0 < its_message->get_entries().size()) {
                                subscription_messages.push_front({its_message, its_address});
                                found_client->second->set_acknowledged(false);
                            }
                        }
                    }
                }
            }
        }
    }
    for (const auto s : subscription_messages) {
        serialize_and_send(s.first, s.second);
    }
}

void service_discovery_impl::start_subscription_expiration_timer() {
    subscription_expiration_timer_.expires_at(next_subscription_expiration_);
        subscription_expiration_timer_.async_wait(
                std::bind(&service_discovery_impl::expire_subscriptions,
                          shared_from_this(),
                          std::placeholders::_1));
}

void service_discovery_impl::stop_subscription_expiration_timer() {
    subscription_expiration_timer_.cancel();
}

void service_discovery_impl::expire_subscriptions(const boost::system::error_code &_error) {
    if (!_error) {
        next_subscription_expiration_ = host_->expire_subscriptions();
        start_subscription_expiration_timer();
    }
}

bool service_discovery_impl::check_ipv4_address(
        boost::asio::ip::address its_address) {
    //Check unallowed ipv4 address
    bool is_valid = true;
    std::shared_ptr<configuration> its_configuration =
            host_->get_configuration();

    if(its_configuration) {
        boost::asio::ip::address_v4::bytes_type its_unicast_address =
                its_configuration.get()->get_unicast_address().to_v4().to_bytes();
        boost::asio::ip::address_v4::bytes_type endpoint_address =
                its_address.to_v4().to_bytes();

        //same address as unicast address of DUT not allowed
        if(its_unicast_address
                == endpoint_address) {
            VSOMEIP_ERROR << "Subscribers endpoint IP address is same as DUT's address! : "
                    << its_address.to_string();
            is_valid = false;
        }

        // first 3 triples must match
        its_unicast_address[3] = 0x00;
        endpoint_address[3] = 0x00;

        if(its_unicast_address
                != endpoint_address) {
#if 1
            VSOMEIP_ERROR<< "First 3 triples of subscribers endpoint IP address are not valid!";
#endif
            is_valid = false;

        } else {
#if 0
            VSOMEIP_DEBUG << "First 3 triples of subscribers endpoint IP address are valid!";
#endif
        }
    }
    return is_valid;
}

}  // namespace sd
}  // namespace vsomeip

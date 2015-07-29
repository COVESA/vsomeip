// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/configuration.hpp>
#include <vsomeip/logger.hpp>

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
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/endpoint.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../message/include/serializer.hpp"
#include "../../routing/include/eventgroupinfo.hpp"
#include "../../routing/include/servicegroup.hpp"
#include "../../routing/include/serviceinfo.hpp"

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(service_discovery_host *_host)
        : host_(_host), io_(_host->get_io()), serializer_(
                std::make_shared<serializer>()), deserializer_(
                std::make_shared<deserializer>()) {
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
    default_ = std::make_shared < service_discovery_fsm
            > ("default", shared_from_this());

    std::shared_ptr < configuration > its_configuration =
            host_->get_configuration();
    if (its_configuration) {
        unicast_ = its_configuration->get_unicast();

        std::set < std::string > its_servicegroups =
                its_configuration->get_servicegroups();
        for (auto its_group : its_servicegroups) {
            if (its_group != "default"
                    && its_configuration->is_local_servicegroup(its_group)) {
                additional_[its_group] = std::make_shared
                        < service_discovery_fsm
                        > (its_group, shared_from_this());
            }
        }

        port_ = its_configuration->get_service_discovery_port();
        reliable_ = (its_configuration->get_service_discovery_protocol()
                == "tcp");

        serializer_->create_data(
                reliable_ ?
                        VSOMEIP_MAX_TCP_MESSAGE_SIZE :
                        VSOMEIP_MAX_UDP_MESSAGE_SIZE);

        host_->create_service_discovery_endpoint(
                its_configuration->get_service_discovery_multicast(), port_,
                reliable_);

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
    std::lock_guard<std::mutex> its_lock(requested_mutex_);
    auto find_service = requested_.find(_service);
    if (find_service != requested_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
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

void service_discovery_impl::release_service(service_t _service,
        instance_t _instance) {
    auto find_service = requested_.find(_service);
    if (find_service != requested_.end()) {
        find_service->second.erase(_instance);
    }
}

void service_discovery_impl::subscribe(service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl, client_t _client) {
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

    std::shared_ptr < endpoint > its_reliable = host_->find_or_create_remote_client(
            _service, _instance, true, _client);
    std::shared_ptr < endpoint > its_unreliable = host_->find_or_create_remote_client(
            _service, _instance, false, _client);
    std::shared_ptr < subscription > its_subscription = std::make_shared
            < subscription > (_major, _ttl, its_reliable, its_unreliable, _client);
    subscribed_[_service][_instance][_eventgroup][_client] = its_subscription;

    if (!its_subscription->is_acknowleged()) {
        bool has_address(false);
        boost::asio::ip::address its_address;

        std::shared_ptr<endpoint> its_endpoint
            = host_->find_or_create_remote_client(_service, _instance, false, _client);

        if (its_endpoint) {
            has_address = its_endpoint->get_remote_address(its_address);
            its_subscription->set_endpoint(its_endpoint, false);
        }

        its_endpoint = host_->find_or_create_remote_client(_service, _instance, true, _client);
        if (its_endpoint) {
            has_address = has_address || its_endpoint->get_remote_address(its_address);
            its_subscription->set_endpoint(its_endpoint, true);
        }

        if (has_address) {
            std::shared_ptr < runtime > its_runtime = runtime_.lock();
            if (!its_runtime)
                return;

            std::shared_ptr < message_impl > its_message
                = its_runtime->create_message();

            // TODO: consume major & ttl
            insert_subscription(its_message, _service, _instance, _eventgroup,
                    its_subscription);
            its_message->set_session(get_session(its_address));

            serializer_->serialize(its_message.get());

            if (host_->send_to(
                    std::make_shared<endpoint_definition>(
                            its_address, port_, reliable_),
                    serializer_->get_data(),
                    serializer_->get_size())) {
                increment_session(its_address);
            }
            serializer_->reset();
        }
    }
}

void service_discovery_impl::unsubscribe(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    // TODO: add client_id!!!
    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                for (auto its_client : found_eventgroup->second) {
                    its_client.second->set_ttl(0); // is read once and removed afterwards!
                }
            }
        }
    }
}

session_t service_discovery_impl::get_session(
        const boost::asio::ip::address &_address) {
    session_t its_session;
    auto found_session = sessions_.find(_address);
    if (found_session == sessions_.end()) {
        its_session = sessions_[_address] = 1;
    } else {
        its_session = found_session->second;
    }
    return its_session;
}

void service_discovery_impl::increment_session(
        const boost::asio::ip::address &_address) {
    auto found_session = sessions_.find(_address);
    if (found_session != sessions_.end()) {
        found_session->second++;
        if (0 == found_session->second) {
            found_session->second++;
            // TODO: what about the reboot flag?
        }
    }
}

void service_discovery_impl::insert_option(
        std::shared_ptr<message_impl> &_message,
        std::shared_ptr<entry_impl> _entry,
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _is_reliable) {
    if (unicast_ == _address) {
        if (unicast_.is_v4()) {
            ipv4_address_t its_address = unicast_.to_v4().to_bytes();
            std::shared_ptr < ipv4_option_impl > its_option =
                    _message->create_ipv4_option(false);
            if (its_option) {
                its_option->set_address(its_address);
                its_option->set_port(_port);
                its_option->set_udp(!_is_reliable);
                _entry->assign_option(its_option, 1);
            }
        } else {
            ipv6_address_t its_address = unicast_.to_v6().to_bytes();
            std::shared_ptr < ipv6_option_impl > its_option =
                    _message->create_ipv6_option(false);
            if (its_option) {
                its_option->set_address(its_address);
                its_option->set_port(_port);
                its_option->set_udp(!_is_reliable);
                _entry->assign_option(its_option, 1);
            }
        }
    } else {
        if (_address.is_v4()) {
            ipv4_address_t its_address = _address.to_v4().to_bytes();
            std::shared_ptr < ipv4_option_impl > its_option =
                    _message->create_ipv4_option(true);
            if (its_option) {
                its_option->set_address(its_address);
                its_option->set_port(_port);
                _entry->assign_option(its_option, 1);
            }
        } else {
            ipv6_address_t its_address = _address.to_v6().to_bytes();
            std::shared_ptr < ipv6_option_impl > its_option =
                    _message->create_ipv6_option(true);
            if (its_option) {
                its_option->set_address(its_address);
                its_option->set_port(_port);
                _entry->assign_option(its_option, 1);
            }
        }
    }
}

void service_discovery_impl::insert_find_entries(
        std::shared_ptr<message_impl> &_message, requests_t &_requests) {
    std::lock_guard<std::mutex> its_lock(requested_mutex_);
    for (auto its_service : _requests) {
        for (auto its_instance : its_service.second) {
            auto its_request = its_instance.second;
            std::shared_ptr < serviceentry_impl > its_entry =
                    _message->create_service_entry();
            if (its_entry) {
                its_entry->set_type(entry_type_e::FIND_SERVICE);
                its_entry->set_service(its_service.first);
                its_entry->set_instance(its_instance.first);
                its_entry->set_major_version(its_request->get_major());
                its_entry->set_minor_version(its_request->get_minor());
                its_entry->set_ttl(its_request->get_ttl());
            } else {
                VSOMEIP_ERROR << "Failed to create service entry!";
            }
        }
    }
}

void service_discovery_impl::insert_offer_entries(
        std::shared_ptr<message_impl> &_message, services_t &_services) {
    for (auto its_service : _services) {
        for (auto its_instance : its_service.second) {
            auto its_info = its_instance.second;
            std::shared_ptr < serviceentry_impl > its_entry =
                    _message->create_service_entry();
            if (its_entry) {
                its_entry->set_type(entry_type_e::OFFER_SERVICE);
                its_entry->set_service(its_service.first);
                its_entry->set_instance(its_instance.first);
                its_entry->set_major_version(its_info->get_major());
                its_entry->set_minor_version(its_info->get_minor());
                its_entry->set_ttl(its_info->get_ttl());

                std::shared_ptr < endpoint > its_endpoint =
                        its_info->get_endpoint(true);
                if (its_endpoint) {
                    insert_option(_message, its_entry, unicast_,
                            its_endpoint->get_local_port(), true);
                    if (0 == its_info->get_ttl()) {
                        host_->del_routing_info(its_service.first,
                                its_instance.first, true);
                    }
                }

                its_endpoint = its_info->get_endpoint(false);
                if (its_endpoint) {
                    insert_option(_message, its_entry, unicast_,
                            its_endpoint->get_local_port(), false);
                    if (0 == its_info->get_ttl()) {
                        host_->del_routing_info(its_service.first,
                                its_instance.first, false);
                    }
                }
            } else {
                VSOMEIP_ERROR << "Failed to create service entry.";
            }
        }
    }
}

void service_discovery_impl::insert_subscription(
        std::shared_ptr<message_impl> &_message, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<subscription> &_subscription) {
    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_major_version(_subscription->get_major());
    its_entry->set_ttl(_subscription->get_ttl());
    std::shared_ptr < endpoint > its_endpoint = _subscription->get_endpoint(
            true);
    if (its_endpoint) {
        insert_option(_message, its_entry, unicast_, its_endpoint->get_local_port(),
                true);
    }
    its_endpoint = _subscription->get_endpoint(false);
    if (its_endpoint) {
        insert_option(_message, its_entry, unicast_, its_endpoint->get_local_port(),
                false);
    }
}

void service_discovery_impl::insert_subscription_ack(
        std::shared_ptr<message_impl> &_message, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<eventgroupinfo> &_info) {
    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP_ACK);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_major_version(_info->get_major());
    its_entry->set_ttl(_info->get_ttl());

    boost::asio::ip::address its_address;
    uint16_t its_port;
    if (_info->get_multicast(its_address, its_port)) {
        insert_option(_message, its_entry, its_address, its_port, false);
    }
}

void service_discovery_impl::send(const std::string &_name,
        bool _is_announcing) {

    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (!its_runtime)
        return;

    std::shared_ptr < message_impl > its_message =
            its_runtime->create_message();

    // TODO: optimize building of SD message (common options, utilize the two runs)

    // If we are the default group and not in main phase, include "FindOffer"-entries
    if (_name == "default" && !_is_announcing) {
        insert_find_entries(its_message, requested_);
    }

    // Always include the "OfferService"-entries for the service group
    services_t its_offers = host_->get_offered_services(_name);
    insert_offer_entries(its_message, its_offers);

    // Serialize and send
    if (its_message->get_entries().size() > 0) {
        its_message->set_session(get_session(unicast_));
        if (host_->send(VSOMEIP_SD_CLIENT, its_message, true)) {
            increment_session (unicast_);
        }
    }
}

// Interface endpoint_host
void service_discovery_impl::on_message(const byte_t *_data, length_t _length) {
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
        std::vector < std::shared_ptr<option_impl> > its_options =
                its_message->get_options();
        for (auto its_entry : its_message->get_entries()) {
            if (its_entry->is_service_entry()) {
                std::shared_ptr < serviceentry_impl > its_service_entry =
                        std::dynamic_pointer_cast < serviceentry_impl
                                > (its_entry);
                process_serviceentry(its_service_entry, its_options);
            } else {
                std::shared_ptr < eventgroupentry_impl > its_eventgroup_entry =
                        std::dynamic_pointer_cast < eventgroupentry_impl
                                > (its_entry);
                process_eventgroupentry(its_eventgroup_entry, its_options);
            }
        }
    }
}

void service_discovery_impl::on_offer_change(const std::string &_name) {
    if (_name == "default") {
        default_->process(ev_offer_change());
    } else {
        auto found_group = additional_.find(_name);
        if (found_group != additional_.end()) {
            found_group->second->process(ev_offer_change());
        }
    }
}

// Entry processing
void service_discovery_impl::process_serviceentry(
        std::shared_ptr<serviceentry_impl> &_entry,
        const std::vector<std::shared_ptr<option_impl> > &_options) {
    service_t its_service = _entry->get_service();
    instance_t its_instance = _entry->get_instance();
    major_version_t its_major = _entry->get_major_version();
    minor_version_t its_minor = _entry->get_minor_version();
    ttl_t its_ttl = _entry->get_ttl();

    for (auto i : { 1, 2 }) {
        for (auto its_index : _entry->get_options(i)) {
            std::vector < byte_t > its_option_address;
            uint16_t its_option_port = VSOMEIP_INVALID_PORT;
            std::shared_ptr < option_impl > its_option = _options[its_index];
            switch (its_option->get_type()) {
            case option_type_e::IP4_ENDPOINT: {
                std::shared_ptr < ipv4_option_impl > its_ipv4_option =
                        std::dynamic_pointer_cast < ipv4_option_impl
                                > (its_option);

                boost::asio::ip::address_v4 its_ipv4_address(
                        its_ipv4_option->get_address());
                boost::asio::ip::address its_address(its_ipv4_address);
                its_option_port = its_ipv4_option->get_port();

                handle_service_availability(its_service, its_instance,
                        its_major, its_minor, its_ttl, its_address,
                        its_option_port, !its_ipv4_option->is_udp());
                break;
            }
            case option_type_e::IP6_ENDPOINT: {
                std::shared_ptr < ipv6_option_impl > its_ipv6_option =
                        std::dynamic_pointer_cast < ipv6_option_impl
                                > (its_option);

                boost::asio::ip::address_v6 its_ipv6_address(
                        its_ipv6_option->get_address());
                boost::asio::ip::address its_address(its_ipv6_address);
                its_option_port = its_ipv6_option->get_port();

                handle_service_availability(its_service, its_instance,
                        its_major, its_minor, its_ttl, its_address,
                        its_option_port, !its_ipv6_option->is_udp());
                break;
            }
            case option_type_e::IP4_MULTICAST:
            case option_type_e::IP6_MULTICAST:
                VSOMEIP_ERROR << "Invalid service option (Multicast)";
                break;
            default:
                VSOMEIP_WARNING << "Unsupported service option";
                break;
            }
        }
    }
}

void service_discovery_impl::process_eventgroupentry(
        std::shared_ptr<eventgroupentry_impl> &_entry,
        const std::vector<std::shared_ptr<option_impl> > &_options) {
    service_t its_service = _entry->get_service();
    instance_t its_instance = _entry->get_instance();
    eventgroup_t its_eventgroup = _entry->get_eventgroup();
    entry_type_e its_type = _entry->get_type();
    major_version_t its_major = _entry->get_major_version();
    ttl_t its_ttl = _entry->get_ttl();

    boost::asio::ip::address its_reliable_address;
    uint16_t its_reliable_port = VSOMEIP_INVALID_PORT;
    boost::asio::ip::address its_unreliable_address;
    uint16_t its_unreliable_port = VSOMEIP_INVALID_PORT;

    for (auto i : { 1, 2 }) {
        for (auto its_index : _entry->get_options(i)) {
            std::vector < byte_t > its_option_address;
            std::shared_ptr < option_impl > its_option = _options[its_index];
            switch (its_option->get_type()) {
            case option_type_e::IP4_ENDPOINT: {
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == _entry->get_type()) {
                    std::shared_ptr < ipv4_option_impl > its_ipv4_option =
                            std::dynamic_pointer_cast < ipv4_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v4 its_ipv4_address(
                            its_ipv4_option->get_address());

                    // TODO: add error handling (port already set) here
                    if (its_ipv4_option->is_udp()) {
                        its_unreliable_address = its_ipv4_address;
                        its_unreliable_port = its_ipv4_option->get_port();
                    } else {
                        its_reliable_address = its_ipv4_address;
                        its_reliable_port = its_ipv4_option->get_port();
                    }
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv4 Endpoint)";
                }
                break;
            }
            case option_type_e::IP6_ENDPOINT: {
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == _entry->get_type()) {
                    std::shared_ptr < ipv6_option_impl > its_ipv6_option =
                            std::dynamic_pointer_cast < ipv6_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v6 its_ipv6_address(
                            its_ipv6_option->get_address());

                    // TODO: add error handling (port already set) here
                    if (its_ipv6_option->is_udp()) {
                        its_unreliable_address = its_ipv6_address;
                        its_unreliable_port = its_ipv6_option->get_port();
                    } else {
                        its_unreliable_address = its_ipv6_address;
                        its_reliable_port = its_ipv6_option->get_port();
                    }
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv6 Endpoint)";
                }
                break;
            }
            case option_type_e::IP4_MULTICAST:
                if (entry_type_e::SUBSCRIBE_EVENTGROUP_ACK
                        == _entry->get_type()) {
                    std::shared_ptr < ipv4_option_impl > its_ipv4_option =
                            std::dynamic_pointer_cast < ipv4_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v4 its_ipv4_address(
                            its_ipv4_option->get_address());

                    its_unreliable_address = its_ipv4_address;
                    its_unreliable_port = its_ipv4_option->get_port();
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv4 Multicast)";
                }
                break;
            case option_type_e::IP6_MULTICAST:
                if (entry_type_e::SUBSCRIBE_EVENTGROUP_ACK
                        == _entry->get_type()) {
                    std::shared_ptr < ipv6_option_impl > its_ipv6_option =
                            std::dynamic_pointer_cast < ipv6_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v6 its_ipv6_address(
                            its_ipv6_option->get_address());

                    its_unreliable_address = its_ipv6_address;
                    its_unreliable_port = its_ipv6_option->get_port();
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv6 Multicast)";
                }
                break;
            default:
                VSOMEIP_WARNING << "Unsupported eventgroup option";
                break;
            }
        }
    }

    if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
        handle_eventgroup_subscription(its_service, its_instance,
                its_eventgroup, its_major, its_ttl,
                (its_reliable_port != VSOMEIP_INVALID_PORT ?
                        its_reliable_address : its_unreliable_address),
                its_reliable_port, its_unreliable_port);
    } else {
        handle_eventgroup_subscription_ack(its_service, its_instance,
                its_eventgroup, its_major, its_ttl, its_unreliable_address,
                its_unreliable_port);
    }
}

void service_discovery_impl::handle_service_availability(service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        ttl_t _ttl, const boost::asio::ip::address &_address, uint16_t _port,
        bool _reliable) {

    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (!its_runtime)
        return;

    if (0 < _ttl) {
        host_->add_routing_info(_service, _instance, _major, _minor, _ttl,
                _address, _port, _reliable);

        auto found_service = subscribed_.find(_service);
        if (found_service != subscribed_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                if (0 < found_instance->second.size()) {
                    std::shared_ptr < message_impl > its_message =
                            its_runtime->create_message();
                    for (auto its_eventgroup : found_instance->second) {
                        for (auto its_client : its_eventgroup.second) {
                            std::shared_ptr < subscription
                                    > its_subscription(its_client.second);
                            if (!its_subscription->is_acknowleged()) {
                                its_subscription->set_endpoint(
                                        host_->find_or_create_remote_client(_service,
                                                _instance, true, its_client.first), true);
                                its_subscription->set_endpoint(
                                        host_->find_or_create_remote_client(_service,
                                                _instance, false, its_client.first), false);

                                // TODO: consume major & ttl
                                insert_subscription(its_message, _service,
                                        _instance, its_eventgroup.first,
                                        its_subscription);
                            }
                        }

                    }

                    if (0 < its_message->get_entries().size()) {
                        its_message->set_session(get_session(_address));
                        serializer_->serialize(its_message.get());
                        if (host_->send_to(
                                std::make_shared < endpoint_definition
                                        > (_address, port_, reliable_),
                                serializer_->get_data(),
                                serializer_->get_size())) {
                            increment_session(_address);
                        }
                        serializer_->reset();
                    }
                }
            }
        }
    } else {
        auto found_service = subscribed_.find(_service);
        if (found_service != subscribed_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                for (auto &its_eventgroup : found_instance->second) {
                    for (auto its_client : its_eventgroup.second) {
                        its_client.second->set_acknowledged(false);
                    }
                }
            }
        }
        host_->del_routing_info(_service, _instance, _reliable);
    }
}

void service_discovery_impl::handle_eventgroup_subscription(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        ttl_t _ttl, const boost::asio::ip::address &_address,
        uint16_t _reliable_port, uint16_t _unreliable_port) {

    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (!its_runtime)
        return;

    std::shared_ptr < message_impl > its_message =
            its_runtime->create_message();
    if (its_message) {
        std::shared_ptr < eventgroupinfo > its_info = host_->find_eventgroup(
                _service, _instance, _eventgroup);

        bool is_nack(false);
        std::shared_ptr < endpoint_definition > its_subscriber;
        std::shared_ptr < endpoint_definition > its_target;

        // Could not find eventgroup --> send Nack
        if (!its_info || _major > its_info->get_major()
                || _ttl > its_info->get_ttl()) {
            its_info = std::make_shared < eventgroupinfo > (_major, 0);
            is_nack = true;
        } else {
            boost::asio::ip::address its_target_address;
            uint16_t its_target_port;
            if (VSOMEIP_INVALID_PORT != _unreliable_port) {
                its_subscriber = std::make_shared < endpoint_definition
                        > (_address, _unreliable_port, false);
                if (its_info->get_multicast(its_target_address,
                        its_target_port)) {
                    its_target = std::make_shared < endpoint_definition
                            > (its_target_address, its_target_port, false);
                } else {
                    its_target = its_subscriber;
                }
            } else {
                its_subscriber = std::make_shared < endpoint_definition
                        > (_address, _reliable_port, true);
                its_target = its_subscriber;
            }
        }

        insert_subscription_ack(its_message, _service, _instance, _eventgroup,
                its_info);

        its_message->set_session(get_session(_address));
        serializer_->serialize(its_message.get());
        if (host_->send_to(
                std::make_shared < endpoint_definition
                        > (_address, port_, reliable_), serializer_->get_data(),
                serializer_->get_size())) {
            increment_session(_address);
        }
        serializer_->reset();

        // Finally register the new subscriber and send him all the fields(!)
        if (!is_nack) {
            host_->on_subscribe(_service, _instance, _eventgroup,
                    its_subscriber, its_target);
        }
    }
}

void service_discovery_impl::handle_eventgroup_subscription_ack(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        major_version_t _major, ttl_t _ttl,
        const boost::asio::ip::address &_address, uint16_t _port) {
    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                for (auto its_client : found_eventgroup->second) {
                    its_client.second->set_acknowledged(true);
                    if (_address.is_multicast()) {
                        host_->on_subscribe_ack(_service, _instance, _address,
                                _port);
                    }
                }

            }
        }
    }
}

}  // namespace sd
}  // namespace vsomeip

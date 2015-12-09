// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

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
          smallest_ttl_(DEFAULT_TTL) {
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
        eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl, client_t _client,
        subscription_type_e _subscription_type) {
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

    std::shared_ptr < endpoint > its_unreliable;
    std::shared_ptr < endpoint > its_reliable;
    bool has_address(false);
    boost::asio::ip::address its_address;

    get_subscription_endpoints(_subscription_type, its_unreliable, its_reliable,
            &its_address, &has_address, _service, _instance, _client);

    // New subscription
    std::shared_ptr < subscription > its_subscription = std::make_shared
            < subscription > (_major, _ttl, its_reliable, its_unreliable, _subscription_type);
    subscribed_[_service][_instance][_eventgroup][_client] = its_subscription;

    if (has_address) {
        std::shared_ptr<runtime> its_runtime = runtime_.lock();
        if (!its_runtime)
            return;

        std::shared_ptr<message_impl> its_message
            = its_runtime->create_message();

        // TODO: consume major & ttl
        insert_subscription(its_message, _service, _instance, _eventgroup,
                its_subscription);
        serialize_and_send(its_message, its_address);

        its_subscription->set_acknowledged(false);
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

    std::lock_guard<std::mutex> its_lock(subscribed_mutex_);

    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                auto found_client = found_eventgroup->second.find(_client);
                if (found_client != found_eventgroup->second.end()) {
                    found_client->second->set_ttl(0);
                }
            }
        }
    }

    boost::asio::ip::address its_address;

    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (!its_runtime)
        return;

    std::shared_ptr < subscription > its_subscription =
            subscribed_[_service][_instance][_eventgroup][_client];

    auto endpoint = its_subscription->get_endpoint(false);
    if (endpoint) {
        endpoint->get_remote_address(its_address);
    } else {
        endpoint = its_subscription->get_endpoint(true);
        if (endpoint) {
            endpoint->get_remote_address(its_address);
        }
    }

    std::shared_ptr < message_impl > its_message
        = its_runtime->create_message();

    insert_subscription(its_message, _service, _instance, _eventgroup,
            its_subscription);

    std::pair<session_t, bool> its_session = get_session(its_address);
    its_message->set_session(its_session.first);
    its_message->set_reboot_flag(its_session.second);

    serialize_and_send(its_message, its_address);
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
    auto found_session = sessions_.find(_address);
    if (found_session == sessions_.end()) {
        its_session = sessions_[_address] = { 1, true };
    } else {
        its_session = found_session->second;
    }
    return its_session;
}

void service_discovery_impl::increment_session(
        const boost::asio::ip::address &_address) {
    auto found_session = sessions_.find(_address);
    if (found_session != sessions_.end()) {
        found_session->second.first++;
        if (found_session->second.first == 0) {  // Wrap
            found_session->second = { 1, false };
        }
    }
}

bool service_discovery_impl::is_reboot(
        const boost::asio::ip::address &_address,
        bool _reboot_flag, session_t _session) {

    bool result(false);
#ifdef VSOMEIP_TODO
    // Reboot detection: Either the flag has changed from false to true,
    // or the session identifier overrun while the flag is true
    if (reboots_.find(_address) == reboots_.end()) {
        if (_reboot_flag) {
            reboots_.insert(_address);
            result = true;
        }
    } else {
        auto its_last_session = sessions_receiving_.find(_address);
        if(its_last_session != sessions_receiving_.end()) {
            if (_reboot_flag && its_last_session->second >= _session) {
                result = true;
            }
        }
    }

    sessions_receiving_[_address] = _session;
#else
    (void)_address;
    (void)_reboot_flag;
    (void)_session;
#endif
    return result;
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
                its_option->set_layer_four_protocol(
                        _is_reliable ? layer_four_protocol_e::TCP :
                                layer_four_protocol_e::UDP);
                _entry->assign_option(its_option, 1);
            }
        } else {
            ipv6_address_t its_address = unicast_.to_v6().to_bytes();
            std::shared_ptr < ipv6_option_impl > its_option =
                    _message->create_ipv6_option(false);
            if (its_option) {
                its_option->set_address(its_address);
                its_option->set_port(_port);
                its_option->set_layer_four_protocol(
                        _is_reliable ? layer_four_protocol_e::TCP :
                                layer_four_protocol_e::UDP);
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
                its_option->set_layer_four_protocol(
                        _is_reliable ? layer_four_protocol_e::TCP :
                                layer_four_protocol_e::UDP);
                _entry->assign_option(its_option, 1);
            }
        } else {
            ipv6_address_t its_address = _address.to_v6().to_bytes();
            std::shared_ptr < ipv6_option_impl > its_option =
                    _message->create_ipv6_option(true);
            if (its_option) {
                its_option->set_address(its_address);
                its_option->set_port(_port);
                its_option->set_layer_four_protocol(
                        _is_reliable ? layer_four_protocol_e::TCP :
                                layer_four_protocol_e::UDP);
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
            insert_offer_service(_message, its_service.first,
                    its_instance.first, its_instance.second);
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
        std::shared_ptr<eventgroupinfo> &_info, ttl_t _ttl) {
    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP_ACK);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_major_version(_info->get_major());
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
                std::shared_ptr<eventgroupinfo> &_info) {
    std::shared_ptr < eventgroupentry_impl > its_entry =
            _message->create_eventgroup_entry();
    // SWS_SD_00316 and SWS_SD_00385
    its_entry->set_type(entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK);
    its_entry->set_service(_service);
    its_entry->set_instance(_instance);
    its_entry->set_eventgroup(_eventgroup);
    its_entry->set_major_version(_info->get_major());
    // SWS_SD_00432
    its_entry->set_ttl(0x0);

    boost::asio::ip::address its_address;
    uint16_t its_port;
    if (_info->get_multicast(its_address, its_port)) {
        insert_option(_message, its_entry, its_address, its_port, false);
    }
}

void service_discovery_impl::send(bool _is_announcing) {

    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (!its_runtime)
        return;

    std::shared_ptr < message_impl > its_message =
            its_runtime->create_message();

    // TODO: optimize building of SD message (common options, utilize the two runs)

    // If we are not in main phase, include "FindOffer"-entries
    if (!_is_announcing) {
        insert_find_entries(its_message, requested_);
    }

    // Always include the "OfferService"-entries for the service group
    services_t its_offers = host_->get_offered_services();
    insert_offer_entries(its_message, its_offers);

    // Serialize and send
    if (its_message->get_entries().size() > 0) {
        std::pair<session_t, bool> its_session = get_session(unicast_);
        its_message->set_session(its_session.first);
        its_message->set_reboot_flag(its_session.second);
        if (host_->send(VSOMEIP_SD_CLIENT, its_message, true)) {
            increment_session (unicast_);
        }
    }
}

// Interface endpoint_host
void service_discovery_impl::on_message(const byte_t *_data, length_t _length,
        const boost::asio::ip::address &_sender) {
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
        if (is_reboot(_sender,
                its_message->get_reboot_flag(), its_message->get_session())) {
            host_->expire_subscriptions(_sender);
            host_->expire_services(_sender);
        }

        ttl_t expired = stop_ttl_timer();
        smallest_ttl_ = host_->update_routing_info(expired);

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
        const std::vector<std::shared_ptr<option_impl> > &_options) {

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
                VSOMEIP_ERROR << "Invalid service option (Multicast)";
                break;
            case option_type_e::UNKNOWN:
            default:
                VSOMEIP_ERROR << "Unsupported service option";
                break;
            }
        }
    }

    if (0 < its_ttl) {
        switch(its_type) {
            case entry_type_e::FIND_SERVICE:
                process_findservice_serviceentry(its_service, its_instance,
                                                 its_major, its_minor);
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

    host_->add_routing_info(_service, _instance,
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
                        if (its_subscription->is_acknowledged()) {
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
                            its_subscription->set_endpoint(its_unreliable,
                                    false);

                            // TODO: consume major & ttl
                            insert_subscription(its_message,
                                    _service, _instance,
                                    its_eventgroup.first,
                                    its_subscription);

                            its_subscription->set_acknowledged(false);
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
                        its_message->set_session(its_session.first);
                        its_message->set_reboot_flag(its_session.second);
                        serializer_->serialize(its_message.get());
                        if (host_->send_to(its_target,
                                           serializer_->get_data(),
                                           serializer_->get_size())) {
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
        minor_version_t _minor) {
    services_t offered_services = host_->get_offered_services();
    auto found_service = offered_services.find(_service);
    if (found_service != offered_services.end()) {
        if (_instance != ANY_INSTANCE) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                std::shared_ptr<serviceinfo> its_info = found_instance->second;
                send_unicast_offer_service(its_info, _service, _instance,
                        _major, _minor);
            }
        } else {
            // send back all available instances
            for (const auto &found_instance : found_service->second) {
                send_unicast_offer_service(found_instance.second, _service,
                        _instance, _major, _minor);
            }
        }
    }
}

void service_discovery_impl::send_unicast_offer_service(
        const std::shared_ptr<const serviceinfo> &_info, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    if (_major == ANY_MAJOR || _major == _info->get_major()) {
        if (_minor == 0xFFFFFFFF || _minor == _info->get_minor()) {
            std::shared_ptr<runtime> its_runtime = runtime_.lock();
            if (!its_runtime) {
                return;
            }
            std::shared_ptr<message_impl> its_message =
                    its_runtime->create_message();
            insert_offer_service(its_message, _service, _instance, _info);

            serialize_and_send(its_message, get_current_remote_address());
        }
    }
}

void service_discovery_impl::insert_offer_service(
        std::shared_ptr < message_impl > _message, service_t _service,
        instance_t _instance, const std::shared_ptr<const serviceinfo> &_info) {
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

        std::shared_ptr < endpoint > its_endpoint =
                _info->get_endpoint(true);
        if (its_endpoint) {
            insert_option(_message, its_entry, unicast_,
                    its_endpoint->get_local_port(), true);
            if (0 == _info->get_ttl()) {
                host_->del_routing_info(_service,
                        _instance, true, false);
            }
        }

        its_endpoint = _info->get_endpoint(false);
        if (its_endpoint) {
            insert_option(_message, its_entry, unicast_,
                    its_endpoint->get_local_port(), false);
            if (0 == _info->get_ttl()) {
                host_->del_routing_info(_service,
                        _instance, false, true);
            }
        }
    } else {
        VSOMEIP_ERROR << "Failed to create service entry.";
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

    if (_entry->get_owning_message()->get_return_code() != return_code) {
        VSOMEIP_ERROR << "Invalid return code in SD header";
        send_eventgroup_subscription_nack(its_service, its_instance,
                                          its_eventgroup, its_major);
        return;
    }

    if(its_type == entry_type_e::SUBSCRIBE_EVENTGROUP) {
        if (_entry->get_num_options(1) == 0
                && _entry->get_num_options(2) == 0) {
            VSOMEIP_ERROR << "Invalid number of options in SubscribeEventGroup entry";
            send_eventgroup_subscription_nack(its_service, its_instance,
                    its_eventgroup, its_major);
            return;
        }
        if(_entry->get_owning_message()->get_options_length() < 12) {
            VSOMEIP_ERROR << "Invalid options length in SD message";
            send_eventgroup_subscription_nack(its_service, its_instance,
                    its_eventgroup, its_major);
            return;
        }
        if (_options.size()
                < (_entry->get_num_options(1) + _entry->get_num_options(2))) {
            VSOMEIP_ERROR << "Fewer options in SD message than "
                             "referenced in EventGroup entry";
            send_eventgroup_subscription_nack(its_service, its_instance,
                    its_eventgroup, its_major);
            return;
        }
    }

    boost::asio::ip::address its_reliable_address;
    uint16_t its_reliable_port(ILLEGAL_PORT);
    boost::asio::ip::address its_unreliable_address;
    uint16_t its_unreliable_port(ILLEGAL_PORT);

    for (auto i : { 1, 2 }) {
        for (auto its_index : _entry->get_options(uint8_t(i))) {
            std::shared_ptr < option_impl > its_option;
            try {
                its_option = _options.at(its_index);
            } catch(const std::out_of_range& e) {
                VSOMEIP_ERROR << "Fewer options in SD message than "
                                 "referenced in EventGroup entry for "
                                 "option run number: " << i;
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
                    send_eventgroup_subscription_nack(its_service, its_instance,
                            its_eventgroup, its_major);
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
                        send_eventgroup_subscription_nack(its_service,
                                its_instance, its_eventgroup, its_major);
                        return;
                    }
                    // TODO: add error handling (port already set) here
                    if (its_ipv4_option->get_layer_four_protocol()
                            == layer_four_protocol_e::UDP) {
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
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
                    std::shared_ptr < ipv6_option_impl > its_ipv6_option =
                            std::dynamic_pointer_cast < ipv6_option_impl
                                    > (its_option);

                    boost::asio::ip::address_v6 its_ipv6_address(
                            its_ipv6_option->get_address());
                    if (!check_layer_four_protocol(its_ipv6_option)) {
                        send_eventgroup_subscription_nack(its_service,
                                its_instance, its_eventgroup, its_major);
                        return;
                    }
                    // TODO: add error handling (port already set) here
                    if (its_ipv6_option->get_layer_four_protocol()
                            == layer_four_protocol_e::UDP) {
                        its_unreliable_address = its_ipv6_address;
                        its_unreliable_port = its_ipv6_option->get_port();
                    } else {
                        its_reliable_address = its_ipv6_address;
                        its_reliable_port = its_ipv6_option->get_port();
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

                    its_unreliable_address = its_ipv4_address;
                    its_unreliable_port = its_ipv4_option->get_port();
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

                    its_unreliable_address = its_ipv6_address;
                    its_unreliable_port = its_ipv6_option->get_port();
                } else {
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv6 Multicast)";
                }
                break;
            case option_type_e::UNKNOWN:
            default:
                VSOMEIP_WARNING << "Unsupported eventgroup option";
                send_eventgroup_subscription_nack(its_service, its_instance,
                                                  its_eventgroup, its_major);
                break;
            }
        }
    }

    if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
        handle_eventgroup_subscription(its_service, its_instance,
                its_eventgroup, its_major, its_ttl,
                (its_reliable_port != ILLEGAL_PORT ?
                        its_reliable_address : its_unreliable_address),
                its_reliable_port, its_unreliable_port);
    } else {
        handle_eventgroup_subscription_ack(its_service, its_instance,
                its_eventgroup, its_major, its_ttl, its_unreliable_address,
                its_unreliable_port);
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
        std::shared_ptr < endpoint_definition > its_reliable_subscriber,
            its_unreliable_subscriber;
        std::shared_ptr < endpoint_definition > its_reliable_target,
            its_unreliable_target;

        // Could not find eventgroup or wrong version
        if (!its_info || _major != its_info->get_major()) {
            // Create a temporary info object with TTL=0 --> send NACK
            its_info = std::make_shared < eventgroupinfo > (_major, 0);
            is_nack = true;
            insert_subscription_nack(its_message, _service, _instance, _eventgroup,
                its_info);
            serialize_and_send(its_message, _address);
            //TODO add check if required tcp connection is open
            return;
        } else {
            boost::asio::ip::address its_target_address;
            uint16_t its_target_port;
            if (ILLEGAL_PORT != _unreliable_port) {
                its_unreliable_subscriber = endpoint_definition::get(
                        _address, _unreliable_port, false);
                if (its_info->get_multicast(its_target_address,
                        its_target_port)) {
                    its_unreliable_target = endpoint_definition::get(
                        its_target_address, its_target_port, false);
                } else {
                    its_unreliable_target = its_unreliable_subscriber;
                }
            }
            if (ILLEGAL_PORT != _reliable_port) {
                its_reliable_subscriber = endpoint_definition::get(
                        _address, _reliable_port, true);
                its_reliable_target = its_reliable_subscriber;
            }
        }

        if (_ttl == 0) { // --> unsubscribe
            if (its_unreliable_target) {
                host_->on_unsubscribe(_service, _instance, _eventgroup, its_unreliable_target);
            }
            if (its_reliable_target) {
                host_->on_unsubscribe(_service, _instance, _eventgroup, its_reliable_target);
            }
            return;
        }

        insert_subscription_ack(its_message, _service, _instance, _eventgroup,
                its_info, _ttl);

        serialize_and_send(its_message, _address);

        // Finally register the new subscriber and send him all the fields(!)
        if (!is_nack) {
            if (its_unreliable_target && its_unreliable_subscriber) {
                host_->on_subscribe(_service, _instance, _eventgroup,
                        its_unreliable_subscriber, its_unreliable_target);
            }
            if (its_reliable_target && its_reliable_subscriber) {
                host_->on_subscribe(_service, _instance, _eventgroup,
                        its_reliable_subscriber, its_reliable_target);
            }
        }
    }
}

void service_discovery_impl::handle_eventgroup_subscription_ack(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        major_version_t _major, ttl_t _ttl,
        const boost::asio::ip::address &_address, uint16_t _port) {
    (void)_major;
    (void)_ttl;

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

void service_discovery_impl::serialize_and_send(
        std::shared_ptr<message_impl> _message,
        const boost::asio::ip::address &_address) {
    std::pair<session_t, bool> its_session = get_session(_address);
    _message->set_session(its_session.first);
    _message->set_reboot_flag(its_session.second);
    if(!serializer_->serialize(_message.get())) {
        VSOMEIP_ERROR << "service_discovery_impl::serialize_and_send: serialization error.";
        return;
    }
    if (host_->send_to(endpoint_definition::get(_address, port_, reliable_),
            serializer_->get_data(), serializer_->get_size())) {
        increment_session(_address);
    }
    serializer_->reset();
}

void service_discovery_impl::start_ttl_timer() {
    ttl_timer_.expires_from_now(std::chrono::seconds(smallest_ttl_));
    ttl_timer_.async_wait(
            std::bind(&service_discovery_impl::check_ttl, shared_from_this(),
                      std::placeholders::_1));
}

ttl_t service_discovery_impl::stop_ttl_timer() {
    ttl_t remaining = ttl_t(std::chrono::duration_cast<
                                std::chrono::seconds
                            >(ttl_timer_.expires_from_now()).count());
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
    return true;
}

void service_discovery_impl::send_eventgroup_subscription_nack(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup, major_version_t _major) {
    std::shared_ptr<runtime> its_runtime = runtime_.lock();
    if (!its_runtime) {
        return;
    }
    std::shared_ptr<message_impl> its_message = its_runtime->create_message();
    if (its_message) {
        std::shared_ptr<eventgroupinfo> its_info = host_->find_eventgroup(
                _service, _instance, _eventgroup);
        if(!its_info) {
            // Create a temporary info object with TTL=0
            its_info = std::make_shared < eventgroupinfo > (_major, 0);
        }
        insert_subscription_nack(its_message, _service, _instance, _eventgroup,
                its_info);
        serialize_and_send(its_message, get_current_remote_address());
    }
}

bool service_discovery_impl::check_layer_four_protocol(
        const std::shared_ptr<const ip_option_impl> _ip_option) const {
    if (_ip_option->get_layer_four_protocol() == layer_four_protocol_e::UNKNOWN) {
        VSOMEIP_ERROR << "Invalid layer 4 protocol in IP endpoint option";
        return false;
    }
    return true;
}

}  // namespace sd
}  // namespace vsomeip

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include <random>
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
#include "../include/service_discovery_host.hpp"
#include "../include/service_discovery_impl.hpp"
#include "../include/serviceentry_impl.hpp"
#include "../include/subscription.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/endpoint.hpp"
#include "../../endpoints/include/client_endpoint.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/serializer.hpp"
#include "../../routing/include/eventgroupinfo.hpp"
#include "../../routing/include/serviceinfo.hpp"
#include "../../plugin/include/plugin_manager.hpp"

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(service_discovery_host *_host)
        : io_(_host->get_io()),
          host_(_host),
          port_(VSOMEIP_SD_DEFAULT_PORT),
          reliable_(false),
          serializer_(
                  std::make_shared<serializer>(
                          host_->get_configuration()->get_buffer_shrink_threshold())),
          deserializer_(
                  std::make_shared<deserializer>(
                          host_->get_configuration()->get_buffer_shrink_threshold())),
          ttl_timer_(_host->get_io()),
          ttl_timer_runtime_(VSOMEIP_SD_DEFAULT_CYCLIC_OFFER_DELAY / 2),
          ttl_(VSOMEIP_SD_DEFAULT_TTL),
          subscription_expiration_timer_(_host->get_io()),
          max_message_size_(VSOMEIP_MAX_UDP_SD_PAYLOAD),
          initial_delay_(0),
          offer_debounce_time_(VSOMEIP_SD_DEFAULT_OFFER_DEBOUNCE_TIME),
          repetitions_base_delay_(VSOMEIP_SD_DEFAULT_REPETITIONS_BASE_DELAY),
          repetitions_max_(VSOMEIP_SD_DEFAULT_REPETITIONS_MAX),
          cyclic_offer_delay_(VSOMEIP_SD_DEFAULT_CYCLIC_OFFER_DELAY),
          offer_debounce_timer_(_host->get_io()),
          find_debounce_time_(VSOMEIP_SD_DEFAULT_FIND_DEBOUNCE_TIME),
          find_debounce_timer_(_host->get_io()),
          main_phase_timer_(_host->get_io()),
          is_suspended_(false),
          is_diagnosis_(false),
          last_msg_received_timer_(_host->get_io()),
          last_msg_received_timer_timeout_(VSOMEIP_SD_DEFAULT_CYCLIC_OFFER_DELAY +
                                           (VSOMEIP_SD_DEFAULT_CYCLIC_OFFER_DELAY / 10)) {
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
    runtime_ = std::dynamic_pointer_cast<sd::runtime>(plugin_manager::get()->get_plugin(plugin_type_e::SD_RUNTIME_PLUGIN, VSOMEIP_SD_LIBRARY));

    std::shared_ptr < configuration > its_configuration =
            host_->get_configuration();
    if (its_configuration) {
        unicast_ = its_configuration->get_unicast_address();
        sd_multicast_ = its_configuration->get_sd_multicast();
        boost::system::error_code ec;
        sd_multicast_address_ = boost::asio::ip::address::from_string(sd_multicast_, ec);

        port_ = its_configuration->get_sd_port();
        reliable_ = (its_configuration->get_sd_protocol()
                == "tcp");
        max_message_size_ = (reliable_ ? VSOMEIP_MAX_TCP_SD_PAYLOAD :
                VSOMEIP_MAX_UDP_SD_PAYLOAD);

        ttl_ = its_configuration->get_sd_ttl();

        // generate random initial delay based on initial delay min and max
        std::int32_t initial_delay_min =
                its_configuration->get_sd_initial_delay_min();
        if (initial_delay_min < 0) {
            initial_delay_min = VSOMEIP_SD_DEFAULT_INITIAL_DELAY_MIN;
        }
        std::int32_t initial_delay_max =
                its_configuration->get_sd_initial_delay_max();
        if (initial_delay_max < 0) {
            initial_delay_max = VSOMEIP_SD_DEFAULT_INITIAL_DELAY_MAX;
        }
        if (initial_delay_min > initial_delay_max) {
            const std::uint32_t tmp(initial_delay_min);
            initial_delay_min = initial_delay_max;
            initial_delay_max = tmp;
        }

        std::random_device r;
        std::mt19937 e(r());
        std::uniform_int_distribution<std::uint32_t> distribution(
                initial_delay_min, initial_delay_max);
        initial_delay_ = std::chrono::milliseconds(distribution(e));


        repetitions_base_delay_ = std::chrono::milliseconds(
                its_configuration->get_sd_repetitions_base_delay());
        repetitions_max_ = its_configuration->get_sd_repetitions_max();
        cyclic_offer_delay_ = std::chrono::milliseconds(
                its_configuration->get_sd_cyclic_offer_delay());
        offer_debounce_time_ = std::chrono::milliseconds(
                its_configuration->get_sd_offer_debounce_time());
        ttl_timer_runtime_ = cyclic_offer_delay_ / 2;

        ttl_factor_offers_ = its_configuration->get_ttl_factor_offers();
        ttl_factor_subscriptions_ = its_configuration->get_ttl_factor_subscribes();
        last_msg_received_timer_timeout_ = cyclic_offer_delay_
                + (cyclic_offer_delay_ / 10);
    } else {
        VSOMEIP_ERROR << "SD: no configuration found!";
    }
}

void service_discovery_impl::start() {
    if (!endpoint_) {
        endpoint_ = host_->create_service_discovery_endpoint(
                sd_multicast_, port_, reliable_);
        if (!endpoint_) {
            VSOMEIP_ERROR << "Couldn't start service discovery";
            return;
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(sessions_received_mutex_);
        sessions_received_.clear();
    }
    {
        std::lock_guard<std::mutex> its_lock(serialize_mutex_);
        sessions_sent_.clear();
    }

    if (is_suspended_) {
        // make sure to sent out FindService messages after resume
        std::lock_guard<std::mutex> its_lock(requested_mutex_);
        for (const auto &s : requested_) {
            for (const auto &i : s.second) {
                i.second->set_sent_counter(0);
            }
        }
        if (endpoint_) {
            // rejoin multicast group
            endpoint_->join(sd_multicast_);
        }
    }
    is_suspended_ = false;
    start_main_phase_timer();
    start_offer_debounce_timer(true);
    start_find_debounce_timer(true);
    start_ttl_timer();
}

void service_discovery_impl::stop() {
    is_suspended_ = true;
    stop_ttl_timer();
    stop_last_msg_received_timer();
}

void service_discovery_impl::request_service(service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        ttl_t _ttl) {
    std::lock_guard<std::mutex> its_lock(requested_mutex_);
    auto find_service = requested_.find(_service);
    if (find_service != requested_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance == find_service->second.end()) {
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
    std::lock_guard<std::mutex> its_lock(requested_mutex_);
    auto find_service = requested_.find(_service);
    if (find_service != requested_.end()) {
        find_service->second.erase(_instance);
    }
}

std::shared_ptr<request>
service_discovery_impl::find_request(service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(requested_mutex_);
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
                        if (found_client->second->get_major() != _major) {
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

    get_subscription_endpoints(its_unreliable, its_reliable,
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
                        _subscription_type, subscribe_count);
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

            const remote_offer_type_e its_offer_type = get_remote_offer_type(
                    _service, _instance);

            if (its_offer_type == remote_offer_type_e::UNRELIABLE &&
                    !its_subscription->get_endpoint(true) &&
                    its_subscription->get_endpoint(false)) {
                if (its_subscription->get_endpoint(false)->is_connected()) {
                    insert_subscription(its_message,
                            _service, _instance,
                            _eventgroup,
                            its_subscription, its_offer_type);
                } else {
                    its_subscription->set_udp_connection_established(false);
                }
            } else if (its_offer_type == remote_offer_type_e::RELIABLE &&
                    its_subscription->get_endpoint(true) &&
                    !its_subscription->get_endpoint(false)) {
                if (its_subscription->get_endpoint(true)->is_connected()) {
                    insert_subscription(its_message,
                            _service, _instance,
                            _eventgroup,
                            its_subscription, its_offer_type);
                } else {
                    its_subscription->set_tcp_connection_established(false);
                }
            } else if (its_offer_type == remote_offer_type_e::RELIABLE_UNRELIABLE &&
                    its_subscription->get_endpoint(true) &&
                    its_subscription->get_endpoint(false)) {
                if (its_subscription->get_endpoint(true)->is_connected() &&
                        its_subscription->get_endpoint(false)->is_connected()) {
                    insert_subscription(its_message,
                            _service, _instance,
                            _eventgroup,
                            its_subscription, its_offer_type);
                } else {
                    if (!its_subscription->get_endpoint(true)->is_connected()) {
                        its_subscription->set_tcp_connection_established(false);
                    }
                    if (!its_subscription->get_endpoint(false)->is_connected()) {
                        its_subscription->set_udp_connection_established(false);
                    }
                }
            }

            if(0 < its_message->get_entries().size()) {
                its_subscription->set_acknowledged(false);
            }
        }
    }
    if (has_address && its_message->get_entries().size()
            && its_message->get_options().size()) {
        serialize_and_send(its_message, its_address);
    }
}

void service_discovery_impl::get_subscription_endpoints(
        std::shared_ptr<endpoint>& _unreliable,
        std::shared_ptr<endpoint>& _reliable, boost::asio::ip::address* _address,
        bool* _has_address,
        service_t _service, instance_t _instance, client_t _client) const {
    _reliable = host_->find_or_create_remote_client(_service, _instance,
            true, _client);
    _unreliable = host_->find_or_create_remote_client(_service,
            _instance, false, _client);
    if (_unreliable) {
        std::shared_ptr<client_endpoint> its_client_endpoint =
                std::dynamic_pointer_cast<client_endpoint>(_unreliable);
        if (its_client_endpoint) {
            *_has_address = its_client_endpoint->get_remote_address(
                    *_address);
        }
    }
    if (_reliable) {
        std::shared_ptr<client_endpoint> its_client_endpoint =
                std::dynamic_pointer_cast<client_endpoint>(_reliable);
        if (its_client_endpoint) {
            *_has_address = *_has_address
                    || its_client_endpoint->get_remote_address(
                            *_address);
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
                const remote_offer_type_e its_offer_type = get_remote_offer_type(
                        _service, _instance);
                auto found_eventgroup = found_instance->second.find(_eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    auto found_client = found_eventgroup->second.find(_client);
                    if (found_client != found_eventgroup->second.end()) {
                        its_subscription = found_client->second;
                        its_subscription->set_ttl(0);
                        found_eventgroup->second.erase(_client);
                        auto endpoint = its_subscription->get_endpoint(false);
                        if (endpoint) {
                            std::shared_ptr<client_endpoint> its_client_endpoint =
                                    std::dynamic_pointer_cast<client_endpoint>(
                                            endpoint);
                            if (its_client_endpoint) {
                                has_address =
                                        its_client_endpoint->get_remote_address(
                                                its_address);
                            }
                        } else {
                            endpoint = its_subscription->get_endpoint(true);
                            if (endpoint) {
                                std::shared_ptr<client_endpoint> its_client_endpoint =
                                        std::dynamic_pointer_cast<
                                                client_endpoint>(endpoint);
                                if (its_client_endpoint) {
                                    has_address =
                                            its_client_endpoint->get_remote_address(
                                                    its_address);
                                }
                            } else {
                                return;
                            }
                        }
                        insert_subscription(its_message, _service, _instance,
                                _eventgroup, its_subscription, its_offer_type);
                    }
                }
            }
        }
    }
    if (has_address && its_message->get_entries().size()
            && its_message->get_options().size()) {
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

void service_discovery_impl::unsubscribe_client(service_t _service,
                                                instance_t _instance,
                                                client_t _client) {
    std::shared_ptr<runtime> its_runtime = runtime_.lock();
    if (!its_runtime) {
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
                const remote_offer_type_e its_offer_type = get_remote_offer_type(
                        _service, _instance);
                for (auto &found_eventgroup : found_instance->second) {
                    auto found_client = found_eventgroup.second.find(_client);
                    if (found_client != found_eventgroup.second.end()) {
                        its_subscription = found_client->second;
                        its_subscription->set_ttl(0);
                        found_eventgroup.second.erase(_client);
                        if (!has_address) {
                            auto endpoint = its_subscription->get_endpoint(
                                    false);
                            if (endpoint) {
                                std::shared_ptr<client_endpoint> its_client_endpoint =
                                        std::dynamic_pointer_cast<
                                                client_endpoint>(endpoint);
                                if (its_client_endpoint) {
                                    has_address =
                                            its_client_endpoint->get_remote_address(
                                                    its_address);
                                }
                            } else {
                                endpoint = its_subscription->get_endpoint(true);
                                if (endpoint) {
                                    std::shared_ptr<client_endpoint> its_client_endpoint =
                                            std::dynamic_pointer_cast<
                                                    client_endpoint>(endpoint);
                                    if (its_client_endpoint) {
                                        has_address =
                                                its_client_endpoint->get_remote_address(
                                                        its_address);
                                    }
                                } else {
                                    return;
                                }
                            }
                        }
                        insert_subscription(its_message, _service, _instance,
                                found_eventgroup.first, its_subscription, its_offer_type);
                    }
                }
            }
        }
    }
    if (has_address && its_message->get_entries().size()
            && its_message->get_options().size()) {
        serialize_and_send(its_message, its_address);
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

    auto its_received = sessions_received_.find(_sender);
    bool is_multicast = _destination.is_multicast();

    // Initialize both sessions with 0. Thus, the session identifier
    // for the session not being received from the network is stored
    // as 0 and will never trigger the reboot detection.
    session_t its_multicast_session(0), its_unicast_session(0);

    // Initialize both flags with true. Thus, the flag not being
    // received from the network will never trigger the reboot detection.
    bool its_multicast_reboot_flag(true), its_unicast_reboot_flag(true);

    if (is_multicast) {
        its_multicast_session = _session;
        its_multicast_reboot_flag = _reboot_flag;
    } else {
        its_unicast_session = _session;
        its_unicast_reboot_flag = _reboot_flag;
    }

    if (its_received == sessions_received_.end()) {
        sessions_received_[_sender]
            = std::make_tuple(its_multicast_session, its_unicast_session,
                    its_multicast_reboot_flag, its_unicast_reboot_flag);
    } else {
        // Reboot detection: Either the flag has changed from false to true,
        // or the session identifier overrun while the flag is true.
        if (_reboot_flag
            && ((is_multicast && !std::get<2>(its_received->second))
                || (!is_multicast && !std::get<3>(its_received->second)))) {
            result = true;
        } else {
            session_t its_old_session;
            bool its_old_reboot_flag;

            if (is_multicast) {
                its_old_session = std::get<0>(its_received->second);
                its_old_reboot_flag = std::get<2>(its_received->second);
            } else {
                its_old_session = std::get<1>(its_received->second);
                its_old_reboot_flag = std::get<3>(its_received->second);
            }

            if (its_old_reboot_flag && _reboot_flag
                    && its_old_session >= _session) {
                result = true;
            }
        }

        if (result == false) {
            // no reboot -> update session/flag
            if (is_multicast) {
                std::get<0>(its_received->second) = its_multicast_session;
                std::get<2>(its_received->second) = its_multicast_reboot_flag;
            } else {
                std::get<1>(its_received->second) = its_unicast_session;
                std::get<3>(its_received->second) = its_unicast_reboot_flag;
            }
        } else {
            // reboot -> reset the sender data
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
        std::shared_ptr<message_impl> &_message, const requests_t &_requests,
        uint32_t _start, uint32_t &_size, bool &_done) {
    std::lock_guard<std::mutex> its_lock(requested_mutex_);
    uint32_t its_size(0);
    uint32_t i = 0;

    _done = true;
    for (auto its_service : _requests) {
        for (auto its_instance : its_service.second) {
            auto its_request = its_instance.second;

            // check if release_service was called
            auto the_service = requested_.find(its_service.first);
            if ( the_service != requested_.end() ) {
                auto the_instance = the_service->second.find(its_instance.first);
                if(the_instance != the_service->second.end() ) {
                    uint8_t its_sent_counter = its_request->get_sent_counter();
                    if (its_sent_counter != repetitions_max_ + 1) {
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

        }
    }
    _size = its_size;
}

void service_discovery_impl::insert_offer_entries(
        std::shared_ptr<message_impl> &_message, const services_t &_services,
        uint32_t &_start, uint32_t _size, bool &_done, bool _ignore_phase) {
    uint32_t i = 0;
    uint32_t its_size(_size);
    for (const auto its_service : _services) {
        for (const auto its_instance : its_service.second) {
            if ((!is_suspended_)
                    && ((!is_diagnosis_) || (is_diagnosis_ && !host_->get_configuration()->is_someip(its_service.first, its_instance.first)))) {
                // Only insert services with configured endpoint(s)
                if ((_ignore_phase || its_instance.second->is_in_mainphase())
                        && (its_instance.second->get_endpoint(false)
                                || its_instance.second->get_endpoint(true))) {
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
    }
    _start = i;
    _done = true;
}

bool service_discovery_impl::insert_subscription(
        std::shared_ptr<message_impl> &_message, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<subscription> &_subscription,
        remote_offer_type_e _offer_type) {
    bool ret(false);
    std::shared_ptr<endpoint> its_reliable_endpoint(_subscription->get_endpoint(true));
    std::shared_ptr<endpoint> its_unreliable_endpoint(_subscription->get_endpoint(false));

    bool insert_reliable(false);
    bool insert_unreliable(false);
    switch (_offer_type) {
        case remote_offer_type_e::RELIABLE:
            if (its_reliable_endpoint) {
                insert_reliable = true;
            }
            break;
        case remote_offer_type_e::UNRELIABLE:
            if (its_unreliable_endpoint) {
                insert_unreliable = true;
            }
            break;
        case remote_offer_type_e::RELIABLE_UNRELIABLE:
            if (its_reliable_endpoint && its_unreliable_endpoint) {
                insert_reliable = true;
                insert_unreliable = true;
            }
            break;
        default:
            break;
    }

    if (!insert_reliable && !insert_unreliable) {
        VSOMEIP_WARNING << __func__ << ": Didn't insert subscription as "
                "subscription doesn't match offer type: ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "] "
                << _offer_type;
        return false;
    }
    std::shared_ptr<eventgroupentry_impl> its_entry;
    if (insert_reliable && its_reliable_endpoint) {
        const std::uint16_t its_port = its_reliable_endpoint->get_local_port();
        if (its_port) {
            its_entry = _message->create_eventgroup_entry();
            its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP);
            its_entry->set_service(_service);
            its_entry->set_instance(_instance);
            its_entry->set_eventgroup(_eventgroup);
            its_entry->set_counter(_subscription->get_counter());
            its_entry->set_major_version(_subscription->get_major());
            its_entry->set_ttl(_subscription->get_ttl());
            insert_option(_message, its_entry, unicast_, its_port, true);
            ret = true;
        } else {
            VSOMEIP_WARNING << __func__ << ": Didn't insert subscription as "
                    "local reliable port is zero: ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
            ret = false;
        }
    }
    if (insert_unreliable && its_unreliable_endpoint) {
        const std::uint16_t its_port = its_unreliable_endpoint->get_local_port();
        if (its_port) {
            if (!its_entry) {
                its_entry = _message->create_eventgroup_entry();
                its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP);
                its_entry->set_service(_service);
                its_entry->set_instance(_instance);
                its_entry->set_eventgroup(_eventgroup);
                its_entry->set_counter(_subscription->get_counter());
                its_entry->set_major_version(_subscription->get_major());
                its_entry->set_ttl(_subscription->get_ttl());
            }
            insert_option(_message, its_entry, unicast_, its_port, false);
            ret = true;
        } else {
            VSOMEIP_WARNING << __func__ << ": Didn't insert subscription as "
                    " local unreliable port is zero: ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
            ret = false;
        }
    }
    return ret;
}

bool service_discovery_impl::insert_nack_subscription_on_resubscribe(std::shared_ptr<message_impl> &_message,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            std::shared_ptr<subscription> &_subscription, remote_offer_type_e _offer_type) {
    bool ret(false);
    // SIP_SD_844:
    // This method is used for not acknowledged subscriptions on renew subscription
    // Two entries: Stop subscribe & subscribe within one SD-Message
    // One option: Both entries reference it

    const std::function<std::shared_ptr<eventgroupentry_impl>(ttl_t)> insert_entry
            = [&](ttl_t _ttl) {
        std::shared_ptr<eventgroupentry_impl> its_entry =
                _message->create_eventgroup_entry();
        // SUBSCRIBE_EVENTGROUP and STOP_SUBSCRIBE_EVENTGROUP are identical
        its_entry->set_type(entry_type_e::SUBSCRIBE_EVENTGROUP);
        its_entry->set_service(_service);
        its_entry->set_instance(_instance);
        its_entry->set_eventgroup(_eventgroup);
        its_entry->set_counter(_subscription->get_counter());
        its_entry->set_major_version(_subscription->get_major());
        its_entry->set_ttl(_ttl);
        return its_entry;
    };

    std::shared_ptr<endpoint> its_reliable_endpoint(_subscription->get_endpoint(true));
    std::shared_ptr<endpoint> its_unreliable_endpoint(_subscription->get_endpoint(false));

    if (_offer_type == remote_offer_type_e::UNRELIABLE &&
            !its_reliable_endpoint && its_unreliable_endpoint) {
        if (its_unreliable_endpoint->is_connected()) {
            const std::uint16_t its_port = its_unreliable_endpoint->get_local_port();
            if (its_port) {
                std::shared_ptr<eventgroupentry_impl> its_stop_entry = insert_entry(0);
                std::shared_ptr<eventgroupentry_impl> its_entry = insert_entry(_subscription->get_ttl());
                insert_option(_message, its_stop_entry, unicast_, its_port, false);
                insert_option(_message, its_entry, unicast_, its_port, false);
                ret = true;
            } else {
                VSOMEIP_WARNING << __func__ << ": Didn't insert subscription as "
                        "local unreliable port is zero: ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
            }
        } else {
            _subscription->set_udp_connection_established(false);
        }
    } else if (_offer_type == remote_offer_type_e::RELIABLE &&
            its_reliable_endpoint && !its_unreliable_endpoint) {
        if (its_reliable_endpoint->is_connected()) {
            const std::uint16_t its_port = its_reliable_endpoint->get_local_port();
            if (its_port) {
                std::shared_ptr<eventgroupentry_impl> its_stop_entry = insert_entry(0);
                std::shared_ptr<eventgroupentry_impl> its_entry = insert_entry(_subscription->get_ttl());
                insert_option(_message, its_stop_entry, unicast_, its_port, true);
                insert_option(_message, its_entry, unicast_, its_port, true);
                ret = true;
            } else {
                VSOMEIP_WARNING << __func__ << ": Didn't insert subscription as "
                        "local reliable port is zero: ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
            }
        } else {
            _subscription->set_tcp_connection_established(false);
        }
    } else if (_offer_type == remote_offer_type_e::RELIABLE_UNRELIABLE &&
            its_reliable_endpoint && its_unreliable_endpoint) {
        if (its_reliable_endpoint->is_connected() &&
                its_unreliable_endpoint->is_connected()) {
            const std::uint16_t its_reliable_port = its_reliable_endpoint->get_local_port();
            const std::uint16_t its_unreliable_port = its_unreliable_endpoint->get_local_port();
            if (its_reliable_port && its_unreliable_port) {
                std::shared_ptr<eventgroupentry_impl> its_stop_entry = insert_entry(0);
                std::shared_ptr<eventgroupentry_impl> its_entry = insert_entry(_subscription->get_ttl());
                insert_option(_message, its_stop_entry, unicast_, its_reliable_port, true);
                insert_option(_message, its_entry, unicast_, its_reliable_port, true);
                insert_option(_message, its_stop_entry, unicast_, its_unreliable_port, false);
                insert_option(_message, its_entry, unicast_, its_unreliable_port, false);
                ret = true;
            } else if (!its_reliable_port) {
                VSOMEIP_WARNING << __func__ << ": Didn't insert subscription as "
                        "local reliable port is zero: ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
            } else if (!its_unreliable_port) {
                VSOMEIP_WARNING << __func__ << ": Didn't insert subscription as "
                        "local unreliable port is zero: ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
            }
        } else {
            if (!its_reliable_endpoint->is_connected()) {
                _subscription->set_tcp_connection_established(false);
            }
            if (!its_unreliable_endpoint->is_connected()) {
                _subscription->set_udp_connection_established(false);
            }
        }
    } else {
        VSOMEIP_WARNING << __func__ << ": Couldn't insert StopSubscribe/Subscribe ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "] "
                << _offer_type;
    }
    return ret;
}

void service_discovery_impl::insert_subscription_ack(
        std::shared_ptr<message_impl> &_message, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        const std::shared_ptr<eventgroupinfo> &_info, ttl_t _ttl,
        uint8_t _counter, major_version_t _major, uint16_t _reserved,
        const std::shared_ptr<endpoint_definition> &_target) {
    std::unique_lock<std::mutex> its_lock(_message->get_message_lock());
    _message->increase_number_contained_acks();
    for (auto its_entry : _message->get_entries()) {
        if (its_entry->is_eventgroup_entry()) {
            std::shared_ptr < eventgroupentry_impl > its_eventgroup_entry =
                    std::dynamic_pointer_cast < eventgroupentry_impl
                            > (its_entry);
            if(its_eventgroup_entry->get_type() == entry_type_e::SUBSCRIBE_EVENTGROUP_ACK
                    && its_eventgroup_entry->get_service() == _service
                    && its_eventgroup_entry->get_instance() == _instance
                    && its_eventgroup_entry->get_eventgroup() == _eventgroup
                    && its_eventgroup_entry->get_major_version() == _major
                    && its_eventgroup_entry->get_reserved() == _reserved
                    && its_eventgroup_entry->get_counter() == _counter
                    && its_eventgroup_entry->get_ttl() == _ttl) {
                if (_target) {
                    if (_target->is_reliable()) {
                        if (!its_eventgroup_entry->get_target(true)) {
                            its_eventgroup_entry->add_target(_target);
                        }
                    } else {
                        if (!its_eventgroup_entry->get_target(false)) {
                            its_eventgroup_entry->add_target(_target);
                        }
                    }
                }
                return;
            }
        }
    }

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
    if (_target) {
        its_entry->add_target(_target);
    }

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
    std::unique_lock<std::mutex> its_lock(_message->get_message_lock());
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
    _message->increase_number_contained_acks();
}

bool service_discovery_impl::send(bool _is_announcing) {
    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (its_runtime) {
        std::vector< std::shared_ptr< message_impl > > its_messages;
        std::shared_ptr < message_impl > its_message;

        if(_is_announcing) {
            its_message = its_runtime->create_message();
            its_messages.push_back(its_message);

            services_t its_offers = host_->get_offered_services();
            fill_message_with_offer_entries(its_runtime, its_message,
                    its_messages, its_offers, false);

            // Serialize and send
            return serialize_and_send_messages(its_messages);
        }
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
    VSOMEIP_INFO << msg.str();
#endif
    std::lock_guard<std::mutex> its_session_lock(sessions_received_mutex_);

    if(is_suspended_) {
        return;
    }
    // ignore all SD messages with source address equal to node's unicast address
    if (!check_source_address(_sender)) {
        return;
    }
    if (_destination == sd_multicast_address_) {
        std::lock_guard<std::mutex> its_lock(last_msg_received_timer_mutex_);
        boost::system::error_code ec;
        last_msg_received_timer_.cancel(ec);
        last_msg_received_timer_.expires_from_now(last_msg_received_timer_timeout_, ec);
        last_msg_received_timer_.async_wait(
                std::bind(
                        &service_discovery_impl::on_last_msg_received_timer_expired,
                        shared_from_this(), std::placeholders::_1));
    }

    current_remote_address_ = _sender;
    deserializer_->set_data(_data, _length);
    std::shared_ptr < message_impl
            > its_message(deserializer_->deserialize_sd_message());
    deserializer_->reset();
    if (its_message) {
        // ignore all messages which are sent with invalid header fields
        if(!check_static_header_fields(its_message)) {
            return;
        }
        // Expire all subscriptions / services in case of reboot
        if (is_reboot(_sender, _destination,
                its_message->get_reboot_flag(), its_message->get_session())) {
            VSOMEIP_INFO << "Reboot detected: IP=" << _sender.to_string();
            remove_remote_offer_type_by_ip(_sender);
            host_->expire_subscriptions(_sender);
            host_->expire_services(_sender);
        }

        std::vector < std::shared_ptr<option_impl> > its_options =
                its_message->get_options();

        std::shared_ptr<runtime> its_runtime = runtime_.lock();
        if (!its_runtime) {
            return;
        }

        std::shared_ptr < message_impl > its_message_response
            = its_runtime->create_message();

        const std::uint8_t its_required_acks =
                its_message->get_number_required_acks();
        its_message_response->set_number_required_acks(its_required_acks);
        std::shared_ptr<sd_message_identifier_t> its_message_id =
                std::make_shared<sd_message_identifier_t>(
                        its_message->get_session(), _sender, _destination,
                        its_message_response);

        std::vector<std::pair<std::uint16_t, std::shared_ptr<message_impl>>> its_resubscribes;
        // 28 Bytes for SD-Header + Length Entries and Options Array
        its_resubscribes.push_back(std::make_pair(28, its_runtime->create_message()));

        const message_impl::entries_t& its_entries = its_message->get_entries();
        const message_impl::entries_t::const_iterator its_end = its_entries.end();
        bool is_stop_subscribe_subscribe(false);

        for (auto iter = its_entries.begin(); iter != its_end; iter++) {
            if ((*iter)->is_service_entry()) {
                std::shared_ptr < serviceentry_impl > its_service_entry =
                        std::dynamic_pointer_cast < serviceentry_impl
                                > (*iter);
                bool its_unicast_flag = its_message->get_unicast_flag();
                process_serviceentry(its_service_entry, its_options,
                        its_unicast_flag, &its_resubscribes);
            } else {
                std::shared_ptr < eventgroupentry_impl > its_eventgroup_entry =
                        std::dynamic_pointer_cast < eventgroupentry_impl
                                > (*iter);
                bool force_initial_events(false);
                if (is_stop_subscribe_subscribe) {
                    force_initial_events = true;
                }
                is_stop_subscribe_subscribe = check_stop_subscribe_subscribe(
                        iter, its_end, its_message->get_options());
                process_eventgroupentry(its_eventgroup_entry, its_options,
                        its_message_response, _destination,
                        its_message_id, is_stop_subscribe_subscribe, force_initial_events);
            }
        }

        // send answer directly if SubscribeEventgroup entries were (n)acked
        if (its_required_acks || its_message_response->get_number_required_acks() > 0) {
            bool sent(false);
            {
                std::lock_guard<std::mutex> its_lock(response_mutex_);
                if (its_message_response->all_required_acks_contained()) {
                    update_subscription_expiration_timer(its_message_response);
                    serialize_and_send(its_message_response, _sender);
                    // set required acks to 0xFF to mark message as sent
                    its_message_response->set_number_required_acks((std::numeric_limits<uint8_t>::max)());
                    sent = true;
                }
            }
            if (sent) {
                for (const auto &fie : its_message_response->forced_initial_events_get()) {
                    host_->send_initial_events(fie.service_, fie.instance_,
                            fie.eventgroup_, fie.target_);
                }
                if (its_message_response->initial_events_required()) {
                    for (const auto& ack_tuple :
                            get_eventgroups_requiring_initial_events(its_message_response)) {
                        host_->send_initial_events(std::get<0>(ack_tuple),
                                std::get<1>(ack_tuple), std::get<2>(ack_tuple),
                                std::get<3>(ack_tuple));
                    }
                }
            }
        }
        for (const auto& response : its_resubscribes) {
            if (response.second->get_entries().size() && response.second->get_options().size()) {
                serialize_and_send(response.second, _sender);
            }
        }
    } else {
        VSOMEIP_ERROR << "service_discovery_impl::on_message: deserialization error.";
        return;
    }
}

// Entry processing
void service_discovery_impl::process_serviceentry(
        std::shared_ptr<serviceentry_impl> &_entry,
        const std::vector<std::shared_ptr<option_impl> > &_options,
        bool _unicast_flag,
        std::vector<std::pair<std::uint16_t, std::shared_ptr<message_impl>>>* _resubscribes) {

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
                        its_unreliable_address, its_unreliable_port, _resubscribes);
                break;
            case entry_type_e::UNKNOWN:
            default:
                VSOMEIP_ERROR << "Unsupported serviceentry type";
        }

    } else {
        std::shared_ptr<request> its_request = find_request(its_service, its_instance);
        if (its_request) {
            std::lock_guard<std::mutex> its_lock(requested_mutex_);
            // ID: SIP_SD_830
            its_request->set_sent_counter(std::uint8_t(repetitions_max_ + 1));
        }
        remove_remote_offer_type(its_service, its_instance,
                                 (its_reliable_port != ILLEGAL_PORT ?
                                         its_reliable_address : its_unreliable_address));
        unsubscribe_all(its_service, its_instance);
        if (!is_diagnosis_ && !is_suspended_) {
            host_->del_routing_info(its_service, its_instance,
                                    (its_reliable_port != ILLEGAL_PORT),
                                    (its_unreliable_port != ILLEGAL_PORT));
        }
    }
}

void service_discovery_impl::process_offerservice_serviceentry(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, ttl_t _ttl,
        const boost::asio::ip::address &_reliable_address,
        uint16_t _reliable_port,
        const boost::asio::ip::address &_unreliable_address,
        uint16_t _unreliable_port,
        std::vector<std::pair<std::uint16_t, std::shared_ptr<message_impl>>>* _resubscribes) {
    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (!its_runtime)
        return;

    std::shared_ptr<request> its_request = find_request(_service, _instance);
    if (its_request) {
        std::lock_guard<std::mutex> its_lock(requested_mutex_);
        its_request->set_sent_counter(std::uint8_t(repetitions_max_ + 1));
    }
    remote_offer_type_e offer_type(remote_offer_type_e::UNKNOWN);
    if (_reliable_port != ILLEGAL_PORT
            && _unreliable_port != ILLEGAL_PORT
            && !_reliable_address.is_unspecified()
            && !_unreliable_address.is_unspecified()) {
        offer_type = remote_offer_type_e::RELIABLE_UNRELIABLE;
    } else if (_unreliable_port != ILLEGAL_PORT
            && !_unreliable_address.is_unspecified()) {
        offer_type = remote_offer_type_e::UNRELIABLE;
    } else if (_reliable_port != ILLEGAL_PORT
            && !_reliable_address.is_unspecified()) {
        offer_type = remote_offer_type_e::RELIABLE;
    } else {
        VSOMEIP_WARNING << __func__ << ": unknown remote offer type ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "]";
    }

    if (update_remote_offer_type(_service,_instance, offer_type,
            _reliable_address, _unreliable_address)) {
        VSOMEIP_WARNING << __func__ << ": Remote offer type changed ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "]";
    }


    host_->add_routing_info(_service, _instance,
                            _major, _minor,
                            _ttl * get_ttl_factor(_service, _instance, ttl_factor_offers_),
                            _reliable_address, _reliable_port,
                            _unreliable_address, _unreliable_port);

    const std::function<void(std::uint16_t)> check_space =
            [&_resubscribes, &its_runtime](std::uint16_t _number) {
        if (_resubscribes->back().first + _number > VSOMEIP_MAX_UDP_MESSAGE_SIZE) {
            // 28 Bytes for SD-Header + Length Entries and Options Array
            _resubscribes->push_back(std::make_pair(28 + _number,
                                                    its_runtime->create_message()));
        } else {
            _resubscribes->back().first =
                    static_cast<std::uint16_t>(_resubscribes->back().first + _number);
        }
    };

    std::lock_guard<std::mutex> its_lock(subscribed_mutex_);
    auto found_service = subscribed_.find(_service);
    if (found_service != subscribed_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            if (0 < found_instance->second.size()) {
                const remote_offer_type_e its_offer_type =
                        get_remote_offer_type(_service, _instance);
                for (auto its_eventgroup : found_instance->second) {
                    for (auto its_client : its_eventgroup.second) {
                        std::shared_ptr<subscription> its_subscription(its_client.second);
                        std::shared_ptr<endpoint> its_unreliable;
                        std::shared_ptr<endpoint> its_reliable;
                        bool has_address(false);
                        boost::asio::ip::address its_address;
                        get_subscription_endpoints(
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
                            if (its_offer_type == remote_offer_type_e::UNRELIABLE) {
                                if (its_unreliable && its_unreliable->is_connected()) {
                                    // 28 = 16 (subscription) + 12 (option)
                                    check_space(28);
                                    const std::size_t options_size_before =
                                            _resubscribes->back().second->get_options().size();
                                    if (insert_subscription(_resubscribes->back().second,
                                            _service, _instance,
                                            its_eventgroup.first,
                                            its_subscription, its_offer_type)) {
                                        its_subscription->set_acknowledged(false);
                                        const std::size_t options_size_after =
                                                _resubscribes->back().second->get_options().size();
                                        const std::size_t diff = options_size_after - options_size_before;
                                        _resubscribes->back().first =
                                                static_cast<std::uint16_t>(
                                                        _resubscribes->back().first + (12u * diff - 12u));
                                    } else {
                                        _resubscribes->back().first =
                                                static_cast<std::uint16_t>(
                                                        _resubscribes->back().first - 28);
                                    }
                                }
                            } else if (its_offer_type == remote_offer_type_e::RELIABLE) {
                                if (its_reliable && its_reliable->is_connected()) {
                                    // 28 = 16 (subscription) + 12 (option)
                                    check_space(28);
                                    const std::size_t options_size_before =
                                            _resubscribes->back().second->get_options().size();
                                    if (insert_subscription(_resubscribes->back().second,
                                            _service, _instance,
                                            its_eventgroup.first,
                                            its_subscription, its_offer_type)) {
                                        its_subscription->set_acknowledged(false);
                                        const std::size_t options_size_after =
                                                _resubscribes->back().second->get_options().size();
                                        const std::size_t diff = options_size_after - options_size_before;
                                        _resubscribes->back().first =
                                                static_cast<std::uint16_t>(
                                                        _resubscribes->back().first + (12u * diff - 12u));
                                    } else {
                                        _resubscribes->back().first =
                                                static_cast<std::uint16_t>(
                                                        _resubscribes->back().first - 28);
                                    }
                                } else {
                                    its_client.second->set_tcp_connection_established(false);
                                    // restart TCP endpoint if not connected
                                    if (its_reliable) {
                                        its_reliable->restart();
                                    }
                                }
                            } else if (its_offer_type == remote_offer_type_e::RELIABLE_UNRELIABLE) {
                                if (its_reliable && its_unreliable &&
                                        its_reliable->is_connected() &&
                                        its_unreliable->is_connected()) {
                                    // 40 = 16 (subscription) + 2x12 (option)
                                    check_space(40);
                                    const std::size_t options_size_before =
                                            _resubscribes->back().second->get_options().size();
                                    if (insert_subscription(_resubscribes->back().second,
                                            _service, _instance,
                                            its_eventgroup.first,
                                            its_subscription, its_offer_type)) {
                                        its_subscription->set_acknowledged(false);
                                        const std::size_t options_size_after =
                                                _resubscribes->back().second->get_options().size();
                                        const std::size_t diff = options_size_after - options_size_before;
                                        _resubscribes->back().first =
                                                static_cast<std::uint16_t>(
                                                        _resubscribes->back().first + (12u * diff - 24u));
                                    } else {
                                        _resubscribes->back().first =
                                                static_cast<std::uint16_t>(
                                                        _resubscribes->back().first - 40);
                                    }
                                } else if (its_reliable && !its_reliable->is_connected()) {
                                    its_client.second->set_tcp_connection_established(false);
                                    // restart TCP endpoint if not connected
                                    its_reliable->restart();
                                }
                            } else {
                                VSOMEIP_WARNING << __func__ << ": unknown remote offer type ["
                                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "]";
                            }
                        } else {
                            // 56 = 2x16 (subscription) + 2x12 (option)
                            check_space(56);
                            const std::size_t options_size_before =
                                    _resubscribes->back().second->get_options().size();
                            if (insert_nack_subscription_on_resubscribe(_resubscribes->back().second,
                                    _service, _instance, its_eventgroup.first,
                                    its_subscription, its_offer_type) ) {
                                const std::size_t options_size_after =
                                        _resubscribes->back().second->get_options().size();
                                const std::size_t diff = options_size_after - options_size_before;
                                _resubscribes->back().first =
                                        static_cast<std::uint16_t>(
                                                _resubscribes->back().first + (12u * diff - 24u));
                            } else {
                                _resubscribes->back().first =
                                        static_cast<std::uint16_t>(
                                                _resubscribes->back().first - 56u);
                            }

                            // restart TCP endpoint if not connected
                            if (its_reliable && !its_reliable->is_connected()) {
                                its_reliable->restart();
                            }
                        }
                    }
                }
            }
        }
    }
}

void service_discovery_impl::process_findservice_serviceentry(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, bool _unicast_flag) {

    if (_instance != ANY_INSTANCE) {
        std::shared_ptr<serviceinfo> its_info = host_->get_offered_service(
                _service, _instance);
        if (its_info) {
            send_uni_or_multicast_offerservice(_service, _instance, _major,
                    _minor, its_info, _unicast_flag);
        }
    } else {
        std::map<instance_t, std::shared_ptr<serviceinfo>> offered_instances =
                host_->get_offered_service_instances(_service);
        // send back all available instances
        for (const auto &found_instance : offered_instances) {
            send_uni_or_multicast_offerservice(_service, found_instance.first, _major,
                    _minor, found_instance.second, _unicast_flag);
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
                if (current_remote_address_.is_unspecified()) {
                    VSOMEIP_ERROR << "service_discovery_impl::"
                            "send_unicast_offer_service current remote address "
                            "is unspecified, won't send offer.";
                } else {
                    serialize_and_send(its_message, current_remote_address_);
                }
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

void service_discovery_impl::on_endpoint_connected(
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
                    const remote_offer_type_e its_offer_type =
                            get_remote_offer_type(_service, _instance);
                    for(const auto &its_eventgroup : found_instance->second) {
                        for(const auto &its_client : its_eventgroup.second) {
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
                            std::shared_ptr<subscription> its_subscription(its_client.second);
                            if (its_subscription) {
                                if (!its_subscription->is_tcp_connection_established() ||
                                        !its_subscription->is_udp_connection_established()) {
                                    const std::shared_ptr<const endpoint> its_reliable_endpoint(
                                            its_subscription->get_endpoint(true));
                                    const std::shared_ptr<const endpoint> its_unreliable_endpoint(
                                            its_subscription->get_endpoint(false));
                                    if(its_reliable_endpoint && its_reliable_endpoint->is_connected()) {
                                        if(its_reliable_endpoint.get() == _endpoint.get()) {
                                            // mark tcp as established
                                            its_subscription->set_tcp_connection_established(true);
                                        }
                                    }
                                    if(its_unreliable_endpoint && its_unreliable_endpoint->is_connected()) {
                                        if(its_reliable_endpoint.get() == _endpoint.get()) {
                                            // mark udp as established
                                            its_subscription->set_udp_connection_established(true);
                                        }
                                    }
                                    if ((its_reliable_endpoint && its_unreliable_endpoint &&
                                            its_subscription->is_tcp_connection_established() &&
                                            its_subscription->is_udp_connection_established()) ||
                                            (its_reliable_endpoint && !its_unreliable_endpoint &&
                                                    its_subscription->is_tcp_connection_established()) ||
                                                    (its_unreliable_endpoint && !its_reliable_endpoint &&
                                                            its_subscription->is_udp_connection_established())) {
                                        std::shared_ptr<endpoint> its_unreliable;
                                        std::shared_ptr<endpoint> its_reliable;
                                        get_subscription_endpoints(
                                                its_unreliable, its_reliable, &its_address,
                                                &has_address, _service, _instance,
                                                its_client.first);
                                        its_subscription->set_endpoint(its_reliable, true);
                                        its_subscription->set_endpoint(its_unreliable, false);
                                        insert_subscription(its_message, _service,
                                                _instance, its_eventgroup.first,
                                                its_subscription, its_offer_type);
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
    if (has_address && its_message->get_entries().size()
            && its_message->get_options().size()) {
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
                    unicast_.to_v6().to_bytes(), its_unreliable->get_local_port(),
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
            }

            if (its_unreliable) {
                insert_option(_message, its_entry, unicast_,
                        its_unreliable->get_local_port(), false);
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
        const boost::asio::ip::address &_destination,
        const std::shared_ptr<sd_message_identifier_t> &_message_id,
        bool _is_stop_subscribe_subscribe,
        bool _force_initial_events) {
    service_t its_service = _entry->get_service();
    instance_t its_instance = _entry->get_instance();
    eventgroup_t its_eventgroup = _entry->get_eventgroup();
    entry_type_e its_type = _entry->get_type();
    major_version_t its_major = _entry->get_major_version();
    ttl_t its_ttl = _entry->get_ttl();
    uint16_t its_reserved = _entry->get_reserved();
    uint8_t its_counter = _entry->get_counter();

    bool has_two_options_ = (_entry->get_num_options(1) + _entry->get_num_options(2) >= 2) ? true : false;

    if (_entry->get_owning_message()->get_return_code() != return_code) {
        boost::system::error_code ec;
        VSOMEIP_ERROR << "Invalid return code in SD header "
                << _message_id->sender_.to_string(ec) << " session: "
                << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
        if(its_ttl > 0) {
            if (has_two_options_) {
                its_message_response->decrease_number_required_acks();
            }
            insert_subscription_nack(its_message_response, its_service, its_instance,
                                     its_eventgroup, its_counter, its_major, its_reserved);
        }
        return;
    }

    if(its_type == entry_type_e::SUBSCRIBE_EVENTGROUP) {
        if( _destination.is_multicast() ) {
            boost::system::error_code ec;
            VSOMEIP_ERROR << "Received a SubscribeEventGroup entry on multicast address "
                    << _message_id->sender_.to_string(ec) << " session: "
                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
            if(its_ttl > 0) {
                if (has_two_options_) {
                    its_message_response->decrease_number_required_acks();
                }
                insert_subscription_nack(its_message_response, its_service, its_instance,
                                         its_eventgroup, its_counter, its_major, its_reserved);
            }
            return;
        }
        if (_entry->get_num_options(1) == 0
                && _entry->get_num_options(2) == 0) {
            boost::system::error_code ec;
            VSOMEIP_ERROR << "Invalid number of options in SubscribeEventGroup entry "
                    << _message_id->sender_.to_string(ec) << " session: "
                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
            if(its_ttl > 0) {
                // increase number of required acks by one as number required acks
                // is calculated based on the number of referenced options
                its_message_response->increase_number_required_acks();
                insert_subscription_nack(its_message_response, its_service, its_instance,
                                         its_eventgroup, its_counter, its_major, its_reserved);
            }
            return;
        }
        if(_entry->get_owning_message()->get_options_length() < 12) {
            boost::system::error_code ec;
            VSOMEIP_ERROR << "Invalid options length in SD message "
                    << _message_id->sender_.to_string(ec) << " session: "
                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
            if(its_ttl > 0) {
                if (has_two_options_) {
                    its_message_response->decrease_number_required_acks();
                }
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
            boost::system::error_code ec;
            VSOMEIP_ERROR << "Fewer options in SD message than "
                             "referenced in EventGroup entry or malformed option received "
                    << _message_id->sender_.to_string(ec) << " session: "
                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
            if(its_ttl > 0) {
                // set to 0 to ensure an answer containing at least this subscribe_nack is sent out
                its_message_response->set_number_required_acks(0);
                insert_subscription_nack(its_message_response, its_service, its_instance,
                                         its_eventgroup, its_counter, its_major, its_reserved);
            }
            return;
        }
        if(_entry->get_owning_message()->get_someip_length() < _entry->get_owning_message()->get_length()
                && its_ttl > 0) {
            boost::system::error_code ec;
            VSOMEIP_ERROR  << std::dec << "SomeIP length field in SubscribeEventGroup message header: ["
                                << _entry->get_owning_message()->get_someip_length()
                                << "] bytes, is shorter than length of deserialized message: ["
                                << (uint32_t) _entry->get_owning_message()->get_length() << "] bytes. "
                                << _message_id->sender_.to_string(ec) << " session: "
                                << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
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
#ifdef _WIN32
                e; // silence MSVC warining C4101
#endif
                boost::system::error_code ec;
                VSOMEIP_ERROR << "Fewer options in SD message than "
                                 "referenced in EventGroup entry for "
                                 "option run number: " << i << " "
                                 << _message_id->sender_.to_string(ec) << " session: "
                                 << std::hex << std::setw(4) << std::setfill('0')
                                 << _message_id->session_;
                if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type && its_ttl > 0) {
                    if (has_two_options_) {
                        its_message_response->decrease_number_required_acks();
                    }
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
                            if (has_two_options_) {
                                its_message_response->decrease_number_required_acks();
                            }
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

                        // reject subscription referencing two conflicting options of same protocol type
                        // ID: SIP_SD_1144
                        if (is_first_reliable == is_second_reliable
                                && its_second_port != ILLEGAL_PORT) {
                            if (its_ttl > 0) {
                                if (has_two_options_) {
                                     its_message_response->decrease_number_required_acks();
                                }
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                          its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            boost::system::error_code ec;
                            VSOMEIP_ERROR << "Multiple IPv4 endpoint options of same kind referenced! "
                                    << _message_id->sender_.to_string(ec) << " session: "
                                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_
                                    << " is_first_reliable: " << is_first_reliable;
                            return;
                        }

                        if(!check_ipv4_address(its_first_address)
                                || 0 == its_first_port) {
                            if(its_ttl > 0) {
                                if (has_two_options_) {
                                    its_message_response->decrease_number_required_acks();
                                }
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                         its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            boost::system::error_code ec;
                            VSOMEIP_ERROR << "Invalid port or IP address in first IPv4 endpoint option specified! "
                                    << _message_id->sender_.to_string(ec) << " session: "
                                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                            return;
                        }
                    } else
                    if (its_second_port == ILLEGAL_PORT) {
                        its_second_address = its_ipv4_address;
                        its_second_port = its_ipv4_option->get_port();
                        is_second_reliable = (its_ipv4_option->get_layer_four_protocol()
                                              == layer_four_protocol_e::TCP);

                        // reject subscription referencing two conflicting options of same protocol type
                        // ID: SIP_SD_1144
                        if (is_second_reliable == is_first_reliable
                                && its_first_port != ILLEGAL_PORT) {
                            if (its_ttl > 0) {
                                if (has_two_options_) {
                                     its_message_response->decrease_number_required_acks();
                                }
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                          its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            boost::system::error_code ec;
                            VSOMEIP_ERROR << "Multiple IPv4 endpoint options of same kind referenced! "
                                    << _message_id->sender_.to_string(ec) << " session: "
                                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_
                                    << " is_second_reliable: " << is_second_reliable;
                            return;
                        }

                        if(!check_ipv4_address(its_second_address)
                                || 0 == its_second_port) {
                            if(its_ttl > 0) {
                                if (has_two_options_) {
                                    its_message_response->decrease_number_required_acks();
                                }
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                         its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            boost::system::error_code ec;
                            VSOMEIP_ERROR << "Invalid port or IP address in second IPv4 endpoint option specified! "
                                    << _message_id->sender_.to_string(ec) << " session: "
                                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                            return;
                        }
                    } else {
                        // TODO: error message, too many endpoint options!
                    }
                } else {
                    boost::system::error_code ec;
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv4 Endpoint)"
                            << _message_id->sender_.to_string(ec) << " session: "
                            << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
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
                            if (has_two_options_) {
                                its_message_response->decrease_number_required_acks();
                            }
                            insert_subscription_nack(its_message_response, its_service, its_instance,
                                                     its_eventgroup, its_counter, its_major, its_reserved);
                        }
                        boost::system::error_code ec;
                        VSOMEIP_ERROR << "Invalid layer 4 protocol type in IPv6 endpoint option specified! "
                                << _message_id->sender_.to_string(ec) << " session: "
                                << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                        return;
                    }

                    if (its_first_port == ILLEGAL_PORT) {
                        its_first_address = its_ipv6_address;
                        its_first_port = its_ipv6_option->get_port();
                        is_first_reliable = (its_ipv6_option->get_layer_four_protocol()
                                             == layer_four_protocol_e::TCP);

                        if (is_first_reliable == is_second_reliable
                                && its_second_port != ILLEGAL_PORT) {
                            if (its_ttl > 0) {
                                if (has_two_options_) {
                                     its_message_response->decrease_number_required_acks();
                                }
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                          its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            boost::system::error_code ec;
                            VSOMEIP_ERROR << "Multiple IPv6 endpoint options of same kind referenced! "
                                    << _message_id->sender_.to_string(ec) << " session: "
                                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_
                                    << " is_first_reliable: " << is_first_reliable;
                            return;
                        }
                    } else
                    if (its_second_port == ILLEGAL_PORT) {
                        its_second_address = its_ipv6_address;
                        its_second_port = its_ipv6_option->get_port();
                        is_second_reliable = (its_ipv6_option->get_layer_four_protocol()
                                              == layer_four_protocol_e::TCP);

                        if (is_second_reliable == is_first_reliable
                                && its_first_port != ILLEGAL_PORT) {
                            if (its_ttl > 0) {
                                if (has_two_options_) {
                                     its_message_response->decrease_number_required_acks();
                                }
                                insert_subscription_nack(its_message_response, its_service, its_instance,
                                                          its_eventgroup, its_counter, its_major, its_reserved);
                            }
                            boost::system::error_code ec;
                            VSOMEIP_ERROR << "Multiple IPv6 endpoint options of same kind referenced! "
                                    << _message_id->sender_.to_string(ec) << " session: "
                                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_
                                    << " is_second_reliable: " << is_second_reliable;
                            return;
                        }
                    } else {
                        // TODO: error message, too many endpoint options!
                    }
                } else {
                    boost::system::error_code ec;
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv6 Endpoint) "
                            << _message_id->sender_.to_string(ec) << " session: "
                            << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
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
                    // ID: SIP_SD_946, ID: SIP_SD_1144
                    if (its_first_port != ILLEGAL_PORT
                            && its_second_port != ILLEGAL_PORT) {
                        boost::system::error_code ec;
                        VSOMEIP_ERROR << "Multiple IPv4 multicast options referenced! "
                                << _message_id->sender_.to_string(ec) << " session: "
                                << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                        return;
                    }
                } else {
                    boost::system::error_code ec;
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv4 Multicast) "
                            << _message_id->sender_.to_string(ec) << " session: "
                            << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
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
                    // ID: SIP_SD_946, ID: SIP_SD_1144
                    if (its_first_port != ILLEGAL_PORT
                            && its_second_port != ILLEGAL_PORT) {
                        boost::system::error_code ec;
                        VSOMEIP_ERROR << "Multiple IPv6 multicast options referenced! "
                                << _message_id->sender_.to_string(ec) << " session: "
                                << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                        return;
                    }
                } else {
                    boost::system::error_code ec;
                    VSOMEIP_ERROR
                            << "Invalid eventgroup option (IPv6 Multicast) "
                            << _message_id->sender_.to_string(ec) << " session: "
                            << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                }
                break;
            case option_type_e::CONFIGURATION: {
                its_message_response->decrease_number_required_acks();
                break;
            }
            case option_type_e::UNKNOWN:
            default:
                boost::system::error_code ec;
                VSOMEIP_WARNING << "Unsupported eventgroup option "
                    << _message_id->sender_.to_string(ec) << " session: "
                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                if(its_ttl > 0) {
                    its_message_response->decrease_number_required_acks();
                    insert_subscription_nack(its_message_response, its_service, its_instance,
                                             its_eventgroup, its_counter, its_major, its_reserved);
                    return;
                }
                break;
            }
        }
    }

    if (entry_type_e::SUBSCRIBE_EVENTGROUP == its_type) {
        handle_eventgroup_subscription(its_service, its_instance,
                its_eventgroup, its_major, its_ttl, its_counter, its_reserved,
                its_first_address, its_first_port, is_first_reliable,
                its_second_address, its_second_port, is_second_reliable, its_message_response,
                _message_id, _is_stop_subscribe_subscribe, _force_initial_events);
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
        const std::shared_ptr<sd_message_identifier_t> &_message_id,
        bool _is_stop_subscribe_subscribe, bool _force_initial_events) {

    if (its_message) {
        bool has_reliable_events(false);
        bool has_unreliable_events(false);
        bool has_two_options_ = (_first_port != ILLEGAL_PORT && _second_port != ILLEGAL_PORT) ? true : false;
        auto its_eventgroup = host_->find_eventgroup(_service, _instance, _eventgroup);
        if (its_eventgroup) {
            its_eventgroup->get_reliability(has_reliable_events, has_unreliable_events);
        }

        bool reliablility_nack(false);
        if (has_reliable_events && !has_unreliable_events) {
            if (!(_first_port != ILLEGAL_PORT && _is_first_reliable) &&
                    !(_second_port != ILLEGAL_PORT && _is_second_reliable)) {
                reliablility_nack = true;
            }
        } else if (!has_reliable_events && has_unreliable_events) {
            if (!(_first_port != ILLEGAL_PORT && !_is_first_reliable) &&
                    !(_second_port != ILLEGAL_PORT && !_is_second_reliable)) {
                reliablility_nack = true;
            }
        } else if (has_reliable_events && has_unreliable_events) {
            if (_first_port == ILLEGAL_PORT || _second_port == ILLEGAL_PORT) {
                reliablility_nack = true;
            }
            if (_is_first_reliable == _is_second_reliable) {
                reliablility_nack = true;
            }
        }
        if (reliablility_nack && _ttl > 0) {
            if (has_two_options_) {
                its_message->decrease_number_required_acks();
            }
            insert_subscription_nack(its_message, _service, _instance,
                _eventgroup, _counter, _major, _reserved);
            boost::system::error_code ec;
            VSOMEIP_WARNING << "Subscription for ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
                    << " not valid: Event configuration does not match the provided "
                    << "endpoint options: "
                    << _first_address.to_string(ec) << ":" << std::dec << _first_port << " "
                    << _second_address.to_string(ec) << ":" << std::dec << _second_port
                    << " " << _message_id->sender_.to_string(ec) << " session: "
                    << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
            return;
        }

        std::shared_ptr < eventgroupinfo > its_info = host_->find_eventgroup(
                _service, _instance, _eventgroup);

        struct subscriber_target_t {
            std::shared_ptr<endpoint_definition> subscriber_;
            std::shared_ptr<endpoint_definition> target_;
        };
        std::array<subscriber_target_t, 2> its_targets =
            { subscriber_target_t(), subscriber_target_t() };

        // Could not find eventgroup or wrong version
        if (!its_info || _major != its_info->get_major()) {
            // Create a temporary info object with TTL=0 --> send NACK
            if( its_info && (_major != its_info->get_major())) {
                boost::system::error_code ec;
                VSOMEIP_ERROR << "Requested major version:[" << (uint32_t) _major
                        << "] in subscription to service: ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
                        << " does not match with services major version:[" << (uint32_t) its_info->get_major()
                        << "] " << _message_id->sender_.to_string(ec) << " session: "
                        << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
            } else {
                VSOMEIP_ERROR << "Requested eventgroup:[" << _eventgroup
                        << "] not found for subscription to service:["
                        << _service << "] instance:[" << _instance << "]";
            }
            its_info = std::make_shared < eventgroupinfo > (_major, 0);
            if(_ttl > 0) {
                if (has_two_options_) {
                    its_message->decrease_number_required_acks();
                }
                insert_subscription_nack(its_message, _service, _instance,
                        _eventgroup, _counter, _major, _reserved);
            }
            return;
        } else {
            boost::asio::ip::address its_first_address, its_second_address;
            uint16_t its_first_port, its_second_port;
            if (ILLEGAL_PORT != _first_port) {
                its_targets[0].subscriber_ = endpoint_definition::get(
                        _first_address, _first_port, _is_first_reliable, _service, _instance);
                if (!_is_first_reliable &&
                    its_info->get_multicast(its_first_address, its_first_port)) { // udp multicast
                    its_targets[0].target_ = endpoint_definition::get(
                        its_first_address, its_first_port, false, _service, _instance);
                } else if(_is_first_reliable) { // tcp unicast
                    its_targets[0].target_ = its_targets[0].subscriber_;
                    // check if TCP connection is established by client
                    if(_ttl > 0 && !is_tcp_connected(_service, _instance, its_targets[0].target_)) {
                        insert_subscription_nack(its_message, _service, _instance,
                            _eventgroup, _counter, _major, _reserved);
                        boost::system::error_code ec;
                        VSOMEIP_ERROR << "TCP connection to target1: ["
                                << its_targets[0].target_->get_address().to_string()
                                << ":" << its_targets[0].target_->get_port()
                                << "] not established for subscription to: ["
                                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "] "
                                << _message_id->sender_.to_string(ec) << " session: "
                                << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                        if (has_two_options_) {
                            its_message->decrease_number_required_acks();
                        }
                        return;
                    }
                } else { // udp unicast
                    its_targets[0].target_ = its_targets[0].subscriber_;
                }
            }
            if (ILLEGAL_PORT != _second_port) {
                its_targets[1].subscriber_ = endpoint_definition::get(
                        _second_address, _second_port, _is_second_reliable, _service, _instance);
                if (!_is_second_reliable &&
                    its_info->get_multicast(its_second_address, its_second_port)) { // udp multicast
                    its_targets[1].target_ = endpoint_definition::get(
                        its_second_address, its_second_port, false, _service, _instance);
                } else if (_is_second_reliable) { // tcp unicast
                    its_targets[1].target_ = its_targets[1].subscriber_;
                    // check if TCP connection is established by client
                    if(_ttl > 0 && !is_tcp_connected(_service, _instance, its_targets[1].target_)) {
                        insert_subscription_nack(its_message, _service, _instance,
                            _eventgroup, _counter, _major, _reserved);
                        boost::system::error_code ec;
                        VSOMEIP_ERROR << "TCP connection to target2 : ["
                                << its_targets[1].target_->get_address().to_string()
                                << ":" << its_targets[1].target_->get_port()
                                << "] not established for subscription to: ["
                                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "] "
                                << _message_id->sender_.to_string(ec) << " session: "
                                << std::hex << std::setw(4) << std::setfill('0') << _message_id->session_;
                        if (has_two_options_) {
                            its_message->decrease_number_required_acks();
                        }
                        return;
                    }
                } else { // udp unicast
                    its_targets[1].target_ = its_targets[1].subscriber_;
                }
            }
        }

        if (_ttl == 0) { // --> unsubscribe
            for (const auto &target : its_targets) {
                if (target.subscriber_) {
                    if (!_is_stop_subscribe_subscribe) {
                        host_->on_unsubscribe(_service, _instance, _eventgroup, target.subscriber_);
                    }
                }
            }
            return;
        }

        for (const auto &target : its_targets) {
            if (!target.target_) {
                continue;
            }
            host_->on_remote_subscription(_service, _instance,
                    _eventgroup, target.subscriber_, target.target_,
                    _ttl * get_ttl_factor(_service, _instance, ttl_factor_subscriptions_),
                    _message_id,
                    [&](remote_subscription_state_e _rss, client_t _subscribing_remote_client) {
                switch (_rss) {
                    case remote_subscription_state_e::SUBSCRIPTION_ACKED:
                        insert_subscription_ack(its_message, _service,
                                _instance, _eventgroup, its_info, _ttl,
                                _counter, _major, _reserved);
                        if (_force_initial_events) {
                            // processing subscription of StopSubscribe/Subscribe
                            // sequence
                            its_message->forced_initial_events_add(
                                    message_impl::forced_initial_events_t(
                                        { target.subscriber_, _service, _instance,
                                                _eventgroup }));
                        }
                        break;
                    case remote_subscription_state_e::SUBSCRIPTION_NACKED:
                    case remote_subscription_state_e::SUBSCRIPTION_ERROR:
                        insert_subscription_nack(its_message, _service,
                                _instance, _eventgroup, _counter, _major,
                                _reserved);
                        break;
                    case remote_subscription_state_e::SUBSCRIPTION_PENDING:
                        if (target.target_ && target.subscriber_) {
                            std::shared_ptr<subscriber_t> subscriber_ =
                                    std::make_shared<subscriber_t>();
                            subscriber_->subscriber = target.subscriber_;
                            subscriber_->target = target.target_;
                            subscriber_->response_message_id_ = _message_id;
                            subscriber_->eventgroupinfo_ = its_info;
                            subscriber_->ttl_ = _ttl;
                            subscriber_->major_ = _major;
                            subscriber_->reserved_ = _reserved;
                            subscriber_->counter_ = _counter;

                            std::lock_guard<std::mutex> its_lock(pending_remote_subscriptions_mutex_);
                            pending_remote_subscriptions_[_service]
                                                         [_instance]
                                                         [_eventgroup]
                                                         [_subscribing_remote_client].push_back(subscriber_);
                        }
                    default:
                        break;
                }
            });
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
                        host_->on_subscribe_nack(client.first, _service,
                                _instance, _eventgroup, ANY_EVENT, DEFAULT_SUBSCRIPTION);
                        break;
                    }
                }

                // Restart TCP connection only for non selective subscriptions
                for (auto client : found_eventgroup->second) {
                    if( !client.second->is_acknowledged()
                            && client.first == VSOMEIP_ROUTING_CLIENT ) {
                        auto endpoint = client.second->get_endpoint(true);
                        if(endpoint) {
                            endpoint->restart();
                        }
                    }
                }

                // Remove nacked subscription only for selective events
                if(nackedClient != VSOMEIP_ROUTING_CLIENT) {
                    found_eventgroup->second.erase(nackedClient);
                }
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
                        host_->on_subscribe_ack(its_client.first, _service,
                                _instance, _eventgroup, ANY_EVENT, DEFAULT_SUBSCRIPTION);
                    }
                    if (_address.is_multicast()) {
                        host_->on_subscribe_ack(_service, _instance, _address,
                                _port);
                    }
                }
            }
        }
    }
}

bool service_discovery_impl::is_tcp_connected(service_t _service,
         instance_t _instance,
         std::shared_ptr<vsomeip::endpoint_definition> its_endpoint) {
    bool is_connected = false;
    std::shared_ptr<serviceinfo> its_info = host_->get_offered_service(_service,
            _instance);
    if (its_info) {
        //get reliable server endpoint
        auto its_reliable_server_endpoint = std::dynamic_pointer_cast<
                tcp_server_endpoint_impl>(its_info->get_endpoint(true));
        if (its_reliable_server_endpoint
                && its_reliable_server_endpoint->is_established(its_endpoint)) {
            is_connected = true;
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
    if (host_->send_to(endpoint_definition::get(_address, port_, reliable_, _message->get_service(), _message->get_instance()),
            serializer_->get_data(), serializer_->get_size(), port_)) {
        increment_session(_address);
    }
    serializer_->reset();
}

void service_discovery_impl::start_ttl_timer() {
    std::lock_guard<std::mutex> its_lock(ttl_timer_mutex_);
    boost::system::error_code ec;
    ttl_timer_.expires_from_now(std::chrono::milliseconds(ttl_timer_runtime_), ec);
    ttl_timer_.async_wait(
            std::bind(&service_discovery_impl::check_ttl, shared_from_this(),
                      std::placeholders::_1));
}

void service_discovery_impl::stop_ttl_timer() {
    std::lock_guard<std::mutex> its_lock(ttl_timer_mutex_);
    boost::system::error_code ec;
    ttl_timer_.cancel(ec);
}

void service_discovery_impl::check_ttl(const boost::system::error_code &_error) {
    if (!_error) {
        host_->update_routing_info(ttl_timer_runtime_);
        start_ttl_timer();
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
                const remote_offer_type_e its_offer_type =
                        get_remote_offer_type(_service, _instance);
                for (auto found_eventgroup : found_instance->second) {
                    auto found_client = found_eventgroup.second.find(_client);
                    if (found_client != found_eventgroup.second.end()) {
                        std::shared_ptr<endpoint> its_unreliable;
                        std::shared_ptr<endpoint> its_reliable;
                        bool has_address(false);
                        boost::asio::ip::address its_address;
                        get_subscription_endpoints(
                                its_unreliable, its_reliable, &its_address,
                                &has_address, _service, _instance,
                                found_client->first);
                        std::shared_ptr<endpoint> endpoint;
                        if (_reliable) {
                            endpoint = its_reliable;
                            found_client->second->set_endpoint(its_reliable, true);
                            if (its_unreliable &&
                                    !host_->has_identified(found_client->first, _service, _instance, false)) {
                                return;
                            }
                        } else {
                            endpoint = its_unreliable;
                            found_client->second->set_endpoint(its_unreliable, false);
                            if (its_reliable &&
                                    !host_->has_identified(found_client->first, _service, _instance, true)) {
                                return;
                            }
                        }
                        if (endpoint) {
                            if (!has_address) {
                                VSOMEIP_WARNING << "service_discovery_impl::"
                                        "send_subscriptions couldn't determine "
                                        "address for service.instance: "
                                        << std::hex << std::setw(4) << std::setfill('0')
                                        << _service << "." << _instance;
                                continue;
                            }
                            std::shared_ptr<message_impl> its_message
                                = its_runtime->create_message();

                            if (its_reliable && its_unreliable) {
                                if (its_reliable->is_connected() && its_unreliable->is_connected()) {
                                    insert_subscription(its_message, _service,
                                            _instance, found_eventgroup.first,
                                            found_client->second, its_offer_type);
                                    found_client->second->set_tcp_connection_established(true);
                                    found_client->second->set_udp_connection_established(true);
                                } else {
                                    found_client->second->set_tcp_connection_established(false);
                                    found_client->second->set_udp_connection_established(false);
                                }
                            } else {
                                if(_reliable) {
                                    if(endpoint->is_connected()) {
                                        insert_subscription(its_message, _service,
                                                _instance, found_eventgroup.first,
                                                found_client->second, its_offer_type);
                                        found_client->second->set_tcp_connection_established(true);
                                    } else {
                                        // don't insert reliable endpoint option if the
                                        // TCP client endpoint is not yet connected
                                        found_client->second->set_tcp_connection_established(false);
                                    }
                                } else {
                                    if (endpoint->is_connected()) {
                                        insert_subscription(its_message, _service,
                                                _instance, found_eventgroup.first,
                                                found_client->second, its_offer_type);
                                        found_client->second->set_udp_connection_established(true);
                                    } else {
                                        // don't insert unreliable endpoint option if the
                                        // UDP client endpoint is not yet connected
                                        found_client->second->set_udp_connection_established(false);
                                    }
                                }
                            }
                            if (its_message->get_entries().size()
                                    && its_message->get_options().size()) {
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
    std::lock_guard<std::mutex> its_lock(subscription_expiration_timer_mutex_);
    start_subscription_expiration_timer_unlocked();
}

void service_discovery_impl::start_subscription_expiration_timer_unlocked() {
    subscription_expiration_timer_.expires_at(next_subscription_expiration_);
        subscription_expiration_timer_.async_wait(
                std::bind(&service_discovery_impl::expire_subscriptions,
                          shared_from_this(),
                          std::placeholders::_1));
}

void service_discovery_impl::stop_subscription_expiration_timer() {
    std::lock_guard<std::mutex> its_lock(subscription_expiration_timer_mutex_);
    stop_subscription_expiration_timer_unlocked();
}

void service_discovery_impl::stop_subscription_expiration_timer_unlocked() {
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
            VSOMEIP_INFO << "First 3 triples of subscribers endpoint IP address are valid!";
#endif
        }
    }
    return is_valid;
}

void service_discovery_impl::offer_service(service_t _service,
                                           instance_t _instance,
                                           std::shared_ptr<serviceinfo> _info) {
    std::lock_guard<std::mutex> its_lock(collected_offers_mutex_);
    // check if offer is in map
    bool found(false);
    const auto its_service = collected_offers_.find(_service);
    if (its_service != collected_offers_.end()) {
        const auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            found = true;
        }
    }
    if (!found) {
        collected_offers_[_service][_instance] = _info;
    }
}

void service_discovery_impl::start_offer_debounce_timer(bool _first_start) {
    std::lock_guard<std::mutex> its_lock(offer_debounce_timer_mutex_);
    boost::system::error_code ec;
    if (_first_start) {
        offer_debounce_timer_.expires_from_now(initial_delay_, ec);
    } else {
        offer_debounce_timer_.expires_from_now(offer_debounce_time_, ec);
    }
    if (ec) {
        VSOMEIP_ERROR<< "service_discovery_impl::start_offer_debounce_timer "
        "setting expiry time of timer failed: " << ec.message();
    }
    offer_debounce_timer_.async_wait(
            std::bind(
                    &service_discovery_impl::on_offer_debounce_timer_expired,
                    this, std::placeholders::_1));
}



void service_discovery_impl::start_find_debounce_timer(bool _first_start) {
    std::lock_guard<std::mutex> its_lock(find_debounce_timer_mutex_);
    boost::system::error_code ec;
    if (_first_start) {
        find_debounce_timer_.expires_from_now(initial_delay_, ec);
    } else {
        find_debounce_timer_.expires_from_now(find_debounce_time_, ec);
    }
    if (ec) {
        VSOMEIP_ERROR<< "service_discovery_impl::start_find_debounce_timer "
        "setting expiry time of timer failed: " << ec.message();
    }
    find_debounce_timer_.async_wait(
            std::bind(
                    &service_discovery_impl::on_find_debounce_timer_expired,
                    this, std::placeholders::_1));
}

//initial delay
void service_discovery_impl::on_find_debounce_timer_expired(
        const boost::system::error_code &_error) {
    if(_error) { // timer was canceled
        return;
    }
    // only copy the accumulated requests of the initial wait phase
    // if the sent counter for the request is zero.
    requests_t repetition_phase_finds;
    bool new_finds(false);
    {
        std::lock_guard<std::mutex> its_lock(requested_mutex_);
        for (const auto its_service : requested_) {
            for (const auto its_instance : its_service.second) {
                if( its_instance.second->get_sent_counter() == 0) {
                    repetition_phase_finds[its_service.first][its_instance.first] = its_instance.second;
                }
            }
        }
        if (repetition_phase_finds.size()) {
            new_finds = true;
        }
    }

    if (!new_finds) {
        start_find_debounce_timer(false);
        return;
    }

    // Sent out finds for the first time as initial wait phase ended
    std::shared_ptr<runtime> its_runtime = runtime_.lock();
    if (its_runtime) {
        std::vector<std::shared_ptr<message_impl>> its_messages;
        std::shared_ptr<message_impl> its_message =
                its_runtime->create_message();
        its_messages.push_back(its_message);
        // Serialize and send FindService (increments sent counter in requested_ map)
        fill_message_with_find_entries(its_runtime, its_message, its_messages,
                repetition_phase_finds);
        serialize_and_send_messages(its_messages);
    }

    std::chrono::milliseconds its_delay(repetitions_base_delay_);
    std::uint8_t its_repetitions(1);

    std::shared_ptr<boost::asio::steady_timer> its_timer = std::make_shared<
            boost::asio::steady_timer>(host_->get_io());
    {
        std::lock_guard<std::mutex> its_lock(find_repetition_phase_timers_mutex_);
        find_repetition_phase_timers_[its_timer] = repetition_phase_finds;
    }

    boost::system::error_code ec;
    its_timer->expires_from_now(its_delay, ec);
    if (ec) {
        VSOMEIP_ERROR<< "service_discovery_impl::on_find_debounce_timer_expired "
        "setting expiry time of timer failed: " << ec.message();
    }
    its_timer->async_wait(
            std::bind(
                    &service_discovery_impl::on_find_repetition_phase_timer_expired,
                    this, std::placeholders::_1, its_timer, its_repetitions,
                    its_delay.count()));
    start_find_debounce_timer(false);
}

void service_discovery_impl::on_offer_debounce_timer_expired(
        const boost::system::error_code &_error) {
    if(_error) { // timer was canceled
        return;
    }

    // copy the accumulated offers of the initial wait phase
    services_t repetition_phase_offers;
    bool new_offers(false);
    {
        std::vector<services_t::iterator> non_someip_services;
        std::lock_guard<std::mutex> its_lock(collected_offers_mutex_);
        if (collected_offers_.size()) {
            if (is_diagnosis_) {
                for (services_t::iterator its_service = collected_offers_.begin();
                        its_service != collected_offers_.end(); its_service++) {
                    for (auto its_instance : its_service->second) {
                        if (!host_->get_configuration()->is_someip(
                                its_service->first, its_instance.first)) {
                            non_someip_services.push_back(its_service);
                        }
                    }
                }
                for (auto its_service : non_someip_services) {
                    repetition_phase_offers.insert(*its_service);
                    collected_offers_.erase(its_service);
                }
            } else {
                repetition_phase_offers = collected_offers_;
                collected_offers_.clear();
            }

            new_offers = true;
        }
    }

    if (!new_offers) {
        start_offer_debounce_timer(false);
        return;
    }

    // Sent out offers for the first time as initial wait phase ended
    std::shared_ptr<runtime> its_runtime = runtime_.lock();
    if (its_runtime) {
        std::vector<std::shared_ptr<message_impl>> its_messages;
        std::shared_ptr<message_impl> its_message =
                its_runtime->create_message();
        its_messages.push_back(its_message);
        fill_message_with_offer_entries(its_runtime, its_message, its_messages,
                repetition_phase_offers, true);

        // Serialize and send
        serialize_and_send_messages(its_messages);
    }

    std::chrono::milliseconds its_delay(0);
    std::uint8_t its_repetitions(0);
    if (repetitions_max_) {
        // start timer for repetition phase the first time
        // with 2^0 * repetitions_base_delay
        its_delay = repetitions_base_delay_;
        its_repetitions = 1;
    } else {
        // if repetitions_max is set to zero repetition phase is skipped,
        // therefore wait one cyclic offer delay before entering main phase
        its_delay = cyclic_offer_delay_;
        its_repetitions = 0;
    }

    std::shared_ptr<boost::asio::steady_timer> its_timer = std::make_shared<
            boost::asio::steady_timer>(host_->get_io());

    {
        std::lock_guard<std::mutex> its_lock(repetition_phase_timers_mutex_);
        repetition_phase_timers_[its_timer] = repetition_phase_offers;
    }

    boost::system::error_code ec;
    its_timer->expires_from_now(its_delay, ec);
    if (ec) {
        VSOMEIP_ERROR<< "service_discovery_impl::on_offer_debounce_timer_expired "
        "setting expiry time of timer failed: " << ec.message();
    }
    its_timer->async_wait(
            std::bind(
                    &service_discovery_impl::on_repetition_phase_timer_expired,
                    this, std::placeholders::_1, its_timer, its_repetitions,
                    its_delay.count()));
    start_offer_debounce_timer(false);
}

void service_discovery_impl::on_repetition_phase_timer_expired(
        const boost::system::error_code &_error,
        std::shared_ptr<boost::asio::steady_timer> _timer,
        std::uint8_t _repetition, std::uint32_t _last_delay) {
    if (_error) {
        return;
    }
    if (_repetition == 0) {
        std::lock_guard<std::mutex> its_lock(repetition_phase_timers_mutex_);
        // we waited one cyclic offer delay, the offers can now be sent in the
        // main phase and the timer can be deleted
        move_offers_into_main_phase(_timer);
    } else {
        std::lock_guard<std::mutex> its_lock(repetition_phase_timers_mutex_);
        auto its_timer_pair = repetition_phase_timers_.find(_timer);
        if (its_timer_pair != repetition_phase_timers_.end()) {
            std::chrono::milliseconds new_delay(0);
            std::uint8_t repetition(0);
            bool move_to_main(false);
            if (_repetition <= repetitions_max_) {
                // sent offers, double time to wait and start timer again.

                new_delay = std::chrono::milliseconds(_last_delay * 2);
                repetition = ++_repetition;
            } else {
                // repetition phase is now over we have to sleep one cyclic
                // offer delay before it's allowed to sent the offer again.
                // If the last offer was sent shorter than half the
                // configured cyclic_offer_delay_ago the offers are directly
                // moved into the mainphase to avoid potentially sleeping twice
                // the cyclic offer delay before moving the offers in to main
                // phase
                if (last_offer_shorter_half_offer_delay_ago()) {
                    move_to_main = true;
                } else {
                    new_delay = cyclic_offer_delay_;
                    repetition = 0;
                }
            }
            std::shared_ptr<runtime> its_runtime = runtime_.lock();
            if (its_runtime) {
                std::vector<std::shared_ptr<message_impl>> its_messages;
                std::shared_ptr<message_impl> its_message =
                        its_runtime->create_message();
                its_messages.push_back(its_message);
                fill_message_with_offer_entries(its_runtime, its_message,
                        its_messages, its_timer_pair->second, true);

                // Serialize and send
                serialize_and_send_messages(its_messages);
            }
            if (move_to_main) {
                move_offers_into_main_phase(_timer);
                return;
            }
            boost::system::error_code ec;
            its_timer_pair->first->expires_from_now(new_delay, ec);
            if (ec) {
                VSOMEIP_ERROR <<
                "service_discovery_impl::on_repetition_phase_timer_expired "
                "setting expiry time of timer failed: " << ec.message();
            }
            its_timer_pair->first->async_wait(
                    std::bind(
                            &service_discovery_impl::on_repetition_phase_timer_expired,
                            this, std::placeholders::_1, its_timer_pair->first,
                            repetition, new_delay.count()));
        }
    }
}


void service_discovery_impl::on_find_repetition_phase_timer_expired(
        const boost::system::error_code &_error,
        std::shared_ptr<boost::asio::steady_timer> _timer,
        std::uint8_t _repetition, std::uint32_t _last_delay) {
    if (_error) {
        return;
    }

    std::lock_guard<std::mutex> its_lock(find_repetition_phase_timers_mutex_);
    auto its_timer_pair = find_repetition_phase_timers_.find(_timer);
    if (its_timer_pair != find_repetition_phase_timers_.end()) {
        std::chrono::milliseconds new_delay(0);
        std::uint8_t repetition(0);
        if (_repetition <= repetitions_max_) {
            // sent findService entries in one message, double time to wait and start timer again.
            std::shared_ptr<runtime> its_runtime = runtime_.lock();
            if (its_runtime) {
                std::vector<std::shared_ptr<message_impl>> its_messages;
                std::shared_ptr<message_impl> its_message =
                        its_runtime->create_message();
                its_messages.push_back(its_message);
                fill_message_with_find_entries(its_runtime, its_message,
                        its_messages, its_timer_pair->second);
                serialize_and_send_messages(its_messages);
            }
            new_delay = std::chrono::milliseconds(_last_delay * 2);
            repetition = ++_repetition;
        } else {
            // repetition phase is now over, erase the timer on next expiry time
            find_repetition_phase_timers_.erase(its_timer_pair);
            return;
        }
        boost::system::error_code ec;
        its_timer_pair->first->expires_from_now(new_delay, ec);
        if (ec) {
            VSOMEIP_ERROR <<
            "service_discovery_impl::on_find_repetition_phase_timer_expired "
            "setting expiry time of timer failed: " << ec.message();
        }
        its_timer_pair->first->async_wait(
                std::bind(
                        &service_discovery_impl::on_find_repetition_phase_timer_expired,
                        this, std::placeholders::_1, its_timer_pair->first,
                        repetition, new_delay.count()));
    }
}


void service_discovery_impl::move_offers_into_main_phase(
        const std::shared_ptr<boost::asio::steady_timer> &_timer) {
    // HINT: make sure to lock the repetition_phase_timers_mutex_ before calling
    // this function
    // set flag on all serviceinfos bound to this timer
    // that they will be included in the cyclic offers from now on
    const auto its_timer = repetition_phase_timers_.find(_timer);
    if (its_timer != repetition_phase_timers_.end()) {
        for (const auto its_service : its_timer->second) {
            for (const auto instance : its_service.second) {
                instance.second->set_is_in_mainphase(true);
            }
        }
        repetition_phase_timers_.erase(_timer);
    }
}

void service_discovery_impl::fill_message_with_offer_entries(
        std::shared_ptr<runtime> _runtime,
        std::shared_ptr<message_impl> _message,
        std::vector<std::shared_ptr<message_impl>> &_messages,
        const services_t &_offers, bool _ignore_phase) {
    uint32_t its_remaining(max_message_size_);
    uint32_t its_start(0);
    bool is_done(false);
    while (!is_done) {
        insert_offer_entries(_message, _offers, its_start, its_remaining,
                is_done, _ignore_phase);
        if (!is_done) {
            its_remaining = max_message_size_;
            _message = _runtime->create_message();
            _messages.push_back(_message);
        }
    }
}

void service_discovery_impl::fill_message_with_find_entries(
        std::shared_ptr<runtime> _runtime,
        std::shared_ptr<message_impl> _message,
        std::vector<std::shared_ptr<message_impl>> &_messages,
        const requests_t &_requests) {
    uint32_t its_start(0);
    uint32_t its_size(0);
    bool is_done(false);
    while (!is_done) {
        insert_find_entries(_message, _requests, its_start, its_size,
                is_done);
        its_start += its_size / VSOMEIP_SOMEIP_SD_ENTRY_SIZE;
        if (!is_done) {
            its_start = 0;
            _message = _runtime->create_message();
            _messages.push_back(_message);
        }
    };
}


bool service_discovery_impl::serialize_and_send_messages(
        const std::vector<std::shared_ptr<message_impl>> &_messages) {
    bool has_sent(false);
    // lock serialize mutex here as well even if we don't use the
    // serializer as it's used to guard access to the sessions_sent_ map
    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    for (const auto m : _messages) {
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
    return has_sent;
}

void service_discovery_impl::stop_offer_service(
        service_t _service, instance_t _instance,
        std::shared_ptr<serviceinfo> _info) {
    bool stop_offer_required(false);
    // delete from initial phase offers
    {
        std::lock_guard<std::mutex> its_lock(collected_offers_mutex_);
        if (collected_offers_.size()) {
            auto its_service = collected_offers_.find(_service);
            if (its_service != collected_offers_.end()) {
                auto its_instance = its_service->second.find(_instance);
                if (its_instance != its_service->second.end()) {
                    if (its_instance->second == _info) {
                        its_service->second.erase(its_instance);

                        if (!collected_offers_[its_service->first].size()) {
                            collected_offers_.erase(its_service);
                        }
                    }
                }
            }
        }
        // no need to sent out a stop offer message here as all services
        // instances contained in the collected offers weren't broadcasted yet
    }

    // delete from repetition phase offers
    {
        std::lock_guard<std::mutex> its_lock(repetition_phase_timers_mutex_);
        for (auto rpt = repetition_phase_timers_.begin();
                rpt != repetition_phase_timers_.end();) {
            auto its_service = rpt->second.find(_service);
            if (its_service != rpt->second.end()) {
                auto its_instance = its_service->second.find(_instance);
                if (its_instance != its_service->second.end()) {
                    if (its_instance->second == _info) {
                        its_service->second.erase(its_instance);
                        stop_offer_required = true;
                        if (!rpt->second[its_service->first].size()) {
                            rpt->second.erase(_service);
                        }
                    }
                }
            }
            if (!rpt->second.size()) {
                rpt = repetition_phase_timers_.erase(rpt);
            } else {
                ++rpt;
            }
        }
    }
    // sent stop offer
    if(_info->is_in_mainphase() || stop_offer_required) {
        send_stop_offer(_service, _instance, _info);
    }
    // sent out NACKs for all pending subscriptions
    remote_subscription_not_acknowledge_all(_service, _instance);
}

bool service_discovery_impl::send_stop_offer(
        service_t _service, instance_t _instance,
        std::shared_ptr<serviceinfo> _info) {
    std::shared_ptr < runtime > its_runtime = runtime_.lock();
    if (its_runtime) {
        if (_info->get_endpoint(false) || _info->get_endpoint(true)) {
            std::vector<std::shared_ptr<message_impl>> its_messages;
            std::shared_ptr<message_impl> its_message;
            its_message = its_runtime->create_message();
            its_messages.push_back(its_message);

            uint32_t its_size(max_message_size_);
            insert_offer_service(its_message, _service, _instance, _info, its_size);

            // Serialize and send
            return serialize_and_send_messages(its_messages);
        }
    }
    return false;
}

void service_discovery_impl::start_main_phase_timer() {
    std::lock_guard<std::mutex> its_lock(main_phase_timer_mutex_);
    boost::system::error_code ec;
    main_phase_timer_.expires_from_now(cyclic_offer_delay_);
    if (ec) {
        VSOMEIP_ERROR<< "service_discovery_impl::start_main_phase_timer "
        "setting expiry time of timer failed: " << ec.message();
    }
    main_phase_timer_.async_wait(
            std::bind(&service_discovery_impl::on_main_phase_timer_expired,
                    this, std::placeholders::_1));
}

void service_discovery_impl::on_main_phase_timer_expired(
        const boost::system::error_code &_error) {
    if (_error) {
        return;
    }
    send(true);
    start_main_phase_timer();
}

void service_discovery_impl::send_uni_or_multicast_offerservice(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, const std::shared_ptr<const serviceinfo> &_info,
        bool _unicast_flag) {
    if (_unicast_flag) { // SID_SD_826
        if (last_offer_shorter_half_offer_delay_ago()) { // SIP_SD_89
            send_unicast_offer_service(_info, _service, _instance, _major, _minor);
        } else { // SIP_SD_90
            send_multicast_offer_service(_info, _service, _instance, _major, _minor);
        }
    } else { // SID_SD_826
        send_unicast_offer_service(_info, _service, _instance, _major, _minor);
    }
}

bool service_discovery_impl::last_offer_shorter_half_offer_delay_ago() {
    //get remaining time to next offer since last offer
    std::chrono::milliseconds remaining(0);
    {
        std::lock_guard<std::mutex> its_lock(main_phase_timer_mutex_);
        remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                main_phase_timer_.expires_from_now());
    }
    if (std::chrono::milliseconds(0) > remaining) {
        remaining = cyclic_offer_delay_;
    }
    const std::chrono::milliseconds half_cyclic_offer_delay =
            cyclic_offer_delay_ / 2;

    return remaining > half_cyclic_offer_delay;
}

bool service_discovery_impl::check_source_address(
        const boost::asio::ip::address &its_source_address) const {
   bool is_valid = true;
   std::shared_ptr<configuration> its_configuration =
           host_->get_configuration();

   if(its_configuration) {
       boost::asio::ip::address its_unicast_address =
               its_configuration.get()->get_unicast_address();
       // check if source address is same as nodes unicast address
       if(its_unicast_address
               == its_source_address) {
           VSOMEIP_ERROR << "Source address of message is same as DUT's unicast address! : "
                   << its_source_address.to_string();
           is_valid = false;
       }
   }
   return is_valid;
}

void service_discovery_impl::set_diagnosis_mode(const bool _activate) {
    is_diagnosis_ = _activate;
}

bool service_discovery_impl::get_diagnosis_mode() {
    return is_diagnosis_;
}

void service_discovery_impl::remote_subscription_acknowledge(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        client_t _client, bool _acknowledged,
        const std::shared_ptr<sd_message_identifier_t> &_sd_message_id) {
    std::shared_ptr<subscriber_t> its_subscriber;
    {
        std::lock_guard<std::mutex> its_lock(pending_remote_subscriptions_mutex_);
        const auto its_service = pending_remote_subscriptions_.find(_service);
        if (its_service != pending_remote_subscriptions_.end()) {
            const auto its_instance = its_service->second.find(_instance);
            if (its_instance != its_service->second.end()) {
                const auto its_eventgroup = its_instance->second.find(_eventgroup);
                if (its_eventgroup != its_instance->second.end()) {
                    const auto its_client = its_eventgroup->second.find(_client);
                    if (its_client != its_eventgroup->second.end()) {
                        for (auto iter = its_client->second.begin();
                                iter != its_client->second.end();) {
                            if ((*iter)->response_message_id_ == _sd_message_id) {
                                its_subscriber = *iter;
                                iter = its_client->second.erase(iter);
                                break;
                            } else {
                                iter++;
                            }
                        }

                        // delete if necessary
                        if (!its_client->second.size()) {
                            its_eventgroup->second.erase(its_client);
                            if (!its_eventgroup->second.size()) {
                                its_instance->second.erase(its_eventgroup);
                                if (!its_instance->second.size()) {
                                    its_service->second.erase(its_instance);
                                    if (!its_service->second.size()) {
                                        pending_remote_subscriptions_.erase(
                                                its_service);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (its_subscriber) {
        remote_subscription_acknowledge_subscriber(_service, _instance,
                _eventgroup, its_subscriber, _acknowledged);
    }
}

void service_discovery_impl::update_subscription_expiration_timer(
        const std::shared_ptr<message_impl> &_message) {
    std::lock_guard<std::mutex> its_lock(subscription_expiration_timer_mutex_);
    const std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now();
    stop_subscription_expiration_timer_unlocked();
    for (const auto &entry : _message->get_entries()) {
        if (entry->get_type() == entry_type_e::SUBSCRIBE_EVENTGROUP_ACK
                && entry->get_ttl()) {
            const std::chrono::steady_clock::time_point its_expiration = now
                    + std::chrono::seconds(
                            entry->get_ttl()
                                    * get_ttl_factor(
                                            entry->get_service(),
                                            entry->get_instance(),
                                            ttl_factor_subscriptions_));
            if (its_expiration < next_subscription_expiration_) {
                next_subscription_expiration_ = its_expiration;
            }
        }
    }
    start_subscription_expiration_timer_unlocked();
}

void service_discovery_impl::remote_subscription_acknowledge_subscriber(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        const std::shared_ptr<subscriber_t> &_subscriber, bool _acknowledged) {
    std::shared_ptr<message_impl> its_response = _subscriber->response_message_id_->response_;
    bool sent(false);
    {
        std::lock_guard<std::mutex> its_lock(response_mutex_);
        if (_acknowledged) {
            insert_subscription_ack(its_response, _service, _instance,
                    _eventgroup, _subscriber->eventgroupinfo_,
                    _subscriber->ttl_, _subscriber->counter_,
                    _subscriber->major_, _subscriber->reserved_, _subscriber->subscriber);
        } else {
            insert_subscription_nack(its_response, _service, _instance,
                    _eventgroup, _subscriber->counter_,
                    _subscriber->major_, _subscriber->reserved_);
        }

        if (its_response->all_required_acks_contained()) {
            update_subscription_expiration_timer(its_response);
            serialize_and_send(its_response, _subscriber->response_message_id_->sender_);
            // set required acks to 0xFF to mark message as sent
            its_response->set_number_required_acks((std::numeric_limits<uint8_t>::max)());
            sent = true;
        } else {
            its_response->set_initial_events_required(true);
        }
    }
    if (sent) {
        for (const auto& ack_tuple : get_eventgroups_requiring_initial_events(
                its_response)) {
            host_->send_initial_events(std::get<0>(ack_tuple),
                    std::get<1>(ack_tuple), std::get<2>(ack_tuple),
                    std::get<3>(ack_tuple));
        }
        for (const auto &fie : its_response->forced_initial_events_get()) {
            host_->send_initial_events(fie.service_, fie.instance_,
                    fie.eventgroup_, fie.target_);
        }
    }
}

void service_discovery_impl::remote_subscription_not_acknowledge_all(
        service_t _service, instance_t _instance) {
    std::map<eventgroup_t, std::vector<std::shared_ptr<subscriber_t>>>its_pending_subscriptions;
    {
        std::lock_guard<std::mutex> its_lock(pending_remote_subscriptions_mutex_);
        const auto its_service = pending_remote_subscriptions_.find(_service);
        if (its_service != pending_remote_subscriptions_.end()) {
            const auto its_instance = its_service->second.find(_instance);
            if (its_instance != its_service->second.end()) {
                for (const auto &its_eventgroup : its_instance->second) {
                    for (const auto &its_client : its_eventgroup.second) {
                        its_pending_subscriptions[its_eventgroup.first].insert(
                                its_pending_subscriptions[its_eventgroup.first].end(),
                                its_client.second.begin(),
                                its_client.second.end());
                    }
                }
                // delete everything from this service instance
                its_service->second.erase(its_instance);
                if (!its_service->second.size()) {
                    pending_remote_subscriptions_.erase(its_service);
                }
            }
        }
    }
    for (const auto &eg : its_pending_subscriptions) {
        for (const auto &its_subscriber : eg.second) {
            remote_subscription_acknowledge_subscriber(_service, _instance,
                    eg.first, its_subscriber, false);
        }
    }
}

void service_discovery_impl::remote_subscription_not_acknowledge_all() {
    std::map<service_t,
        std::map<instance_t,
            std::map<eventgroup_t, std::vector<std::shared_ptr<subscriber_t>>>>> to_be_nacked;
    {
        std::lock_guard<std::mutex> its_lock(pending_remote_subscriptions_mutex_);
        for (const auto &its_service : pending_remote_subscriptions_) {
            for (const auto &its_instance : its_service.second) {
                for (const auto &its_eventgroup : its_instance.second) {
                    for (const auto &its_client : its_eventgroup.second) {
                        to_be_nacked[its_service.first]
                                    [its_instance.first]
                                    [its_eventgroup.first].insert(
                                                    to_be_nacked[its_service.first][its_instance.first][its_eventgroup.first].end(),
                                                    its_client.second.begin(),
                                                    its_client.second.end());
                    }
                }
            }
        }
        pending_remote_subscriptions_.clear();
    }
    for (const auto &s : to_be_nacked) {
        for (const auto &i : s.second) {
            for (const auto &eg : i.second) {
                for (const auto &sub : eg.second) {
                    remote_subscription_acknowledge_subscriber(s.first, i.first,
                            eg.first, sub, false);
                }
            }
        }
    }
}

bool service_discovery_impl::check_stop_subscribe_subscribe(
        message_impl::entries_t::const_iterator _iter,
        message_impl::entries_t::const_iterator _end,
        const message_impl::options_t& _options) const {
    const message_impl::entries_t::const_iterator its_next = std::next(_iter);
    if ((*_iter)->get_ttl() > 0
            || (*_iter)->get_type() != entry_type_e::STOP_SUBSCRIBE_EVENTGROUP
            || its_next == _end
            || (*its_next)->get_type() != entry_type_e::SUBSCRIBE_EVENTGROUP) {
        return false;
    }

    return (*static_cast<eventgroupentry_impl*>(_iter->get())).is_matching_subscribe(
            *(static_cast<eventgroupentry_impl*>(its_next->get())), _options);
}

configuration::ttl_factor_t service_discovery_impl::get_ttl_factor(
        service_t _service, instance_t _instance,
        const configuration::ttl_map_t& _ttl_map) const {
    configuration::ttl_factor_t its_ttl_factor(1);
    auto found_service = _ttl_map.find(_service);
    if (found_service != _ttl_map.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            its_ttl_factor = found_instance->second;
        }
    }
    return its_ttl_factor;
}

void service_discovery_impl::on_last_msg_received_timer_expired(
        const boost::system::error_code &_error) {
    if (!_error) {
        // we didn't receive a multicast message within 110% of the cyclic_offer_delay_
        VSOMEIP_WARNING << "Didn't receive a multicast SD message for " <<
                std::dec << last_msg_received_timer_timeout_.count() << "ms.";
        // rejoin multicast group
        if (endpoint_) {
            endpoint_->join(sd_multicast_);
        }
        {
            boost::system::error_code ec;
            std::lock_guard<std::mutex> its_lock(last_msg_received_timer_mutex_);
            last_msg_received_timer_.expires_from_now(last_msg_received_timer_timeout_, ec);
            last_msg_received_timer_.async_wait(
                    std::bind(
                            &service_discovery_impl::on_last_msg_received_timer_expired,
                            shared_from_this(), std::placeholders::_1));
        }
    }
}

void service_discovery_impl::stop_last_msg_received_timer() {
    std::lock_guard<std::mutex> its_lock(last_msg_received_timer_mutex_);
    boost::system::error_code ec;
    last_msg_received_timer_.cancel(ec);
}

service_discovery_impl::remote_offer_type_e service_discovery_impl::get_remote_offer_type(
        service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(remote_offer_types_mutex_);
    auto found_si = remote_offer_types_.find(std::make_pair(_service, _instance));
    if (found_si != remote_offer_types_.end()) {
        return found_si->second;
    }
    return remote_offer_type_e::UNKNOWN;
}

bool service_discovery_impl::update_remote_offer_type(
        service_t _service, instance_t _instance,
        remote_offer_type_e _offer_type,
        const boost::asio::ip::address &_reliable_address,
        const boost::asio::ip::address &_unreliable_address) {
    bool ret(false);
    std::lock_guard<std::mutex> its_lock(remote_offer_types_mutex_);
    const std::pair<service_t, instance_t> its_si_pair = std::make_pair(_service, _instance);
    auto found_si = remote_offer_types_.find(its_si_pair);
    if (found_si != remote_offer_types_.end()) {
        if (found_si->second != _offer_type ) {
            found_si->second = _offer_type;
            ret = true;
        }
    } else {
        remote_offer_types_[its_si_pair] = _offer_type;
    }
    switch (_offer_type) {
        case remote_offer_type_e::UNRELIABLE:
            remote_offers_by_ip_[_unreliable_address].insert(its_si_pair);
            break;
        case remote_offer_type_e::RELIABLE:
            remote_offers_by_ip_[_reliable_address].insert(its_si_pair);
            break;
        case remote_offer_type_e::RELIABLE_UNRELIABLE:
            remote_offers_by_ip_[_unreliable_address].insert(its_si_pair);
            break;
        case remote_offer_type_e::UNKNOWN:
        default:
            VSOMEIP_WARNING << __func__ << ": unkown offer type ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "]"
                    << _offer_type;
            break;
    }
    return ret;
}

void service_discovery_impl::remove_remote_offer_type(
        service_t _service, instance_t _instance,
        const boost::asio::ip::address &_address) {
    std::lock_guard<std::mutex> its_lock(remote_offer_types_mutex_);
    const std::pair<service_t, instance_t> its_si_pair =
            std::make_pair(_service, _instance);
    remote_offer_types_.erase(its_si_pair);
    auto found_services = remote_offers_by_ip_.find(_address);
    if (found_services != remote_offers_by_ip_.end()) {
        found_services->second.erase(its_si_pair);
    }
}

void service_discovery_impl::remove_remote_offer_type_by_ip(
        const boost::asio::ip::address &_address) {
    std::lock_guard<std::mutex> its_lock(remote_offer_types_mutex_);
    auto found_services = remote_offers_by_ip_.find(_address);
    if (found_services != remote_offers_by_ip_.end()) {
        for (const auto& si : found_services->second) {
            remote_offer_types_.erase(si);
        }
    }
    remote_offers_by_ip_.erase(_address);
}

std::vector<std::tuple<service_t, instance_t, eventgroup_t,
        std::shared_ptr<endpoint_definition>>>
service_discovery_impl::get_eventgroups_requiring_initial_events(
        const std::shared_ptr<message_impl>& _response) const {
    std::vector<std::tuple<service_t, instance_t, eventgroup_t,
            std::shared_ptr<endpoint_definition>>> its_acks;
    for (const auto &e : _response->get_entries()) {
        if (e->get_type() == entry_type_e::SUBSCRIBE_EVENTGROUP_ACK
                && e->get_ttl() > 0) {
            const std::shared_ptr<eventgroupentry_impl> casted_e =
                    std::static_pointer_cast<eventgroupentry_impl>(e);
            // only entries which require initial events have a target set
            const std::shared_ptr<endpoint_definition> its_reliable =
                    casted_e->get_target(true);
            if (its_reliable) {
                its_acks.push_back(
                        std::make_tuple(e->get_service(), e->get_instance(),
                                casted_e->get_eventgroup(), its_reliable));
            }
            const std::shared_ptr<endpoint_definition> its_unreliable =
                    casted_e->get_target(false);
            if (its_unreliable) {
                its_acks.push_back(
                        std::make_tuple(e->get_service(), e->get_instance(),
                                casted_e->get_eventgroup(),
                                its_unreliable));
            }
        }
    }
    return its_acks;
}

}  // namespace sd
}  // namespace vsomeip

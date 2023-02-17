// Copyright (C) 2014-2018 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <climits>
#include <iomanip>
#include <memory>
#include <sstream>
#include <forward_list>

#ifndef _WIN32
#include <unistd.h>
#include <cstdio>
#include <time.h>
#endif

#include <boost/asio/steady_timer.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/event.hpp"
#include "../include/eventgroupinfo.hpp"
#include "../include/remote_subscription.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_impl.hpp"
#include "../include/routing_manager_stub.hpp"
#include "../include/serviceinfo.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../security/include/security.hpp"

#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../endpoints/include/local_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_client_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../endpoints/include/virtual_server_endpoint_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/serializer.hpp"
#include "../../service_discovery/include/constants.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../service_discovery/include/service_discovery.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../../plugin/include/plugin_manager_impl.hpp"
#ifdef USE_DLT
#include "../../tracing/include/connector_impl.hpp"
#endif

#ifndef ANDROID
#include "../../e2e_protection/include/buffer/buffer.hpp"
#include "../../e2e_protection/include/e2exf/config.hpp"

#include "../../e2e_protection/include/e2e/profile/e2e_provider.hpp"
#endif

#ifdef USE_DLT
#include "../../tracing/include/connector_impl.hpp"
#endif

namespace vsomeip_v3 {

#ifdef ANDROID
namespace sd {
runtime::~runtime() {}
}
#endif

routing_manager_impl::routing_manager_impl(routing_manager_host *_host) :
        routing_manager_base(_host),
        version_log_timer_(_host->get_io()),
        if_state_running_(false),
        sd_route_set_(false),
        routing_running_(false),
        status_log_timer_(_host->get_io()),
        memory_log_timer_(_host->get_io()),
        ep_mgr_impl_(std::make_shared<endpoint_manager_impl>(this, io_, configuration_)),
        pending_remote_offer_id_(0),
        last_resume_(std::chrono::steady_clock::now().min()),
        statistics_log_timer_(_host->get_io()),
        ignored_statistics_counter_(0)
{
}

routing_manager_impl::~routing_manager_impl() {
    utility::remove_lockfile(configuration_);
    utility::reset_client_ids();
}

boost::asio::io_service & routing_manager_impl::get_io() {
    return routing_manager_base::get_io();
}

client_t routing_manager_impl::get_client() const {
    return routing_manager_base::get_client();
}

std::set<client_t> routing_manager_impl::find_local_clients(service_t _service, instance_t _instance) {
    return routing_manager_base::find_local_clients(_service, _instance);
}

client_t routing_manager_impl::find_local_client(service_t _service, instance_t _instance) {
    return routing_manager_base::find_local_client(_service, _instance);
}

bool routing_manager_impl::is_subscribe_to_any_event_allowed(credentials_t _credentials, client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    return routing_manager_base::is_subscribe_to_any_event_allowed(_credentials, _client,
            _service, _instance, _eventgroup);
}

void routing_manager_impl::init() {
    routing_manager_base::init(ep_mgr_impl_);

    // TODO: Only instantiate the stub if needed
    stub_ = std::make_shared<routing_manager_stub>(this, configuration_);
    stub_->init();

    if (configuration_->is_sd_enabled()) {
        VSOMEIP_INFO<< "Service Discovery enabled. Trying to load module.";
        auto its_plugin = plugin_manager::get()->get_plugin(
                plugin_type_e::SD_RUNTIME_PLUGIN, VSOMEIP_SD_LIBRARY);
        if (its_plugin) {
            VSOMEIP_INFO << "Service Discovery module loaded.";
            discovery_ = std::dynamic_pointer_cast<sd::runtime>(its_plugin)->create_service_discovery(this, configuration_);
            discovery_->init();
        } else {
            VSOMEIP_ERROR << "Service Discovery module could not be loaded!";
            std::exit(EXIT_FAILURE);
        }
    }

#ifndef ANDROID
    if( configuration_->is_e2e_enabled()) {
        VSOMEIP_INFO << "E2E protection enabled.";

        const char *its_e2e_module = getenv(VSOMEIP_ENV_E2E_PROTECTION_MODULE);
        std::string plugin_name = its_e2e_module != nullptr ? its_e2e_module : VSOMEIP_E2E_LIBRARY;

        auto its_plugin = plugin_manager::get()->get_plugin(plugin_type_e::APPLICATION_PLUGIN, plugin_name);
        if (its_plugin) {
            VSOMEIP_INFO << "E2E module loaded.";
            e2e_provider_ = std::dynamic_pointer_cast<e2e::e2e_provider>(its_plugin);
        }
    }

    if(e2e_provider_) {
        std::map<e2exf::data_identifier_t, std::shared_ptr<cfg::e2e>> its_e2e_configuration = configuration_->get_e2e_configuration();
        for (auto &identifier : its_e2e_configuration) {
            if(!e2e_provider_->add_configuration(identifier.second)) {
                VSOMEIP_INFO << "Unknown E2E profile: " << identifier.second->profile << ", skipping ...";
            }
        }
    }
#endif
}

void routing_manager_impl::start() {
#ifndef _WIN32
    boost::asio::ip::address its_multicast;
    try {
        its_multicast = boost::asio::ip::address::from_string(configuration_->get_sd_multicast());
    } catch (...) {
        VSOMEIP_ERROR << "Illegal multicast address \""
                << configuration_->get_sd_multicast()
                << "\". Please check your configuration.";
    }

    netlink_connector_ = std::make_shared<netlink_connector>(
            host_->get_io(), configuration_->get_unicast_address(), its_multicast);
    netlink_connector_->register_net_if_changes_handler(
            std::bind(&routing_manager_impl::on_net_interface_or_route_state_changed,
            this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    netlink_connector_->start();
#else
    {
        std::lock_guard<std::mutex> its_lock(pending_sd_offers_mutex_);
        start_ip_routing();
    }
#endif

    stub_->start();
    host_->on_state(state_type_e::ST_REGISTERED);

    if (configuration_->log_version()) {
        std::lock_guard<std::mutex> its_lock(version_log_timer_mutex_);
        version_log_timer_.expires_from_now(
                std::chrono::seconds(0));
        version_log_timer_.async_wait(std::bind(&routing_manager_impl::log_version_timer_cbk,
                this, std::placeholders::_1));
    }
#ifndef _WIN32
    if (configuration_->log_memory()) {
        std::lock_guard<std::mutex> its_lock(memory_log_timer_mutex_);
        boost::system::error_code ec;
        memory_log_timer_.expires_from_now(std::chrono::seconds(0), ec);
        memory_log_timer_.async_wait(
                std::bind(&routing_manager_impl::memory_log_timer_cbk, this,
                        std::placeholders::_1));
    }
#endif
    if (configuration_->log_status()) {
        std::lock_guard<std::mutex> its_lock(status_log_timer_mutex_);
        boost::system::error_code ec;
        status_log_timer_.expires_from_now(std::chrono::seconds(0), ec);
        status_log_timer_.async_wait(
                std::bind(&routing_manager_impl::status_log_timer_cbk, this,
                        std::placeholders::_1));
    }

    if (configuration_->log_statistics()) {
        std::lock_guard<std::mutex> its_lock(statistics_log_timer_mutex_);
        boost::system::error_code ec;
        statistics_log_timer_.expires_from_now(std::chrono::seconds(0), ec);
        statistics_log_timer_.async_wait(
                std::bind(&routing_manager_impl::statistics_log_timer_cbk, this,
                        std::placeholders::_1));
    }
}

void routing_manager_impl::stop() {
    // Ensure to StopOffer all services that are offered by the application hosting the rm
    local_services_map_t its_services;
    {
        std::lock_guard<std::mutex> its_lock(local_services_mutex_);
        for (const auto& s : local_services_) {
            for (const auto& i : s.second) {
                if (std::get<2>(i.second) == client_) {
                    its_services[s.first][i.first] = i.second;
                }
            }
        }

    }
    for (const auto& s : its_services) {
        for (const auto& i : s.second) {
            on_stop_offer_service(std::get<2>(i.second), s.first, i.first,
                    std::get<0>(i.second), std::get<1>(i.second));
        }
    }

    {
        std::lock_guard<std::mutex> its_lock(version_log_timer_mutex_);
        version_log_timer_.cancel();
    }
#ifndef _WIN32
    {
        boost::system::error_code ec;
        std::lock_guard<std::mutex> its_lock(memory_log_timer_mutex_);
        memory_log_timer_.cancel(ec);
    }
    if (netlink_connector_) {
        netlink_connector_->stop();
    }
#endif

    {
        std::lock_guard<std::mutex> its_lock(status_log_timer_mutex_);
        boost::system::error_code ec;
        status_log_timer_.cancel(ec);
    }

    {
        std::lock_guard<std::mutex> its_lock(statistics_log_timer_mutex_);
        boost::system::error_code ec;
        statistics_log_timer_.cancel(ec);
    }

    host_->on_state(state_type_e::ST_DEREGISTERED);

    if (discovery_)
        discovery_->stop();
    stub_->stop();

    for (const auto& client : ep_mgr_->get_connected_clients()) {
        if (client != VSOMEIP_ROUTING_CLIENT) {
            remove_local(client, true);
        }
    }
}

bool routing_manager_impl::insert_offer_command(service_t _service, instance_t _instance, uint8_t _command,
                client_t _client, major_version_t _major, minor_version_t _minor) {
    std::lock_guard<std::mutex> its_lock(offer_serialization_mutex_);
    // flag to indicate whether caller of this function can start directly processing the command
    bool must_process(false);
    auto found_service_instance = offer_commands_.find(std::make_pair(_service, _instance));
    if (found_service_instance != offer_commands_.end()) {
        // if nothing is queued
        if (found_service_instance->second.empty()) {
            must_process = true;
        }
        found_service_instance->second.push_back(
                std::make_tuple(_command, _client, _major, _minor));
    } else {
        // nothing is queued -> add command to queue and process command directly
        offer_commands_[std::make_pair(_service, _instance)].push_back(
                std::make_tuple(_command, _client, _major, _minor));
        must_process = true;
    }
    return must_process;
}

bool routing_manager_impl::erase_offer_command(service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(offer_serialization_mutex_);
    auto found_service_instance = offer_commands_.find(std::make_pair(_service, _instance));
    if (found_service_instance != offer_commands_.end()) {
        // erase processed command
        if (!found_service_instance->second.empty()) {
            found_service_instance->second.pop_front();
            if (!found_service_instance->second.empty()) {
                // check for other commands to be processed
                auto its_command = found_service_instance->second.front();
                if (std::get<0>(its_command) == VSOMEIP_OFFER_SERVICE) {
                    io_.post([&, its_command, _service, _instance](){
                        offer_service(std::get<1>(its_command), _service, _instance,
                            std::get<2>(its_command), std::get<3>(its_command), false);
                    });
                } else {
                    io_.post([&, its_command, _service, _instance](){
                        stop_offer_service(std::get<1>(its_command), _service, _instance,
                            std::get<2>(its_command), std::get<3>(its_command), false);
                    });
                }
            }
        }
    }
    return true;
}

bool routing_manager_impl::offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {

    return offer_service(_client, _service, _instance, _major, _minor, true);
}

bool routing_manager_impl::offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor,
        bool _must_queue) {

    VSOMEIP_INFO << "OFFER("
        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance
        << ":" << std::dec << int(_major) << "." << std::dec << _minor << "]"
        << " (" << std::boolalpha << _must_queue << ")";

    // only queue commands if method was NOT called via erase_offer_command()
    if (_must_queue) {
        if (!insert_offer_command(_service, _instance, VSOMEIP_OFFER_SERVICE,
                _client, _major, _minor)) {
            return false;
        }
    }

    // Check if the application hosted by routing manager is allowed to offer
    // offer_service requests of local proxies are checked in rms::on:message
    if (_client == get_client()) {
#ifdef _WIN32
        std::uint32_t its_routing_uid = ANY_UID;
        std::uint32_t its_routing_gid = ANY_GID;
#else
        std::uint32_t its_routing_uid = getuid();
        std::uint32_t its_routing_gid = getgid();
#endif
        if (!security::get()->is_offer_allowed(its_routing_uid, its_routing_gid,
                        _client, _service, _instance)) {
            VSOMEIP_WARNING << "routing_manager_impl::offer_service: "
                    << std::hex << "Security: Client 0x" << _client
                    << " isn't allowed to offer the following service/instance "
                    << _service << "/" << _instance
                    << " ~> Skip offer!";
            erase_offer_command(_service, _instance);
            return false;
        }
    }

    if (!handle_local_offer_service(_client, _service, _instance, _major, _minor)) {
        erase_offer_command(_service, _instance);
        return false;
    }

    {
        std::lock_guard<std::mutex> its_lock(pending_sd_offers_mutex_);
        if (if_state_running_) {
            init_service_info(_service, _instance, true);
        } else {
            pending_sd_offers_.push_back(std::make_pair(_service, _instance));
        }
    }

    if (discovery_) {
        std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
        if (its_info) {
            discovery_->offer_service(its_info);
        }
    }

    {
        std::lock_guard<std::mutex> ist_lock(pending_subscription_mutex_);
        std::set<event_t> its_already_subscribed_events;
        for (auto &ps : pending_subscriptions_) {
            if (ps.service_ == _service
                    && ps.instance_ == _instance
                    && ps.major_ == _major) {
                insert_subscription(ps.service_, ps.instance_,
                        ps.eventgroup_, ps.event_, client_, &its_already_subscribed_events);
#if 0
                VSOMEIP_ERROR << __func__
                        << ": event="
                        << std::hex << ps.service_ << "."
                        << std::hex << ps.instance_ << "."
                        << std::hex << ps.event_;
#endif
            }
        }

        send_pending_subscriptions(_service, _instance, _major);
    }
    stub_->on_offer_service(_client, _service, _instance, _major, _minor);
    on_availability(_service, _instance, true, _major, _minor);
    erase_offer_command(_service, _instance);
    return true;
}

void routing_manager_impl::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {

    stop_offer_service(_client, _service, _instance, _major, _minor, true);
}

void routing_manager_impl::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor,
        bool _must_queue) {

    VSOMEIP_INFO << "STOP OFFER("
        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance
        << ":" << std::dec << int(_major) << "." << _minor << "]"
        << " (" << std::boolalpha << _must_queue << ")";

    if (_must_queue) {
        if (!insert_offer_command(_service, _instance, VSOMEIP_STOP_OFFER_SERVICE,
                _client, _major, _minor)) {
            return;
        }
    }

    bool is_local(false);
    {
        std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
        is_local = (its_info && its_info->is_local());
    }
    if (is_local) {
        {
            std::lock_guard<std::mutex> its_lock(pending_sd_offers_mutex_);
            for (auto it = pending_sd_offers_.begin(); it != pending_sd_offers_.end(); ) {
                if (it->first == _service && it->second == _instance) {
                    it = pending_sd_offers_.erase(it);
                    break;
                } else {
                    ++it;
                }
            }
        }

        on_stop_offer_service(_client, _service, _instance, _major, _minor);
        stub_->on_stop_offer_service(_client, _service, _instance, _major, _minor);
        on_availability(_service, _instance, false, _major, _minor);
    } else {
        VSOMEIP_WARNING << __func__ << " received STOP_OFFER("
                << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance
                << ":" << std::dec << int(_major) << "." << _minor << "] "
                << "for remote service --> ignore";
        erase_offer_command(_service, _instance);
    }
}

void routing_manager_impl::request_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {

    VSOMEIP_INFO << "REQUEST("
        << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << ":"
        << std::dec << int(_major) << "." << std::dec << _minor << "]";

    routing_manager_base::request_service(_client,
            _service, _instance, _major, _minor);

    auto its_info = find_service(_service, _instance);
    if (!its_info) {
        requested_service_add(_client, _service, _instance, _major, _minor);
        if (discovery_) {
            if (!configuration_->is_local_service(_service, _instance)) {
                // Non local service instance ~> tell SD to find it!
                discovery_->request_service(_service, _instance, _major, _minor,
                        DEFAULT_TTL);
            } else {
                VSOMEIP_INFO << std::hex
                        << "Avoid trigger SD find-service message"
                        << " for local service/instance/major/minor: "
                        << _service << "/" << _instance << std::dec
                        << "/" << (uint32_t)_major << "/" << _minor;
            }
        }
    } else {
        if ((_major == its_info->get_major()
                || DEFAULT_MAJOR == its_info->get_major()
                || ANY_MAJOR == _major)
                && (_minor <= its_info->get_minor()
                        || DEFAULT_MINOR == its_info->get_minor()
                        || _minor == ANY_MINOR)) {
            if(!its_info->is_local()) {
                requested_service_add(_client, _service, _instance, _major, _minor);
                if (discovery_) {
                    // Non local service instance ~> tell SD to find it!
                    discovery_->request_service(_service, _instance, _major,
                            _minor, DEFAULT_TTL);
                }
                its_info->add_client(_client);
                ep_mgr_impl_->find_or_create_remote_client(
                        _service, _instance, true);
            }
        }
    }

    if (_client == get_client()) {
        stub_->create_local_receiver();

        service_data_t request = { _service, _instance, _major, _minor };
        std::set<service_data_t> requests;
        requests.insert(request);
        stub_->handle_requests(_client, requests);
    }
}

void routing_manager_impl::release_service(client_t _client, service_t _service,
        instance_t _instance) {

    VSOMEIP_INFO << "RELEASE("
        << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << "]";

    if (host_->get_client() == _client) {
        std::lock_guard<std::mutex> its_lock(pending_subscription_mutex_);
        remove_pending_subscription(_service, _instance, 0xFFFF, ANY_EVENT);
    }
    routing_manager_base::release_service(_client, _service, _instance);
    requested_service_remove(_client, _service, _instance);

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info && !its_info->is_local()) {
        if (!its_info->get_requesters_size()) {
            if (discovery_) {
                discovery_->release_service(_service, _instance);
                discovery_->unsubscribe_all(_service, _instance);
            }
            ep_mgr_impl_->clear_client_endpoints(_service, _instance, true);
            ep_mgr_impl_->clear_client_endpoints(_service, _instance, false);
            its_info->set_endpoint(nullptr, true);
            its_info->set_endpoint(nullptr, false);
            unset_all_eventpayloads(_service, _instance);
        }
    } else {
        if (discovery_) {
            discovery_->release_service(_service, _instance);
        }
    }
}

void routing_manager_impl::subscribe(client_t _client, uid_t _uid, gid_t _gid,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        major_version_t _major, event_t _event) {

    VSOMEIP_INFO << "SUBSCRIBE("
        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << ":"
        << std::hex << std::setw(4) << std::setfill('0') << _event << ":"
        << std::dec << (uint16_t)_major << "]";
    const client_t its_local_client = find_local_client(_service, _instance);
    if (get_client() == its_local_client) {
#ifdef VSOMEIP_ENABLE_COMPAT
        routing_manager_base::set_incoming_subscription_state(_client, _service, _instance,
                _eventgroup, _event, subscription_state_e::IS_SUBSCRIBING);
#endif
        auto self = shared_from_this();
        host_->on_subscription(_service, _instance, _eventgroup, _client, _uid, _gid, true,
            [this, self, _client, _uid, _gid, _service, _instance, _eventgroup,
                _event, _major]
                    (const bool _subscription_accepted) {
            (void) ep_mgr_->find_or_create_local(_client);
            if (!_subscription_accepted) {
                stub_->send_subscribe_nack(_client, _service, _instance, _eventgroup, _event);
                VSOMEIP_INFO << "Subscription request from client: 0x" << std::hex
                             << _client << std::dec << " for eventgroup: 0x" << _eventgroup
                             << " rejected from application handler.";
                return;
            } else {
                stub_->send_subscribe_ack(_client, _service, _instance, _eventgroup, _event);
            }
            routing_manager_base::subscribe(_client, _uid, _gid, _service, _instance, _eventgroup, _major, _event);
#ifdef VSOMEIP_ENABLE_COMPAT
            send_pending_notify_ones(_service, _instance, _eventgroup, _client);
            routing_manager_base::erase_incoming_subscription_state(_client, _service, _instance,
                    _eventgroup, _event);
#endif
        });
    } else {
        if (discovery_) {
            std::set<event_t> its_already_subscribed_events;

            // Note: The calls to insert_subscription & handle_subscription_state must not
            // run concurrently to a call to on_subscribe_ack. Therefore the lock is acquired
            // before calling insert_subscription and released after the call to
            // handle_subscription_state.
            std::unique_lock<std::mutex> its_critical(remote_subscription_state_mutex_);
            bool inserted = insert_subscription(_service, _instance, _eventgroup,
                    _event, _client, &its_already_subscribed_events);
            if (inserted) {
                if (0 == its_local_client) {
                    handle_subscription_state(_client, _service, _instance, _eventgroup, _event);
                    its_critical.unlock();
                    static const ttl_t configured_ttl(configuration_->get_sd_ttl());
                    notify_one_current_value(_client, _service, _instance,
                            _eventgroup, _event, its_already_subscribed_events);

                    auto its_info = find_eventgroup(_service, _instance, _eventgroup);
                    if (its_info) {
                        discovery_->subscribe(_service, _instance, _eventgroup,
                                _major, configured_ttl,
                                its_info->is_selective() ? _client : VSOMEIP_ROUTING_CLIENT,
                                its_info);
                    }
                } else {
                    its_critical.unlock();
                    if (is_available(_service, _instance, _major)) {
                        stub_->send_subscribe(ep_mgr_->find_local(_service, _instance),
                               _client, _service, _instance, _eventgroup, _major, _event,
                               PENDING_SUBSCRIPTION_ID);
                    }
                }
            }
            if (get_client() == _client) {
                std::lock_guard<std::mutex> ist_lock(pending_subscription_mutex_);
                subscription_data_t subscription = {
                    _service, _instance, _eventgroup, _major, _event, _uid, _gid
                };
                pending_subscriptions_.insert(subscription);
            }
        } else {
            VSOMEIP_ERROR<< "SOME/IP eventgroups require SD to be enabled!";
        }
    }
}

void routing_manager_impl::unsubscribe(client_t _client, uid_t _uid, gid_t _gid,
    service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event) {

    VSOMEIP_INFO << "UNSUBSCRIBE("
        << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "."
        << std::hex << std::setw(4) << std::setfill('0') << _event << "]";

    bool last_subscriber_removed(true);

    std::shared_ptr<eventgroupinfo> its_info
        = find_eventgroup(_service, _instance, _eventgroup);
    if (its_info) {
        for (const auto& e : its_info->get_events()) {
            if (e->get_event() == _event || ANY_EVENT == _event)
                e->remove_subscriber(_eventgroup, _client);
        }
        for (const auto& e : its_info->get_events()) {
            if (e->has_subscriber(_eventgroup, ANY_CLIENT)) {
                last_subscriber_removed = false;
                break;
            }
        }
    }

    if (discovery_) {
        host_->on_subscription(_service, _instance, _eventgroup, _client, _uid, _gid, false,
                [](const bool _subscription_accepted){ (void)_subscription_accepted; });
        if (0 == find_local_client(_service, _instance)) {
            if (last_subscriber_removed) {
                unset_all_eventpayloads(_service, _instance, _eventgroup);
                {
                    auto tuple = std::make_tuple(_service, _instance, _eventgroup, _client);
                    std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
                    remote_subscription_state_.erase(tuple);
                }
            }

            if (its_info &&
                    (last_subscriber_removed || its_info->is_selective())) {

                discovery_->unsubscribe(_service, _instance, _eventgroup,
                        its_info->is_selective() ? _client : VSOMEIP_ROUTING_CLIENT);
            }
        } else {
            if (get_client() == _client) {
                std::lock_guard<std::mutex> ist_lock(pending_subscription_mutex_);
                remove_pending_subscription(_service, _instance, _eventgroup, _event);
                stub_->send_unsubscribe(
                        ep_mgr_->find_local(_service, _instance),
                        _client, _service, _instance, _eventgroup, _event,
                        PENDING_SUBSCRIPTION_ID);
            }
        }
        ep_mgr_impl_->clear_multicast_endpoints(_service, _instance);

    } else {
        VSOMEIP_ERROR<< "SOME/IP eventgroups require SD to be enabled!";
    }
}

bool routing_manager_impl::send(client_t _client,
        std::shared_ptr<message> _message) {
    return routing_manager_base::send(_client, _message);
}

bool routing_manager_impl::send(client_t _client, const byte_t *_data,
        length_t _size, instance_t _instance, bool _reliable,
        client_t _bound_client,
        credentials_t _credentials,
        uint8_t _status_check, bool _sent_from_remote) {
    bool is_sent(false);
    if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
        std::shared_ptr<endpoint> its_target;
        bool is_request = utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS]);
        bool is_notification = utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]);
        bool is_response = utility::is_response(_data[VSOMEIP_MESSAGE_TYPE_POS]);

        client_t its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN],
                _data[VSOMEIP_CLIENT_POS_MAX]);
        service_t its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        method_t its_method = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);

        bool is_service_discovery
            = (its_service == sd::service && its_method == sd::method);

        if (is_request) {
            its_target = ep_mgr_->find_local(its_service, _instance);
        } else if (!is_notification) {
            its_target = find_local(its_client);
        } else if (is_notification && _client && !is_service_discovery) { // Selective notifications!
            if (_client == get_client()) {
#ifdef USE_DLT
                const uint16_t its_data_size
                    = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                trace::header its_header;
                if (its_header.prepare(its_target, true, _instance))
                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                            _data, its_data_size);
#endif
                deliver_message(_data, _size, _instance, _reliable, _bound_client, _credentials, _status_check, _sent_from_remote);
                return true;
            }
            its_target = find_local(_client);
        }

        if (its_target) {
#ifdef USE_DLT
            if ((is_request && its_client == get_client()) ||
                    (is_response && find_local_client(its_service, _instance) == get_client())) {
                const uint16_t its_data_size
                    = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                trace::header its_header;
                if (its_header.prepare(its_target, true, _instance))
                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                            _data, its_data_size);
            }
#endif
            is_sent = send_local(its_target, get_client(), _data, _size, _instance, _reliable, VSOMEIP_SEND, _status_check);
        } else {
            // Check whether hosting application should get the message
            // If not, check routes to external
            if ((its_client == host_->get_client() && is_response)
                    || (find_local_client(its_service, _instance)
                            == host_->get_client() && is_request)) {
                // TODO: Find out how to handle session id here
                is_sent = deliver_message(_data, _size, _instance, _reliable, VSOMEIP_ROUTING_CLIENT, _credentials, _status_check);
            } else {
                e2e_buffer its_buffer;

                if (e2e_provider_) {
                    if ( !is_service_discovery) {
                        service_t its_service = VSOMEIP_BYTES_TO_WORD(
                                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
                        method_t its_method = VSOMEIP_BYTES_TO_WORD(
                                _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
#ifndef ANDROID
                        if (e2e_provider_->is_protected({its_service, its_method})) {
                            // Find out where the protected area starts
                            size_t its_base = e2e_provider_->get_protection_base({its_service, its_method});

                            // Build a corresponding buffer
                            its_buffer.assign(_data + its_base, _data + _size);

                            e2e_provider_->protect({ its_service, its_method }, its_buffer, _instance);

                            // Prepend header
                            its_buffer.insert(its_buffer.begin(), _data, _data + its_base);

                            _data = its_buffer.data();
                       }
#endif
                    }
                }
                if (is_request) {
                    its_target = ep_mgr_impl_->find_or_create_remote_client(
                            its_service, _instance, _reliable);
                    if (its_target) {
#ifdef USE_DLT
                        const uint16_t its_data_size
                            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                        trace::header its_header;
                        if (its_header.prepare(its_target, true, _instance))
                            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                    _data, its_data_size);
#endif
                        is_sent = its_target->send(_data, _size);
                    } else {
                        const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                                _data[VSOMEIP_SESSION_POS_MIN],
                                _data[VSOMEIP_SESSION_POS_MAX]);
                        VSOMEIP_ERROR<< "Routing info for remote service could not be found! ("
                                << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                                << std::hex << std::setw(4) << std::setfill('0') << its_method << "] "
                                << std::hex << std::setw(4) << std::setfill('0') << its_session;
                    }
                } else {
                    std::shared_ptr<serviceinfo> its_info(find_service(its_service, _instance));
                    if (its_info || is_service_discovery) {
                        if (is_notification && !is_service_discovery) {
                            send_local_notification(get_client(), _data, _size, _instance, _reliable, _status_check);
                            method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
                                    _data[VSOMEIP_METHOD_POS_MAX]);
                            std::shared_ptr<event> its_event = find_event(its_service, _instance, its_method);
                            if (its_event) {
#ifdef USE_DLT
                                bool has_sent(false);
#endif
                                std::set<std::shared_ptr<endpoint_definition>> its_targets;
                                // we need both endpoints as clients can subscribe to events via TCP and UDP
                                std::shared_ptr<endpoint> its_udp_server_endpoint = its_info->get_endpoint(false);
                                std::shared_ptr<endpoint> its_tcp_server_endpoint = its_info->get_endpoint(true);

                                if (its_udp_server_endpoint || its_tcp_server_endpoint) {
                                    const auto its_reliability = its_event->get_reliability();
                                    for (auto its_group : its_event->get_eventgroups()) {
                                        auto its_eventgroup = find_eventgroup(its_service, _instance, its_group);
                                        if (its_eventgroup) {
                                            // Unicast targets
                                            for (const auto &its_remote : its_eventgroup->get_unicast_targets()) {
                                                if (its_remote->is_reliable() && its_tcp_server_endpoint) {
                                                    if (its_reliability == reliability_type_e::RT_RELIABLE
                                                            || its_reliability == reliability_type_e::RT_BOTH) {
                                                        its_targets.insert(its_remote);
                                                    }
                                                } else if (its_udp_server_endpoint && !its_eventgroup->is_sending_multicast()) {
                                                    if (its_reliability == reliability_type_e::RT_UNRELIABLE
                                                            || its_reliability == reliability_type_e::RT_BOTH) {
                                                        its_targets.insert(its_remote);
                                                    }
                                                }
                                            }
                                            // Send to multicast targets if subscribers are still interested
                                            if (its_eventgroup->is_sending_multicast()) {
                                                if (its_reliability == reliability_type_e::RT_UNRELIABLE
                                                        || its_reliability == reliability_type_e::RT_BOTH) {
                                                    boost::asio::ip::address its_address;
                                                    uint16_t its_port;
                                                    if (its_eventgroup->get_multicast(its_address, its_port)) {
                                                        std::shared_ptr<endpoint_definition> its_multicast_target;
                                                        its_multicast_target = endpoint_definition::get(its_address,
                                                                its_port, false, its_service, _instance);
                                                        its_targets.insert(its_multicast_target);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                for (auto const &target : its_targets) {
                                    if (target->is_reliable()) {
                                        its_tcp_server_endpoint->send_to(target, _data, _size);
                                    } else {
                                        its_udp_server_endpoint->send_to(target, _data, _size);
                                    }
#ifdef USE_DLT
                                    has_sent = true;
#endif
                                }
#ifdef USE_DLT
                                if (has_sent) {
                                    const uint16_t its_data_size
                                        = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                                    trace::header its_header;
                                    if (its_header.prepare(nullptr, true, _instance))
                                        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                                _data, its_data_size);
                                }
#endif
                            }
                        } else {
                            if ((utility::is_response(_data[VSOMEIP_MESSAGE_TYPE_POS])
                                 || utility::is_error(_data[VSOMEIP_MESSAGE_TYPE_POS]))
                                    && !its_info->is_local()) {
                                // We received a response/error but neither the hosting application
                                // nor another local client could be found --> drop
                                const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                                        _data[VSOMEIP_SESSION_POS_MIN],
                                        _data[VSOMEIP_SESSION_POS_MAX]);
                                VSOMEIP_ERROR
                                    << "routing_manager_impl::send: Received response/error for unknown client ("
                                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                                    << std::hex << std::setw(4) << std::setfill('0') << its_method << "] "
                                    << std::hex << std::setw(4) << std::setfill('0') << its_session;
                                return false;
                            }
                            its_target = is_service_discovery ?
                                         (sd_info_ ? sd_info_->get_endpoint(false) : nullptr) : its_info->get_endpoint(_reliable);
                            if (its_target) {
#ifdef USE_DLT
                                const uint16_t its_data_size
                                    = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                                trace::header its_header;
                                if (its_header.prepare(its_target, true, _instance))
                                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                            _data, its_data_size);
#endif
                                is_sent = its_target->send(_data, _size);
                            } else {
                                const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                                        _data[VSOMEIP_SESSION_POS_MIN],
                                        _data[VSOMEIP_SESSION_POS_MAX]);
                                VSOMEIP_ERROR << "Routing error. Endpoint for service ("
                                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                                        << std::hex << std::setw(4) << std::setfill('0') << its_method << "] "
                                        << std::hex << std::setw(4) << std::setfill('0') << its_session
                                        << " could not be found!";
                            }
                        }
                    } else {
                        if (!is_notification) {
                            const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                                    _data[VSOMEIP_SESSION_POS_MIN],
                                    _data[VSOMEIP_SESSION_POS_MAX]);
                            VSOMEIP_ERROR << "Routing error. Not hosting service ("
                                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                                    << std::hex << std::setw(4) << std::setfill('0') << its_method << "] "
                                    << std::hex << std::setw(4) << std::setfill('0') << its_session;
                        }
                    }
                }
            }
        }
    }

    return (is_sent);
}

bool routing_manager_impl::send_to(
        const client_t _client,
        const std::shared_ptr<endpoint_definition> &_target,
        std::shared_ptr<message> _message) {
    bool is_sent(false);

    std::shared_ptr<serializer> its_serializer(get_serializer());
    if (its_serializer->serialize(_message.get())) {
        const byte_t *its_data = its_serializer->get_data();
        length_t its_size = its_serializer->get_size();
        e2e_buffer its_buffer;
        if (e2e_provider_) {
            service_t its_service = VSOMEIP_BYTES_TO_WORD(
                    its_data[VSOMEIP_SERVICE_POS_MIN],
                    its_data[VSOMEIP_SERVICE_POS_MAX]);
            method_t its_method = VSOMEIP_BYTES_TO_WORD(
                    its_data[VSOMEIP_METHOD_POS_MIN],
                    its_data[VSOMEIP_METHOD_POS_MAX]);
#ifndef ANDROID
            if (e2e_provider_->is_protected({its_service, its_method})) {
                auto its_base = e2e_provider_->get_protection_base({its_service, its_method});
                its_buffer.assign(its_data + its_base, its_data + its_size);
                e2e_provider_->protect({its_service, its_method}, its_buffer, _message->get_instance());
                its_buffer.insert(its_buffer.begin(), its_data, its_data + its_base);
                its_data = its_buffer.data();
           }
#endif
        }

        const_cast<byte_t*>(its_data)[VSOMEIP_CLIENT_POS_MIN] = VSOMEIP_WORD_BYTE1(_client);
        const_cast<byte_t*>(its_data)[VSOMEIP_CLIENT_POS_MAX] = VSOMEIP_WORD_BYTE0(_client);

        is_sent = send_to(_target, its_data, its_size, _message->get_instance());

        its_serializer->reset();
        put_serializer(its_serializer);
    } else {
        VSOMEIP_ERROR<< "routing_manager_impl::send_to: serialization failed.";
    }
    return (is_sent);
}

bool routing_manager_impl::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size, instance_t _instance) {
    std::shared_ptr<endpoint> its_endpoint =
            ep_mgr_impl_->find_server_endpoint(
                    _target->get_remote_port(), _target->is_reliable());

    if (its_endpoint) {
#ifdef USE_DLT
        const uint16_t its_data_size
            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

        trace::header its_header;
        if (its_header.prepare(its_endpoint, true, _instance))
            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                    _data, its_data_size);
#else
        (void) _instance;
#endif
        return its_endpoint->send_to(_target, _data, _size);
    }
    return false;
}

bool routing_manager_impl::send_via_sd(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size, uint16_t _sd_port) {
    std::shared_ptr<endpoint> its_endpoint =
            ep_mgr_impl_->find_server_endpoint(_sd_port,
                    _target->is_reliable());

    if (its_endpoint) {
#ifdef USE_DLT
        if (tc_->is_sd_enabled()) {
            const uint16_t its_data_size
                = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

            trace::header its_header;
            if (its_header.prepare(its_endpoint, true, 0x0))
                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                        _data, its_data_size);

        }
#endif
        return its_endpoint->send_to(_target, _data, _size);
    }

    return false;
}

void routing_manager_impl::register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _notifier,
        const std::set<eventgroup_t> &_eventgroups, const event_type_e _type,
        reliability_type_e _reliability,
        std::chrono::milliseconds _cycle, bool _change_resets_cycle,
        bool _update_on_change,
        epsilon_change_func_t _epsilon_change_func,
        bool _is_provided, bool _is_shadow, bool _is_cache_placeholder) {
    auto its_event = find_event(_service, _instance, _notifier);
    bool is_first(false);
    if (its_event) {
        if (!its_event->has_ref(_client, _is_provided)) {
            is_first = true;
        }
    } else {
        is_first = true;
    }
    if (is_first) {
        routing_manager_base::register_event(_client,
                _service, _instance,
                _notifier,
                _eventgroups, _type, _reliability,
                _cycle, _change_resets_cycle, _update_on_change,
                _epsilon_change_func, _is_provided, _is_shadow,
                _is_cache_placeholder);
    }
    VSOMEIP_INFO << "REGISTER EVENT("
        << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
        << std::hex << std::setw(4) << std::setfill('0') << _notifier
        << ":is_provider=" << std::boolalpha << _is_provided << "]";
}

void routing_manager_impl::register_shadow_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _notifier,
        const std::set<eventgroup_t> &_eventgroups, event_type_e _type,
        reliability_type_e _reliability, bool _is_provided) {
    routing_manager_base::register_event(_client,
            _service, _instance,
            _notifier,
            _eventgroups, _type, _reliability,
            std::chrono::milliseconds::zero(), false, true,
            nullptr,
            _is_provided, true);
}

void routing_manager_impl::unregister_shadow_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, bool _is_provided) {
    routing_manager_base::unregister_event(_client, _service, _instance,
            _event, _is_provided);
}

void routing_manager_impl::notify_one(service_t _service, instance_t _instance,
        event_t _event, std::shared_ptr<payload> _payload, client_t _client,
        bool _force
#ifdef VSOMEIP_ENABLE_COMPAT
        , bool _remote_subscriber
#endif
        ) {
    if (find_local(_client)) {
        routing_manager_base::notify_one(_service, _instance, _event, _payload,
                _client, _force
#ifdef VSOMEIP_ENABLE_COMPAT
                , _remote_subscriber
#endif
                );
    } else {
        std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
        if (its_event) {
            std::set<std::shared_ptr<endpoint_definition> > its_targets;
            const auto its_reliability = its_event->get_reliability();
            for (const auto& g : its_event->get_eventgroups()) {
                const auto its_eventgroup = find_eventgroup(_service, _instance, g);
                if (its_eventgroup) {
                    const auto its_subscriptions = its_eventgroup->get_remote_subscriptions();
                    for (const auto &s : its_subscriptions) {
                        if (s->has_client(_client)) {
                            if (its_reliability == reliability_type_e::RT_RELIABLE
                                    || its_reliability == reliability_type_e::RT_BOTH) {
                                const auto its_reliable = s->get_reliable();
                                if (its_reliable)
                                    its_targets.insert(its_reliable);
                            }
                            if (its_reliability == reliability_type_e::RT_UNRELIABLE
                                    || its_reliability == reliability_type_e::RT_BOTH) {
                                const auto its_unreliable = s->get_unreliable();
                                if (its_unreliable)
                                    its_targets.insert(its_unreliable);
                            }
                        }
                    }
                }
            }

            if (its_targets.size() > 0) {
                for (const auto &its_target : its_targets) {
                    its_event->set_payload(_payload, _client, its_target, _force);
                }
            }
        } else {
            VSOMEIP_WARNING << "Attempt to update the undefined event/field ["
                << std::hex << _service << "." << _instance << "." << _event
                << "]";
        }
    }
}

void routing_manager_impl::on_availability(service_t _service, instance_t _instance,
        bool _is_available, major_version_t _major, minor_version_t _minor) {
    host_->on_availability(_service, _instance, _is_available, _major, _minor);
}


bool routing_manager_impl::offer_service_remotely(service_t _service,
                                                  instance_t _instance,
                                                  std::uint16_t _port,
                                                  bool _reliable,
                                                  bool _magic_cookies_enabled) {
    bool ret = true;

    if(!is_available(_service, _instance, ANY_MAJOR)) {
        VSOMEIP_ERROR << __func__ << ": Service ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance
                << "] is not offered locally! Won't offer it remotely.";
        ret = false;
    } else {
        // update service info in configuration
        if (!configuration_->remote_offer_info_add(_service, _instance, _port,
                _reliable, _magic_cookies_enabled)) {
            ret = false;
        } else {
            // trigger event registration again to create shadow events
            const client_t its_offering_client = find_local_client(_service, _instance);
            if (its_offering_client == VSOMEIP_ROUTING_CLIENT) {
                VSOMEIP_ERROR << __func__ << " didn't find offering client for service ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance
                        << "]";
                ret = false;
            } else {
                if (!stub_->send_provided_event_resend_request(its_offering_client,
                        pending_remote_offer_add(_service, _instance))) {
                    VSOMEIP_ERROR << __func__ << ": Couldn't send event resend"
                        << "request to client 0x" << std::hex << std::setw(4)
                        << std::setfill('0') << its_offering_client << " providing service ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance
                        << "]";

                    ret = false;
                }
            }
        }
    }
    return ret;
}

bool routing_manager_impl::stop_offer_service_remotely(service_t _service,
                                                       instance_t _instance,
                                                       std::uint16_t _port,
                                                       bool _reliable,
                                                       bool _magic_cookies_enabled) {
    bool ret = true;
    bool service_still_offered_remote(false);
    // update service configuration
    if (!configuration_->remote_offer_info_remove(_service, _instance, _port,
            _reliable, _magic_cookies_enabled, &service_still_offered_remote)) {
        VSOMEIP_ERROR << __func__ << " couldn't remove remote offer info for service ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance
                << "] from configuration";
        ret = false;
    }
    std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
    std::shared_ptr<endpoint> its_server_endpoint;
    if (its_info) {
        its_server_endpoint = its_info->get_endpoint(_reliable);
    }
    // don't deregister events if the service is still offered remotely
    if (!service_still_offered_remote) {
        const client_t its_offering_client = find_local_client(_service, _instance);
        major_version_t its_major(0);
        minor_version_t its_minor(0);
        if (its_info) {
            its_major = its_info->get_major();
            its_minor = its_info->get_minor();
        }
        // unset payload and clear subcribers
        routing_manager_base::stop_offer_service(its_offering_client,
                _service, _instance, its_major, its_minor);
        // unregister events
        for (const event_t its_event_id : find_events(_service, _instance)) {
            unregister_shadow_event(its_offering_client, _service, _instance,
                    its_event_id, true);
        }
        clear_targets_and_pending_sub_from_eventgroups(_service, _instance);
        clear_remote_subscriber(_service, _instance);

        if (discovery_ && its_info) {
            discovery_->stop_offer_service(its_info);
            its_info->set_endpoint(std::shared_ptr<endpoint>(), _reliable);
        }
    } else {
        // service is still partly offered
        if (discovery_ && its_info) {
            std::shared_ptr<serviceinfo> its_copied_info =
                    std::make_shared<serviceinfo>(*its_info);
            its_info->set_endpoint(std::shared_ptr<endpoint>(), _reliable);
            // ensure to not send StopOffer for endpoint on which the service is
            // still offered
            its_copied_info->set_endpoint(std::shared_ptr<endpoint>(), !_reliable);
            discovery_->stop_offer_service(its_copied_info);
        }
    }

    cleanup_server_endpoint(_service, its_server_endpoint);
    return ret;
}

void routing_manager_impl::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination,
        client_t _bound_client, credentials_t _credentials,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {
#if 0
    std::stringstream msg;
    msg << "rmi::on_message: ";
    for (uint32_t i = 0; i < _size; ++i)
    msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    (void)_bound_client;
    service_t its_service;
    method_t its_method;
    uint8_t its_check_status = e2e::profile_interface::generic_check_status::E2E_OK;
    instance_t its_instance(0x0);
#ifdef USE_DLT
    bool is_forwarded(true);
#endif
    if (_size >= VSOMEIP_SOMEIP_HEADER_SIZE) {
        its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
                _data[VSOMEIP_SERVICE_POS_MAX]);
        if (its_service == VSOMEIP_SD_SERVICE) {
            its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
                            _data[VSOMEIP_METHOD_POS_MAX]);
            if (discovery_ && its_method == sd::method) {
                if (configuration_->get_sd_port() == _remote_port) {
                    if (!_remote_address.is_unspecified()) {
                        discovery_->on_message(_data, _size, _remote_address, _destination);
                    } else {
                        VSOMEIP_ERROR << "Ignored SD message from unknown address.";
                    }
                } else {
                    VSOMEIP_ERROR << "Ignored SD message from unknown port ("
                            << _remote_port << ")";
                }
            }
        } else {
            if(_destination.is_multicast()) {
                its_instance = ep_mgr_impl_->find_instance_multicast(its_service, _remote_address);
            } else {
                its_instance = ep_mgr_impl_->find_instance(its_service, _receiver);
            }
            if (its_instance == 0xFFFF) {
                its_method = VSOMEIP_BYTES_TO_WORD(
                        _data[VSOMEIP_METHOD_POS_MIN],
                        _data[VSOMEIP_METHOD_POS_MAX]);
                const client_t its_client = VSOMEIP_BYTES_TO_WORD(
                        _data[VSOMEIP_CLIENT_POS_MIN],
                        _data[VSOMEIP_CLIENT_POS_MAX]);
                const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                        _data[VSOMEIP_SESSION_POS_MIN],
                        _data[VSOMEIP_SESSION_POS_MAX]);
                boost::system::error_code ec;
                VSOMEIP_ERROR << "Received message on invalid port: ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_method << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_session << "] from: "
                        << _remote_address.to_string(ec) << ":" << std::dec << _remote_port;
            }
            //Ignore messages with invalid message type
            if(_size >= VSOMEIP_MESSAGE_TYPE_POS) {
                if(!utility::is_valid_message_type(static_cast<message_type_e>(_data[VSOMEIP_MESSAGE_TYPE_POS]))) {
                    VSOMEIP_ERROR << "Ignored SomeIP message with invalid message type.";
                    return;
                }
            }
            return_code_e return_code = check_error(_data, _size, its_instance);
            if(!(_size >= VSOMEIP_MESSAGE_TYPE_POS && utility::is_request_no_return(_data[VSOMEIP_MESSAGE_TYPE_POS]))) {
                if (return_code != return_code_e::E_OK && return_code != return_code_e::E_NOT_OK) {
                    send_error(return_code, _data, _size, its_instance,
                            _receiver->is_reliable(), _receiver,
                            _remote_address, _remote_port);
                    return;
                }
            } else if(return_code != return_code_e::E_OK && return_code != return_code_e::E_NOT_OK) {
                //Ignore request no response message if an error occured
                return;
            }

            // Security checks if enabled!
            if (security::get()->is_enabled()) {
                if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
                    client_t requester = VSOMEIP_BYTES_TO_WORD(
                            _data[VSOMEIP_CLIENT_POS_MIN],
                            _data[VSOMEIP_CLIENT_POS_MAX]);
                    its_method = VSOMEIP_BYTES_TO_WORD(
                               _data[VSOMEIP_METHOD_POS_MIN],
                               _data[VSOMEIP_METHOD_POS_MAX]);
                    if (!configuration_->is_offered_remote(its_service, its_instance)) {
                        VSOMEIP_WARNING << std::hex << "Security: Received a remote request "
                                << "for service/instance " << its_service << "/" << its_instance
                                << " which isn't offered remote ~> Skip message!";
                        return;
                    }
                    if (find_local(requester)) {
                        VSOMEIP_WARNING << std::hex << "Security: Received a remote request "
                                << "from client identifier 0x" << requester
                                << " which is already used locally ~> Skip message!";
                        return;
                    }
                    if (!security::get()->is_remote_client_allowed()) {
                        // check if policy allows remote requests.
                        VSOMEIP_WARNING << "routing_manager_impl::on_message: "
                                << std::hex << "Security: Remote client with client ID 0x" << requester
                                << " is not allowed to communicate with service/instance/method "
                                << its_service << "/" << its_instance
                                << "/" << its_method;
                        return;
                    }
                }
            }
            if (e2e_provider_) {
                its_method = VSOMEIP_BYTES_TO_WORD(
                           _data[VSOMEIP_METHOD_POS_MIN],
                           _data[VSOMEIP_METHOD_POS_MAX]);
#ifndef ANDROID
                if (e2e_provider_->is_checked({its_service, its_method})) {
                    auto its_base = e2e_provider_->get_protection_base({its_service, its_method});
                    e2e_buffer its_buffer(_data + its_base, _data + _size);
                    e2e_provider_->check({its_service, its_method},
                            its_buffer, its_instance, its_check_status);

                    if (its_check_status != e2e::profile_interface::generic_check_status::E2E_OK) {
                        VSOMEIP_INFO << "E2E protection: CRC check failed for service: "
                                << std::hex << its_service << " method: " << its_method;
                    }
                }
#endif
            }
            // Common way of message handling
#ifdef USE_DLT
            is_forwarded =
#endif
            on_message(its_service, its_instance, _data, _size, _receiver->is_reliable(),
                    _bound_client, _credentials, its_check_status, true);
        }
    }
#ifdef USE_DLT
    if (is_forwarded) {
        const uint16_t its_data_size
            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

        trace::header its_header;
        const boost::asio::ip::address_v4 its_remote_address =
                _remote_address.is_v4() ? _remote_address.to_v4() :
                        boost::asio::ip::address_v4::from_string("6.6.6.6");
        trace::protocol_e its_protocol =
                _receiver->is_local() ? trace::protocol_e::local :
                _receiver->is_reliable() ? trace::protocol_e::tcp :
                    trace::protocol_e::udp;
        its_header.prepare(its_remote_address, _remote_port, its_protocol, false,
                its_instance);
        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data,
                its_data_size);
    }
#endif
}

bool routing_manager_impl::on_message(
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _size,
        bool _reliable, client_t _bound_client,
        credentials_t _credentials,
        uint8_t _check_status,
        bool _is_from_remote) {
#if 0
    std::stringstream msg;
    msg << "rmi::on_message("
            << std::hex << std::setw(4) << std::setfill('0')
            << _service << ", " << _instance << "): ";
    for (uint32_t i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    client_t its_client;
    bool is_forwarded(true);

    if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        its_client = find_local_client(_service, _instance);
    } else {
        its_client = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_CLIENT_POS_MIN],
                _data[VSOMEIP_CLIENT_POS_MAX]);
    }

    if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        is_forwarded = deliver_notification(_service, _instance, _data, _size,
                _reliable, _bound_client, _credentials, _check_status, _is_from_remote);
    } else if (its_client == host_->get_client()) {
        deliver_message(_data, _size, _instance,
                _reliable, _bound_client, _credentials, _check_status, _is_from_remote);
    } else {
        send(its_client, _data, _size, _instance, _reliable,
                _bound_client, _credentials, _check_status, _is_from_remote); //send to proxy
    }
    return is_forwarded;
}

void routing_manager_impl::on_notification(client_t _client,
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _size, bool _notify_one) {
    event_t its_event_id = VSOMEIP_BYTES_TO_WORD(
            _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
    std::shared_ptr<event> its_event = find_event(_service, _instance, its_event_id);
    if (its_event) {
        uint32_t its_length = utility::get_payload_size(_data, _size);
        std::shared_ptr<payload> its_payload =
                runtime::get()->create_payload(
                                    &_data[VSOMEIP_PAYLOAD_POS],
                                    its_length);

        if (_notify_one) {
            notify_one(_service, _instance, its_event->get_event(),
                    its_payload, _client, true
#ifdef VSOMEIP_ENABLE_COMPAT
                    , false
#endif
                    );
        } else {
            if (its_event->is_field()) {
                if (!its_event->set_payload_notify_pending(its_payload)) {
                    its_event->set_payload(its_payload, false);
                }
            } else {
                 its_event->set_payload(its_payload, false, true);
            }
        }
    }
}

bool routing_manager_impl::is_last_stop_callback(const uint32_t _callback_id) {
    bool last_callback(false);
    auto found_id = callback_counts_.find(_callback_id);
    if (found_id != callback_counts_.end()) {
        found_id->second--;
        if (found_id->second == 0) {
            last_callback = true;
        }
    }
    if (last_callback) {
        callback_counts_.erase(_callback_id);
    }
    return last_callback;
}

void routing_manager_impl::on_stop_offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    {
        std::lock_guard<std::mutex> its_lock(local_services_mutex_);
        auto found_service = local_services_.find(_service);
        if (found_service != local_services_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                if (   std::get<0>(found_instance->second) != _major
                    || std::get<1>(found_instance->second) != _minor
                    || std::get<2>(found_instance->second) != _client) {
                    VSOMEIP_WARNING
                            << "routing_manager_impl::on_stop_offer_service: "
                            << "trying to delete service not matching exactly "
                            << "the one offered previously: " << "[" << std::hex
                            << std::setw(4) << std::setfill('0') << _service
                            << "." << _instance << "." << std::dec
                            << static_cast<std::uint32_t>(_major)
                            << "." << _minor << "] by application: "
                            << std::hex << std::setw(4) << std::setfill('0')
                            << _client << ". Stored: [" << std::hex
                            << std::setw(4) << std::setfill('0') << _service
                            << "." << _instance << "." << std::dec
                            << static_cast<std::uint32_t>(std::get<0>(found_instance->second)) << "."
                            << std::get<1>(found_instance->second)
                            << "] by application: " << std::hex << std::setw(4)
                            << std::setfill('0') << std::get<2>(found_instance->second);
                }
                if (std::get<2>(found_instance->second) == _client) {
                    found_service->second.erase(_instance);
                    if (found_service->second.size() == 0) {
                        local_services_.erase(_service);
                    }
                }
            }
        }
    }

    routing_manager_base::stop_offer_service(_client, _service, _instance,
            _major, _minor);

    /**
     * Hold reliable & unreliable server-endpoints from service info
     * because if "del_routing_info" is called those entries could be freed
     * and we can't be sure this happens synchronous when SD is active.
     * After triggering "del_routing_info" this endpoints gets cleanup up
     * within this method if they not longer used by any other local service.
     */
    std::shared_ptr<endpoint> its_reliable_endpoint;
    std::shared_ptr<endpoint> its_unreliable_endpoint;
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info) {
        its_reliable_endpoint = its_info->get_endpoint(true);
        its_unreliable_endpoint = its_info->get_endpoint(false);
        static std::atomic<uint32_t> callback_id(0);
        const uint32_t its_callback_id = ++callback_id;

        struct ready_to_stop_t {
            ready_to_stop_t() : reliable_(false), unreliable_(false){}
            std::atomic<bool> reliable_;
            std::atomic<bool> unreliable_;
        };
        auto ready_to_stop = std::make_shared<ready_to_stop_t>();
        auto ptr = shared_from_this();

        auto callback = [&, its_callback_id, ptr, its_info, its_reliable_endpoint, its_unreliable_endpoint,
                         ready_to_stop, _service, _instance, _major, _minor]
                         (std::shared_ptr<endpoint> _endpoint, service_t _stopped_service) {
            (void)_stopped_service;
            if (its_reliable_endpoint && its_reliable_endpoint == _endpoint) {
                ready_to_stop->reliable_ = true;
            }
            if (its_unreliable_endpoint && its_unreliable_endpoint == _endpoint) {
                ready_to_stop->unreliable_ = true;
            }
            if ((its_unreliable_endpoint && !ready_to_stop->unreliable_) ||
                (its_reliable_endpoint && !ready_to_stop->reliable_)) {
                {
                    std::lock_guard<std::mutex> its_lock(callback_counts_mutex_);
                    if (is_last_stop_callback(its_callback_id)) {
                        erase_offer_command(_service, _instance);
                    }
                }
                return;
            }

            if (discovery_) {
                if (its_info->get_major() == _major && its_info->get_minor() == _minor) {
                    discovery_->stop_offer_service(its_info);
                }
            }
            del_routing_info(_service, _instance, (its_reliable_endpoint != nullptr),
                    (its_unreliable_endpoint != nullptr));

            for (const auto& ep: {its_reliable_endpoint, its_unreliable_endpoint}) {
                if (ep) {
                    if (ep_mgr_impl_->remove_instance(_service, ep.get())) {
                        {
                            std::lock_guard<std::mutex> its_lock(callback_counts_mutex_);
                            callback_counts_[its_callback_id]++;
                        }
                        // last instance -> pass ANY_INSTANCE and shutdown completely
                        ep->prepare_stop(
                            [&, _service, _instance, its_callback_id, ptr, its_reliable_endpoint, its_unreliable_endpoint]
                            (std::shared_ptr<endpoint> _endpoint,
                            service_t _stopped_service) {
                                (void)_stopped_service;
                                if (ep_mgr_impl_->remove_server_endpoint(
                                                _endpoint->get_local_port(),
                                                _endpoint->is_reliable())) {
                                    _endpoint->stop();
                                }
                                {
                                    std::lock_guard<std::mutex> its_lock(callback_counts_mutex_);
                                    if (is_last_stop_callback(its_callback_id)) {
                                        erase_offer_command(_service, _instance);
                                    }
                                }
                            }, ANY_SERVICE);
                    }
                    // Clear service info and service group
                    clear_service_info(_service, _instance, ep->is_reliable());
                }
            }
            {
                std::lock_guard<std::mutex> its_lock(callback_counts_mutex_);
                if (is_last_stop_callback(its_callback_id)) {
                    erase_offer_command(_service, _instance);
                }
            }
        };

        // determine callback count
        for (const auto& ep : {its_reliable_endpoint, its_unreliable_endpoint}) {
            if (ep) {
                std::lock_guard<std::mutex> its_lock(callback_counts_mutex_);
                auto found_id = callback_counts_.find(its_callback_id);
                if (found_id != callback_counts_.end()) {
                    found_id->second++;
                } else {
                    callback_counts_[its_callback_id] = 1;
                }
            }
        }
        for (const auto& ep : {its_reliable_endpoint, its_unreliable_endpoint}) {
            if (ep) {
                ep->prepare_stop(callback, _service);
            }
        }

        if (!its_reliable_endpoint && !its_unreliable_endpoint) {
            {
                std::lock_guard<std::mutex> its_lock(callback_counts_mutex_);
                callback_counts_.erase(its_callback_id);
            }
            erase_offer_command(_service, _instance);
        }

        std::lock_guard<std::mutex> its_eventgroups_lock(eventgroups_mutex_);
        auto find_service = eventgroups_.find(_service);
        if (find_service != eventgroups_.end()) {
            auto find_instance = find_service->second.find(_instance);
            if (find_instance != find_service->second.end()) {
                for (auto e : find_instance->second) {
                    e.second->clear_remote_subscriptions();
                }
            }
        }
    } else {
        erase_offer_command(_service, _instance);
    }
}

bool routing_manager_impl::deliver_message(const byte_t *_data, length_t _size,
        instance_t _instance, bool _reliable, client_t _bound_client, credentials_t _credentials,
        uint8_t _status_check, bool _is_from_remote) {
    bool is_delivered(false);
    std::uint32_t its_sender_uid = std::get<0>(_credentials);
    std::uint32_t its_sender_gid = std::get<1>(_credentials);

    auto its_deserializer = get_deserializer();
    its_deserializer->set_data(_data, _size);
    std::shared_ptr<message_impl> its_message(its_deserializer->deserialize_message());
    its_deserializer->reset();
    put_deserializer(its_deserializer);

    if (its_message) {
        its_message->set_instance(_instance);
        its_message->set_reliable(_reliable);
        its_message->set_check_result(_status_check);
        its_message->set_uid(std::get<0>(_credentials));
        its_message->set_gid(std::get<1>(_credentials));

        if (!_is_from_remote) {
            if (utility::is_notification(its_message->get_message_type())) {
                if (!is_response_allowed(_bound_client, its_message->get_service(),
                        its_message->get_instance(), its_message->get_method())) {
                    VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                            << " : routing_manager_impl::deliver_message: "
                            << std::hex << " received a notification from client 0x" << _bound_client
                            << " which does not offer service/instance/event "
                            << its_message->get_service() << "/" << its_message->get_instance()
                            << "/" << its_message->get_method()
                            << " ~> Skip message!";
                    return false;
                } else {
                    if (!security::get()->is_client_allowed(own_uid_, own_gid_,
                            get_client(), its_message->get_service(),
                            its_message->get_instance(), its_message->get_method())) {
                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                << " : routing_manager_impl::deliver_message: "
                                << " isn't allowed to receive a notification from service/instance/event "
                                << its_message->get_service() << "/" << its_message->get_instance()
                                << "/" << its_message->get_method()
                                << " respectively from client 0x" << _bound_client
                                << " ~> Skip message!";
                        return false;
                    }
                }
            } else if (utility::is_request(its_message->get_message_type())) {
                if (security::get()->is_enabled()
                        && its_message->get_client() != _bound_client) {
                    VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                            << " : routing_manager_impl::deliver_message:"
                            << " received a request from client 0x" << std::setw(4) << std::setfill('0')
                            << its_message->get_client() << " to service/instance/method "
                            << its_message->get_service() << "/" << its_message->get_instance()
                            << "/" << its_message->get_method() << " which doesn't match the bound client 0x"
                            << std::setw(4) << std::setfill('0') << _bound_client
                            << " ~> Skip message!";
                    return false;
                }

                if (!security::get()->is_client_allowed(its_sender_uid, its_sender_gid,
                        its_message->get_client(), its_message->get_service(),
                        its_message->get_instance(), its_message->get_method())) {
                    VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                            << " : routing_manager_impl::deliver_message: "
                            << " isn't allowed to send a request to service/instance/method "
                            << its_message->get_service() << "/" << its_message->get_instance()
                            << "/" << its_message->get_method()
                            << " ~> Skip message!";
                    return false;
                }
            } else { // response
                if (!is_response_allowed(_bound_client, its_message->get_service(),
                        its_message->get_instance(), its_message->get_method())) {
                    VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                            << " : routing_manager_impl::deliver_message: "
                            << " received a response from client 0x" << _bound_client
                            << " which does not offer service/instance/method "
                            << its_message->get_service() << "/" << its_message->get_instance()
                            << "/" << its_message->get_method()
                            << " ~> Skip message!";
                    return false;
                } else {
                    if (!security::get()->is_client_allowed(own_uid_, own_gid_,
                            get_client(), its_message->get_service(),
                            its_message->get_instance(), its_message->get_method())) {
                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                << " : routing_manager_impl::deliver_message: "
                                << " isn't allowed to receive a response from service/instance/method "
                                << its_message->get_service() << "/" << its_message->get_instance()
                                << "/" << its_message->get_method()
                                << " respectively from client 0x" << _bound_client
                                << " ~> Skip message!";
                        return false;
                    }
                }
            }
        } else {
            if (!security::get()->is_remote_client_allowed()) {
                // if the message is from remote, check if
                // policy allows remote requests.
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                        << " : routing_manager_impl::deliver_message: "
                        << std::hex << "Remote clients are not allowed"
                        << " to communicate with service/instance/method "
                        << its_message->get_service() << "/" << its_message->get_instance()
                        << "/" << its_message->get_method()
                        << " respectively with client 0x" << get_client()
                        << " ~> Skip message!";
                return false;
            } else if (utility::is_notification(its_message->get_message_type())) {
                if (!security::get()->is_client_allowed(own_uid_, own_gid_,
                        get_client(), its_message->get_service(),
                        its_message->get_instance(), its_message->get_method())) {
                    VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                            << " : routing_manager_impl::deliver_message: "
                            << " isn't allowed to receive a notification from service/instance/event "
                            << its_message->get_service() << "/" << its_message->get_instance()
                            << "/" << its_message->get_method()
                            << " respectively from remote client"
                            << " ~> Skip message!";
                    return false;
                }
            }
        }

        host_->on_message(std::move(its_message));
        is_delivered = true;
    } else {
        VSOMEIP_ERROR << "Routing manager: deliver_message: "
                      << "SomeIP-Header deserialization failed!";
    }
    return is_delivered;
}

bool routing_manager_impl::deliver_notification(
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _length,
        bool _reliable, client_t _bound_client,
        credentials_t _credentials,
        uint8_t _status_check, bool _is_from_remote) {
    event_t its_event_id = VSOMEIP_BYTES_TO_WORD(
            _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
    client_t its_client_id = VSOMEIP_BYTES_TO_WORD(
            _data[VSOMEIP_CLIENT_POS_MIN], _data[VSOMEIP_CLIENT_POS_MAX]);

    std::shared_ptr<event> its_event = find_event(_service, _instance, its_event_id);
    if (its_event) {
        if (!its_event->is_provided()) {
            if (its_event->get_subscribers().size() == 0) {
                // no subscribers for this specific event / check subscriptions
                // to other events of the event's eventgroups
                bool cache_event = false;
                for (const auto& eg : its_event->get_eventgroups()) {
                    std::shared_ptr<eventgroupinfo> egi = find_eventgroup(_service, _instance, eg);
                    if (egi) {
                        for (const auto &e : egi->get_events()) {
                            cache_event = (e->get_subscribers().size() > 0);
                            if (cache_event) {
                                break;
                            }
                        }
                        if (cache_event) {
                            break;
                        }
                    }
                }
                if (!cache_event) {
                    return true; // as there is nothing to do
                }
            }
            const uint32_t its_length(utility::get_payload_size(_data, _length));
            if (its_length != _length - VSOMEIP_FULL_HEADER_SIZE) {
                VSOMEIP_ERROR << "Message length mismatch, dropping message!";
                return false;
            }
            std::shared_ptr<payload> its_payload
                = runtime::get()->create_payload(&_data[VSOMEIP_PAYLOAD_POS],
                                                 its_length);
            if (!its_event->set_payload_dont_notify(its_payload)) {
                // do not forward the notification as it was filtered
                return false;
            }
        }

        // incoming events statistics
        (void) insert_event_statistics(
                _service,
                _instance,
                its_event_id,
                utility::get_payload_size(_data, _length));

        if (its_event->get_type() != event_type_e::ET_SELECTIVE_EVENT) {
            for (const auto& its_local_client : its_event->get_subscribers()) {
                if (its_local_client == host_->get_client()) {
                    deliver_message(_data, _length, _instance, _reliable,
                            _bound_client, _credentials, _status_check, _is_from_remote);
                } else {
                    std::shared_ptr<endpoint> its_local_target = find_local(its_local_client);
                    if (its_local_target) {
                        send_local(its_local_target, VSOMEIP_ROUTING_CLIENT,
                                _data, _length, _instance, _reliable, VSOMEIP_SEND, _status_check);
                    }
                }
            }
        } else {
            // TODO: Check whether it makes more sense to set the client id
            // for internal selective events. This would create some extra
            // effort but we could avoid this hack.
            if (its_client_id == VSOMEIP_ROUTING_CLIENT)
                its_client_id = get_client();

            auto its_subscribers = its_event->get_subscribers();
            if (its_subscribers.find(its_client_id) != its_subscribers.end()) {
                if (its_client_id == host_->get_client()) {
                    deliver_message(_data, _length, _instance, _reliable,
                            _bound_client, _credentials, _status_check, _is_from_remote);
                } else {
                    std::shared_ptr<endpoint> its_local_target = find_local(its_client_id);
                    if (its_local_target) {
                        send_local(its_local_target, VSOMEIP_ROUTING_CLIENT,
                                _data, _length, _instance, _reliable, VSOMEIP_SEND, _status_check);
                    }
                }
            }
        }
    } else {
        VSOMEIP_WARNING << __func__ << ": Event ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_event_id << "]"
                << " is not registered. The message is dropped.";
    }

    return true;
}

std::shared_ptr<eventgroupinfo> routing_manager_impl::find_eventgroup(
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) const {
    return routing_manager_base::find_eventgroup(_service, _instance, _eventgroup);
}

std::shared_ptr<endpoint> routing_manager_impl::create_service_discovery_endpoint(
        const std::string &_address, uint16_t _port, bool _reliable) {
    std::shared_ptr<endpoint> its_service_endpoint =
            ep_mgr_impl_->find_server_endpoint(_port, _reliable);
    if (!its_service_endpoint) {
        try {
            its_service_endpoint =
                    ep_mgr_impl_->create_server_endpoint(_port,
                            _reliable, true);

            if (its_service_endpoint) {
                sd_info_ = std::make_shared<serviceinfo>(
                        VSOMEIP_SD_SERVICE, VSOMEIP_SD_INSTANCE,
                        ANY_MAJOR, ANY_MINOR, DEFAULT_TTL,
                        false); // false, because we do _not_ want to announce it...
                sd_info_->set_endpoint(its_service_endpoint, _reliable);
                its_service_endpoint->add_default_target(VSOMEIP_SD_SERVICE,
                        _address, _port);
                if (!_reliable) {
#if defined(_WIN32) || defined(ANDROID)
                    dynamic_cast<udp_server_endpoint_impl*>(
                            its_service_endpoint.get())->join(_address);
#else
                    reinterpret_cast<udp_server_endpoint_impl*>(
                            its_service_endpoint.get())->join(_address);
#endif
                }
            } else {
                VSOMEIP_ERROR<< "Service Discovery endpoint could not be created. "
                "Please check your network configuration.";
            }
        } catch (const std::exception &e) {
            VSOMEIP_ERROR << "Server endpoint creation failed: Service "
                    "Discovery endpoint could not be created: " << e.what();
        }
    }
    return its_service_endpoint;
}

services_t routing_manager_impl::get_offered_services() const {
    services_t its_services;
    for (const auto& s : get_services()) {
        for (const auto& i : s.second) {
            if (i.second->is_local()) {
                its_services[s.first][i.first] = i.second;
            }
        }
    }
    return its_services;
}

std::shared_ptr<serviceinfo> routing_manager_impl::get_offered_service(
        service_t _service, instance_t _instance) const {
    std::shared_ptr<serviceinfo> its_info;
    its_info = find_service(_service, _instance);
    if (its_info && !its_info->is_local()) {
        its_info.reset();
    }
    return its_info;
}

std::map<instance_t, std::shared_ptr<serviceinfo>>
routing_manager_impl::get_offered_service_instances(service_t _service) const {
    std::map<instance_t, std::shared_ptr<serviceinfo>> its_instances;
    const services_t its_services(get_services());
    const auto found_service = its_services.find(_service);
    if (found_service != its_services.end()) {
        for (const auto& i : found_service->second) {
            if (i.second->is_local()) {
                its_instances[i.first] = i.second;
            }
        }
    }
    return its_instances;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////
void routing_manager_impl::init_service_info(
        service_t _service, instance_t _instance, bool _is_local_service) {
    std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
    if (!its_info) {
        VSOMEIP_ERROR << "routing_manager_impl::init_service_info: couldn't "
                "find serviceinfo for service: ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "]"
                << " is_local_service=" << _is_local_service;
        return;
    }
    if (configuration_) {
        // Create server endpoints for local services only
        if (_is_local_service) {
            const bool is_someip = configuration_->is_someip(_service, _instance);
            uint16_t its_reliable_port = configuration_->get_reliable_port(
                    _service, _instance);
            bool _is_found(false);
            if (ILLEGAL_PORT != its_reliable_port) {
                std::shared_ptr<endpoint> its_reliable_endpoint  =
                        ep_mgr_impl_->find_or_create_server_endpoint(
                                its_reliable_port, true, is_someip, _service,
                                _instance, _is_found);
                if (its_reliable_endpoint) {
                    its_info->set_endpoint(its_reliable_endpoint, true);
                }
            }
            uint16_t its_unreliable_port = configuration_->get_unreliable_port(
                    _service, _instance);
            if (ILLEGAL_PORT != its_unreliable_port) {
                std::shared_ptr<endpoint> its_unreliable_endpoint =
                        ep_mgr_impl_->find_or_create_server_endpoint(
                                its_unreliable_port, false, is_someip, _service,
                                _instance, _is_found);
                if (its_unreliable_endpoint) {
                    its_info->set_endpoint(its_unreliable_endpoint, false);
                }
            }

            if (ILLEGAL_PORT == its_reliable_port
                   && ILLEGAL_PORT == its_unreliable_port) {
                   VSOMEIP_INFO << "Port configuration missing for ["
                           << std::hex << _service << "." << _instance
                           << "]. Service is internal.";
            }
        }
    } else {
        VSOMEIP_ERROR << "Missing vsomeip configuration.";
    }
}

void routing_manager_impl::remove_local(client_t _client, bool _remove_uid) {
    auto clients_subscriptions = get_subscriptions(_client);
    {
        std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
        for (const auto& s : clients_subscriptions) {
            remote_subscription_state_.erase(std::tuple_cat(s, std::make_tuple(_client)));
        }
    }
    routing_manager_base::remove_local(_client, clients_subscriptions, _remove_uid);

    std::forward_list<std::pair<service_t, instance_t>> services_to_release_;
    {
        std::lock_guard<std::mutex> its_lock(requested_services_mutex_);
        auto its_client = requested_services_.find(_client);
        if (its_client != requested_services_.end()) {
            for (const auto& its_service : its_client->second) {
                for (const auto& its_instance : its_service.second) {
                    services_to_release_.push_front(
                        { its_service.first, its_instance.first });
                }
            }
        }
    }
    for (const auto &s : services_to_release_) {
        release_service(_client, s.first, s.second);
    }
}

bool routing_manager_impl::is_field(service_t _service, instance_t _instance,
        event_t _event) const {
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    auto find_service = events_.find(_service);
    if (find_service != events_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
            auto find_event = find_instance->second.find(_event);
            if (find_event != find_instance->second.end())
                return find_event->second->is_field();
        }
    }
    return false;
}

void routing_manager_impl::add_routing_info(
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor, ttl_t _ttl,
        const boost::asio::ip::address &_reliable_address,
        uint16_t _reliable_port,
        const boost::asio::ip::address &_unreliable_address,
        uint16_t _unreliable_port) {

    // Create/Update service info
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (!its_info) {
        boost::asio::ip::address its_unicast_address
            = configuration_->get_unicast_address();
        bool is_local(false);
        if (_reliable_port != ILLEGAL_PORT
                && its_unicast_address == _reliable_address)
            is_local = true;
        else if (_unreliable_port != ILLEGAL_PORT
                && its_unicast_address == _unreliable_address)
            is_local = true;

        its_info = create_service_info(_service, _instance, _major, _minor, _ttl, is_local);
        init_service_info(_service, _instance, is_local);
    } else if (its_info->is_local()) {
        // We received a service info for a service which is already offered locally
        VSOMEIP_ERROR << "routing_manager_impl::add_routing_info: "
            << "rejecting routing info. Remote: "
            << ((_reliable_port != ILLEGAL_PORT) ? _reliable_address.to_string()
                    : _unreliable_address.to_string()) << " is trying to offer ["
            << std::hex << std::setfill('0') << std::setw(4) << _service << "."
            << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
            << std::dec << static_cast<std::uint32_t>(_major) << "."
            << std::dec << _minor
            << "] on port " << ((_reliable_port != ILLEGAL_PORT) ? _reliable_port
                    : _unreliable_port) << " offered previously on this node: ["
            << std::hex << std::setfill('0') << std::setw(4) << _service << "."
            << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
            << std::dec << static_cast<std::uint32_t>(its_info->get_major())
            << "." << its_info->get_minor() << "]";
        return;
    } else {
        its_info->set_ttl(_ttl);
    }

    // Check whether remote services are unchanged
    bool is_reliable_known(false);
    bool is_unreliable_known(false);
    ep_mgr_impl_->is_remote_service_known(_service, _instance, _major,
            _minor, _reliable_address, _reliable_port, &is_reliable_known,
            _unreliable_address, _unreliable_port, &is_unreliable_known);

    bool udp_inserted(false);
    bool tcp_inserted(false);
    // Add endpoint(s) if necessary
    if (_reliable_port != ILLEGAL_PORT && !is_reliable_known) {
        std::shared_ptr<endpoint_definition> endpoint_def_tcp
            = endpoint_definition::get(_reliable_address, _reliable_port, true, _service, _instance);
        if (_unreliable_port != ILLEGAL_PORT && !is_unreliable_known) {
            std::shared_ptr<endpoint_definition> endpoint_def_udp
                = endpoint_definition::get(_unreliable_address, _unreliable_port, false, _service, _instance);
            ep_mgr_impl_->add_remote_service_info(_service, _instance,
                    endpoint_def_tcp, endpoint_def_udp);
            udp_inserted = true;
            tcp_inserted = true;
        } else {
            ep_mgr_impl_->add_remote_service_info(_service, _instance,
                    endpoint_def_tcp);
            tcp_inserted = true;
        }

        // check if service was requested and establish TCP connection if necessary
        {
            bool connected(false);
            std::lock_guard<std::mutex> its_lock(requested_services_mutex_);
            for(const auto &client_id : requested_services_) {
                auto found_service = client_id.second.find(_service);
                if (found_service != client_id.second.end()) {
                    auto found_instance = found_service->second.find(_instance);
                    if (found_instance != found_service->second.end()) {
                        for (const auto &major_minor_pair : found_instance->second) {
                            if ((major_minor_pair.first == _major
                                    || _major == DEFAULT_MAJOR
                                    || major_minor_pair.first == ANY_MAJOR)
                                    && (major_minor_pair.second <= _minor
                                            || _minor == DEFAULT_MINOR
                                            || major_minor_pair.second == ANY_MINOR)) {
                                // SWS_SD_00376 establish TCP connection to service
                                // service is marked as available later in on_connect()
                                if(!connected) {
                                    if (udp_inserted) {
                                        // atomically create reliable and unreliable endpoint
                                        ep_mgr_impl_->find_or_create_remote_client(
                                                _service, _instance);
                                    } else {
                                        ep_mgr_impl_->find_or_create_remote_client(
                                                _service, _instance, true);
                                    }
                                    connected = true;
                                }
                                its_info->add_client(client_id.first);
                                break;
                            }
                        }
                    }
                }
            }
        }
    } else if (_reliable_port != ILLEGAL_PORT && is_reliable_known) {
        std::lock_guard<std::mutex> its_lock(requested_services_mutex_);
        for(const auto &client_id : requested_services_) {
            auto found_service = client_id.second.find(_service);
            if (found_service != client_id.second.end()) {
                auto found_instance = found_service->second.find(_instance);
                if (found_instance != found_service->second.end()) {
                    for (const auto &major_minor_pair : found_instance->second) {
                        if ((major_minor_pair.first == _major
                                || _major == DEFAULT_MAJOR
                                || major_minor_pair.first == ANY_MAJOR)
                                && (major_minor_pair.second <= _minor
                                        || _minor == DEFAULT_MINOR
                                        || major_minor_pair.second == ANY_MINOR)) {
                            std::shared_ptr<endpoint> ep = its_info->get_endpoint(true);
                            if (ep && ep->is_established() &&
                                    !stub_->contained_in_routing_info(
                                    VSOMEIP_ROUTING_CLIENT, _service, _instance,
                                    its_info->get_major(),
                                    its_info->get_minor())) {
                                on_availability(_service, _instance,
                                        true, its_info->get_major(), its_info->get_minor());
                                stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT,
                                        _service, _instance,
                                        its_info->get_major(),
                                        its_info->get_minor());
                                if (discovery_) {
                                    discovery_->on_endpoint_connected(
                                            _service, _instance, ep);
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    if (_unreliable_port != ILLEGAL_PORT && !is_unreliable_known) {
        if (!udp_inserted) {
            std::shared_ptr<endpoint_definition> endpoint_def
                = endpoint_definition::get(_unreliable_address, _unreliable_port, false, _service, _instance);
            ep_mgr_impl_->add_remote_service_info(_service, _instance, endpoint_def);
            // check if service was requested and increase requester count if necessary
            {
                bool connected(false);
                std::lock_guard<std::mutex> its_lock(requested_services_mutex_);
                for (const auto &client_id : requested_services_) {
                    const auto found_service = client_id.second.find(_service);
                    if (found_service != client_id.second.end()) {
                        const auto found_instance = found_service->second.find(
                                _instance);
                        if (found_instance != found_service->second.end()) {
                            for (const auto &major_minor_pair : found_instance->second) {
                                if ((major_minor_pair.first == _major
                                        || _major == DEFAULT_MAJOR
                                        || major_minor_pair.first == ANY_MAJOR)
                                        && (major_minor_pair.second <= _minor
                                                || _minor == DEFAULT_MINOR
                                                || major_minor_pair.second
                                                        == ANY_MINOR)) {
                                    if(!connected) {
                                        ep_mgr_impl_->find_or_create_remote_client(_service, _instance,
                                                false);
                                        connected = true;
                                    }
                                    its_info->add_client(client_id.first);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (!is_reliable_known && !tcp_inserted) {
            // UDP only service can be marked as available instantly
            on_availability(_service, _instance, true, _major, _minor);
            stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, _major, _minor);
        }
        if (discovery_) {
            std::shared_ptr<endpoint> ep = its_info->get_endpoint(false);
            if (ep && ep->is_established()) {
                discovery_->on_endpoint_connected(_service, _instance, ep);
            }
        }
    } else if (_unreliable_port != ILLEGAL_PORT && is_unreliable_known) {
        std::lock_guard<std::mutex> its_lock(requested_services_mutex_);
        for(const auto &client_id : requested_services_) {
            auto found_service = client_id.second.find(_service);
            if (found_service != client_id.second.end()) {
                auto found_instance = found_service->second.find(_instance);
                if (found_instance != found_service->second.end()) {
                    for (const auto &major_minor_pair : found_instance->second) {
                        if ((major_minor_pair.first == _major
                                || _major == DEFAULT_MAJOR
                                || major_minor_pair.first == ANY_MAJOR)
                                && (major_minor_pair.second <= _minor
                                        || _minor == DEFAULT_MINOR
                                        || major_minor_pair.second == ANY_MINOR)) {
                            if (!stub_->contained_in_routing_info(
                                    VSOMEIP_ROUTING_CLIENT, _service, _instance,
                                    its_info->get_major(),
                                    its_info->get_minor())) {
                                on_availability(_service, _instance,
                                        true, its_info->get_major(), its_info->get_minor());
                                stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT,
                                        _service, _instance,
                                        its_info->get_major(),
                                        its_info->get_minor());
                                if (discovery_) {
                                    std::shared_ptr<endpoint> ep = its_info->get_endpoint(false);
                                    if (ep && ep->is_established()) {
                                        discovery_->on_endpoint_connected(
                                                _service, _instance,
                                                ep);
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

void routing_manager_impl::del_routing_info(service_t _service, instance_t _instance,
        bool _has_reliable, bool _has_unreliable) {

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if(!its_info)
        return;

    on_availability(_service, _instance, false,
            its_info->get_major(), its_info->get_minor());
    stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance,
            its_info->get_major(), its_info->get_minor());
    // Implicit unsubscribe

    std::vector<std::shared_ptr<event>> its_events;
    {
        std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
        auto found_service = eventgroups_.find(_service);
        if (found_service != eventgroups_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                for (auto &its_eventgroup : found_instance->second) {
                    // As the service is gone, all subscriptions to its events
                    // do no longer exist and the last received payload is no
                    // longer valid.
                    for (auto &its_event : its_eventgroup.second->get_events()) {
                        const auto its_subscribers = its_event->get_subscribers();
                        for (const auto& its_subscriber : its_subscribers) {
                            if (its_subscriber != get_client()) {
                                its_event->remove_subscriber(
                                        its_eventgroup.first, its_subscriber);
                            }
                        }
                        its_events.push_back(its_event);
                        remove_pending_subscription(_service, _instance,
                                its_eventgroup.first, its_event->get_event());
                    }

                }
            }
        }
    }
    for (const auto& e : its_events) {
        e->unset_payload(true);
    }

    {
        std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
        std::set<std::tuple<
            service_t, instance_t, eventgroup_t, client_t> > its_invalid;

        for (const auto& its_state : remote_subscription_state_) {
            if (std::get<0>(its_state.first) == _service
                    && std::get<1>(its_state.first) == _instance) {
                its_invalid.insert(its_state.first);
            }
        }

        for (const auto& its_key : its_invalid)
            remote_subscription_state_.erase(its_key);
    }

    {
        std::lock_guard<std::mutex> its_lock(remote_subscribers_mutex_);
        auto found_service = remote_subscribers_.find(_service);
        if (found_service != remote_subscribers_.end()) {
            if (found_service->second.erase(_instance) > 0 &&
                    !found_service->second.size()) {
                remote_subscribers_.erase(found_service);
            }
        }
    }

    if (_has_reliable) {
        ep_mgr_impl_->clear_client_endpoints(_service, _instance, true);
        ep_mgr_impl_->clear_remote_service_info(_service, _instance, true);
    }
    if (_has_unreliable) {
        ep_mgr_impl_->clear_client_endpoints(_service, _instance, false);
        ep_mgr_impl_->clear_remote_service_info(_service, _instance, false);
    }

    ep_mgr_impl_->clear_multicast_endpoints(_service, _instance);

    if (_has_reliable)
        clear_service_info(_service, _instance, true);
    if (_has_unreliable)
        clear_service_info(_service, _instance, false);

    // For expired services using only unreliable endpoints that have never been created before
    if (!_has_reliable && !_has_unreliable) {
        ep_mgr_impl_->clear_remote_service_info(_service, _instance, true);
        ep_mgr_impl_->clear_remote_service_info(_service, _instance, false);
        clear_service_info(_service, _instance, true);
        clear_service_info(_service, _instance, false);
    }
}

void routing_manager_impl::update_routing_info(std::chrono::milliseconds _elapsed) {
    std::map<service_t, std::vector<instance_t> > its_expired_offers;

    {
        std::lock_guard<std::mutex> its_lock(services_remote_mutex_);
        for (const auto &s : services_remote_) {
            for (const auto &i : s.second) {
                ttl_t its_ttl = i.second->get_ttl();
                if (its_ttl < DEFAULT_TTL) { // do not touch "forever"
                    std::chrono::milliseconds precise_ttl = i.second->get_precise_ttl();
                    if (precise_ttl.count() < _elapsed.count() || precise_ttl.count() == 0) {
                        i.second->set_ttl(0);
                        its_expired_offers[s.first].push_back(i.first);
                    } else {
                        std::chrono::milliseconds its_new_ttl(precise_ttl - _elapsed);
                        i.second->set_precise_ttl(its_new_ttl);
                    }
                }
            }
        }
    }

    for (const auto &s : its_expired_offers) {
        for (const auto &i : s.second) {
            if (discovery_) {
                discovery_->unsubscribe_all(s.first, i);
            }
            del_routing_info(s.first, i, true, true);
            VSOMEIP_INFO << "update_routing_info: elapsed=" << _elapsed.count()
                    << " : delete service/instance "
                    << std::hex << std::setw(4) << std::setfill('0') << s.first
                    << "." << std::hex << std::setw(4) << std::setfill('0') << i;
        }
    }
}

void routing_manager_impl::expire_services(
        const boost::asio::ip::address &_address) {
    expire_services(_address, configuration::port_range_t(ANY_PORT, ANY_PORT),
            false);
}

void routing_manager_impl::expire_services(
        const boost::asio::ip::address &_address, std::uint16_t _port,
        bool _reliable) {
    expire_services(_address, configuration::port_range_t(_port, _port),
            _reliable);
}

void routing_manager_impl::expire_services(
        const boost::asio::ip::address &_address,
        const configuration::port_range_t& _range, bool _reliable) {
    std::map<service_t, std::vector<instance_t> > its_expired_offers;

    const bool expire_all = (_range.first == ANY_PORT
            && _range.second == ANY_PORT);

    for (auto &s : get_services_remote()) {
        for (auto &i : s.second) {
            boost::asio::ip::address its_address;
            std::shared_ptr<client_endpoint> its_client_endpoint =
                    std::dynamic_pointer_cast<client_endpoint>(
                            i.second->get_endpoint(_reliable));
            if (!its_client_endpoint && expire_all) {
                its_client_endpoint = std::dynamic_pointer_cast<client_endpoint>(
                                i.second->get_endpoint(!_reliable));
            }
            if (its_client_endpoint) {
                if ((expire_all || (its_client_endpoint->get_remote_port() >= _range.first
                                    && its_client_endpoint->get_remote_port() <= _range.second))
                        && its_client_endpoint->get_remote_address(its_address)
                        && its_address == _address) {
                    if (discovery_) {
                        discovery_->unsubscribe_all(s.first, i.first);
                    }
                    its_expired_offers[s.first].push_back(i.first);
                }
            }
        }
    }

    for (auto &s : its_expired_offers) {
        for (auto &i : s.second) {
            VSOMEIP_INFO << "expire_services for address: " << _address
                    << " : delete service/instance "
                    << std::hex << std::setw(4) << std::setfill('0') << s.first
                    << "." << std::hex << std::setw(4) << std::setfill('0') << i
                    << " port [" << std::dec << _range.first << "," << _range.second
                    << "] reliability=" << std::boolalpha << _reliable;
            del_routing_info(s.first, i, true, true);
        }
    }
}

void
routing_manager_impl::expire_subscriptions(
        const boost::asio::ip::address &_address) {
    expire_subscriptions(_address,
            configuration::port_range_t(ANY_PORT, ANY_PORT), false);
}

void
routing_manager_impl::expire_subscriptions(
        const boost::asio::ip::address &_address, std::uint16_t _port,
        bool _reliable) {
    expire_subscriptions(_address, configuration::port_range_t(_port, _port),
            _reliable);
}

void
routing_manager_impl::expire_subscriptions(
        const boost::asio::ip::address &_address,
        const configuration::port_range_t& _range, bool _reliable) {
    const bool expire_all = (_range.first == ANY_PORT
            && _range.second == ANY_PORT);

    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (const auto &its_service : eventgroups_) {
        for (const auto &its_instance : its_service.second) {
            for (const auto &its_eventgroup : its_instance.second) {
                const auto its_info = its_eventgroup.second;
                for (auto its_subscription
                        : its_info->get_remote_subscriptions()) {
                    // Note: get_remote_subscription delivers a copied
                    // set of subscriptions. Thus, its is possible to
                    // to remove them within the loop.
                    auto its_ep_definition = (_reliable) ?
                                    its_subscription->get_reliable() :
                                    its_subscription->get_unreliable();

                    if (!its_ep_definition && expire_all)
                        its_ep_definition = (!_reliable) ?
                                its_subscription->get_reliable() :
                                its_subscription->get_unreliable();

                    if (its_ep_definition
                            && its_ep_definition->get_address() == _address
                            && (expire_all ||
                                    (its_ep_definition->get_remote_port() >= _range.first
                                    && its_ep_definition->get_remote_port() <= _range.second))) {

                        // TODO: Check whether subscriptions to different hosts are valid.
                        // IF yes, we probably need to simply reset the corresponding
                        // endpoint instead of removing the subscription...
                        VSOMEIP_INFO << __func__
                                << ": removing subscription to "
                                << std::hex << its_info->get_service() << "."
                                << std::hex << its_info->get_instance() << "."
                                << std::hex << its_info->get_eventgroup()
                                << " from target "
                                << its_ep_definition->get_address() << ":"
                                << std::dec << its_ep_definition->get_port()
                                << " reliable="
                                << std::boolalpha << its_ep_definition->is_reliable();
                        if (expire_all) {
                            its_ep_definition = (!its_ep_definition->is_reliable()) ?
                                    its_subscription->get_reliable() :
                                    its_subscription->get_unreliable();
                            if (its_ep_definition) {
                                VSOMEIP_INFO << __func__
                                        << ": removing subscription to "
                                        << std::hex << its_info->get_service() << "."
                                        << std::hex << its_info->get_instance() << "."
                                        << std::hex << its_info->get_eventgroup()
                                        << " from target "
                                        << its_ep_definition->get_address() << ":"
                                        << std::dec << its_ep_definition->get_port()
                                        << " reliable="
                                        << std::boolalpha << its_ep_definition->is_reliable();
                            }
                        }
                        on_remote_unsubscribe(its_subscription);
                    }
                }
            }
        }
    }
}

void routing_manager_impl::init_routing_info() {
    VSOMEIP_INFO<< "Service Discovery disabled. Using static routing information.";
    for (auto i : configuration_->get_remote_services()) {
        boost::asio::ip::address its_address(
                boost::asio::ip::address::from_string(
                    configuration_->get_unicast_address(i.first, i.second)));
        uint16_t its_reliable_port
            = configuration_->get_reliable_port(i.first, i.second);
        uint16_t its_unreliable_port
            = configuration_->get_unreliable_port(i.first, i.second);
        major_version_t its_major
            = configuration_->get_major_version(i.first, i.second);
        minor_version_t its_minor
            = configuration_->get_minor_version(i.first, i.second);
        ttl_t its_ttl
            = configuration_->get_ttl(i.first, i.second);

        if (its_reliable_port != ILLEGAL_PORT
                || its_unreliable_port != ILLEGAL_PORT) {

            VSOMEIP_INFO << "Adding static remote service ["
                << std::hex << std::setw(4) << std::setfill('0')
                << i.first << "." << i.second
                << std::dec << ":" <<  +its_major  << "." << its_minor
                << "]";

            add_routing_info(i.first, i.second,
                    its_major, its_minor, its_ttl,
                    its_address, its_reliable_port,
                    its_address, its_unreliable_port);

            if(its_reliable_port != ILLEGAL_PORT) {
                ep_mgr_impl_->find_or_create_remote_client(
                        i.first, i.second, true);
            }
            if(its_unreliable_port != ILLEGAL_PORT) {
                ep_mgr_impl_->find_or_create_remote_client(
                        i.first, i.second, false);
            }
        }
    }
}

void routing_manager_impl::on_remote_subscribe(
        std::shared_ptr<remote_subscription> &_subscription,
        const remote_subscription_callback_t &_callback) {
    auto its_eventgroupinfo = _subscription->get_eventgroupinfo();
    if (!its_eventgroupinfo) {
        VSOMEIP_ERROR << __func__ << " eventgroupinfo is invalid";
        return;
    }
    const ttl_t its_ttl = _subscription->get_ttl();

    const auto its_service = its_eventgroupinfo->get_service();
    const auto its_instance = its_eventgroupinfo->get_instance();
    const auto its_eventgroup = its_eventgroupinfo->get_eventgroup();
    const auto its_major = its_eventgroupinfo->get_major();

    // Get remote port(s)
    auto its_reliable = _subscription->get_reliable();
    if (its_reliable) {
        uint16_t its_port
            = configuration_->get_reliable_port(its_service, its_instance);
        its_reliable->set_remote_port(its_port);
    }

    auto its_unreliable = _subscription->get_unreliable();
    if (its_unreliable) {
        uint16_t its_port
            = configuration_->get_unreliable_port(its_service, its_instance);
        its_unreliable->set_remote_port(its_port);
    }

    // Calculate expiration time
    const std::chrono::steady_clock::time_point its_expiration
        = std::chrono::steady_clock::now() + std::chrono::seconds(its_ttl);

    // Try to update the subscription. This will fail, if the subscription does
    // not exist or is still (partly) pending.
    remote_subscription_id_t its_id;
    std::set<client_t> its_added;
    auto its_result = its_eventgroupinfo->update_remote_subscription(
            _subscription, its_expiration, its_added, its_id, true);
    if (its_result) {
        if (!_subscription->is_pending()) { // resubscription without change
            _callback(_subscription);
        } else if (!its_added.empty()) { // new clients for a selective subscription
            const client_t its_offering_client
                = find_local_client(its_service, its_instance);
            send_subscription(its_offering_client,
                    its_service, its_instance, its_eventgroup, its_major,
                    its_added, _subscription->get_id());
        } else { // identical subscription is not yet processed
            std::stringstream its_warning;
            its_warning << __func__ << " a remote subscription is already pending ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]"
                << " from ";
            if (its_reliable && its_unreliable)
                its_warning << "[";
            if (its_reliable)
                its_warning << its_reliable->get_address().to_string()
                    << ":" << std::dec << its_reliable->get_port();
            if (its_reliable && its_unreliable)
                its_warning << ", ";
            if (its_unreliable)
                its_warning << its_unreliable->get_address().to_string()
                    << ":" << std::dec << its_unreliable->get_port();
            if (its_reliable && its_unreliable)
                its_warning << "]";
            VSOMEIP_WARNING << its_warning.str();
            _callback(_subscription);
        }
    } else { // new subscription
        auto its_id
            = its_eventgroupinfo->add_remote_subscription(_subscription);

        const client_t its_offering_client
            = find_local_client(its_service, its_instance);
        send_subscription(its_offering_client,
                its_service, its_instance, its_eventgroup, its_major,
                _subscription->get_clients(), its_id);
    }
}

void routing_manager_impl::on_remote_unsubscribe(
        std::shared_ptr<remote_subscription> &_subscription) {
    std::shared_ptr<eventgroupinfo> its_info
        = _subscription->get_eventgroupinfo();
    if (!its_info) {
        VSOMEIP_ERROR << __func__
                << ": Received Unsubscribe for unregistered eventgroup.";
        return;
    }

    const auto its_service = its_info->get_service();
    const auto its_instance = its_info->get_instance();
    const auto its_eventgroup = its_info->get_eventgroup();
    const auto its_major = its_info->get_major();

    // Get remote port(s)
    auto its_reliable = _subscription->get_reliable();
    if (its_reliable) {
        uint16_t its_port
            = configuration_->get_reliable_port(its_service, its_instance);
        its_reliable->set_remote_port(its_port);
    }

    auto its_unreliable = _subscription->get_unreliable();
    if (its_unreliable) {
        uint16_t its_port
            = configuration_->get_unreliable_port(its_service, its_instance);
        its_unreliable->set_remote_port(its_port);
    }

    remote_subscription_id_t its_id(0);
    std::set<client_t> its_removed;
    auto its_result = its_info->update_remote_subscription(
            _subscription, std::chrono::steady_clock::now(),
            its_removed, its_id, false);

    if (its_result) {
        const client_t its_offering_client
            = find_local_client(its_service, its_instance);
        send_unsubscription(its_offering_client,
                its_service, its_instance, its_eventgroup, its_major,
                its_removed, its_id);
    }
}

void routing_manager_impl::on_subscribe_ack_with_multicast(
        service_t _service, instance_t _instance,
        const boost::asio::ip::address &_sender,
        const boost::asio::ip::address &_address, uint16_t _port) {
    ep_mgr_impl_->find_or_create_multicast_endpoint(_service,
            _instance, _sender, _address, _port);
}

void routing_manager_impl::on_subscribe_ack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        event_t _event, remote_subscription_id_t _id) {
    std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        auto its_subscription = its_eventgroup->get_remote_subscription(_id);
        if (its_subscription) {
            its_subscription->set_client_state(_client,
                    remote_subscription_state_e::SUBSCRIPTION_ACKED);

            auto its_parent = its_subscription->get_parent();
            if (its_parent) {
                its_parent->set_client_state(_client,
                        remote_subscription_state_e::SUBSCRIPTION_ACKED);
                if (!its_subscription->is_pending()) {
                    its_eventgroup->remove_remote_subscription(_id);
                }
            }

            if (discovery_) {
                std::lock_guard<std::mutex> its_lock(remote_subscribers_mutex_);
                remote_subscribers_[_service][_instance][VSOMEIP_ROUTING_CLIENT].insert(
                        its_subscription->get_subscriber());
                discovery_->update_remote_subscription(its_subscription);

                VSOMEIP_INFO << "REMOTE SUBSCRIBE("
                    << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
                    << " from " << its_subscription->get_subscriber()->get_address()
                    << ":" << std::dec << its_subscription->get_subscriber()->get_port()
                    << (its_subscription->get_subscriber()->is_reliable() ? " reliable" : " unreliable")
                    << " was accepted";

                return;
            }
        } else {
            const auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, _client);
            const auto its_state = remote_subscription_state_.find(its_tuple);
            if (its_state != remote_subscription_state_.end()) {
                if (its_state->second == subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED) {
                    // Already notified!
                    return;
                }
            }
            remote_subscription_state_[its_tuple] = subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED;
        }

        std::set<client_t> subscribed_clients;
        if (_client == VSOMEIP_ROUTING_CLIENT) {
            for (const auto &its_event : its_eventgroup->get_events()) {
                if (_event == ANY_EVENT || _event == its_event->get_event()) {
                    const auto &its_subscribers = its_event->get_subscribers();
                    subscribed_clients.insert(its_subscribers.begin(), its_subscribers.end());
                }
            }
        } else {
            subscribed_clients.insert(_client);
        }

        for (const auto &its_subscriber : subscribed_clients) {
            if (its_subscriber == get_client()) {
                if (_event == ANY_EVENT) {
                    for (const auto &its_event : its_eventgroup->get_events()) {
                        host_->on_subscription_status(_service, _instance,
                                _eventgroup, its_event->get_event(),
                                0x0 /*OK*/);
                    }
                } else {
                    host_->on_subscription_status(_service, _instance,
                            _eventgroup, _event, 0x0 /*OK*/);
                }
            } else {
                stub_->send_subscribe_ack(its_subscriber, _service,
                        _instance, _eventgroup, _event);
            }
        }
     }
}

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_remote_client(
        service_t _service, instance_t _instance, bool _reliable) {
    return ep_mgr_impl_->find_or_create_remote_client(_service,
            _instance, _reliable);
}

void routing_manager_impl::on_subscribe_nack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        event_t _event, remote_subscription_id_t _id) {
    (void)_event; // TODO: Remove completely?

    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        auto its_subscription = its_eventgroup->get_remote_subscription(_id);
        if (its_subscription) {
            its_subscription->set_client_state(_client,
                    remote_subscription_state_e::SUBSCRIPTION_NACKED);

            auto its_parent = its_subscription->get_parent();
            if (its_parent) {
                its_parent->set_client_state(_client,
                        remote_subscription_state_e::SUBSCRIPTION_NACKED);
                if (!its_subscription->is_pending()) {
                    its_eventgroup->remove_remote_subscription(_id);
                }
            }

            if (discovery_) {
                discovery_->update_remote_subscription(its_subscription);
                VSOMEIP_INFO << "REMOTE SUBSCRIBE("
                    << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
                    << " from " << its_subscription->get_subscriber()->get_address()
                    << ":" << std::dec << its_subscription->get_subscriber()->get_port()
                    << (its_subscription->get_subscriber()->is_reliable() ? " reliable" : " unreliable")
                    << " was not accepted";
            }
        }
    }
}

return_code_e routing_manager_impl::check_error(const byte_t *_data, length_t _size,
        instance_t _instance) {

    service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
            _data[VSOMEIP_SERVICE_POS_MAX]);

    if (_size >= VSOMEIP_PAYLOAD_POS) {
        if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])
                || utility::is_request_no_return(_data[VSOMEIP_MESSAGE_TYPE_POS]) ) {
            if (_data[VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION) {
                VSOMEIP_WARNING << "Received a message with unsupported protocol version for service 0x"
                        << std::hex << its_service;
                return return_code_e::E_WRONG_PROTOCOL_VERSION;
            }
            if (_instance == 0xFFFF) {
                VSOMEIP_WARNING << "Receiving endpoint is not configured for service 0x"
                        << std::hex << its_service;
                return return_code_e::E_UNKNOWN_SERVICE;
            }
            // Check interface version of service/instance
            auto its_info = find_service(its_service, _instance);
            if (its_info) {
                major_version_t its_version = _data[VSOMEIP_INTERFACE_VERSION_POS];
                if (its_version != its_info->get_major()) {
                    VSOMEIP_WARNING << "Received a message with unsupported interface version for service 0x"
                            << std::hex << its_service;
                    return return_code_e::E_WRONG_INTERFACE_VERSION;
                }
            }
            if (_data[VSOMEIP_RETURN_CODE_POS] != static_cast<byte_t> (return_code_e::E_OK)) {
                // Request calls must to have return code E_OK set!
                VSOMEIP_WARNING << "Received a message with unsupported return code set for service 0x"
                        << std::hex << its_service;
                return return_code_e::E_NOT_OK;
            }
        }
    } else {
        // Message shorter than vSomeIP message header
        VSOMEIP_WARNING << "Received a message message which is shorter than vSomeIP message header!";
        return return_code_e::E_MALFORMED_MESSAGE;
    }
    return return_code_e::E_OK;
}

void routing_manager_impl::send_error(return_code_e _return_code,
        const byte_t *_data, length_t _size,
        instance_t _instance, bool _reliable,
        endpoint* const _receiver,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {

    client_t its_client = 0;
    service_t its_service = 0;
    method_t its_method = 0;
    session_t its_session = 0;
    major_version_t its_version = 0;

    if (_size >= VSOMEIP_CLIENT_POS_MAX) {
        its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN],
                _data[VSOMEIP_CLIENT_POS_MAX]);
    }
    if (_size >= VSOMEIP_SERVICE_POS_MAX) {
        its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
    }
    if (_size >= VSOMEIP_METHOD_POS_MAX) {
        its_method = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
    }
    if (_size >= VSOMEIP_SESSION_POS_MAX) {
        its_session = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SESSION_POS_MIN],
                _data[VSOMEIP_SESSION_POS_MAX]);
    }
    if( _size >= VSOMEIP_INTERFACE_VERSION_POS) {
        its_version = _data[VSOMEIP_INTERFACE_VERSION_POS];
    }

    auto error_message = runtime::get()->create_message(_reliable);
    error_message->set_client(its_client);
    error_message->set_instance(_instance);
    error_message->set_interface_version(its_version);
    error_message->set_message_type(message_type_e::MT_ERROR);
    error_message->set_method(its_method);
    error_message->set_return_code(_return_code);
    error_message->set_service(its_service);
    error_message->set_session(its_session);
    {
        std::shared_ptr<serializer> its_serializer(get_serializer());
        if (its_serializer->serialize(error_message.get())) {
            if (_receiver) {
                auto its_endpoint_def = std::make_shared<endpoint_definition>(
                        _remote_address, _remote_port,
                        _receiver->is_reliable());
                its_endpoint_def->set_remote_port(_receiver->get_local_port());
                std::shared_ptr<endpoint> its_endpoint =
                        ep_mgr_impl_->find_server_endpoint(
                                its_endpoint_def->get_remote_port(),
                                its_endpoint_def->is_reliable());
                if (its_endpoint) {
                    #ifdef USE_DLT
                        const uint16_t its_data_size
                            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                        trace::header its_header;
                        if (its_header.prepare(its_endpoint, true, _instance))
                            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                    _data, its_data_size);
                    #else
                        (void) _instance;
                    #endif
                    its_endpoint->send_error(its_endpoint_def,
                            its_serializer->get_data(), its_serializer->get_size());
                }
            }
            its_serializer->reset();
            put_serializer(its_serializer);
        } else {
            VSOMEIP_ERROR<< "Failed to serialize error message.";
        }
    }
}

void routing_manager_impl::clear_remote_subscriber(
        service_t _service, instance_t _instance, client_t _client,
        const std::shared_ptr<endpoint_definition> &_target) {
    std::lock_guard<std::mutex> its_lock(remote_subscribers_mutex_);
    auto its_service = remote_subscribers_.find(_service);
    if (its_service != remote_subscribers_.end()) {
        auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            auto its_client = its_instance->second.find(_client);
            if (its_client != its_instance->second.end()) {
                if (its_client->second.erase(_target)) {
                    if (!its_client->second.size()) {
                        its_instance->second.erase(_client);
                    }
                }
            }
        }
    }
}

std::chrono::steady_clock::time_point
routing_manager_impl::expire_subscriptions(bool _force) {
    std::map<std::shared_ptr<remote_subscription>,
        std::set<client_t> > its_expired_subscriptions;

    std::chrono::steady_clock::time_point now
        = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point its_next_expiration
        = std::chrono::steady_clock::now() + std::chrono::hours(24);
    {
        std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);

        for (auto &its_service : eventgroups_) {
            for (auto &its_instance : its_service.second) {
                for (auto &its_eventgroup : its_instance.second) {
                    auto its_subscriptions
                        = its_eventgroup.second->get_remote_subscriptions();
                    for (auto &s : its_subscriptions) {
                        for (auto its_client : s->get_clients()) {
                            if (_force) {
                                its_expired_subscriptions[s].insert(its_client);
                            } else {
                                auto its_expiration = s->get_expiration(its_client);
                                if (its_expiration != std::chrono::steady_clock::time_point()) {
                                    if (its_expiration < now) {
                                        its_expired_subscriptions[s].insert(its_client);
                                    } else if (its_expiration < its_next_expiration) {
                                        its_next_expiration = its_expiration;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto &s : its_expired_subscriptions) {
        auto its_info = s.first->get_eventgroupinfo();
        if (its_info) {
            auto its_service = its_info->get_service();
            auto its_instance = its_info->get_instance();
            auto its_eventgroup = its_info->get_eventgroup();
            auto its_major = its_info->get_major();

            remote_subscription_id_t its_id;
            auto its_result = its_info->update_remote_subscription(
                    s.first, std::chrono::steady_clock::now(),
                    s.second, its_id, false);
            if (its_result) {
                const client_t its_offering_client
                    = find_local_client(its_service, its_instance);
                send_unsubscription(its_offering_client,
                        its_service, its_instance, its_eventgroup, its_major,
                        s.second, s.first->get_id());
            }

            if (s.first->get_unreliable()) {
                VSOMEIP_INFO << "Expired subscription ["
                        << std::hex << std::setfill('0') << std::setw(4) << its_service << "."
                        << std::hex << std::setfill('0') << std::setw(4) << its_instance << "."
                        << std::hex << std::setfill('0') << std::setw(4) << its_eventgroup << "] unreliable from "
                        << s.first->get_unreliable()->get_address() << ":"
                        << std::dec << s.first->get_unreliable()->get_port();
            }

            if (s.first->get_reliable()) {
                VSOMEIP_INFO << "Expired subscription ["
                        << std::hex << std::setfill('0') << std::setw(4) << its_service << "."
                        << std::hex << std::setfill('0') << std::setw(4) << its_instance << "."
                        << std::hex << std::setfill('0') << std::setw(4) << its_eventgroup << "] reliable from "
                        << s.first->get_reliable()->get_address() << ":"
                        << std::dec << s.first->get_reliable()->get_port();
            }
        }
    }

    return its_next_expiration;
}

void routing_manager_impl::log_version_timer_cbk(boost::system::error_code const & _error) {
    if (!_error) {

#ifndef VSOMEIP_VERSION
#define VSOMEIP_VERSION "unknown version"
#endif
        static int its_counter(0);
        static uint32_t its_interval = configuration_->get_log_version_interval();

        bool is_diag_mode(false);

        if (discovery_) {
            is_diag_mode = discovery_->get_diagnosis_mode();
        }
        std::stringstream its_last_resume;
        {
            std::lock_guard<std::mutex> its_lock(last_resume_mutex_);
            if (last_resume_ != std::chrono::steady_clock::time_point::min()) {
                its_last_resume << " | " << std::dec
                        << std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - last_resume_).count() << "s";
            }
        }

        VSOMEIP_INFO << "vSomeIP " << VSOMEIP_VERSION << " | ("
                << ((is_diag_mode == true) ? "diagnosis)" : "default)")
                << its_last_resume.str();

        its_counter++;
        if (its_counter == 6) {
            ep_mgr_->log_client_states();
            ep_mgr_impl_->log_client_states();
            its_counter = 0;
        }

        {
            std::lock_guard<std::mutex> its_lock(version_log_timer_mutex_);
            version_log_timer_.expires_from_now(std::chrono::seconds(its_interval));
            version_log_timer_.async_wait(
                    std::bind(&routing_manager_impl::log_version_timer_cbk,
                              this, std::placeholders::_1));
        }
    }
}

bool routing_manager_impl::handle_local_offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major,minor_version_t _minor) {
    {
        std::lock_guard<std::mutex> its_lock(local_services_mutex_);
        auto found_service = local_services_.find(_service);
        if (found_service != local_services_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                const major_version_t its_stored_major(std::get<0>(found_instance->second));
                const minor_version_t its_stored_minor(std::get<1>(found_instance->second));
                const client_t its_stored_client(std::get<2>(found_instance->second));
                if (   its_stored_major == _major
                    && its_stored_minor == _minor
                    && its_stored_client == _client) {
                    VSOMEIP_WARNING << "routing_manager_impl::handle_local_offer_service: "
                        << "Application: " << std::hex << std::setfill('0')
                        << std::setw(4) << _client << " is offering: ["
                        << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                        << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                        << std::dec << static_cast<std::uint32_t>(_major) << "."
                        << _minor << "] offered previously by itself.";
                    return false;
                } else if (   its_stored_major == _major
                           && its_stored_minor == _minor
                           && its_stored_client != _client) {
                    // check if previous offering application is still alive
                    bool already_pinged(false);
                    {
                        std::lock_guard<std::mutex> its_lock(pending_offers_mutex_);
                        auto found_service2 = pending_offers_.find(_service);
                        if (found_service2 != pending_offers_.end()) {
                            auto found_instance2 = found_service2->second.find(_instance);
                            if (found_instance2 != found_service2->second.end()) {
                                if(std::get<2>(found_instance2->second) == _client) {
                                    already_pinged = true;
                                } else {
                                    VSOMEIP_ERROR << "routing_manager_impl::handle_local_offer_service: "
                                        << "rejecting service registration. Application: "
                                        << std::hex << std::setfill('0') << std::setw(4)
                                        << _client << " is trying to offer ["
                                        << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                                        << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                                        << std::dec << static_cast<std::uint32_t>(_major) << "."
                                        << std::dec << _minor
                                        << "] current pending offer by application: " << std::hex
                                        << std::setfill('0') << std::setw(4)
                                        << its_stored_client << ": ["
                                        << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                                        << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                                        << std::dec << static_cast<std::uint32_t>(its_stored_major)
                                        << "." << its_stored_minor << "]";
                                    return false;
                                }
                            }
                        }
                    }
                    if (!already_pinged) {
                        // find out endpoint of previously offering application
                        std::shared_ptr<local_client_endpoint_base_impl>
                            its_old_endpoint
                                = std::dynamic_pointer_cast<local_client_endpoint_base_impl>(
                                        find_local(its_stored_client));
                        if (its_old_endpoint) {
                            std::lock_guard<std::mutex> its_lock(pending_offers_mutex_);
                            if(stub_->send_ping(its_stored_client)) {
                                pending_offers_[_service][_instance] =
                                        std::make_tuple(_major, _minor, _client,
                                                        its_stored_client);
                                VSOMEIP_WARNING << "OFFER("
                                    << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                                    << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                                    << std::hex << std::setw(4) << std::setfill('0') << _instance
                                    << ":" << std::dec << int(_major) << "." << std::dec << _minor
                                    << "] is now pending. Waiting for pong from application: "
                                    << std::hex << std::setw(4) << std::setfill('0') << its_stored_client;
                                return false;
                            }
                        } else if (its_stored_client == host_->get_client()) {
                            VSOMEIP_ERROR << "routing_manager_impl::handle_local_offer_service: "
                                << "rejecting service registration. Application: "
                                << std::hex << std::setfill('0') << std::setw(4)
                                << _client << " is trying to offer ["
                                << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                                << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                                << std::dec << static_cast<std::uint32_t>(_major) << "."
                                << std::dec << _minor
                                << "] offered previously by routing manager stub itself with application: "
                                << std::hex << std::setfill('0') << std::setw(4)
                                << its_stored_client << ": ["
                                << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                                << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                                << std::dec << static_cast<std::uint32_t>(its_stored_major)
                                << "." << its_stored_minor << "] which is still alive";
                            return false;
                        }
                    } else {
                        return false;
                    }
                } else {
                    VSOMEIP_ERROR << "routing_manager_impl::handle_local_offer_service: "
                        << "rejecting service registration. Application: "
                        << std::hex << std::setfill('0') << std::setw(4)
                        << _client << " is trying to offer ["
                        << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                        << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                        << std::dec << static_cast<std::uint32_t>(_major) << "."
                        << std::dec << _minor
                        << "] offered previously by application: " << std::hex
                        << std::setfill('0') << std::setw(4)
                        << its_stored_client << ": ["
                        << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                        << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                        << std::dec << static_cast<std::uint32_t>(its_stored_major)
                        << "." << its_stored_minor << "]";
                    return false;
                }
            }
        }

        // check if the same service instance is already offered remotely
        if (routing_manager_base::offer_service(_client, _service, _instance,
                _major, _minor)) {
            local_services_[_service][_instance] = std::make_tuple(_major,
                    _minor, _client);
        } else {
            VSOMEIP_ERROR << "routing_manager_impl::handle_local_offer_service: "
                << "rejecting service registration. Application: "
                << std::hex << std::setfill('0') << std::setw(4)
                << _client << " is trying to offer ["
                << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                << std::hex << std::setfill('0') << std::setw(4) << _instance << "."
                << std::dec << static_cast<std::uint32_t>(_major) << "."
                << std::dec << _minor << "]"
                << "] already offered remotely";
            return false;
        }
    }
    return true;
}

void routing_manager_impl::on_pong(client_t _client) {
    std::lock_guard<std::mutex> its_lock(pending_offers_mutex_);
    if (pending_offers_.size() == 0) {
        return;
    }
    for (auto service_iter = pending_offers_.begin();
            service_iter != pending_offers_.end(); ) {
        for (auto instance_iter = service_iter->second.begin();
                instance_iter != service_iter->second.end(); ) {
            if (std::get<3>(instance_iter->second) == _client) {
                // received pong from an application were another application wants
                // to offer its service, delete the other applications offer as
                // the current offering application is still alive
                VSOMEIP_WARNING << "OFFER("
                    << std::hex << std::setw(4) << std::setfill('0')
                    << std::get<2>(instance_iter->second) <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0')
                    << service_iter->first << "."
                    << std::hex << std::setw(4) << std::setfill('0')
                    << instance_iter->first << ":" << std::dec
                    << std::uint32_t(std::get<0>(instance_iter->second))
                    << "." << std::dec << std::get<1>(instance_iter->second)
                    << "] was rejected as application: "
                    << std::hex << std::setw(4) << std::setfill('0') << _client
                    << " is still alive";
                instance_iter = service_iter->second.erase(instance_iter);
            } else {
                ++instance_iter;
            }
        }

        if (service_iter->second.size() == 0) {
            service_iter = pending_offers_.erase(service_iter);
        } else {
            ++service_iter;
        }
    }
}

void routing_manager_impl::register_client_error_handler(client_t _client,
        const std::shared_ptr<endpoint> &_endpoint) {
    _endpoint->register_error_handler(
        std::bind(&routing_manager_impl::handle_client_error, this, _client));
}

void routing_manager_impl::handle_client_error(client_t _client) {
    VSOMEIP_INFO << "Client 0x" << std::hex << get_client()
            << " handles a client error(" << std::hex << _client << ")";
    if (stub_)
        stub_->update_registration(_client, registration_type_e::DEREGISTER_ON_ERROR);

    std::forward_list<std::tuple<client_t, service_t, instance_t, major_version_t,
                                        minor_version_t>> its_offers;
    {
        std::lock_guard<std::mutex> its_lock(pending_offers_mutex_);
        if (pending_offers_.size() == 0) {
            return;
        }

        for (auto service_iter = pending_offers_.begin();
                service_iter != pending_offers_.end(); ) {
            for (auto instance_iter = service_iter->second.begin();
                    instance_iter != service_iter->second.end(); ) {
                if (std::get<3>(instance_iter->second) == _client) {
                    VSOMEIP_WARNING << "OFFER("
                        << std::hex << std::setw(4) << std::setfill('0')
                        << std::get<2>(instance_iter->second) <<"): ["
                        << std::hex << std::setw(4) << std::setfill('0')
                        << service_iter->first << "."
                        << std::hex << std::setw(4) << std::setfill('0')
                        << instance_iter->first << ":" << std::dec
                        << std::uint32_t(std::get<0>(instance_iter->second))
                        << "." << std::dec << std::get<1>(instance_iter->second)
                        << "] is not pending anymore as application: "
                        << std::hex << std::setw(4) << std::setfill('0')
                        << std::get<3>(instance_iter->second)
                        << " is dead. Offering again!";
                    its_offers.push_front(std::make_tuple(
                                    std::get<2>(instance_iter->second),
                                    service_iter->first,
                                    instance_iter->first,
                                    std::get<0>(instance_iter->second),
                                    std::get<1>(instance_iter->second)));
                    instance_iter = service_iter->second.erase(instance_iter);
                } else {
                    ++instance_iter;
                }
            }

            if (service_iter->second.size() == 0) {
                service_iter = pending_offers_.erase(service_iter);
            } else {
                ++service_iter;
            }
        }
    }
    for (const auto &offer : its_offers) {
        offer_service(std::get<0>(offer), std::get<1>(offer), std::get<2>(offer),
                std::get<3>(offer), std::get<4>(offer), true);
    }
}

std::shared_ptr<endpoint_manager_impl> routing_manager_impl::get_endpoint_manager() const {
    return ep_mgr_impl_;
}

void routing_manager_impl::send_subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        event_t _event) {
    auto endpoint = ep_mgr_->find_local(_service, _instance);
    if (endpoint) {
        stub_->send_subscribe(endpoint, _client,
                _service, _instance, _eventgroup, _major, _event, PENDING_SUBSCRIPTION_ID);
    }
}

void routing_manager_impl::set_routing_state(routing_state_e _routing_state) {
    if(discovery_) {
        switch (_routing_state) {
            case routing_state_e::RS_SUSPENDED:
            {
                VSOMEIP_INFO << "Set routing to suspend mode, diagnosis mode is "
                        << ((discovery_->get_diagnosis_mode() == true) ? "active." : "inactive.");
                // stop processing of incoming SD messages
                discovery_->stop();

                // remove all remote subscriptions to remotely offered services on this node
                expire_subscriptions(true);

                // send StopOffer messages for remotely offered services on this node
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        if (its_instance.second->get_endpoint(true) || its_instance.second->get_endpoint(false)) {
                            const client_t its_client(find_local_client(its_service.first, its_instance.first));
                            VSOMEIP_WARNING << "service "
                                << std::hex << std::setw(4) << std::setfill('0') << its_service.first << "."
                                << std::hex << std::setw(4) << std::setfill('0') << its_instance.first << " still offered by "
                                << std::hex << std::setw(4) << std::setfill('0') << its_client;
                        }
                        discovery_->stop_offer_service(its_instance.second);
                    }
                }
                {
                    std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
                    remote_subscription_state_.clear();
                }
                // mark all external services as offline
                services_t its_remote_services;
                {
                    std::lock_guard<std::mutex> its_lock(services_remote_mutex_);
                    its_remote_services = services_remote_;
                }
                for (const auto &s : its_remote_services) {
                    for (const auto &i : s.second) {
                        // determine existing subscriptions to remote service and send StopSubscribe
                        for (auto its_eventgroup : get_subscribed_eventgroups(s.first, i.first)) {
                            discovery_->unsubscribe(s.first, i.first, its_eventgroup, VSOMEIP_ROUTING_CLIENT);
                        }

                        const bool has_reliable(i.second->get_endpoint(true));
                        const bool has_unreliable(i.second->get_endpoint(false));
                        del_routing_info(s.first, i.first, has_reliable, has_unreliable);

                        // clear all cached payloads of remote services
                        unset_all_eventpayloads(s.first, i.first);
                    }
                }
                break;
            }
            case routing_state_e::RS_RESUMED:
            {
                VSOMEIP_INFO << "Set routing to resume mode, diagnosis mode was "
                        << ((discovery_->get_diagnosis_mode() == true) ? "active." : "inactive.");
                {
                    std::lock_guard<std::mutex> its_lock(last_resume_mutex_);
                    last_resume_ = std::chrono::steady_clock::now();
                }

                // Reset relevant in service info
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        its_instance.second->set_ttl(DEFAULT_TTL);
                        its_instance.second->set_is_in_mainphase(false);
                    }
                }
                // Switch SD back to normal operation
                discovery_->set_diagnosis_mode(false);

                if (routing_state_handler_) {
                    routing_state_handler_(_routing_state);
                }

                // start processing of SD messages (incoming remote offers should lead to new subscribe messages)
                discovery_->start();

                // Trigger initial offer phase for relevant services
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        discovery_->offer_service(its_instance.second);
                    }
                }
                break;
            }
            case routing_state_e::RS_DIAGNOSIS:
            {
                VSOMEIP_INFO << "Set routing to diagnosis mode.";
                discovery_->set_diagnosis_mode(true);

                // send StopOffer messages for all someip protocol services
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        if (host_->get_configuration()->is_someip(
                                its_service.first, its_instance.first)) {
                            discovery_->stop_offer_service(its_instance.second);
                        }
                    }
                }
                break;
            }
            case routing_state_e::RS_RUNNING:
                VSOMEIP_INFO << "Set routing to running mode, diagnosis mode was "
                        << ((discovery_->get_diagnosis_mode() == true) ? "active." : "inactive.");

                // Reset relevant in service info
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        if (host_->get_configuration()->is_someip(
                                its_service.first, its_instance.first)) {
                            its_instance.second->set_ttl(DEFAULT_TTL);
                            its_instance.second->set_is_in_mainphase(false);
                        }
                    }
                }
                // Switch SD back to normal operation
                discovery_->set_diagnosis_mode(false);

                // Trigger initial phase for relevant services
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        if (host_->get_configuration()->is_someip(
                                its_service.first, its_instance.first)) {
                            discovery_->offer_service(its_instance.second);
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
}

void routing_manager_impl::on_net_interface_or_route_state_changed(
        bool _is_interface, std::string _if, bool _available) {
    std::lock_guard<std::mutex> its_lock(pending_sd_offers_mutex_);
    auto log_change_message = [&_if, _available, _is_interface](bool _warning) {
        std::stringstream ss;
        ss << (_is_interface ? "Network interface" : "Route") << " \"" << _if
                << "\" state changed: " << (_available ? "up" : "down");
        if (_warning) {
            VSOMEIP_WARNING << ss.str();
        } else {
            VSOMEIP_INFO << ss.str();
        }
    };
    if (_is_interface) {
        if (if_state_running_
                || (_available && !if_state_running_ && routing_running_)) {
            log_change_message(true);
        } else if (!if_state_running_) {
            log_change_message(false);
        }
        if (_available && !if_state_running_) {
            if_state_running_ = true;
            if (!routing_running_) {
                if(configuration_->is_sd_enabled()) {
                    if (sd_route_set_) {
                        start_ip_routing();
                    }
                } else {
                    // Static routing, don't wait for route!
                    start_ip_routing();
                }
            }
        }
    } else {
        if (sd_route_set_
                || (_available && !sd_route_set_ && routing_running_)) {
            log_change_message(true);
        } else if (!sd_route_set_) {
            log_change_message(false);
        }
        if (_available && !sd_route_set_) {
            sd_route_set_ = true;
            if (!routing_running_) {
                if (if_state_running_) {
                    start_ip_routing();
                }
            }
        }
    }
}

void routing_manager_impl::start_ip_routing() {
#ifdef _WIN32
    if_state_running_ = true;
#endif

    if (routing_ready_handler_) {
        routing_ready_handler_();
    }
    if (discovery_) {
        discovery_->start();
    } else {
        init_routing_info();
    }

    for (auto its_service : pending_sd_offers_) {
        init_service_info(its_service.first, its_service.second, true);
    }
    pending_sd_offers_.clear();

    routing_running_ = true;
    VSOMEIP_INFO << VSOMEIP_ROUTING_READY_MESSAGE;
}

void routing_manager_impl::requested_service_add(client_t _client,
                                             service_t _service,
                                             instance_t _instance,
                                             major_version_t _major,
                                             minor_version_t _minor) {
    std::lock_guard<std::mutex> ist_lock(requested_services_mutex_);
    requested_services_[_client][_service][_instance].insert({ _major, _minor });
}

void routing_manager_impl::requested_service_remove(client_t _client,
                                             service_t _service,
                                             instance_t _instance) {
    std::lock_guard<std::mutex> ist_lock(requested_services_mutex_);
    auto found_client = requested_services_.find(_client);
    if (found_client != requested_services_.end()) {
        auto found_service = found_client->second.find(_service);
        if (found_service != found_client->second.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                // delete all requested major/minor versions
                found_service->second.erase(_instance);
                if (!found_service->second.size()) {
                    found_client->second.erase(_service);
                    if (!found_client->second.size()) {
                        requested_services_.erase(client_);
                    }
                }
            }
        }
    }
}

std::set<eventgroup_t>
routing_manager_impl::get_subscribed_eventgroups(
        service_t _service, instance_t _instance) {
    std::set<eventgroup_t> its_eventgroups;

    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    auto found_service = eventgroups_.find(_service);
    if (found_service != eventgroups_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            for (const auto& its_group : found_instance->second) {
                for (const auto& its_event : its_group.second->get_events()) {
                    if (its_event->has_subscriber(its_group.first, ANY_CLIENT)) {
                        its_eventgroups.insert(its_group.first);
                    }
                }
            }
        }
    }

    return its_eventgroups;
}

void routing_manager_impl::clear_targets_and_pending_sub_from_eventgroups(
        service_t _service, instance_t _instance) {
    std::vector<std::shared_ptr<event>> its_events;
    {
        std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
        auto found_service = eventgroups_.find(_service);
        if (found_service != eventgroups_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                for (const auto &its_eventgroup : found_instance->second) {
                    // As the service is gone, all subscriptions to its events
                    // do no longer exist and the last received payload is no
                    // longer valid.
                    for (auto &its_event : its_eventgroup.second->get_events()) {
                        const auto its_subscribers = its_event->get_subscribers();
                        for (const auto& its_subscriber : its_subscribers) {
                            if (its_subscriber != get_client()) {
                                its_event->remove_subscriber(
                                        its_eventgroup.first, its_subscriber);
                            }

                            client_t its_client = VSOMEIP_ROUTING_CLIENT; //is_specific_endpoint_client(its_subscriber, _service, _instance);
                            {
                                std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
                                const auto its_tuple =
                                    std::make_tuple(found_service->first, found_instance->first,
                                                    its_eventgroup.first, its_client);
                                remote_subscription_state_.erase(its_tuple);
                            }
                        }
                        its_events.push_back(its_event);
                    }
                    // TODO dn: find out why this was commented out
                    //its_eventgroup.second->clear_targets();
                    //its_eventgroup.second->clear_pending_subscriptions();
                }
            }
        }
    }
    for (const auto& e : its_events) {
        e->unset_payload(true);
    }
}

void routing_manager_impl::clear_remote_subscriber(service_t _service,
                                                   instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(remote_subscribers_mutex_);
    auto found_service = remote_subscribers_.find(_service);
    if (found_service != remote_subscribers_.end()) {
        if (found_service->second.erase(_instance) > 0 &&
                !found_service->second.size()) {
            remote_subscribers_.erase(found_service);
        }
    }
}


void routing_manager_impl::call_sd_endpoint_connected(
        const boost::system::error_code& _error,
        service_t _service, instance_t _instance,
        const std::shared_ptr<endpoint>& _endpoint,
        std::shared_ptr<boost::asio::steady_timer> _timer) {
    (void)_timer;
    if (_error) {
        return;
    }
    _endpoint->set_established(true);
    if (discovery_) {
        discovery_->on_endpoint_connected(_service, _instance,
                _endpoint);
    }
}

bool routing_manager_impl::create_placeholder_event_and_subscribe(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        event_t _event, client_t _client) {
    bool is_inserted(false);
    // we received a event which was not yet requested/offered
    // create a placeholder field until someone requests/offers this event with
    // full information like eventgroup, field or not etc.
    std::set<eventgroup_t> its_eventgroups({_eventgroup});

    const client_t its_local_client(find_local_client(_service, _instance));
    if (its_local_client == host_->get_client()) {
        // received subscription for event of a service instance hosted by
        // application acting as rm_impl register with own client id and shadow = false
        register_event(host_->get_client(),
                _service, _instance,
                _event,
                its_eventgroups, event_type_e::ET_UNKNOWN, reliability_type_e::RT_UNKNOWN,
                std::chrono::milliseconds::zero(), false, true,
                nullptr, false, false, true);
    } else if (its_local_client != VSOMEIP_ROUTING_CLIENT) {
        // received subscription for event of a service instance hosted on
        // this node register with client id of local_client and set shadow to true
        register_event(its_local_client,
                _service, _instance,
                _event, its_eventgroups, event_type_e::ET_UNKNOWN,
                reliability_type_e::RT_UNKNOWN,
                std::chrono::milliseconds::zero(), false, true,
                nullptr, false, true, true);
    } else {
        // received subscription for event of a unknown or remote service instance
        std::shared_ptr<serviceinfo> its_info = find_service(_service,
                _instance);
        if (its_info && !its_info->is_local()) {
            // remote service, register shadow event with client ID of subscriber
            // which should have called register_event
            register_event(_client,
                    _service, _instance,
                    _event, its_eventgroups, event_type_e::ET_UNKNOWN,
                    reliability_type_e::RT_UNKNOWN,
                    std::chrono::milliseconds::zero(),
                    false, true, nullptr, false, true, true);
        } else {
            VSOMEIP_WARNING
                << "routing_manager_impl::create_placeholder_event_and_subscribe("
                << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "."
                << std::hex << std::setw(4) << std::setfill('0') << _event << "]"
                << " received subscription for unknown service instance.";
        }
    }

    std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
    if (its_event) {
        is_inserted = its_event->add_subscriber(_eventgroup, _client, false);
    }
    return is_inserted;
}

void routing_manager_impl::handle_subscription_state(
        client_t _client, service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, event_t _event) {
#if 0
    VSOMEIP_ERROR << "routing_manager_impl::" << __func__
            << "(" << std::hex << _client << "): "
            << "event="
            << std::hex << _service << "."
            << std::hex << _instance << "."
            << std::hex << _eventgroup << "."
            << std::hex << _event
            << " me="
            << std::hex << get_client();
#endif
    // Note: remote_subscription_state_mutex_ is already locked as this
    // method builds a critical section together with insert_subscription
    // from routing_manager_base.
    // Todo: Improve this situation...
    auto its_event = find_event(_service, _instance, _event);
    client_t its_client(VSOMEIP_ROUTING_CLIENT);
    if (its_event &&
            its_event->get_type() == event_type_e::ET_SELECTIVE_EVENT) {
        its_client = _client;
    }

    auto its_tuple
        = std::make_tuple(_service, _instance, _eventgroup, its_client);
    auto its_state = remote_subscription_state_.find(its_tuple);
    if (its_state != remote_subscription_state_.end()) {
#if 0
        VSOMEIP_ERROR << "routing_manager_impl::" << __func__
                << "(" << std::hex << _client << "): "
                << "event="
                << std::hex << _service << "."
                << std::hex << _instance << "."
                << std::hex << _eventgroup << "."
                << std::hex << _event
                << " state=" << std::hex << (int)its_state->second
                << " me="
                << std::hex << get_client();
#endif
        if (its_state->second == subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED) {
            // Subscription already acknowledged!
            if (_client == get_client()) {
                host_->on_subscription_status(_service, _instance, _eventgroup, _event, 0x0 /*OK*/);
            } else {
                stub_->send_subscribe_ack(_client, _service, _instance, _eventgroup, _event);
            }
        }
    }
}

void routing_manager_impl::register_sd_acceptance_handler(
        const sd_acceptance_handler_t& _handler) const {
    if (discovery_) {
        discovery_->register_sd_acceptance_handler(_handler);
    }
}

void routing_manager_impl::register_reboot_notification_handler(
        const reboot_notification_handler_t& _handler) const {
    if (discovery_) {
        discovery_->register_reboot_notification_handler(_handler);
    }
}

void routing_manager_impl::register_routing_ready_handler(
        const routing_ready_handler_t& _handler) {
    routing_ready_handler_ = _handler;
}

void routing_manager_impl::register_routing_state_handler(
        const routing_state_handler_t& _handler) {
    routing_state_handler_ = _handler;
}

void routing_manager_impl::sd_acceptance_enabled(
        const boost::asio::ip::address& _address,
        const configuration::port_range_t& _range, bool _reliable) {
    expire_subscriptions(_address, _range, _reliable);
    expire_services(_address, _range, _reliable);
}

void routing_manager_impl::memory_log_timer_cbk(
        boost::system::error_code const & _error) {
    if (_error) {
        return;
    }
#ifndef _WIN32
    static const std::uint32_t its_pagesize = static_cast<std::uint32_t>(getpagesize() / 1024);
#else
    static const std::uint32_t its_pagesize = 4096 / 1024;
#endif
    std::FILE *its_file = std::fopen("/proc/self/statm", "r");
    if (!its_file) {
        VSOMEIP_ERROR << "memory_log_timer_cbk: couldn't open:"
                << std::string(std::strerror(errno));
        return;
    }
    std::uint64_t its_size(0);
    std::uint64_t its_rsssize(0);
    std::uint64_t its_sharedpages(0);
    std::uint64_t its_text(0);
    std::uint64_t its_lib(0);
    std::uint64_t its_data(0);
    std::uint64_t its_dirtypages(0);

    if (EOF == std::fscanf(its_file, "%lu %lu %lu %lu %lu %lu %lu", &its_size,
                    &its_rsssize, &its_sharedpages, &its_text, &its_lib,
                    &its_data, &its_dirtypages)) {
        VSOMEIP_ERROR<< "memory_log_timer_cbk: error reading:"
                << std::string(std::strerror(errno));
    }
    std::fclose(its_file);
#ifndef _WIN32
    struct timespec cputs, monots;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cputs);
    clock_gettime(CLOCK_MONOTONIC, &monots);
#endif

    VSOMEIP_INFO << "memory usage: "
            << "VmSize " << std::dec << its_size * its_pagesize << " kB, "
            << "VmRSS " << std::dec << its_rsssize * its_pagesize << " kB, "
            << "shared pages " << std::dec << its_sharedpages * its_pagesize << " kB, "
            << "text " << std::dec << its_text * its_pagesize << " kB, "
            << "data " << std::dec << its_data * its_pagesize << " kB "
#ifndef _WIN32
            << "| monotonic time: " << std::dec << monots.tv_sec << "."
            << std::dec << monots.tv_nsec << " cpu time: "
            << std::dec << cputs.tv_sec << "." << std::dec << cputs.tv_nsec
#endif
            ;

    {
        std::lock_guard<std::mutex> its_lock(memory_log_timer_mutex_);
        boost::system::error_code ec;
        memory_log_timer_.expires_from_now(std::chrono::seconds(
                configuration_->get_log_memory_interval()), ec);
        memory_log_timer_.async_wait(
                std::bind(&routing_manager_impl::memory_log_timer_cbk, this,
                        std::placeholders::_1));
    }
}

void routing_manager_impl::status_log_timer_cbk(
        boost::system::error_code const & _error) {
    if (_error) {
        return;
    }

    ep_mgr_impl_->print_status();
    {
        std::lock_guard<std::mutex> its_lock(status_log_timer_mutex_);
        boost::system::error_code ec;
        status_log_timer_.expires_from_now(std::chrono::seconds(
                configuration_->get_log_status_interval()), ec);
        status_log_timer_.async_wait(
                std::bind(&routing_manager_impl::status_log_timer_cbk, this,
                        std::placeholders::_1));
    }
}

void
routing_manager_impl::on_unsubscribe_ack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        remote_subscription_id_t _id) {
    std::shared_ptr<eventgroupinfo> its_info
        = find_eventgroup(_service, _instance, _eventgroup);
    if (its_info) {
        const auto its_subscription = its_info->get_remote_subscription(_id);
        if (its_subscription) {
            its_info->remove_remote_subscription(_id);

            std::lock_guard<std::mutex> its_lock(remote_subscribers_mutex_);
            remote_subscribers_[_service][_instance].erase(_client);

            if (its_info->get_remote_subscriptions().size() == 0) {
                for (const auto &its_event : its_info->get_events()) {
                    bool has_remote_subscriber(false);
                    for (const auto &its_eventgroup : its_event->get_eventgroups()) {
                       const auto its_eventgroup_info
                           = find_eventgroup(_service, _instance, its_eventgroup);
                        if (its_eventgroup_info
                                && its_eventgroup_info->get_remote_subscriptions().size() > 0) {
                            has_remote_subscriber = true;
                        }
                    }

                    if (!has_remote_subscriber && its_event->is_shadow()) {
                        its_event->unset_payload();
                    }
                }
            }
        } else {
            VSOMEIP_ERROR << __func__
                << ": Unknown StopSubscribe " << std::dec << _id << " for eventgroup ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
        }
    } else {
        VSOMEIP_ERROR << __func__
                << ": Received StopSubscribe for unknown eventgroup: ("
                << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]";
    }
}

void routing_manager_impl::on_connect(const std::shared_ptr<endpoint>& _endpoint) {
    (void)_endpoint;
}
void routing_manager_impl::on_disconnect(const std::shared_ptr<endpoint>& _endpoint) {
    (void)_endpoint;
}
void routing_manager_impl::send_subscription(
        const client_t _offering_client,
        const service_t _service, const instance_t _instance,
        const eventgroup_t _eventgroup, const major_version_t _major,
        const std::set<client_t> &_clients,
        const remote_subscription_id_t _id) {
    if (host_->get_client() == _offering_client) {
        auto self = shared_from_this();
        for (const auto& its_client : _clients) {
            host_->on_subscription(_service, _instance, _eventgroup, its_client, own_uid_, own_gid_, true,
                [this, self, _service, _instance, _eventgroup, its_client, _id]
                        (const bool _is_accepted) {
                try {
                    if (!_is_accepted) {
                        const auto its_callback = std::bind(
                                &routing_manager_stub_host::on_subscribe_nack,
                                std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                                its_client, _service, _instance,
                                _eventgroup, ANY_EVENT, _id);
                        io_.post(its_callback);
                    } else {
                        const auto its_callback = std::bind(
                                &routing_manager_stub_host::on_subscribe_ack,
                                std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                                its_client, _service, _instance,
                                _eventgroup, ANY_EVENT, _id);
                        io_.post(its_callback);
                    }
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR << __func__ << e.what();
                }
            });
        }
    } else { // service hosted by local client
        for (const auto& its_client : _clients) {
            if (!stub_->send_subscribe(find_local(_offering_client), its_client,
                    _service, _instance, _eventgroup, _major, ANY_EVENT, _id)) {
                try {
                    const auto its_callback = std::bind(
                            &routing_manager_stub_host::on_subscribe_nack,
                            std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                            its_client, _service, _instance, _eventgroup,
                            ANY_EVENT, _id);
                    io_.post(its_callback);
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR << __func__ << e.what();
                }
            }
        }
    }
}

void routing_manager_impl::cleanup_server_endpoint(
        service_t _service, const std::shared_ptr<endpoint>& _endpoint) {
    if (_endpoint) {
        // Clear service_instances_, check whether any service still
        // uses this endpoint and clear server endpoint if no service
        // remains using it
        if (ep_mgr_impl_->remove_instance(_service, _endpoint.get())) {
            if (ep_mgr_impl_->remove_server_endpoint(
                    _endpoint->get_local_port(), _endpoint->is_reliable())) {
                // Stop endpoint (close socket) to release its async_handlers!
                _endpoint->stop();
            }
        }
    }
}

pending_remote_offer_id_t routing_manager_impl::pending_remote_offer_add(
        service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(pending_remote_offers_mutex_);
    if (++pending_remote_offer_id_ == 0) {
        pending_remote_offer_id_++;
    }
    pending_remote_offers_[pending_remote_offer_id_] = std::make_pair(_service,
            _instance);
    return pending_remote_offer_id_;
}

std::pair<service_t, instance_t> routing_manager_impl::pending_remote_offer_remove(
        pending_remote_offer_id_t _id) {
    std::lock_guard<std::mutex> its_lock(pending_remote_offers_mutex_);
    std::pair<service_t, instance_t> ret = std::make_pair(ANY_SERVICE,
                                                          ANY_INSTANCE);
    auto found_si = pending_remote_offers_.find(_id);
    if (found_si != pending_remote_offers_.end()) {
        ret = found_si->second;
        pending_remote_offers_.erase(found_si);
    }
    return ret;
}

void routing_manager_impl::on_resend_provided_events_response(
        pending_remote_offer_id_t _id) {
    const std::pair<service_t, instance_t> its_service =
            pending_remote_offer_remove(_id);
    if (its_service.first != ANY_SERVICE) {
        // create server endpoint
        std::shared_ptr<serviceinfo> its_info = find_service(its_service.first,
                its_service.second);
        if (its_info) {
            its_info->set_ttl(DEFAULT_TTL);
            init_service_info(its_service.first, its_service.second, true);
        }
    }
}

void routing_manager_impl::print_stub_status() const {
    stub_->print_endpoint_status();
}

void routing_manager_impl::service_endpoint_connected(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, const std::shared_ptr<endpoint>& _endpoint,
        bool _unreliable_only) {

    if (!_unreliable_only) {
        // Mark only TCP-only and TCP+UDP services available here
        // UDP-only services are already marked as available in add_routing_info
        on_availability(_service, _instance, true, _major, _minor);
        stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance,
                _major, _minor);
    }

    std::shared_ptr<boost::asio::steady_timer> its_timer =
            std::make_shared<boost::asio::steady_timer>(io_);
    boost::system::error_code ec;
    its_timer->expires_from_now(std::chrono::milliseconds(3), ec);
    if (!ec) {
        its_timer->async_wait(
                std::bind(&routing_manager_impl::call_sd_endpoint_connected,
                        std::static_pointer_cast<routing_manager_impl>(
                                shared_from_this()), std::placeholders::_1,
                        _service, _instance, _endpoint, its_timer));
    } else {
        VSOMEIP_ERROR << __func__ << " " << ec.message();
    }
}

void routing_manager_impl::service_endpoint_disconnected(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, const std::shared_ptr<endpoint>& _endpoint) {
    (void)_endpoint;
    on_availability(_service, _instance, false, _major, _minor);
    stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance,
            _major, _minor);
    VSOMEIP_WARNING << __func__ << ": lost connection to remote service: ["
            << std::hex << std::setw(4) << std::setfill('0') << _service << "."
            << std::hex << std::setw(4) << std::setfill('0') << _instance << "]";
}

void
routing_manager_impl::send_unsubscription(client_t _offering_client,
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, major_version_t _major,
        const std::set<client_t> &_removed,
        remote_subscription_id_t _id) {

    (void)_major; // TODO: Remove completely?

    if (host_->get_client() == _offering_client) {
        auto self = shared_from_this();
        for (const auto& its_client : _removed) {
            host_->on_subscription(_service, _instance,
                    _eventgroup, its_client, own_uid_, own_gid_, false,
                [this, self, _service, _instance, _eventgroup,
                 its_client, _id, _offering_client]
                 (const bool _is_accepted) {
                    (void)_is_accepted;
                    try {
                        const auto its_callback = std::bind(
                            &routing_manager_stub_host::on_unsubscribe_ack,
                            std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                            its_client, _service, _instance, _eventgroup, _id);
                        io_.post(its_callback);
                    } catch (const std::exception &e) {
                        VSOMEIP_ERROR << __func__ << e.what();
                    }
                }
            );
        }
    } else {
        for (const auto& its_client : _removed) {
            if (!stub_->send_unsubscribe(find_local(_offering_client), its_client,
                    _service, _instance, _eventgroup, ANY_EVENT, _id)) {
                try {
                    const auto its_callback = std::bind(
                        &routing_manager_stub_host::on_unsubscribe_ack,
                        std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                        its_client, _service, _instance, _eventgroup, _id);
                    io_.post(its_callback);
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR << __func__ << e.what();
                }
            }
        }
    }
}

bool
routing_manager_impl::update_security_policy_configuration(
        uint32_t _uid, uint32_t _gid,
        const std::shared_ptr<policy> &_policy,
        const std::shared_ptr<payload> &_payload,
        const security_update_handler_t &_handler) {

    if (stub_)
        return stub_->update_security_policy_configuration(_uid, _gid,
                          _policy, _payload, _handler);

    return (false);
}

bool
routing_manager_impl::remove_security_policy_configuration(
        uint32_t _uid, uint32_t _gid,
        const security_update_handler_t &_handler) {

    if (stub_)
        return stub_->remove_security_policy_configuration(_uid, _gid,
                          _handler);

    return (false);
}

bool routing_manager_impl::insert_event_statistics(service_t _service, instance_t _instance,
        method_t _method, length_t _length) {

    static uint32_t its_max_messages = configuration_->get_statistics_max_messages();
    std::lock_guard<std::mutex> its_lock(message_statistics_mutex_);
    const auto its_tuple = std::make_tuple(_service, _instance, _method);
    const auto its_main_s = message_statistics_.find(its_tuple);
    if (its_main_s != message_statistics_.end()) {
        // increase counter and calculate moving avergae for payload length
        its_main_s->second.avg_length_ =
                (its_main_s->second.avg_length_ * its_main_s->second.counter_  + _length) /
                (its_main_s->second.counter_ + 1);
        its_main_s->second.counter_++;

        if (its_tuple == message_to_discard_) {
            // check list for entry with least counter value
            uint32_t its_min_count(0xFFFFFFFF);
            auto its_tuple_to_discard = std::make_tuple(0xFFFF, 0xFFFF, 0xFFFF);
            for (const auto& it : message_statistics_) {
                if (it.second.counter_ < its_min_count) {
                    its_min_count = it.second.counter_;
                    its_tuple_to_discard = it.first;
                }
            }
            if (its_min_count != 0xFFFF
                    && its_min_count < its_main_s->second.counter_) {
                // update message to discard with current message
                message_to_discard_ = its_tuple;
            }
        }
    } else {
        if (message_statistics_.size() < its_max_messages) {
            message_statistics_[its_tuple] = {1, _length};
            message_to_discard_ = its_tuple;
        } else {
            // no slot empty
            const auto it = message_statistics_.find(message_to_discard_);
            if (it != message_statistics_.end()
                    && it->second.counter_ == 1) {
                message_statistics_.erase(message_to_discard_);
                message_statistics_[its_tuple] = {1, _length};
                message_to_discard_ = its_tuple;
            } else {
                // ignore message
                ignored_statistics_counter_++;
                return false;
            }
        }
    }
    return true;
}

void routing_manager_impl::statistics_log_timer_cbk(boost::system::error_code const & _error) {
    if (!_error) {
        static uint32_t its_interval = configuration_->get_statistics_interval();
        its_interval = its_interval >= 1000 ? its_interval : 1000;
        static uint32_t its_min_freq = configuration_->get_statistics_min_freq();
        std::stringstream its_log;
        {
            std::lock_guard<std::mutex> its_lock(message_statistics_mutex_);
            for (const auto& s : message_statistics_) {
                if (s.second.counter_ / (its_interval / 1000) >= its_min_freq) {
                    uint16_t its_subscribed(0);
                    std::shared_ptr<event> its_event = find_event(std::get<0>(s.first), std::get<1>(s.first), std::get<2>(s.first));
                    if (its_event) {
                        if (!its_event->is_provided()) {
                            its_subscribed = static_cast<std::uint16_t>(its_event->get_subscribers().size());
                        }
                    }
                    its_log << std::hex << std::setw(4) << std::setfill('0')
                                    << std::get<0>(s.first) << "."
                                    << std::get<1>(s.first) << "."
                                    << std::get<2>(s.first) << ": #="
                                    << std::dec << s.second.counter_ << " L="
                                    << s.second.avg_length_ << " S="
                                    << std::dec << its_subscribed << ", ";
                }
            }

            if (ignored_statistics_counter_) {
                its_log << std::dec << " #ignored: " << ignored_statistics_counter_;
            }

            message_statistics_.clear();
            message_to_discard_ = std::make_tuple(0x00, 0x00, 0x00);
            ignored_statistics_counter_ = 0;
        }

        if (its_log.str().length() > 0) {
            VSOMEIP_INFO << "Received events statistics: [" << its_log.str() << "]";
        }

        {
            std::lock_guard<std::mutex> its_lock(statistics_log_timer_mutex_);
            statistics_log_timer_.expires_from_now(std::chrono::milliseconds(its_interval));
            statistics_log_timer_.async_wait(
                    std::bind(&routing_manager_impl::statistics_log_timer_cbk,
                              this, std::placeholders::_1));
        }
    }
}

} // namespace vsomeip_v3

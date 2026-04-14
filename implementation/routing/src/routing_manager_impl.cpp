// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <climits>
#include <iomanip>
#include <memory>
#include <sstream>
#include <forward_list>
#include <thread>

#if defined(__linux__) || defined(__QNX__)
#include <unistd.h>
#include <cstdio>
#include <time.h>
#endif

#include <boost/asio/steady_timer.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>

#include "logger_ext.hpp"
#include "../include/event.hpp"
#include "../include/eventgroupinfo.hpp"
#include "../include/remote_subscription.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_impl.hpp"
#include "../include/routing_manager_stub.hpp"
#include "../include/serviceinfo.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../endpoints/include/tcp_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_client_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../endpoints/include/abstract_socket_factory.hpp"
#include "../../endpoints/include/virtual_server_endpoint_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/serializer.hpp"
#include "../../plugin/include/plugin_manager_impl.hpp"
#include "../../protocol/include/protocol.hpp"
#include "../../security/include/security.hpp"
#include "../../service_discovery/include/constants.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../service_discovery/include/service_discovery.hpp"
#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/utility.hpp"
#include "../../tracing/include/connector_impl.hpp"

#ifndef ANDROID
#include "../../e2e_protection/include/buffer/buffer.hpp"
#include "../../e2e_protection/include/e2exf/config.hpp"

#include "../../e2e_protection/include/e2e/profile/e2e_provider.hpp"
#endif

namespace vsomeip_v3 {

#define VSOMEIP_LOG_PREFIX "rmi"

#ifdef ANDROID
namespace sd {
runtime::~runtime() { }
}
#endif

routing_manager_impl::routing_manager_impl(routing_manager_host* _host) :
    routing_manager_base(_host), version_log_timer_(_host->get_io()), if_state_running_(false), sd_route_set_(false),
    routing_running_(false), routing_state_(configuration_->get_initial_routing_state()), status_log_timer_(_host->get_io()),
    memory_log_timer_(_host->get_io()), ep_mgr_impl_(std::make_shared<endpoint_manager_impl>(this, io_, configuration_)),
    pending_remote_offer_id_(0), last_resume_(std::chrono::steady_clock::time_point::min()), statistics_log_timer_(_host->get_io()),
    ignored_statistics_counter_(0) {

    VSOMEIP_INFO << "Starting Routing Manager [Host] with state " << routing_state_tostring(routing_state_);
}

routing_manager_impl::~routing_manager_impl() {
    utility::reset_client_ids(configuration_->get_network());
    utility::remove_lockfile(configuration_->get_network());
}

boost::asio::io_context& routing_manager_impl::get_io() {
    return routing_manager_base::get_io();
}

client_t routing_manager_impl::get_client() const {
    return VSOMEIP_ROUTING_CLIENT;
}

vsomeip_sec_client_t routing_manager_impl::get_sec_client() const {

    return routing_manager_base::get_sec_client();
}

std::string routing_manager_impl::get_client_host() const {
    return routing_manager_base::get_client_host();
}

void routing_manager_impl::set_client_host(const std::string& _client_host) {
    routing_manager_base::set_client_host(_client_host);
}

std::set<client_t> routing_manager_impl::find_local_clients(service_t _service, instance_t _instance) {
    std::scoped_lock its_lock(local_services_mutex_);
    return local_services_table_.find_clients(_service, _instance);
}

client_t routing_manager_impl::find_local_client(service_t _service, instance_t _instance) {
    std::scoped_lock its_lock(local_services_mutex_);
    return local_services_table_.find_client(_service, _instance);
}

void routing_manager_impl::on_register_application(client_t _client, const boost::asio::ip::address& _address, port_t _port) {
    if (stub_) {
        stub_->on_register_application(_client, _address, _port);
    }
}

void routing_manager_impl::lazy_load([[maybe_unused]] const std::string& _client_host) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}

void routing_manager_impl::init() {

    stub_ = std::make_shared<routing_manager_stub>(this, configuration_);
    stub_->init();

    if (configuration_->is_sd_enabled()) {
        VSOMEIP_INFO << "Service Discovery enabled. Trying to load module.";
        auto its_plugin = plugin_manager::get()->get_plugin(plugin_type_e::SD_RUNTIME_PLUGIN, VSOMEIP_SD_LIBRARY);
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
    if (configuration_->is_e2e_enabled()) {
        VSOMEIP_INFO << "E2E protection enabled.";

        const char* its_e2e_module = getenv(VSOMEIP_ENV_E2E_PROTECTION_MODULE);
        std::string plugin_name = its_e2e_module != nullptr ? its_e2e_module : VSOMEIP_E2E_LIBRARY;

        auto its_plugin = plugin_manager::get()->get_plugin(plugin_type_e::APPLICATION_PLUGIN, plugin_name);
        if (its_plugin) {
            VSOMEIP_INFO << "E2E module loaded.";
            e2e_provider_ = std::dynamic_pointer_cast<e2e::e2e_provider>(its_plugin);
        }
    }

    if (e2e_provider_) {
        std::map<e2exf::data_identifier_t, std::shared_ptr<cfg::e2e>> its_e2e_configuration = configuration_->get_e2e_configuration();
        for (auto& identifier : its_e2e_configuration) {
            if (!e2e_provider_->add_configuration(identifier.second)) {
                VSOMEIP_INFO << "Unknown E2E profile: " << identifier.second->profile << ", skipping ...";
            }
        }
    }
#endif
}

void routing_manager_impl::start() {
    ep_mgr_impl_->start();

#if defined(__linux__)
    boost::asio::ip::address its_multicast;
    try {
        its_multicast = boost::asio::ip::make_address(configuration_->get_sd_multicast());
    } catch (...) {
        VSOMEIP_ERROR << "Illegal multicast address \"" << configuration_->get_sd_multicast() << "\". Please check your configuration.";
    }

    std::stringstream its_netmask_or_prefix;
    auto its_unicast = configuration_->get_unicast_address();
    if (its_unicast.is_v4())
        its_netmask_or_prefix << "netmask:" << configuration_->get_netmask().to_string();
    else
        its_netmask_or_prefix << "prefix:" << configuration_->get_prefix();

    VSOMEIP_INFO << "Client [" << hex4(get_client()) << "] routes unicast:" << its_unicast.to_string() << ", "
                 << its_netmask_or_prefix.str();

    netlink_connector_ =
            abstract_socket_factory::get()->create_netlink_connector(host_->get_io(), configuration_->get_unicast_address(), its_multicast);
    netlink_connector_->register_net_if_changes_handler(std::bind(&routing_manager_impl::on_net_interface_or_route_state_changed, this,
                                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    netlink_connector_->start();
#else
    start_ip_routing();
#endif

    stub_->start();

#if defined(__linux__) || defined(__QNX__)
    if (configuration_->log_memory()) {
        std::scoped_lock its_lock{memory_log_timer_mutex_};
        memory_log_timer_.expires_after(std::chrono::seconds(0));
        memory_log_timer_.async_wait(std::bind(&routing_manager_impl::memory_log_timer_cbk, this, std::placeholders::_1));
    }
#endif
    {
        std::scoped_lock its_lock{log_timer_mutex_};
        if (configuration_->get_version_log_interval(host_->get_name(), true) > 0) {
            version_log_timer_.expires_after(std::chrono::seconds(0));
            version_log_timer_.async_wait([this](boost::system::error_code const& ec) { this->version_log_timer_cbk(ec, 0); });
        }

        if (configuration_->get_status_log_interval(host_->get_name(), true) > 0) {
            status_log_timer_.expires_after(std::chrono::seconds(0));
            status_log_timer_.async_wait([this](boost::system::error_code const& ec) { this->status_log_timer_cbk(ec); });
        }

        if (configuration_->log_statistics()) {
            statistics_log_timer_.expires_after(std::chrono::seconds(0));
            statistics_log_timer_.async_wait(std::bind(&routing_manager_impl::statistics_log_timer_cbk, this, std::placeholders::_1));
        }
    }

    VSOMEIP_INFO << VSOMEIP_INTERNAL_ROUTING_READY_MESSAGE;
}

void routing_manager_impl::stop() {
#if defined(__linux__)
    {
        std::scoped_lock its_lock{memory_log_timer_mutex_};
        memory_log_timer_.cancel();
    }
    if (netlink_connector_) {
        netlink_connector_->stop();
    }
#endif
    {
        std::scoped_lock its_lock{log_timer_mutex_};
        version_log_timer_.cancel();
        status_log_timer_.cancel();
        statistics_log_timer_.cancel();
    }

    if (discovery_)
        discovery_->stop();

    try_to_send_before_stop();

    stub_->stop();

    ep_mgr_impl_->stop();

    utility::reset_client_ids(configuration_->get_network());
}

bool routing_manager_impl::insert_offer_command(service_t _service, instance_t _instance, uint8_t _command, client_t _client,
                                                major_version_t _major, minor_version_t _minor) {
    // flag to indicate whether caller of this function can start directly processing the command
    bool must_process(false);
    auto found_service_instance = offer_commands_.find(std::make_pair(_service, _instance));
    if (found_service_instance != offer_commands_.end()) {
        // if nothing is queued
        if (found_service_instance->second.empty()) {
            must_process = true;
        }
        found_service_instance->second.push_back(std::make_tuple(_command, _client, _major, _minor));
    } else {
        // nothing is queued -> add command to queue and process command directly
        offer_commands_[std::make_pair(_service, _instance)].push_back(std::make_tuple(_command, _client, _major, _minor));
        must_process = true;
    }
    return must_process;
}

void routing_manager_impl::erase_offer_command(service_t _service, instance_t _instance) {
    auto found_service_instance = offer_commands_.find(std::make_pair(_service, _instance));
    if (found_service_instance != offer_commands_.end()) {
        // erase processed command
        if (!found_service_instance->second.empty()) {
            found_service_instance->second.pop_front();
            if (!found_service_instance->second.empty()) {
                // check for other commands to be processed
                auto its_command = found_service_instance->second.front();
                if (std::get<0>(its_command) == uint8_t(protocol::id_e::OFFER_SERVICE_ID)) {
                    boost::asio::post(io_, [this, its_command, _service, _instance]() {
                        offer_service(std::get<1>(its_command), _service, _instance, std::get<2>(its_command), std::get<3>(its_command),
                                      false);
                    });
                } else {
                    boost::asio::post(io_, [this, its_command, _service, _instance]() {
                        stop_offer_service(std::get<1>(its_command), _service, _instance, std::get<2>(its_command),
                                           std::get<3>(its_command), false);
                    });
                }
            }
        }
    }
}

bool routing_manager_impl::is_local_client(client_t _client) const {
    return find_routing_endpoint(_client) != nullptr;
}

bool routing_manager_impl::offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                         minor_version_t _minor) {

    return offer_service(_client, _service, _instance, _major, _minor, true);
}

bool routing_manager_impl::offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                         minor_version_t _minor, bool _must_queue) {
    std::scoped_lock its_lock{offer_serialization_mutex_};
    // only queue commands if method was NOT called via erase_offer_command()
    if (_must_queue) {
        if (!insert_offer_command(_service, _instance, uint8_t(protocol::id_e::OFFER_SERVICE_ID), _client, _major, _minor)) {
            VSOMEIP_INFO_P << "(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << int(_major) << "."
                           << _minor << "] (" << std::boolalpha << _must_queue << ")"
                           << " not offering service, because insert_offer_command returned false!";
            return false;
        }
    }

    if (!handle_local_offer_service(_client, _service, _instance, _major, _minor)) {
        erase_offer_command(_service, _instance);
        VSOMEIP_INFO_P << "(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << int(_major) << "." << _minor
                       << "] (" << std::boolalpha << _must_queue << ")"
                       << " not offering, returned from handle_local_offer_service!";
        return false;
    }

    {
        std::scoped_lock its_lock(on_state_change_mutex_);
        if (is_external_routing_ready()) {
            init_service_info(_service, _instance, true);
        } else {
            std::scoped_lock its_lock_inner(pending_sd_offers_mutex_);
            pending_sd_offers_.emplace(_service, _instance);
            VSOMEIP_INFO_P << "Added service 0x" << hex4(_service) << " to pending_sd_offers_.size = " << pending_sd_offers_.size();
        }
    }

    if (discovery_) {
        std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
        if (its_info) {
            discovery_->offer_service(its_info);
        }
    }

    stub_->on_offer_service(_client, _service, _instance, _major, _minor);
    // NOTE: Order matters. The 'erase_offer_command' must be done after the on_availability to ensure that the process has completed before
    // starting the next one, otherwise, we may have the availability being reported in the wrong order
    on_availability(_service, _instance, availability_state_e::AS_AVAILABLE, _major, _minor);
    erase_offer_command(_service, _instance);

    VSOMEIP_INFO << "OFFER(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << int(_major) << "." << _minor
                 << "] (" << std::boolalpha << _must_queue << ")";
    return true;
}

void routing_manager_impl::stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                              minor_version_t _minor) {
    stop_offer_service(_client, _service, _instance, _major, _minor, true);
}

void routing_manager_impl::stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                              minor_version_t _minor, bool _must_queue) {
    std::scoped_lock its_lock{offer_serialization_mutex_};

    if (_must_queue) {
        if (!insert_offer_command(_service, _instance, uint8_t(protocol::id_e::STOP_OFFER_SERVICE_ID), _client, _major, _minor)) {
            VSOMEIP_INFO_P << "(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << int(_major) << "."
                           << _minor << "] (" << std::boolalpha << _must_queue << ")"
                           << " STOP-OFFER NOT INSERTED!";
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
            std::scoped_lock its_lock(pending_sd_offers_mutex_);
            for (auto it = pending_sd_offers_.begin(); it != pending_sd_offers_.end();) {
                if (it->first == _service && it->second == _instance) {
                    it = pending_sd_offers_.erase(it);
                    VSOMEIP_INFO_P << "Removed service 0x" << hex4(_service)
                                   << " from pending_sd_offers_.size = " << pending_sd_offers_.size();
                    break;
                } else {
                    ++it;
                }
            }
        }

        on_stop_offer_service_unlocked(_client, _service, _instance, _major, _minor);
        VSOMEIP_INFO << "STOP OFFER(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << std::dec
                     << int(_major) << "." << _minor << "] (" << std::boolalpha << _must_queue << ")";
    } else {
        VSOMEIP_WARNING_P << "Received STOP_OFFER(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":"
                          << int(_major) << "." << _minor << "] for remote service --> ignore";
        erase_offer_command(_service, _instance);
    }

    remove_pending_requests(pending_request_removal_type_e::OFFERING_ONLY, _client, _service, _instance);
}

void routing_manager_impl::request_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                           minor_version_t _minor) {

    VSOMEIP_INFO << "REQUEST(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << int(_major) << "." << _minor
                 << "]";

    routing_manager_base::request_service(_client, _service, _instance, _major, _minor);

    auto its_info = find_service(_service, _instance);
    if (!its_info) {
        add_requested_service(_client, _service, _instance, _major, _minor);
        if (discovery_) {
            if (!configuration_->is_local_service(_service, _instance)) {
                // Non local service instance ~> tell SD to find it!
                discovery_->request_service(_service, _instance, _major, _minor, DEFAULT_TTL);
            } else {
                VSOMEIP_INFO << "Avoid trigger SD find-service message for local service/instance/major/minor: " << hex4(_service) << "/"
                             << hex4(_instance) << "/" << static_cast<int>(_major) << "/" << _minor;
            }
        }
    } else {
        if (_major == its_info->get_major() || DEFAULT_MAJOR == its_info->get_major() || ANY_MAJOR == _major) {
            if (!its_info->is_local()) {
                add_requested_service(_client, _service, _instance, _major, _minor);
                if (discovery_) {
                    // Non local service instance ~> tell SD to find it!
                    discovery_->request_service(_service, _instance, _major, _minor, DEFAULT_TTL);
                }
                its_info->add_client(_client);
                ep_mgr_impl_->find_or_create_remote_client(_service, _instance);
            }
        }
    }
}

void routing_manager_impl::release_service(client_t _client, service_t _service, instance_t _instance) {

    VSOMEIP_INFO << "RELEASE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "]";

    routing_manager_base::release_service(_client, _service, _instance);
    remove_requested_service(_client, _service, _instance, ANY_MAJOR, ANY_MINOR);
    remove_pending_requests(pending_request_removal_type_e::REQUESTING_ONLY, _client, _service, _instance);

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info && !its_info->is_local()) {
        if (0 == its_info->get_requesters_size()) {
            auto its_eventgroups = find_eventgroups(_service, _instance);
            for (const auto& eg : its_eventgroups) {
                auto its_events = eg->get_events();
                for (auto& e : its_events) {
                    e->clear_subscribers();
                }
            }

            if (discovery_) {
                discovery_->release_service(_service, _instance);
                discovery_->unsubscribe_all(_service, _instance);
            }
            ep_mgr_impl_->clear_client_endpoints(_service, _instance, true);
            ep_mgr_impl_->clear_client_endpoints(_service, _instance, false);
            its_info->set_endpoint(nullptr, true);
            its_info->set_endpoint(nullptr, false);
            unset_all_eventpayloads(_service, _instance);
        } else {
            auto its_eventgroups = find_eventgroups(_service, _instance);
            for (const auto& eg : its_eventgroups) {
                auto its_id = eg->get_eventgroup();
                auto its_events = eg->get_events();
                bool eg_has_subscribers{false};
                for (const auto& e : its_events) {
                    e->remove_subscriber(its_id, _client);
                    if (!e->get_subscribers().empty()) {
                        eg_has_subscribers = true;
                    }
                }
                if (discovery_) {
                    discovery_->unsubscribe(_service, _instance, its_id, _client);
                }
                if (!eg_has_subscribers) {
                    for (const auto& e : its_events) {
                        if (e->is_set()) {
                            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "."
                                           << hex4(e->get_event()) << "]";
                        }
                        e->unset_payload(true);
                    }
                }
            }
        }
    } else {
        if (discovery_) {
            // Release the service only if there are no more requesters.
            if (!has_requester(_service, _instance, ANY_MAJOR, ANY_MINOR)) {
                discovery_->release_service(_service, _instance);
            }
        }
    }
}

void routing_manager_impl::subscribe(client_t _client, [[maybe_unused]] const vsomeip_sec_client_t* _sec_client, service_t _service,
                                     instance_t _instance, eventgroup_t _eventgroup, major_version_t _major, event_t _event,
                                     const std::shared_ptr<debounce_filter_impl_t>& _filter) {

    if (routing_state_ == routing_state_e::RS_SUSPENDED) {
        VSOMEIP_INFO_P << "We are suspended --> do nothing.";
        return;
    }

    VSOMEIP_INFO << "SUBSCRIBE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup) << ":"
                 << hex4(_event) << ":" << static_cast<int>(_major) << "]";
    {
        if (discovery_) {
            // Note: The calls to insert_subscription & handle_subscription_state must not
            // run concurrently to a call to on_subscribe_ack. Therefore the lock is acquired
            // before calling insert_subscription and released after the call to
            // handle_subscription_state.
            std::unique_lock<std::mutex> its_critical(remote_subscription_state_mutex_);
            bool inserted = insert_subscription(_service, _instance, _eventgroup, _event, _filter, _client);
            if (const client_t its_local_client = find_local_client(_service, _instance); inserted) {
                if (VSOMEIP_ROUTING_CLIENT == its_local_client) {
                    // TODO this should be an impossible branch by now, because this would need to be offered locally
                    // by the router,
                    handle_subscription_state(_client, _service, _instance, _eventgroup, _event);
                    its_critical.unlock();
                    static const ttl_t configured_ttl(configuration_->get_sd_ttl());
                    notify_one_current_value(_client, _service, _instance, _eventgroup, _event);

                    auto its_info = find_eventgroup(_service, _instance, _eventgroup);
                    // if the subscriber is the rm_host itself: check if service
                    // is available before subscribing via SD otherwise we sent
                    // a StopSubscribe/Subscribe once the first offer is received
                    if (its_info && find_service(_service, _instance)) {
                        discovery_->subscribe(_service, _instance, _eventgroup, _major, configured_ttl,
                                              its_info->is_selective() ? _client : VSOMEIP_ROUTING_CLIENT, its_info);
                    }
                } else {
                    its_critical.unlock();
                    VSOMEIP_ERROR_P << "Router received subscription for local service from client: 0x" << hex4(_client) << ": ["
                                    << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup) << ":" << hex4(_event) << ":"
                                    << static_cast<int>(_major) << "]";
                }
            } else {
                its_critical.unlock();
            }
        } else {
            VSOMEIP_ERROR << "SOME/IP eventgroups require SD to be enabled!";
        }
    }
}

void routing_manager_impl::unsubscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                       event_t _event) {

    VSOMEIP_INFO << "UNSUBSCRIBE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup) << "."
                 << hex4(_event) << "]";

    bool last_subscriber_removed(true);

    std::shared_ptr<eventgroupinfo> its_info = find_eventgroup(_service, _instance, _eventgroup);
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
        if (auto its_local_client = find_local_client(_service, _instance); VSOMEIP_ROUTING_CLIENT == its_local_client) {
            if (last_subscriber_removed) {
                unset_all_eventpayloads(_service, _instance, _eventgroup);
                {
                    auto tuple = std::make_tuple(_service, _instance, _eventgroup, _client);
                    std::scoped_lock its_lock{remote_subscription_state_mutex_};
                    remote_subscription_state_.erase(tuple);
                }
            }

            if (its_info && (last_subscriber_removed || its_info->is_selective())) {

                discovery_->unsubscribe(_service, _instance, _eventgroup, its_info->is_selective() ? _client : VSOMEIP_ROUTING_CLIENT);
            }
        }

        if (last_subscriber_removed) {
            ep_mgr_impl_->clear_multicast_endpoints(_service, _instance);
        }

    } else {
        VSOMEIP_ERROR << "SOME/IP eventgroups require SD to be enabled!";
    }
}

bool routing_manager_impl::send(client_t _client, std::shared_ptr<message> _message, bool _force) {
    if (utility::is_request(_message->get_message_type())) {
        if (!stub_->is_remotely_available(_message->get_service(), _message->get_instance(), _message->get_interface_version())) {
            VSOMEIP_WARNING_P << "[Client=" << hex4(_client) << " _message=" << hex4(_message->get_service()) << "."
                              << hex4(_message->get_method()) << "." << hex2(static_cast<uint8_t>(_message->get_message_type())) << "."
                              << hex2(static_cast<uint8_t>(_message->get_return_code())) << " _force=" << _force
                              << "]: Remote service not available. instance=" << hex4(_message->get_instance())
                              << " version=" << _message->get_interface_version();
            if (!_force) {
                return false;
            }
        }
    }
    {
        bool is_sent(false);
        if (utility::is_request(_message->get_message_type())) {
            _message->set_client(_client);
            if (!host_->is_routing()
                && !is_available(_message->get_service(), _message->get_instance(), _message->get_interface_version())) {
                VSOMEIP_WARNING_P << "this=" << this << "}::send{_client=" << _client << " _message=" << hex4(_message->get_service())
                                  << "." << hex4(_message->get_method()) << "." << static_cast<int>(_message->get_message_type()) << "."
                                  << static_cast<int>(_message->get_return_code()) << " _force=" << _force
                                  << "}: Service not available. instance=" << hex4(_message->get_instance())
                                  << " version=" << hex4(_message->get_interface_version());
                if (!_force) {
                    return is_sent;
                }
            }
        }

        std::shared_ptr<serializer> its_serializer(get_serializer());
        if (its_serializer->serialize(_message.get())) {
            auto const sec_client = get_sec_client();
            is_sent = send(_client, its_serializer->get_data(), its_serializer->get_size(), _message->get_instance(),
                           _message->is_reliable(), get_client(), &sec_client, 0, false, _force);
            its_serializer->reset();
            put_serializer(its_serializer);
        } else {
            VSOMEIP_ERROR_P << "Failed to serialize message. Check message size!";
        }
        return is_sent;
    }
}

bool routing_manager_impl::send(client_t _client, const byte_t* _data, length_t _size, instance_t _instance, bool _reliable,
                                [[maybe_unused]] client_t _bound_client, [[maybe_unused]] const vsomeip_sec_client_t* _sec_client,
                                uint8_t _status_check, [[maybe_unused]] bool _sent_from_remote, [[maybe_unused]] bool _force) {

    bool is_sent(false);
    if (_size <= VSOMEIP_MESSAGE_TYPE_POS) {
        return is_sent;
    }
    bool is_request = utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS]);
    bool is_notification = utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]);
    client_t its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);
    service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
    method_t its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
    client_t its_target_client = VSOMEIP_ROUTING_CLIENT;

    bool is_service_discovery = (its_service == sd::service && its_method == sd::method);

    std::shared_ptr<local_endpoint> its_local_target;
    if (is_request) {
        its_target_client = find_local_client(its_service, _instance);
        its_local_target = find_routing_endpoint(its_target_client);
    } else if (!is_notification) {
        its_local_target = find_routing_endpoint(its_client);
        its_target_client = its_client;
    } else if (is_notification && _client && !is_service_discovery) { // Selective notifications!
        its_local_target = find_routing_endpoint(_client);
        its_target_client = _client;
    }

    if (its_local_target) {
        is_sent = send_local(its_local_target, its_target_client, _data, _size, _instance, _reliable, protocol::id_e::SEND_ID,
                             _status_check, VSOMEIP_ROUTING_CLIENT);
        if (is_sent && (is_notification && find_local_client(its_service, _instance) == VSOMEIP_ROUTING_CLIENT)) {
            trace::header its_header;
            if (its_header.prepare(its_local_target, true, _instance))
                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
        }
        return is_sent;
    }

    std::shared_ptr<boardnet_endpoint> its_target;
    {
        e2e_buffer its_buffer;

        if (e2e_provider_) {
            if (!is_service_discovery) {
                service_t its_service_inner = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
                method_t its_method_inner = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
#ifndef ANDROID
                if (e2e_provider_->is_protected({its_service_inner, its_method_inner})) {
                    // Find out where the protected area starts
                    size_t its_base = e2e_provider_->get_protection_base({its_service_inner, its_method_inner});

                    // Build a corresponding buffer
                    its_buffer.assign(_data + its_base, _data + _size);

                    e2e_provider_->protect({its_service_inner, its_method_inner}, its_buffer, _instance);

                    // Prepend header
                    its_buffer.insert(its_buffer.begin(), _data, _data + its_base);

                    _data = its_buffer.data();
                }
#endif
            }
        }
        if (is_request) {
            its_target = ep_mgr_impl_->find_or_create_remote_client(its_service, _instance, _reliable);
            if (its_target) {
                is_sent = its_target->send(_data, _size);
                if (is_sent) {
                    trace::header its_header;
                    if (its_header.prepare(its_target, true, _instance,
                                           its_target->is_reliable() ? trace::protocol_e::tcp : trace::protocol_e::udp))
                        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
                }
            } else {
                const session_t its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
                VSOMEIP_ERROR << "Routing info for remote service could not be found! (" << hex4(its_client) << "): [" << hex4(its_service)
                              << "." << hex4(_instance) << "." << hex4(its_method) << "] " << hex4(its_session);
            }
        } else {
            std::shared_ptr<serviceinfo> its_info(find_service(its_service, _instance));
            if (its_info || is_service_discovery) {
                if (is_notification && !is_service_discovery) {
                    method_t its_method_inner = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
                    std::shared_ptr<event> its_event = find_event(its_service, _instance, its_method_inner);
                    if (its_event) {
                        bool has_sent(false);
                        std::set<std::shared_ptr<endpoint_definition>> its_targets;
                        // we need both endpoints as clients can subscribe to events via TCP
                        // and UDP
                        auto its_udp_server_endpoint = its_info->get_endpoint(false);
                        auto its_tcp_server_endpoint = its_info->get_endpoint(true);

                        if (its_udp_server_endpoint || its_tcp_server_endpoint) {
                            const auto its_reliability = its_event->get_reliability();
                            for (auto its_group : its_event->get_eventgroups()) {
                                auto its_eventgroup = find_eventgroup(its_service, _instance, its_group);
                                if (its_eventgroup) {
                                    // Unicast targets
                                    for (const auto& its_remote : its_eventgroup->get_unicast_targets()) {
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
                                    // Send to multicast targets if subscribers are still
                                    // interested
                                    if (its_eventgroup->is_sending_multicast()) {
                                        if (its_reliability == reliability_type_e::RT_UNRELIABLE
                                            || its_reliability == reliability_type_e::RT_BOTH) {
                                            boost::asio::ip::address its_address;
                                            uint16_t its_port;
                                            if (its_eventgroup->get_multicast(its_address, its_port)) {
                                                std::shared_ptr<endpoint_definition> its_multicast_target;
                                                its_multicast_target =
                                                        endpoint_definition::get(its_address, its_port, false, its_service, _instance);
                                                its_targets.insert(its_multicast_target);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        for (auto const& target : its_targets) {
                            if (target->is_reliable()) {
                                its_tcp_server_endpoint->send_to(target, _data, _size);
                            } else {
                                its_udp_server_endpoint->send_to(target, _data, _size);
                            }
                            has_sent = true;
                        }
                        if (has_sent) {
                            trace::header its_header;
                            if (its_header.prepare(nullptr, true, _instance, trace::protocol_e::unknown))
                                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
                        }
                    }
                } else {
                    if ((utility::is_response(_data[VSOMEIP_MESSAGE_TYPE_POS]) || utility::is_error(_data[VSOMEIP_MESSAGE_TYPE_POS]))
                        && its_info && !its_info->is_local()) {
                        // We received a response/error but neither the hosting application
                        // nor another local client could be found --> drop
                        const session_t its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
                        VSOMEIP_ERROR_P << ": Received response/error for unknown client (" << hex4(its_client) << "): ["
                                        << hex4(its_service) << "." << hex4(_instance) << "." << hex4(its_method) << "] "
                                        << hex4(its_session);
                        return false;
                    }
                    its_target =
                            is_service_discovery ? (sd_info_ ? sd_info_->get_endpoint(false) : nullptr) : its_info->get_endpoint(_reliable);
                    if (its_target) {
                        is_sent = its_target->send(_data, _size);
                        if (is_sent) {
                            trace::header its_header;
                            if (its_header.prepare(its_target, true, _instance,
                                                   its_target->is_reliable() ? trace::protocol_e::tcp : trace::protocol_e::udp))
                                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
                        }
                    } else {
                        const session_t its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
                        VSOMEIP_ERROR_P << "Routing error. Endpoint for service (" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                        << hex4(_instance) << "." << hex4(its_method) << "] " << hex4(its_session)
                                        << " could not be found!";
                    }
                }
            } else {
                if (!is_notification) {
                    const session_t its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
                    VSOMEIP_ERROR_P << "Routing error. Not hosting service (" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                    << hex4(_instance) << "." << hex4(its_method) << "] " << hex4(its_session);
                }
            }
        }
    }

    return is_sent;
}

bool routing_manager_impl::send_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target,
                                   std::shared_ptr<message> _message) {

    bool is_sent(false);

    std::shared_ptr<serializer> its_serializer(get_serializer());
    if (its_serializer->serialize(_message.get())) {
        const byte_t* its_data = its_serializer->get_data();
        length_t its_size = its_serializer->get_size();
        e2e_buffer its_buffer;
        if (e2e_provider_) {
            service_t its_service = bithelper::read_uint16_be(&its_data[VSOMEIP_SERVICE_POS_MIN]);
            method_t its_method = bithelper::read_uint16_be(&its_data[VSOMEIP_METHOD_POS_MIN]);
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

        uint8_t its_client[2] = {0};
        bithelper::write_uint16_le(_client, its_client);
        const_cast<byte_t*>(its_data)[VSOMEIP_CLIENT_POS_MIN] = its_client[1];
        const_cast<byte_t*>(its_data)[VSOMEIP_CLIENT_POS_MAX] = its_client[0];

        is_sent = send_to(_target, its_data, its_size, _message->get_instance());

        its_serializer->reset();
        put_serializer(its_serializer);
    } else {
        VSOMEIP_ERROR_P << "Serialization failed.";
    }
    return is_sent;
}

bool routing_manager_impl::send_to(const std::shared_ptr<endpoint_definition>& _target, const byte_t* _data, uint32_t _size,
                                   instance_t _instance) {
    bool is_sent{false};
    auto its_endpoint = ep_mgr_impl_->find_server_endpoint(_target->get_remote_port(), _target->is_reliable());

    if (its_endpoint) {
        is_sent = its_endpoint->send_to(_target, _data, _size);
        if (is_sent) {
            trace::header its_header;
            if (its_header.prepare(its_endpoint, true, _instance,
                                   its_endpoint->is_reliable() ? trace::protocol_e::tcp : trace::protocol_e::udp))
                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
        }
    }
    return is_sent;
}

bool routing_manager_impl::send_via_sd(const std::shared_ptr<endpoint_definition>& _target, const byte_t* _data, uint32_t _size,
                                       uint16_t _sd_port) {
    bool is_sent{false};
    auto its_endpoint = ep_mgr_impl_->find_server_endpoint(_sd_port, _target->is_reliable());

    if (its_endpoint) {
        is_sent = its_endpoint->send_to(_target, _data, _size);
        if (is_sent && tc_->is_sd_enabled()) {
            trace::header its_header;
            if (its_header.prepare(its_endpoint, true, 0x0, its_endpoint->is_reliable() ? trace::protocol_e::tcp : trace::protocol_e::udp))
                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
        }
    }
    return is_sent;
}

void routing_manager_impl::register_shadow_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                                 const std::set<eventgroup_t>& _eventgroups, event_type_e _type,
                                                 reliability_type_e _reliability, bool _is_provided, bool _is_cyclic) {

    register_event(_client, _service, _instance, _notifier, _eventgroups, _type, _reliability,
                   (_is_cyclic ? std::chrono::milliseconds(1) : std::chrono::milliseconds::zero()), false, true, nullptr, _is_provided,
                   true);
}

void routing_manager_impl::unregister_shadow_event(client_t _client, service_t _service, instance_t _instance, event_t _event,
                                                   bool _is_provided) {
    unregister_event(_client, _service, _instance, _event, _is_provided);
}

void routing_manager_impl::notify_one(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload,
                                      client_t _client, bool _force) {
    std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
    if (its_event) {
        std::set<std::shared_ptr<endpoint_definition>> its_targets;
        const auto its_reliability = its_event->get_reliability();
        for (const auto g : its_event->get_eventgroups()) {
            const auto its_eventgroup = find_eventgroup(_service, _instance, g);
            if (its_eventgroup) {
                const auto its_subscriptions = its_eventgroup->get_remote_subscriptions();
                for (const auto& s : its_subscriptions) {
                    if (s->has_client(_client)) {
                        if (its_reliability == reliability_type_e::RT_RELIABLE || its_reliability == reliability_type_e::RT_BOTH) {
                            const auto its_reliable = s->get_reliable();
                            if (its_reliable)
                                its_targets.insert(its_reliable);
                        }
                        if (its_reliability == reliability_type_e::RT_UNRELIABLE || its_reliability == reliability_type_e::RT_BOTH) {
                            const auto its_unreliable = s->get_unreliable();
                            if (its_unreliable)
                                its_targets.insert(its_unreliable);
                        }
                    }
                }
            }
        }

        if (its_targets.size() > 0) {
            for (const auto& its_target : its_targets) {
                its_event->set_payload(_payload, _client, its_target, _force);
            }
        }
    } else {
        VSOMEIP_WARNING << "Attempt to update the undefined event/field [" << hex4(_service) << "." << hex4(_instance) << "."
                        << hex4(_event) << "]";
    }
}

void routing_manager_impl::on_availability([[maybe_unused]] service_t _service, [[maybe_unused]] instance_t _instance,
                                           [[maybe_unused]] availability_state_e _state, [[maybe_unused]] major_version_t _major,
                                           [[maybe_unused]] minor_version_t _minor) { }

bool routing_manager_impl::offer_service_remotely(service_t _service, instance_t _instance, std::uint16_t _port, bool _reliable,
                                                  bool _magic_cookies_enabled) {
    bool ret = true;

    if (!is_available(_service, _instance, ANY_MAJOR)) {
        VSOMEIP_ERROR_P << "Service [" << hex4(_service) << "." << hex4(_instance) << "] is not offered locally! Won't offer it remotely.";
        ret = false;
    } else {
        // update service info in configuration
        if (!configuration_->remote_offer_info_add(_service, _instance, _port, _reliable, _magic_cookies_enabled)) {
            ret = false;
        } else {
            // trigger event registration again to create shadow events
            const client_t its_offering_client = find_local_client(_service, _instance);
            if (its_offering_client == VSOMEIP_ROUTING_CLIENT) {
                VSOMEIP_ERROR_P << "Didn't find offering client for service [" << hex4(_service) << "." << hex4(_instance) << "]";
                ret = false;
            } else {
                if (!stub_->send_provided_event_resend_request(its_offering_client, pending_remote_offer_add(_service, _instance))) {
                    VSOMEIP_ERROR_P << "Couldn't send event resend request to client 0x" << hex4(its_offering_client)
                                    << " providing service [" << hex4(_service) << "." << hex4(_instance) << "]";

                    ret = false;
                }
            }
        }
    }
    return ret;
}

bool routing_manager_impl::stop_offer_service_remotely(service_t _service, instance_t _instance, std::uint16_t _port, bool _reliable,
                                                       bool _magic_cookies_enabled) {
    bool ret = true;
    bool service_still_offered_remote(false);
    // update service configuration
    if (!configuration_->remote_offer_info_remove(_service, _instance, _port, _reliable, _magic_cookies_enabled,
                                                  &service_still_offered_remote)) {
        VSOMEIP_ERROR_P << "Couldn't remove remote offer info for service [" << hex4(_service) << "." << hex4(_instance)
                        << "] from configuration";
        ret = false;
    }
    std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
    std::shared_ptr<boardnet_endpoint> its_server_endpoint;
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
        // unset payload and clear subscribers
        stop_offer_service_base(its_offering_client, _service, _instance, its_major, its_minor);
        // unregister events
        for (const event_t its_event_id : find_events(_service, _instance)) {
            unregister_shadow_event(its_offering_client, _service, _instance, its_event_id, true);
        }
        clear_targets_and_pending_sub_from_eventgroups(_service, _instance);

        if (discovery_ && its_info) {
            discovery_->stop_offer_service(its_info);
            its_info->set_endpoint(nullptr, _reliable);
            VSOMEIP_INFO_P << "Sending StopOffer to [" << hex4(_service) << "." << hex4(_instance) << "." << _port << "] with reliability ("
                           << std::boolalpha << _reliable << ')';
        }
    } else {
        // service is still partly offered
        if (discovery_ && its_info) {
            std::shared_ptr<serviceinfo> its_copied_info = std::make_shared<serviceinfo>(*its_info);
            its_info->set_endpoint(nullptr, _reliable);
            // ensure to not send StopOffer for endpoint on which the service is
            // still offered
            its_copied_info->set_endpoint(nullptr, !_reliable);
            discovery_->stop_offer_service(its_copied_info);
            VSOMEIP_INFO_P << "Only sending the StopOffer to [" << hex4(_service) << '.' << hex4(_instance) << ']' << " with reliability ("
                           << std::boolalpha << !_reliable << ')' << " as the service is still partly offered!";
        }
    }

    cleanup_server_endpoint(_service, its_server_endpoint);
    return ret;
}

void routing_manager_impl::on_message(const byte_t* _data, length_t _length, boardnet_endpoint* _receiver,
                                      const boost::asio::ip::address& _remote_address, port_t _remote_port, bool _is_multicast) {

    uint8_t its_check_status = e2e::profile_interface::generic_check_status::E2E_OK;
    instance_t its_instance(0x0);
    bool is_forwarded(true);
    // message is at least 16-bytes, see also PRS_SOMEIP_00910
    if (_length < VSOMEIP_FULL_HEADER_SIZE) {
        VSOMEIP_ERROR_P << "Dropped message with invalid length " << _length;
        return;
    }

    const message_type_e its_message_type = static_cast<message_type_e>(_data[VSOMEIP_MESSAGE_TYPE_POS]);
    const service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
    const method_t its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
    const client_t its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);
    const session_t its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
    const auto its_return_code = static_cast<return_code_e>(_data[VSOMEIP_RETURN_CODE_POS]);

    // refer to error workflow in PRS_SOMEIP_00195
    // (although note that it is *NOT* strictly followed!)

    // ignore messages with invalid message type
    if (!utility::is_valid_message_type(its_message_type)) {
        VSOMEIP_ERROR_P << "Dropped message with invalid message type 0x" << hex2(static_cast<uint8_t>(its_message_type));
        return;
    }
    // ignore messages with.. unknown remote address? how is this even possible?!
    if (_remote_address.is_unspecified()) {
        VSOMEIP_ERROR_P << "Dropped message with invalid remote address from: " << _remote_address.to_string() << ":" << _remote_port;
        return;
    }

    // Log messages which could cause routing issues.
    if (!is_valid_client_id(its_client, its_message_type)) {
        VSOMEIP_ERROR_P << "Invalid client id. message=" << hex4(its_service) << "." << hex4(its_method) << "."
                        << hex2(static_cast<uint8_t>(its_message_type)) << "." << hex2(static_cast<uint8_t>(its_return_code))
                        << " client=0x" << hex4(its_client) << " source=" << _remote_address << ":" << _remote_port;
    }

    if (its_service == VSOMEIP_SD_SERVICE) {
        if (discovery_ && its_method == sd::method) {
            if (configuration_->get_sd_port() == _remote_port) {
                // ACL check SD message
                if (!is_acl_message_allowed(_receiver, its_service, ANY_INSTANCE, _remote_address)) {
                    return;
                }
                discovery_->on_message(_data, _length, _remote_address, _is_multicast);
            } else {
                VSOMEIP_ERROR << "Ignored SD message from unknown port (" << _remote_port << ")";
            }
        }
    } else {
        if (_is_multicast) {
            its_instance = ep_mgr_impl_->find_instance_multicast(its_service, _remote_address);
        } else {
            its_instance = ep_mgr_impl_->find_instance(its_service, _receiver);
        }

        return_code_e return_code = check_error(_data, _length, its_instance);
        if (return_code != return_code_e::E_OK && return_code != return_code_e::E_NOT_OK) {
            // PRS_SOMEIP_00171/PRS_SOMEIP_00189, no error reply for fire-and-forget
            if (!utility::is_request_no_return(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
                send_error(return_code, _data, _length, its_instance, _receiver->is_reliable(), _receiver, _remote_address, _remote_port);
            }

            // ignore request no response message if an error occurred
            return;
        }

        if (its_instance == 0xFFFF) {
            VSOMEIP_ERROR_P << "Dropped message with no matching instanceId, [" << hex4(its_service) << '.' << hex4(its_instance) << "."
                            << hex4(its_method) << "." << hex4(its_client) << "." << hex4(its_session)
                            << "] from: " << _remote_address.to_string() << ":" << _remote_port << ", multicast: " << std::boolalpha
                            << _is_multicast;
            return;
        }

        // Security checks if enabled!
        if (configuration_->is_security_enabled()) {
            if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
                if (!configuration_->is_offered_remote(its_service, its_instance)) {
                    VSOMEIP_WARNING << "Security: Received a remote request for service/instance " << hex4(its_service) << "/"
                                    << hex4(its_instance) << " which isn't offered remote ~> Skip message!";
                    return;
                }
                if (find_routing_endpoint(its_client)) {
                    VSOMEIP_WARNING << "Security: Received a remote request from client identifier 0x" << hex4(its_client)
                                    << " which is already used locally ~> Skip message!";
                    return;
                }
                if (!configuration_->is_remote_access_allowed()) {
                    // check if policy allows remote requests.
                    VSOMEIP_WARNING_P << "Security: Remote client with client ID 0x" << hex4(its_client)
                                      << " is not allowed to communicate with service/instance/method " << hex4(its_service) << "/"
                                      << hex4(its_instance) << "/" << hex4(its_method);
                    return;
                }
            }
        }
        if (e2e_provider_) {
#ifndef ANDROID
            if (e2e_provider_->is_checked({its_service, its_method})) {
                auto its_base = e2e_provider_->get_protection_base({its_service, its_method});
                e2e_buffer its_buffer(_data + its_base, _data + _length);
                e2e_provider_->check({its_service, its_method}, its_buffer, its_instance, its_check_status);

                if (its_check_status != e2e::profile_interface::generic_check_status::E2E_OK) {
                    VSOMEIP_INFO << "E2E protection: CRC check failed for service: " << hex4(its_service)
                                 << " method: " << hex4(its_method);
                }
            }
#endif
        }

        // ACL check message
        if (!is_acl_message_allowed(_receiver, its_service, its_instance, _remote_address)) {
            return;
        }

        // Common way of message handling
        is_forwarded = on_message(its_service, its_instance, _data, _length, _receiver->is_reliable(), VSOMEIP_ROUTING_CLIENT, nullptr,
                                  its_check_status, true);
    }

    if (is_forwarded) {
        trace::header its_header;
        const boost::asio::ip::address_v4 its_remote_address =
                _remote_address.is_v4() ? _remote_address.to_v4() : boost::asio::ip::make_address_v4("6.6.6.6");
        trace::protocol_e its_protocol = _receiver->is_local() ? trace::protocol_e::local
                : _receiver->is_reliable()                     ? trace::protocol_e::tcp
                                                               : trace::protocol_e::udp;
        its_header.prepare(its_remote_address, _remote_port, its_protocol, false, its_instance);
        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _length);
    }
}

bool routing_manager_impl::on_message(service_t _service, instance_t _instance, const byte_t* _data, length_t _size, bool _reliable,
                                      client_t _bound_client, const vsomeip_sec_client_t* _sec_client, uint8_t _check_status,
                                      bool _is_from_remote) {
    client_t its_client;
    bool is_forwarded(true);

    if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        its_client = find_local_client(_service, _instance);
    } else {
        its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);
    }

    if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        is_forwarded = deliver_notification(_service, _instance, _data, _size, _reliable, _bound_client, _sec_client, _check_status,
                                            _is_from_remote);
    } else {
        send(its_client, _data, _size, _instance, _reliable, _bound_client, _sec_client, _check_status, _is_from_remote,
             false); // send to proxy
    }
    return is_forwarded;
}

void routing_manager_impl::on_notification(client_t _client, service_t _service, instance_t _instance, const byte_t* _data, length_t _size,
                                           bool _notify_one) {
    event_t its_event_id = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
    std::shared_ptr<event> its_event = find_event(_service, _instance, its_event_id);
    if (its_event) {
        uint32_t its_length = utility::get_payload_size(_data, _size);
        std::shared_ptr<payload> its_payload = runtime::get()->create_payload(&_data[VSOMEIP_PAYLOAD_POS], its_length);

        if (_notify_one) {
            notify_one(_service, _instance, its_event->get_event(), its_payload, _client, true);
        } else {
            if (its_event->is_field()) {
                if (!its_event->set_payload_notify_pending(its_payload)) {
                    its_event->set_payload(its_payload, false);
                }
            } else {
                its_event->set_payload(its_payload, VSOMEIP_ROUTING_CLIENT, true);
            }
        }
    }
}

void routing_manager_impl::on_stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                 minor_version_t _minor) {
    std::scoped_lock its_lock{offer_serialization_mutex_};
    on_stop_offer_service_unlocked(_client, _service, _instance, _major, _minor);
}

void routing_manager_impl::on_stop_offer_service_unlocked(client_t _client, service_t _service, instance_t _instance,
                                                          major_version_t _major, minor_version_t _minor) {

    VSOMEIP_INFO << "ON_STOP_OFFER_SERVICE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << std::dec
                 << static_cast<int>(_major) << "." << _minor << "]";
    {
        std::scoped_lock its_lock{local_services_mutex_};
        if (auto entry = local_services_table_.find_entry(_service, _instance); entry) {
            auto [stored_service, stored_instance, stored_major, stored_minor, stored_client] = *entry;
            if (stored_major != _major || stored_minor != _minor || stored_client != _client) {
                VSOMEIP_WARNING_P << "Trying to delete service not matching exactly the one offered previously: [" << hex4(_service) << "."
                                  << hex4(_instance) << "." << static_cast<std::uint32_t>(_major) << "." << _minor
                                  << "] by application: " << hex4(_client) << ". Stored: [" << hex4(_service) << "." << hex4(_instance)
                                  << "." << static_cast<std::uint32_t>(stored_major) << "." << stored_minor
                                  << "] by application: " << hex4(stored_client);
            }
            if (stored_client == _client) {
                local_services_table_.remove(_service, _instance);
            }
        }
    }

    stop_offer_service_base(_client, _service, _instance, _major, _minor);

    /**
     * Hold reliable & unreliable server-endpoints from service info
     * because if "del_routing_info" is called those entries could be freed
     * and we can't be sure this happens synchronous when SD is active.
     * After triggering "del_routing_info" this endpoints gets cleanup up
     * within this method if they not longer used by any other local service.
     */
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info) {
        std::shared_ptr<boardnet_endpoint> its_reliable_endpoint = its_info->get_endpoint(true);
        std::shared_ptr<boardnet_endpoint> its_unreliable_endpoint = its_info->get_endpoint(false);

        for (const auto& ep : {its_reliable_endpoint, its_unreliable_endpoint}) {
            if (ep == nullptr) {
                continue;
            }

            if (ep_mgr_impl_->remove_instance(_service, ep.get())
                && ep_mgr_impl_->remove_server_endpoint(ep->get_local_port(), ep->is_reliable())) {
                ep->stop(false);
            }
        }

        del_routing_info(_service, _instance, its_reliable_endpoint != nullptr, its_unreliable_endpoint != nullptr, false);

        if (discovery_) {
            if (its_info->get_major() == _major && its_info->get_minor() == _minor)
                discovery_->stop_offer_service(its_info);
        }

        std::set<std::shared_ptr<eventgroupinfo>> its_eventgroup_info_set;
        {
            std::scoped_lock its_eventgroups_lock{eventgroups_mutex_};
            const auto search = eventgroups_.find(service_instance_t{_service, _instance});

            if (search != eventgroups_.end()) {
                for (const auto& [eventgroup_id, eventgroup_info] : search->second) {
                    its_eventgroup_info_set.insert(eventgroup_info);
                }
            }
        }

        for (auto e : its_eventgroup_info_set) {
            e->clear_remote_subscriptions();
        }
    }

    // NOTE: Order matters. The 'erase_offer_command' must be done after the on_availability to ensure that the process has completed
    // before starting the next one, otherwise, we may have the availability being reported in the wrong order
    on_availability(_service, _instance, availability_state_e::AS_UNAVAILABLE, _major, _minor);
    stub_->on_stop_offer_service(_client, _service, _instance, _major, _minor);
    erase_offer_command(_service, _instance);
}

bool routing_manager_impl::has_subscribed_eventgroup(service_t _service, instance_t _instance) const {

    std::scoped_lock its_lock{eventgroups_mutex_};
    const auto search = eventgroups_.find(service_instance_t{_service, _instance});
    if (search != eventgroups_.end()) {
        for (const auto& e : search->second) {
            for (const auto& its_event : e.second->get_events()) {
                if (!its_event->get_subscribers().empty()) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool routing_manager_impl::deliver_notification(service_t _service, instance_t _instance, const byte_t* _data, length_t _length,
                                                bool _reliable, [[maybe_unused]] client_t _bound_client,
                                                [[maybe_unused]] const vsomeip_sec_client_t* _sec_client, uint8_t _status_check,
                                                bool _is_from_remote) {

    event_t its_event_id = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
    client_t its_client_id = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);

    std::shared_ptr<event> its_event = find_event(_service, _instance, its_event_id);
    if (its_event) {
        if (!its_event->is_provided()) {
            if (its_event->get_subscribers().size() == 0) {
                // no subscribers for this specific event / check subscriptions
                // to other events of the event's eventgroups
                bool cache_event = false;
                for (const auto eg : its_event->get_eventgroups()) {
                    std::shared_ptr<eventgroupinfo> egi = find_eventgroup(_service, _instance, eg);
                    if (egi) {
                        for (const auto& e : egi->get_events()) {
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
                    VSOMEIP_WARNING_P << "Dropping [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(its_event_id)
                                      << "]. No subscription to corresponding eventgroup.";
                    return true; // as there is nothing to do
                }
            }
        }

        // Update the event with information which might not have been previously available.
        if (auto service = find_service(_service, _instance)) {
            its_event->set_version(service->get_major());
            its_event->set_reliability(_reliable ? reliability_type_e::RT_RELIABLE : reliability_type_e::RT_UNRELIABLE);
        }

        auto its_length = utility::get_payload_size(_data, _length);
        auto its_payload = runtime::get()->create_payload(&_data[VSOMEIP_PAYLOAD_POS], its_length);

        // incoming events statistics
        (void)insert_event_statistics(_service, _instance, its_event_id, its_length);

        // Ignore the filter for messages coming from other local clients
        // as the filter was already applied there.
        auto its_subscribers = its_event->update_and_get_filtered_subscribers(its_payload, _is_from_remote);
        if (its_event->get_type() != event_type_e::ET_SELECTIVE_EVENT) {
            for (const auto its_local_client : its_subscribers) {
                if (std::shared_ptr<local_endpoint> its_local_target = find_routing_endpoint(its_local_client); its_local_target) {
                    send_local(its_local_target, VSOMEIP_ROUTING_CLIENT, _data, _length, _instance, _reliable, protocol::id_e::SEND_ID,
                               _status_check, VSOMEIP_ROUTING_CLIENT);
                }
            }
        } else {
            if (its_subscribers.find(its_client_id) != its_subscribers.end()) {
                if (std::shared_ptr<local_endpoint> its_local_target = find_routing_endpoint(its_client_id); its_local_target) {
                    send_local(its_local_target, VSOMEIP_ROUTING_CLIENT, _data, _length, _instance, _reliable, protocol::id_e::SEND_ID,
                               _status_check, VSOMEIP_ROUTING_CLIENT);
                }
            }
        }

    } else {
        if (has_subscribed_eventgroup(_service, _instance)) {
            if (!is_suppress_event(_service, _instance, its_event_id)) {
                VSOMEIP_WARNING_P << "Caching unregistered event [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(its_event_id)
                                  << "]";
            }

            register_event(host_->get_client(), _service, _instance, its_event_id, {}, event_type_e::ET_UNKNOWN,
                           _reliable ? reliability_type_e::RT_RELIABLE : reliability_type_e::RT_UNRELIABLE,
                           std::chrono::milliseconds::zero(), false, true, nullptr, true, true, true);

            its_event = find_event(_service, _instance, its_event_id);
            if (its_event) {
                auto its_length = utility::get_payload_size(_data, _length);
                auto its_payload = runtime::get()->create_payload(&_data[VSOMEIP_PAYLOAD_POS], its_length);
                its_event->set_payload(its_payload, true);
            } else
                VSOMEIP_ERROR_P << "Event registration failed [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(its_event_id)
                                << "]";
        } else if (!is_suppress_event(_service, _instance, its_event_id)) {
            VSOMEIP_WARNING_P << "Dropping unregistered event [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(its_event_id)
                              << "] Service has no subscribed eventgroup.";
        }
    }
    return true;
}

bool routing_manager_impl::is_suppress_event(service_t _service, instance_t _instance, event_t _event) const {
    bool status = configuration_->check_suppress_events(_service, _instance, _event);

    return status;
}

std::shared_ptr<boardnet_endpoint> routing_manager_impl::create_service_discovery_endpoint(const std::string& _address, uint16_t _port,
                                                                                           bool _reliable) {
    auto its_service_endpoint = ep_mgr_impl_->find_server_endpoint(_port, _reliable);
    if (!its_service_endpoint) {
        try {
            its_service_endpoint = ep_mgr_impl_->create_server_endpoint(_port, _reliable, true);

            if (its_service_endpoint) {
                sd_info_ = std::make_shared<serviceinfo>(VSOMEIP_SD_SERVICE, VSOMEIP_SD_INSTANCE, ANY_MAJOR, ANY_MINOR, DEFAULT_TTL,
                                                         false); // false, because we do _not_ want to announce it...
                sd_info_->set_endpoint(its_service_endpoint, _reliable);
                its_service_endpoint->add_default_target(VSOMEIP_SD_SERVICE, _address, _port);
                if (!_reliable) {
                    auto its_server_endpoint = std::dynamic_pointer_cast<udp_server_endpoint_impl>(its_service_endpoint);
                    if (its_server_endpoint) {
                        its_server_endpoint->set_unicast_sent_callback(std::bind(&sd::service_discovery::sent_messages, discovery_.get(),
                                                                                 std::placeholders::_1, std::placeholders::_2,
                                                                                 std::placeholders::_3));
                        its_server_endpoint->set_receive_own_multicast_messages(true);
                        its_server_endpoint->set_sent_multicast_received_callback(std::bind(&sd::service_discovery::sent_messages,
                                                                                            discovery_.get(), std::placeholders::_1,
                                                                                            std::placeholders::_2, std::placeholders::_3));
                        its_server_endpoint->join(_address);
                    }
                }
            } else {
                VSOMEIP_ERROR << "Service Discovery endpoint could not be created. Please check your network configuration.";
            }
        } catch (const std::exception& e) {
            VSOMEIP_ERROR << "Server endpoint creation failed: Service Discovery endpoint could not be created: " << e.what();
        }
    }
    return its_service_endpoint;
}

services_t routing_manager_impl::get_offered_services() const {
    services_t its_services;
    for (const auto& [service, instances] : get_services()) {
        for (const auto& [instance, info] : instances) {
            if (info) {
                if (info->is_local()) {
                    its_services[service][instance] = info;
                }
            } else {
                VSOMEIP_ERROR_P << "Found instance with NULL ServiceInfo [" << hex4(service) << ":" << hex4(instance) << "]";
            }
        }
    }
    return its_services;
}

std::shared_ptr<serviceinfo> routing_manager_impl::get_offered_service(service_t _service, instance_t _instance) const {
    std::shared_ptr<serviceinfo> its_info;
    its_info = find_service(_service, _instance);
    if (its_info && !its_info->is_local()) {
        its_info.reset();
    }
    return its_info;
}

std::map<instance_t, std::shared_ptr<serviceinfo>> routing_manager_impl::get_offered_service_instances(service_t _service) const {
    std::map<instance_t, std::shared_ptr<serviceinfo>> its_instances;
    const services_t its_services(get_services());
    const auto found_service = its_services.find(_service);
    if (found_service != its_services.end()) {
        for (const auto& [instance, info] : found_service->second) {
            if (info->is_local()) {
                its_instances[instance] = info;
            }
        }
    }
    return its_instances;
}

bool routing_manager_impl::is_acl_message_allowed(boardnet_endpoint* _receiver, service_t _service, instance_t _instance,
                                                  const boost::asio::ip::address& _remote_address) const {
    if (message_acceptance_handler_ && _receiver) {
        // Check the ACL whitelist rules if shall accepts the message
        std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
        const bool is_local = its_info && its_info->is_local();

        message_acceptance_t message_acceptance{_remote_address.to_v4().to_uint(), _receiver->get_local_port(), is_local, _service,
                                                _instance};
        if (!message_acceptance_handler_(message_acceptance)) {
            VSOMEIP_WARNING << "Message from " << _remote_address.to_string() << " with service/instance " << hex4(_service) << "/"
                            << hex4(_instance) << " was rejected by the ACL check.";
            return false;
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////
void routing_manager_impl::init_service_info(service_t _service, instance_t _instance, bool _is_local_service) {
    std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
    if (!its_info) {
        VSOMEIP_ERROR_P << "Couldn't find serviceinfo for service: [" << hex4(_service) << "." << hex4(_instance)
                        << "] is_local_service=" << _is_local_service;
        return;
    }
    if (configuration_) {
        // Create server endpoints for local services only
        if (_is_local_service) {
            const bool is_someip = configuration_->is_someip(_service, _instance);
            uint16_t its_reliable_port = configuration_->get_reliable_port(_service, _instance);
            bool _is_found(false);
            if (ILLEGAL_PORT != its_reliable_port) {
                auto its_reliable_endpoint =
                        ep_mgr_impl_->find_or_create_server_endpoint(its_reliable_port, true, is_someip, _service, _instance, _is_found);
                if (its_reliable_endpoint) {
                    its_info->set_endpoint(its_reliable_endpoint, true);
                }
            }
            uint16_t its_unreliable_port = configuration_->get_unreliable_port(_service, _instance);
            if (ILLEGAL_PORT != its_unreliable_port) {
                auto its_unreliable_endpoint =
                        ep_mgr_impl_->find_or_create_server_endpoint(its_unreliable_port, false, is_someip, _service, _instance, _is_found);
                if (its_unreliable_endpoint) {
                    its_info->set_endpoint(its_unreliable_endpoint, false);
                }
            }

            if (ILLEGAL_PORT == its_reliable_port && ILLEGAL_PORT == its_unreliable_port) {
                VSOMEIP_INFO << "Port configuration missing for [" << hex4(_service) << "." << hex4(_instance) << "]. Service is internal.";
            }
            its_info->set_is_in_preparation(false);
        }
    } else {
        VSOMEIP_ERROR << "Missing vsomeip configuration.";
    }
}

void routing_manager_impl::remove_local(client_t _client) {

    std::set<std::tuple<service_t, instance_t, eventgroup_t>> its_clients_subscriptions;
    its_clients_subscriptions = get_subscriptions(_client);

    for (const auto& s : its_clients_subscriptions) {
        auto [service, instance, eventgroup] = s;
        {
            std::scoped_lock its_lock{remote_subscription_state_mutex_};
            remote_subscription_state_.erase(std::tuple_cat(s, std::make_tuple(_client)));
        }
        unsubscribe(_client, service, instance, eventgroup, ANY_EVENT);
    }
    {
        std::scoped_lock its_lock(local_services_mutex_);
        // Finally remove all services that are implemented by the client.
        auto removed = local_services_table_.remove_all_for_client(_client);
    }

    for (const auto& s : get_requested_services(_client)) {
        release_service(_client, s.service_, s.instance_);
    }
    ep_mgr_impl_->remove_routing_endpoint(_client);
}

bool routing_manager_impl::is_field(service_t _service, instance_t _instance, event_t _event) const {
    std::scoped_lock its_lock{events_mutex_};
    const auto search = events_.find(service_instance_t{_service, _instance});

    if (search != events_.end()) {
        const auto find_event = search->second.find(_event);
        if (find_event != search->second.end()) {
            return find_event->second->is_field();
        }
    }

    return false;
}

// only called from the SD
void routing_manager_impl::add_routing_info(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor,
                                            ttl_t _ttl, const boost::asio::ip::address& _reliable_address, uint16_t _reliable_port,
                                            const boost::asio::ip::address& _unreliable_address, uint16_t _unreliable_port) {

    if (is_suspended()) {
        VSOMEIP_INFO_P << "We are suspended --> do nothing.";
        return;
    }

    // Create/Update service info
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (!its_info) {
        boost::asio::ip::address its_unicast_address = configuration_->get_unicast_address();
        bool is_local(false);
        if (_reliable_port != ILLEGAL_PORT && its_unicast_address == _reliable_address)
            is_local = true;
        else if (_unreliable_port != ILLEGAL_PORT && its_unicast_address == _unreliable_address)
            is_local = true;

        its_info = create_service_info(_service, _instance, _major, _minor, _ttl, is_local);
        init_service_info(_service, _instance, is_local);
    } else if (its_info->is_local()) {
        // We received a service info for a service which is already offered locally
        VSOMEIP_ERROR_P << "Rejecting routing info. Remote: "
                        << ((_reliable_port != ILLEGAL_PORT) ? _reliable_address.to_string() : _unreliable_address.to_string())
                        << " is trying to offer [" << hex4(_service) << "." << hex4(_instance) << "." << static_cast<std::uint32_t>(_major)
                        << "." << _minor << "] on port " << ((_reliable_port != ILLEGAL_PORT) ? _reliable_port : _unreliable_port)
                        << " offered previously on this node: [" << hex4(_service) << "." << hex4(_instance) << "."
                        << static_cast<std::uint32_t>(its_info->get_major()) << "." << its_info->get_minor() << "]";
        return;
    } else {
        its_info->set_ttl(_ttl);
    }

    // Check whether remote services are unchanged
    bool is_reliable_known(false);
    bool is_unreliable_known(false);
    ep_mgr_impl_->is_remote_service_known(_service, _instance, _major, _minor, _reliable_address, _reliable_port, &is_reliable_known,
                                          _unreliable_address, _unreliable_port, &is_unreliable_known);

    bool udp_inserted(false);
    // Add endpoint(s) if necessary
    if (_reliable_port != ILLEGAL_PORT && !is_reliable_known) {
        std::shared_ptr<endpoint_definition> endpoint_def_tcp =
                endpoint_definition::get(_reliable_address, _reliable_port, true, _service, _instance);
        if (_unreliable_port != ILLEGAL_PORT && !is_unreliable_known) {
            std::shared_ptr<endpoint_definition> endpoint_def_udp =
                    endpoint_definition::get(_unreliable_address, _unreliable_port, false, _service, _instance);
            ep_mgr_impl_->add_remote_service_info(_service, _instance, endpoint_def_tcp, endpoint_def_udp);
            udp_inserted = true;
        } else {
            ep_mgr_impl_->add_remote_service_info(_service, _instance, endpoint_def_tcp);
        }

        // check if service was requested and establish TCP connection if necessary
        {
            bool connected(false);
            std::scoped_lock its_lock_inner{requested_services_mutex_};
            for (const client_t its_client : get_requesters_unlocked(_service, _instance, _major)) {
                // SWS_SD_00376 establish TCP connection to service
                // service is marked as available later in on_connect()
                if (!connected) {
                    if (udp_inserted) {
                        // atomically create reliable and unreliable endpoint
                        ep_mgr_impl_->find_or_create_remote_client(_service, _instance);
                    } else {
                        ep_mgr_impl_->find_or_create_remote_client(_service, _instance, true);
                    }
                    connected = true;
                }
                its_info->add_client(its_client);
            }
        }
    } else if (_reliable_port != ILLEGAL_PORT && is_reliable_known) {
        std::scoped_lock its_lock_inner{requested_services_mutex_};
        if (has_requester_unlocked(_service, _instance, _major, _minor)) {
            auto ep = its_info->get_endpoint(true);
            if (ep) {
                if (ep->is_established()
                    && !stub_->contained_in_routing_info(VSOMEIP_ROUTING_CLIENT, _service, _instance, its_info->get_major(),
                                                         its_info->get_minor())) {
                    stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, its_info->get_major(), its_info->get_minor());
                    on_availability(_service, _instance, availability_state_e::AS_AVAILABLE, its_info->get_major(), its_info->get_minor());
                    if (discovery_) {
                        discovery_->on_endpoint_connected(_service, _instance, ep);
                    }
                }
            } else {
                // no endpoint yet, but requested -> create one

                // SWS_SD_00376 establish TCP connection to service
                // service is marked as available later in on_connect()
                ep_mgr_impl_->find_or_create_remote_client(_service, _instance, true);
                for (const client_t its_client : get_requesters_unlocked(_service, _instance, _major)) {
                    its_info->add_client(its_client);
                }
            }
        } else {
            on_availability(_service, _instance, availability_state_e::AS_OFFERED, its_info->get_major(), its_info->get_minor());
        }
    }

    if (_unreliable_port != ILLEGAL_PORT && !is_unreliable_known) {
        if (!udp_inserted) {
            std::shared_ptr<endpoint_definition> endpoint_def =
                    endpoint_definition::get(_unreliable_address, _unreliable_port, false, _service, _instance);
            ep_mgr_impl_->add_remote_service_info(_service, _instance, endpoint_def);
            // check if service was requested and increase requester count if necessary
            {
                bool connected(false);
                std::scoped_lock its_lock_inner{requested_services_mutex_};
                for (const client_t its_client : get_requesters_unlocked(_service, _instance, _major)) {
                    if (!connected) {
                        ep_mgr_impl_->find_or_create_remote_client(_service, _instance, false);
                        connected = true;
                    }
                    its_info->add_client(its_client);
                }
            }
        }
    } else if (_unreliable_port != ILLEGAL_PORT && is_unreliable_known) {
        std::scoped_lock its_lock_inner{requested_services_mutex_};
        if (has_requester_unlocked(_service, _instance, _major, _minor)) {
            if (_reliable_port == ILLEGAL_PORT && !is_reliable_known
                && !stub_->contained_in_routing_info(VSOMEIP_ROUTING_CLIENT, _service, _instance, its_info->get_major(),
                                                     its_info->get_minor())) {
                auto ep = its_info->get_endpoint(false);
                if (ep) {
                    if (ep->is_established()) {
                        stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, its_info->get_major(), its_info->get_minor());
                        on_availability(_service, _instance, availability_state_e::AS_AVAILABLE, its_info->get_major(),
                                        its_info->get_minor());
                        if (discovery_) {
                            discovery_->on_endpoint_connected(_service, _instance, ep);
                        }
                    } else {
                        if (ep->is_closed()) {
                            ep->start();
                        }
                    }
                } else {
                    ep_mgr_impl_->find_or_create_remote_client(_service, _instance, false);
                    for (const client_t its_client : get_requesters_unlocked(_service, _instance, _major)) {
                        its_info->add_client(its_client);
                    }
                }
            }
        } else {
            on_availability(_service, _instance, availability_state_e::AS_OFFERED, _major, _minor);
        }
    }
}

void routing_manager_impl::del_routing_info(service_t _service, instance_t _instance, bool _has_reliable, bool _has_unreliable,
                                            bool _trigger_availability) {

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (!its_info)
        return;

    if (_trigger_availability) {
        on_availability(_service, _instance, availability_state_e::AS_UNAVAILABLE, its_info->get_major(), its_info->get_minor());
        stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, its_info->get_major(), its_info->get_minor());
    }

    // Implicit unsubscribe

    std::vector<std::shared_ptr<event>> its_events;
    {
        std::scoped_lock its_lock{eventgroups_mutex_};
        const auto search = eventgroups_.find(service_instance_t{_service, _instance});

        if (search != eventgroups_.end()) {
            for (const auto& [eventgroup_id, eventgroup_info] : search->second) {
                // As the service is gone, all subscriptions to its events
                // do no longer exist and the last received payload is no
                // longer valid.
                for (auto& its_event : eventgroup_info->get_events()) {
                    const auto its_subscribers = its_event->get_subscribers();
                    for (const auto its_subscriber : its_subscribers) {
                        its_event->remove_subscriber(eventgroup_id, its_subscriber);
                    }
                    its_events.push_back(its_event);
                }
            }
        }
    }

    for (const auto& e : its_events) {
        if (e->is_set()) {
            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(e->get_event()) << "]";
        }
        e->unset_payload(true);
    }

    {
        std::scoped_lock its_lock{remote_subscription_state_mutex_};
        std::set<std::tuple<service_t, instance_t, eventgroup_t, client_t>> its_invalid;

        for (const auto& its_state : remote_subscription_state_) {
            if (std::get<0>(its_state.first) == _service && std::get<1>(its_state.first) == _instance) {
                its_invalid.insert(its_state.first);
            }
        }

        for (const auto& its_key : its_invalid)
            remote_subscription_state_.erase(its_key);
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
    std::map<service_t, std::vector<instance_t>> its_expired_offers;

    {
        std::scoped_lock its_lock{services_remote_mutex_};
        for (const auto& [service, instances] : services_remote_) {
            for (const auto& [instance, info] : instances) {
                ttl_t its_ttl = info->get_ttl();
                if (its_ttl < DEFAULT_TTL) { // do not touch "forever"
                    std::chrono::milliseconds precise_ttl = info->get_precise_ttl();
                    if (precise_ttl.count() < _elapsed.count() || precise_ttl.count() == 0) {
                        info->set_ttl(0);
                        its_expired_offers[service].push_back(instance);
                    } else {
                        std::chrono::milliseconds its_new_ttl(precise_ttl - _elapsed);
                        info->set_precise_ttl(its_new_ttl);
                    }
                }
            }
        }
    }

    for (const auto& [service, instances] : its_expired_offers) {
        for (const auto& instance : instances) {
            if (discovery_) {
                discovery_->unsubscribe_all(service, instance);
                // go to Initial Wait Phase
                discovery_->reset_request_sent_counter(service, instance);
            }
            del_routing_info(service, instance, true, true, true);
            VSOMEIP_INFO_P << "Update_routing_info: elapsed=" << _elapsed.count() << " : delete service/instance " << hex4(service) << "."
                           << hex4(instance);
        }
    }
}

void routing_manager_impl::expire_services(const boost::asio::ip::address& _address) {
    expire_services(_address, port_range_t(ANY_PORT, ANY_PORT), false);
}

void routing_manager_impl::expire_services(const boost::asio::ip::address& _address, std::uint16_t _port, bool _reliable) {
    expire_services(_address, port_range_t(_port, _port), _reliable);
}

void routing_manager_impl::expire_services(const boost::asio::ip::address& _address, const port_range_t& _range, bool _reliable) {
    std::map<service_t, std::vector<instance_t>> its_expired_offers;

    const bool expire_all = _range.is_any();

    for (auto& [service, instances] : get_services_remote()) {
        for (auto& [instance, info] : instances) {
            boost::asio::ip::address its_address;
            std::shared_ptr<client_endpoint> its_client_endpoint =
                    std::dynamic_pointer_cast<client_endpoint>(info->get_endpoint(_reliable));
            if (!its_client_endpoint && expire_all) {
                its_client_endpoint = std::dynamic_pointer_cast<client_endpoint>(info->get_endpoint(!_reliable));
            }
            if (its_client_endpoint) {
                if ((expire_all || _range.contains(its_client_endpoint->get_remote_port()))
                    && its_client_endpoint->get_remote_address(its_address) && its_address == _address) {
                    if (discovery_) {
                        discovery_->unsubscribe_all(service, instance);
                    }
                    its_expired_offers[service].push_back(instance);
                }
            }
        }
    }

    for (auto& [service, instances] : its_expired_offers) {
        for (auto& instance : instances) {
            VSOMEIP_INFO_P << "for address: " << _address << " : delete service/instance " << hex4(service) << "." << hex4(instance)
                           << " port [" << _range.start_ << "," << _range.end_ << "] reliability=" << std::boolalpha << _reliable;
            del_routing_info(service, instance, true, true, true);
        }
    }
}

void routing_manager_impl::expire_subscriptions(const boost::asio::ip::address& _address) {
    expire_subscriptions(_address, port_range_t(ANY_PORT, ANY_PORT), false);
}

void routing_manager_impl::expire_subscriptions(const boost::asio::ip::address& _address, std::uint16_t _port, bool _reliable) {
    expire_subscriptions(_address, port_range_t(_port, _port), _reliable);
}

void routing_manager_impl::expire_subscriptions(const boost::asio::ip::address& _address, const port_range_t& _range, bool _reliable) {
    std::stringstream log_header;
    log_header << "{remote=" << _address << "}: ";

    const bool expire_all = _range.is_any();
    eventgroups_t its_eventgroups;
    {
        std::scoped_lock its_lock{eventgroups_mutex_};
        its_eventgroups = eventgroups_;
    }

    for (const auto& [key, its_eventgroup] : its_eventgroups) {
        for (auto& [eventgroup_id, its_info] : its_eventgroup) {
            for (auto its_subscription : its_info->get_remote_subscriptions()) {
                std::stringstream subscription_details;
                subscription_details << " eventgroup=" << hex4(key.service()) << "." << hex4(key.instance()) << "." << hex4(eventgroup_id)
                                     << " id=" << hex4(its_subscription->get_id());

                if (its_subscription->is_forwarded()) {
                    VSOMEIP_WARNING_P << log_header.str() << "Subscription replaced." << subscription_details.str();
                    continue;
                }

                auto its_ep_definition = _reliable ? its_subscription->get_reliable() : its_subscription->get_unreliable();
                if (!its_ep_definition && expire_all) {
                    its_ep_definition = _reliable ? its_subscription->get_unreliable() : its_subscription->get_reliable();
                }

                if (!its_ep_definition) {
                    continue;
                }

                const bool in_port_range = _range.is_any() || _range.contains(its_ep_definition->get_remote_port());
                if (its_ep_definition->get_address() == _address && in_port_range) {
                    if (its_subscription->is_expired()) {
                        VSOMEIP_WARNING_P << log_header.str() << "Subscription already expired." << subscription_details.str();
                    } else {
                        VSOMEIP_INFO_P << log_header.str() << "Removing subscription." << subscription_details.str();
                        its_subscription->set_expired();
                        on_remote_unsubscribe(its_subscription);
                    }
                }
            }
        }
    }
}

void routing_manager_impl::init_routing_info() {
    VSOMEIP_INFO << "Service Discovery disabled. Using static routing information.";
    for (auto i : configuration_->get_remote_services()) {
        boost::asio::ip::address its_address(boost::asio::ip::make_address(configuration_->get_unicast_address(i.first, i.second)));
        uint16_t its_reliable_port = configuration_->get_reliable_port(i.first, i.second);
        uint16_t its_unreliable_port = configuration_->get_unreliable_port(i.first, i.second);

        if (its_reliable_port != ILLEGAL_PORT || its_unreliable_port != ILLEGAL_PORT) {

            add_routing_info(i.first, i.second, DEFAULT_MAJOR, DEFAULT_MINOR, DEFAULT_TTL, its_address, its_reliable_port, its_address,
                             its_unreliable_port);

            if (its_reliable_port != ILLEGAL_PORT) {
                ep_mgr_impl_->find_or_create_remote_client(i.first, i.second, true);
            }
            if (its_unreliable_port != ILLEGAL_PORT) {
                ep_mgr_impl_->find_or_create_remote_client(i.first, i.second, false);
            }
        }
    }
}

void routing_manager_impl::on_remote_subscribe(std::shared_ptr<remote_subscription>& _subscription,
                                               const remote_subscription_callback_t& _callback) {

    auto its_eventgroupinfo = _subscription->get_eventgroupinfo();
    if (!its_eventgroupinfo) {
        VSOMEIP_ERROR_P << "Eventgroupinfo is invalid";
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
        uint16_t its_port = configuration_->get_reliable_port(its_service, its_instance);
        its_reliable->set_remote_port(its_port);
    }

    auto its_unreliable = _subscription->get_unreliable();
    if (its_unreliable) {
        uint16_t its_port = configuration_->get_unreliable_port(its_service, its_instance);
        its_unreliable->set_remote_port(its_port);
    }

    // Calculate expiration time
    const std::chrono::steady_clock::time_point its_expiration = std::chrono::steady_clock::now() + std::chrono::seconds(its_ttl);

    // Try to update the subscription. This will fail, if the subscription does
    // not exist or is still (partly) pending.
    remote_subscription_id_t its_id;
    std::set<client_t> its_added;
    std::unique_lock<std::mutex> its_update_lock{update_remote_subscription_mutex_};
    if (_subscription->is_expired()) {
        VSOMEIP_WARNING_P << "Remote subscription already expired";
        return;
    } else {
        _subscription->set_forwarded();
    }

    auto its_result = its_eventgroupinfo->update_remote_subscription(_subscription, its_expiration, its_added, its_id, true);
    if (its_result) {
        if (!_subscription->is_pending()) { // resubscription without change
            its_update_lock.unlock();
            _callback(_subscription);
            its_update_lock.lock();
        } else if (!its_added.empty()) { // new clients for a selective subscription
            const client_t its_offering_client = find_local_client(its_service, its_instance);
            send_subscription(its_offering_client, its_service, its_instance, its_eventgroup, its_major, its_added,
                              _subscription->get_id());
        } else { // identical subscription is not yet processed
            std::stringstream its_warning;
            its_warning << "A remote subscription is already pending [" << hex4(its_service) << "." << hex4(its_instance) << "."
                        << hex4(its_eventgroup) << "] from ";
            if (its_reliable && its_unreliable)
                its_warning << "[";
            if (its_reliable)
                its_warning << its_reliable->get_address().to_string() << ":" << its_reliable->get_port();
            if (its_reliable && its_unreliable)
                its_warning << ", ";
            if (its_unreliable)
                its_warning << its_unreliable->get_address().to_string() << ":" << its_unreliable->get_port();
            if (its_reliable && its_unreliable)
                its_warning << "]";
            VSOMEIP_WARNING_P << its_warning.str();

            its_update_lock.unlock();
            _callback(_subscription);
            its_update_lock.lock();
        }
    } else { // new subscription
        if (its_eventgroupinfo->is_remote_subscription_limit_reached(_subscription)) {
            _subscription->set_all_client_states(remote_subscription_state_e::SUBSCRIPTION_NACKED);

            its_update_lock.unlock();
            _callback(_subscription);
            its_update_lock.lock();
            _subscription->clear_destiny();
            return;
        }

        auto its_id_inner = its_eventgroupinfo->add_remote_subscription(_subscription);

        const client_t its_offering_client = find_local_client(its_service, its_instance);
        send_subscription(its_offering_client, its_service, its_instance, its_eventgroup, its_major, _subscription->get_clients(),
                          its_id_inner);
    }
    _subscription->clear_destiny();
}

void routing_manager_impl::on_remote_unsubscribe(std::shared_ptr<remote_subscription>& _subscription) {
    std::shared_ptr<eventgroupinfo> its_info = _subscription->get_eventgroupinfo();
    if (!its_info) {
        VSOMEIP_ERROR_P << "Received Unsubscribe for unregistered eventgroup.";
        return;
    }

    const auto its_service = its_info->get_service();
    const auto its_instance = its_info->get_instance();
    const auto its_eventgroup = its_info->get_eventgroup();
    const auto its_major = its_info->get_major();

    // Get remote port(s)
    auto its_reliable = _subscription->get_reliable();
    if (its_reliable) {
        uint16_t its_port = configuration_->get_reliable_port(its_service, its_instance);
        its_reliable->set_remote_port(its_port);
    }

    auto its_unreliable = _subscription->get_unreliable();
    if (its_unreliable) {
        uint16_t its_port = configuration_->get_unreliable_port(its_service, its_instance);
        its_unreliable->set_remote_port(its_port);
    }

    remote_subscription_id_t its_id(0);
    std::set<client_t> its_removed;
    std::unique_lock<std::mutex> its_update_lock{update_remote_subscription_mutex_};
    auto its_result = its_info->update_remote_subscription(_subscription, std::chrono::steady_clock::now(), its_removed, its_id, false);

    if (its_result) {
        const client_t its_offering_client = find_local_client(its_service, its_instance);
        send_unsubscription(its_offering_client, its_service, its_instance, its_eventgroup, its_major, its_removed, its_id);
    }
}

void routing_manager_impl::on_subscribe_ack_with_multicast(service_t _service, instance_t _instance,
                                                           const boost::asio::ip::address& _sender,
                                                           const boost::asio::ip::address& _address, uint16_t _port) {
    ep_mgr_impl_->find_or_create_multicast_endpoint(_service, _instance, _sender, _address, _port);
}

void routing_manager_impl::on_subscribe_ack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                            event_t _event, remote_subscription_id_t _id) {
    std::unique_lock its_lock{remote_subscription_state_mutex_, std::defer_lock}; // only lock if received an external subscribe_ack
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        auto its_subscription = its_eventgroup->get_remote_subscription(_id);
        if (its_subscription) {
            its_subscription->set_client_state(_client, remote_subscription_state_e::SUBSCRIPTION_ACKED);

            auto its_parent = its_subscription->get_parent();
            if (its_parent) {
                its_parent->set_client_state(_client, remote_subscription_state_e::SUBSCRIPTION_ACKED);
                if (!its_subscription->is_pending()) {
                    its_eventgroup->remove_remote_subscription(_id);
                }
            }

            if (discovery_) {
                discovery_->update_remote_subscription(its_subscription);

                VSOMEIP_INFO << "REMOTE SUBSCRIBE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "."
                             << hex4(_eventgroup) << "] from " << its_subscription->get_subscriber()->get_address() << ":"
                             << its_subscription->get_subscriber()->get_port()
                             << (its_subscription->get_subscriber()->is_reliable() ? " reliable" : " unreliable")
                             << " was accepted. id=" << hex4(_id);

                return;
            }
        } else {
            its_lock.lock();
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
            for (const auto& its_event : its_eventgroup->get_events()) {
                if (_event == ANY_EVENT || _event == its_event->get_event()) {
                    const auto& its_subscribers = its_event->get_subscribers();
                    subscribed_clients.insert(its_subscribers.begin(), its_subscribers.end());
                }
            }
        } else {
            subscribed_clients.insert(_client);
        }

        for (const auto& its_subscriber : subscribed_clients) {
            stub_->send_subscribe_ack(its_subscriber, _service, _instance, _eventgroup, _event);
        }
    }
}

std::shared_ptr<boardnet_endpoint> routing_manager_impl::find_or_create_remote_client(service_t _service, instance_t _instance,
                                                                                      bool _reliable) {
    return ep_mgr_impl_->find_or_create_remote_client(_service, _instance, _reliable);
}

void routing_manager_impl::on_subscribe_nack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                             bool _remove, remote_subscription_id_t _id) {
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        auto its_subscription = its_eventgroup->get_remote_subscription(_id);
        if (its_subscription) {
            its_subscription->set_client_state(_client, remote_subscription_state_e::SUBSCRIPTION_NACKED);

            auto its_parent = its_subscription->get_parent();
            if (its_parent) {
                its_parent->set_client_state(_client, remote_subscription_state_e::SUBSCRIPTION_NACKED);
                if (!its_subscription->is_pending()) {
                    its_eventgroup->remove_remote_subscription(_id);
                }
            }

            if (discovery_) {
                discovery_->update_remote_subscription(its_subscription);
                VSOMEIP_INFO << "REMOTE SUBSCRIBE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "."
                             << hex4(_eventgroup) << "] from " << its_subscription->get_subscriber()->get_address() << ":"
                             << its_subscription->get_subscriber()->get_port()
                             << (its_subscription->get_subscriber()->is_reliable() ? " reliable" : " unreliable")
                             << " was not accepted. id=" << hex4(_id);
            }
            if (_remove)
                its_eventgroup->remove_remote_subscription(_id);
        }
    }
}

return_code_e routing_manager_impl::check_error(const byte_t* _data, length_t /*_size*/, instance_t _instance) const {

    // caller guarantees _size >= VSOMEIP_FULL_HEADER_SIZE

    if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS]) || utility::is_request_no_return(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
        major_version_t its_version = _data[VSOMEIP_INTERFACE_VERSION_POS];

        uint8_t its_protocol_version = _data[VSOMEIP_PROTOCOL_VERSION_POS];
        if (its_protocol_version != VSOMEIP_PROTOCOL_VERSION) {
            VSOMEIP_WARNING_P << "Received a message with unsupported protocol version 0x" << hex2(its_protocol_version)
                              << " for service 0x" << hex4(its_service);
            return return_code_e::E_WRONG_PROTOCOL_VERSION;
        }
        if (_instance == 0xFFFF) {
            VSOMEIP_WARNING_P << "Receiving endpoint is not configured for service 0x" << hex4(its_service);
            return return_code_e::E_UNKNOWN_SERVICE;
        }

        // Check interface version of service/instance
        auto its_info = find_service(its_service, _instance);
        if (its_info) {
            if (its_version != its_info->get_major()) {
                VSOMEIP_WARNING_P << "Received a message with unsupported interface version 0x" << hex2(its_version) << " for service 0x"
                                  << hex4(its_service);
                return return_code_e::E_WRONG_INTERFACE_VERSION;
            }
        }
        uint8_t its_return_code = _data[VSOMEIP_RETURN_CODE_POS];
        if (its_return_code != static_cast<byte_t>(return_code_e::E_OK)) {
            // Request calls must to have return code E_OK set!
            VSOMEIP_WARNING_P << "Received a message with unsupported return code 0x" << hex2(its_return_code) << " set for service 0x"
                              << hex4(its_service);
            return return_code_e::E_NOT_OK;
        }
    }

    return return_code_e::E_OK;
}

void routing_manager_impl::send_error(return_code_e _return_code, const byte_t* _data, length_t _size, instance_t _instance, bool _reliable,
                                      boardnet_endpoint* const _receiver, const boost::asio::ip::address& _remote_address,
                                      std::uint16_t _remote_port) {

    client_t its_client = 0;
    service_t its_service = 0;
    method_t its_method = 0;
    session_t its_session = 0;
    major_version_t its_version = 0;

    if (_size >= VSOMEIP_CLIENT_POS_MAX)
        its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);
    if (_size >= VSOMEIP_SERVICE_POS_MAX)
        its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
    if (_size >= VSOMEIP_METHOD_POS_MAX)
        its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
    if (_size >= VSOMEIP_SESSION_POS_MAX)
        its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
    if (_size >= VSOMEIP_INTERFACE_VERSION_POS)
        its_version = _data[VSOMEIP_INTERFACE_VERSION_POS];

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
                auto its_endpoint_def = std::make_shared<endpoint_definition>(_remote_address, _remote_port, _receiver->is_reliable());
                its_endpoint_def->set_remote_port(_receiver->get_local_port());
                auto its_endpoint =
                        ep_mgr_impl_->find_server_endpoint(its_endpoint_def->get_remote_port(), its_endpoint_def->is_reliable());
                if (its_endpoint) {
                    its_endpoint->send_error(its_endpoint_def, its_serializer->get_data(), its_serializer->get_size());
                    trace::header its_header;
                    if (its_header.prepare(its_endpoint, true, _instance,
                                           its_endpoint->is_reliable() ? trace::protocol_e::tcp : trace::protocol_e::udp))
                        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
                }
            }
            its_serializer->reset();
            put_serializer(its_serializer);
        } else {
            VSOMEIP_ERROR << "Failed to serialize error message.";
        }
    }
}

std::chrono::steady_clock::time_point routing_manager_impl::expire_subscriptions(bool _force) {

    eventgroups_t its_eventgroups;
    std::map<std::shared_ptr<remote_subscription>, std::set<client_t>> its_expired_subscriptions;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point its_next_expiration = std::chrono::steady_clock::now() + std::chrono::hours(24);
    {
        std::scoped_lock its_lock{eventgroups_mutex_};
        its_eventgroups = eventgroups_;
    }

    for (const auto& [key, its_eventgroup] : its_eventgroups) {
        for (const auto& [eventgroup_id, its_info] : its_eventgroup) {
            for (auto s : its_info->get_remote_subscriptions()) {
                if (!s) {
                    VSOMEIP_ERROR_P << "Remote subscription is NULL for eventgroup [" << hex4(key.service()) << "." << hex4(key.instance())
                                    << "." << hex4(eventgroup_id) << "]";
                    continue;
                } else if (s->is_forwarded()) {
                    VSOMEIP_WARNING_P << "New remote subscription replaced expired [" << hex4(key.service()) << "." << hex4(key.instance())
                                      << "." << hex4(eventgroup_id) << "]";
                    continue;
                }
                for (auto its_client : s->get_clients()) {
                    if (_force) {
                        its_expired_subscriptions[s].insert(its_client);
                    } else {
                        auto its_expiration = s->get_expiration(its_client);
                        if (its_expiration != std::chrono::steady_clock::time_point()) {
                            if (its_expiration < now && !s->is_expired()) {
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

    for (auto& [subscription, clients] : its_expired_subscriptions) {
        subscription->set_expired();
        auto its_info = subscription->get_eventgroupinfo();
        if (its_info) {
            auto its_service = its_info->get_service();
            auto its_instance = its_info->get_instance();
            auto its_eventgroup = its_info->get_eventgroup();

            remote_subscription_id_t its_id;
            std::unique_lock<std::mutex> its_update_lock{update_remote_subscription_mutex_};
            auto its_result = its_info->update_remote_subscription(subscription, std::chrono::steady_clock::now(), clients, its_id, false);
            if (its_result) {
                const client_t its_offering_client = find_local_client(its_service, its_instance);
                const auto its_subscription = its_info->get_remote_subscription(its_id);
                if (its_subscription) {
                    its_info->remove_remote_subscription(its_id);

                    if (its_info->get_remote_subscriptions().size() == 0) {
                        for (const auto& its_event : its_info->get_events()) {
                            bool has_remote_subscriber(false);
                            for (const auto& its_eventgroup_inner : its_event->get_eventgroups()) {
                                const auto its_eventgroup_info = find_eventgroup(its_service, its_instance, its_eventgroup_inner);
                                if (its_eventgroup_info && its_eventgroup_info->get_remote_subscriptions().size() > 0) {
                                    has_remote_subscriber = true;
                                }
                            }
                            if (!has_remote_subscriber && its_event->is_shadow()) {
                                if (its_event->is_set()) {
                                    VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(its_service) << "." << hex4(its_instance) << "."
                                                   << hex4(its_event->get_event()) << "]";
                                }
                                its_event->unset_payload();
                            }
                        }
                    }
                } else {
                    VSOMEIP_ERROR_P << "Unknown expired subscription " << its_id << " for eventgroup [" << hex4(its_service) << "."
                                    << hex4(its_instance) << "." << hex4(its_eventgroup) << "]";
                }
                send_expired_subscription(its_offering_client, its_service, its_instance, its_eventgroup, clients, subscription->get_id());
            }

            if (subscription->get_unreliable()) {
                VSOMEIP_INFO << (_force ? "Removed" : "Expired") << " subscription [" << hex4(its_service) << "." << hex4(its_instance)
                             << "." << hex4(its_eventgroup) << "] unreliable from " << subscription->get_unreliable()->get_address() << ":"
                             << subscription->get_unreliable()->get_port();
            }
            if (subscription->get_reliable()) {
                VSOMEIP_INFO << (_force ? "Removed" : "Expired") << " subscription [" << hex4(its_service) << "." << hex4(its_instance)
                             << "." << hex4(its_eventgroup) << "] reliable from " << subscription->get_reliable()->get_address() << ":"
                             << subscription->get_reliable()->get_port();
            }
        }
    }

    return its_next_expiration;
}

void routing_manager_impl::version_log_timer_cbk(boost::system::error_code const& _error, size_t _count) {
    if (!_error) {
        const uint32_t its_interval = configuration_->get_version_log_interval(host_->get_name(), true);

        std::stringstream its_last_resume;
        {
            std::scoped_lock its_lock(last_resume_mutex_);
            if (last_resume_ != std::chrono::steady_clock::time_point::min()) {
                its_last_resume << " | Last resume: "
                                << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_resume_).count()
                                << "s ago.";
            }
        }
        VSOMEIP_INFO << "vSomeIP " << VSOMEIP_VERSION << its_last_resume.str();

        ep_mgr_impl_->log_client_states();
        ep_mgr_impl_->log_server_states();

        if (_count % 3) {
            // log only external connections, UDP+TCP
            log_network_state(false, true);
            log_network_state(true, true);
        } else {
            // log all connections, UDP+TCP
            log_network_state(false, false);
            log_network_state(true, false);
        }

        {
            std::scoped_lock its_lock(log_timer_mutex_);
            version_log_timer_.expires_after(std::chrono::milliseconds(its_interval));
            version_log_timer_.async_wait(
                    [this, count = _count + 1](boost::system::error_code const& ec) { this->version_log_timer_cbk(ec, count); });
        }
    }
}

bool routing_manager_impl::handle_local_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                      minor_version_t _minor) {
    {
        std::scoped_lock its_lock{local_services_mutex_};
        if (auto const prior_entry = local_services_table_.find_entry(_service, _instance); prior_entry) {
            auto const& [its_service, its_instance, its_stored_major, its_stored_minor, its_stored_client] = *prior_entry;

            if (its_stored_major == _major && its_stored_minor == _minor && its_stored_client == _client) {
                VSOMEIP_ERROR_P << "Application: " << hex4(_client) << " is offering: [" << hex4(_service) << "." << hex4(_instance) << "."
                                << static_cast<std::uint32_t>(_major) << "." << _minor << "] offered previously by itself.";
                return false;
            } else if (its_stored_major == _major && its_stored_minor == _minor && its_stored_client != _client) {
                // check if previous offering application is still alive
                bool already_pinged(false);
                {
                    std::scoped_lock its_lock_inner{pending_commands_mutex_};
                    auto found_service2 = pending_offers_.find(_service);
                    if (found_service2 != pending_offers_.end()) {
                        auto found_instance2 = found_service2->second.find(_instance);
                        if (found_instance2 != found_service2->second.end()) {
                            if (std::get<2>(found_instance2->second) == _client) {
                                already_pinged = true;
                            } else {
                                VSOMEIP_ERROR_P
                                        << "Rejecting service registration. Application: " << hex4(_client) << " is trying to offer ["
                                        << hex4(_service) << "." << hex4(_instance) << "." << static_cast<std::uint32_t>(_major) << "."
                                        << _minor << "] current pending offer by application: " << hex4(its_stored_client) << ": ["
                                        << hex4(_service) << "." << hex4(_instance) << "." << static_cast<std::uint32_t>(its_stored_major)
                                        << "." << its_stored_minor << "]";
                                return false;
                            }
                        }
                    }
                }
                if (!already_pinged) {
                    // find out endpoint of previously offering application
                    if (auto its_old_endpoint = find_routing_endpoint(its_stored_client); its_old_endpoint) {
                        std::scoped_lock its_lock_inner{pending_commands_mutex_};
                        if (stub_->send_ping(its_stored_client)) {
                            pending_offers_[_service][_instance] = std::make_tuple(_major, _minor, _client, its_stored_client);
                            VSOMEIP_WARNING << "OFFER(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":"
                                            << int(_major) << "." << _minor
                                            << "] is now pending. Waiting for pong from application: " << hex4(its_stored_client);
                            return false;
                        }
                    }
                } else {
                    VSOMEIP_INFO_P << "(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":" << int(_major)
                                   << "." << _minor << "] client already pinged!";
                    return false;
                }
            } else {
                VSOMEIP_ERROR_P << "Rejecting service registration. Application: " << hex4(_client) << " is trying to offer ["
                                << hex4(_service) << "." << hex4(_instance) << "." << static_cast<std::uint32_t>(_major) << "." << _minor
                                << "] offered previously by application: " << hex4(its_stored_client) << ": [" << hex4(_service) << "."
                                << hex4(_instance) << "." << static_cast<std::uint32_t>(its_stored_major) << "." << its_stored_minor << "]";
                return false;
            }
        }

        // check if the same service instance is already offered remotely
        if (offer_service_base(_client, _service, _instance, _major, _minor)) {
            local_services_table_.add(_service, _instance, _major, _minor, _client);
        } else {
            VSOMEIP_ERROR_P << "Rejecting service registration. Application: " << hex4(_client) << " is trying to offer [" << hex4(_service)
                            << "." << hex4(_instance) << "." << static_cast<std::uint32_t>(_major) << "." << _minor << "]"
                            << "] already offered remotely";
            return false;
        }
    }
    return true;
}

bool routing_manager_impl::handle_service_rerequest(client_t _client, service_t _service, instance_t _instance) {

    bool already_pinged = false;
    client_t offering_client{VSOMEIP_CLIENT_UNSET};

    {
        std::scoped_lock local_services_lock{local_services_mutex_};
        offering_client = local_services_table_.find_client(_service, _instance);
    }

    // Service is not being offered -> process the request anyway
    if (offering_client == VSOMEIP_CLIENT_UNSET) {
        return true;
    }

    {
        std::scoped_lock pending_requests_lock{pending_commands_mutex_};
        auto its_key = service_instance_t{_service, _instance};
        if (auto found_pending = pending_requests_.find(its_key); found_pending != pending_requests_.end()) {
            auto& [pending_offering_client, requesting_clients] = found_pending->second;

            if (pending_offering_client == offering_client) {
                already_pinged = true;
                requesting_clients.insert(_client);
            } else {
                VSOMEIP_WARNING_P << "Offering client mismatch, currently offering client 0x" << hex4(offering_client)
                                  << " previously pinged client 0x" << hex4(pending_offering_client);
            }
        }

        if (!already_pinged) {
            // find out endpoint of previously offering application
            auto its_old_endpoint = find_routing_endpoint(offering_client);
            if (its_old_endpoint) {
                if (stub_->send_ping(offering_client)) {
                    // Add to pending requests
                    pending_requests_[its_key] = std::make_tuple(offering_client, std::set<client_t>{_client});
                    VSOMEIP_WARNING << "REQUEST(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "] pending.";
                    return false;
                }
            }
        } else {
            VSOMEIP_INFO_P << "Offering client: " << hex4(offering_client) << " already pinged for service: [" << hex4(_service) << "."
                           << hex4(_instance) << "]";
            return false;
        }
    }

    return true;
}

void routing_manager_impl::on_pong(client_t _client) {
    {
        std::scoped_lock its_lock{pending_commands_mutex_};
        for (auto service_iter = pending_offers_.begin(); service_iter != pending_offers_.end();) {
            for (auto instance_iter = service_iter->second.begin(); instance_iter != service_iter->second.end();) {
                auto [major, minor, new_client, old_client] = instance_iter->second;
                if (old_client == _client) {
                    // received pong from an application were another application wants
                    // to offer its service, delete the other applications offer as
                    // the current offering application is still alive
                    VSOMEIP_ERROR << "OFFER(" << hex4(new_client) << "): [" << hex4(service_iter->first) << "."
                                  << hex4(instance_iter->first) << ":" << std::uint32_t(major) << "." << minor
                                  << "] was rejected as application: " << hex4(_client) << " is still alive";
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

        for (auto iter = pending_requests_.begin(); iter != pending_requests_.end();) {
            const auto& its_key = iter->first;
            auto service_id = its_key.service();
            auto instance_id = its_key.instance();
            auto [offering_client, requesting_clients] = iter->second;

            if (offering_client == _client) {
                // received pong from an application were another application wants
                // to request its service, processing the request

                protocol::service its_request(service_id, instance_id, ANY_MAJOR, ANY_MINOR);
                std::set<protocol::service> requests;
                requests.insert(its_request);

                for (auto client_id : requesting_clients) {
                    request_service(client_id, service_id, instance_id, ANY_MAJOR, ANY_MINOR);
                    if (configuration_->is_security_enabled()) {
                        stub_->handle_credentials(client_id, requests);
                    }
                    stub_->handle_requests(client_id, requests);
                    VSOMEIP_INFO << "REQUEST(" << hex4(client_id) << "): [" << hex4(service_id) << "." << hex4(instance_id)
                                 << "] processed";
                }

                iter = pending_requests_.erase(iter);
            } else {
                ++iter;
            }
        }
    }
}

void routing_manager_impl::register_client_error_handler(client_t _client, const std::shared_ptr<local_endpoint>& _endpoint) {
    _endpoint->register_error_handler(std::bind(&routing_manager_impl::cleanup_client, this, _client));
}

void routing_manager_impl::cleanup_client(client_t _client) {
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client()) << " handles cleanup of client 0x" << hex4(_client);

    stub_->deregister_client(_client);

    std::forward_list<std::tuple<client_t, service_t, instance_t, major_version_t, minor_version_t>> its_offers;
    {
        std::scoped_lock its_lock{pending_commands_mutex_};
        remove_pending_requests_unlocked(pending_request_removal_type_e::BOTH, _client);
        if (pending_offers_.size() == 0) {
            return;
        }

        for (auto service_iter = pending_offers_.begin(); service_iter != pending_offers_.end();) {
            for (auto instance_iter = service_iter->second.begin(); instance_iter != service_iter->second.end();) {
                auto [major, minor, new_client, old_client] = instance_iter->second;
                if (old_client == _client) {
                    VSOMEIP_WARNING << "OFFER(" << hex4(new_client) << "): [" << hex4(service_iter->first) << "."
                                    << hex4(instance_iter->first) << ":" << std::uint32_t(major) << "." << minor
                                    << "] is not pending anymore as application: " << hex4(old_client) << " is dead. Offering again!";
                    its_offers.push_front(std::make_tuple(new_client, service_iter->first, instance_iter->first, major, minor));
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
    for (const auto& [client, service, instance, major, minor] : its_offers) {
        offer_service(client, service, instance, major, minor, true);
    }
}

std::shared_ptr<endpoint_manager_impl> routing_manager_impl::get_endpoint_manager() const {
    return ep_mgr_impl_;
}

void routing_manager_impl::send_subscribe([[maybe_unused]] client_t _client, [[maybe_unused]] service_t _service,
                                          [[maybe_unused]] instance_t _instance, [[maybe_unused]] eventgroup_t _eventgroup,
                                          [[maybe_unused]] major_version_t _major, [[maybe_unused]] event_t _event,
                                          [[maybe_unused]] const std::shared_ptr<debounce_filter_impl_t>& _filter) {
    VSOMEIP_ERROR_P << "Should not have been called";
}

bool routing_manager_impl::is_suspended() const {
    return routing_state_ == routing_state_e::RS_SUSPENDED || routing_state_ == routing_state_e::RS_DELAYED_RESUME;
}

routing_state_e routing_manager_impl::get_routing_state() {
    return routing_state_;
}

void routing_manager_impl::set_routing_state(routing_state_e _routing_state) {
    if (routing_state_ == _routing_state) {
        VSOMEIP_INFO_P << "No routing state change --> do nothing.";
        return;
    }

    routing_state_ = _routing_state;

    if (discovery_) {
        switch (_routing_state) {
        case routing_state_e::RS_SUSPENDED: {
            VSOMEIP_INFO_P << "Set routing to RS_SUSPENDED";

            // stop processing of incoming SD messages
            discovery_->suspend();

            VSOMEIP_INFO_P << "Inform all applications that we are going to suspend.";
            send_suspend();

            // remove all remote subscriptions to remotely offered services on this node
            VSOMEIP_INFO_P << "Expire subscription.";
            expire_subscriptions(true);

            VSOMEIP_INFO_P << "Stop offered services.";
            std::vector<std::shared_ptr<serviceinfo>> _service_infos;
            // send StopOffer messages for remotely offered services on this node
            for (const auto& [service, instances] : get_offered_services()) {
                for (const auto& [instance, info] : instances) {
                    bool has_reliable(info->get_endpoint(true) != nullptr);
                    bool has_unreliable(info->get_endpoint(false) != nullptr);
                    if (has_reliable || has_unreliable) {
                        const client_t its_client(find_local_client(service, instance));
                        VSOMEIP_WARNING_P << "Service " << hex4(service) << "." << hex4(instance) << " still offered by "
                                          << hex4(its_client);
                    }

                    // collect stop offers to be sent out
                    discovery_->stop_offer_service(info); // does book-keeping changes only due to stop()
                    _service_infos.push_back(info);
                }
            }
            // send collected stop offers packed together in one ore multiple SD messages
            discovery_->send_collected_stop_offers(_service_infos);
            _service_infos.clear();

            {
                std::scoped_lock its_lock{remote_subscription_state_mutex_};
                remote_subscription_state_.clear();
            }

            VSOMEIP_INFO_P << "Clear subscriptions to shadow events.";
            // Remove all subscribers to shadow events
            clear_shadow_subscriptions();

            VSOMEIP_INFO_P << "Unsubscribe external eventgroups.";
            // send StopSubscribes and clear subscribed_ map
            discovery_->unsubscribe_all_on_suspend();

            VSOMEIP_INFO_P << "Mark external service as offline.";
            // mark all external services as offline
            services_t its_remote_services;
            {
                std::scoped_lock its_lock{services_remote_mutex_};
                its_remote_services = services_remote_;
            }
            for (const auto& [service, instances] : its_remote_services) {
                for (const auto& [instance, info] : instances) {
                    const bool has_reliable(info->get_endpoint(true));
                    const bool has_unreliable(info->get_endpoint(false));
                    del_routing_info(service, instance, has_reliable, has_unreliable, true);

                    // clear all cached payloads of remote services
                    unset_all_eventpayloads(service, instance);
                }
            }

            // flush SOME/IP-SD
            // e.g., make sure all of those StopOffers/StopSubs were actually sent!
            if (auto const endpoint = sd_info_ ? sd_info_->get_endpoint(false) : nullptr) {
                if (auto const server_endpoint = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)) {
                    server_endpoint->wait_until_sent();
                }
            }

            // stop all server endpoints
            ep_mgr_impl_->suspend();

            if (routing_state_handler_) {
                routing_state_handler_(_routing_state);
            }

            VSOMEIP_INFO_P << "Set routing to RS_SUSPENDED done";
            break;
        }
        case routing_state_e::RS_RESUMED: {
            {
                std::scoped_lock its_lock(on_state_change_mutex_);
                if (!is_external_routing_ready()) {
                    VSOMEIP_INFO_P << "Network not running, delaying the resume of routing manager (if_state_running_: "
                                   << if_state_running_ << ", sd_route_set_: " << sd_route_set_ << ")";
                    routing_state_ = routing_state_e::RS_DELAYED_RESUME;
                    return;
                }
            }

            VSOMEIP_INFO_P << "Set routing to RS_RESUMED";

            // resume all endpoints
            ep_mgr_impl_->resume();

            // Reset relevant in service info
            VSOMEIP_INFO_P << "Reset service info.";
            for (const auto& its_service : get_offered_services()) {
                for (const auto& its_instance : its_service.second) {
                    its_instance.second->set_ttl(DEFAULT_TTL);
                    its_instance.second->set_is_in_mainphase(false);
                }
            }

            // Switch SD back to normal operation
            VSOMEIP_INFO_P << "Go to normal operation.";

            if (routing_state_handler_) {
                routing_state_handler_(_routing_state);
            }

            // start processing of SD messages (incoming remote offers should lead to new subscribe
            // messages)
            VSOMEIP_INFO_P << "Start service discovery.";
            discovery_->start();

            // if there are any offered services, start offers watchdog
            // TODO: this is awkward as hell - why are we passing services without endpoints to `discovery_`?
            bool trigger_offer_watchdog = false;
            for (const auto& its_service : get_offered_services()) {
                for (const auto& its_instance : its_service.second) {
                    if (its_instance.second->get_endpoint(true) != nullptr || its_instance.second->get_endpoint(false) != nullptr) {
                        trigger_offer_watchdog = true;
                        break;
                    }
                }
            }
            if (trigger_offer_watchdog) {
                discovery_->start_offer_watchdog();
            }

            // Trigger initial offer phase for relevant services
            VSOMEIP_INFO_P << "Offer services.";
            for (const auto& its_service : get_offered_services()) {
                for (const auto& its_instance : its_service.second) {
                    discovery_->offer_service(its_instance.second);
                }
            }

            init_pending_services();

            VSOMEIP_INFO << VSOMEIP_EXTERNAL_ROUTING_READY_MESSAGE;

            {
                std::scoped_lock its_lock(last_resume_mutex_);
                last_resume_ = std::chrono::steady_clock::now();
            }

            if (routing_state_handler_) {
                routing_state_handler_(_routing_state);
            }

            VSOMEIP_INFO_P << "Set routing to RS_RESUMED done";
            break;
        }
        case routing_state_e::RS_DIAGNOSIS:
            VSOMEIP_INFO_P << "Set routing to RS_DIAGNOSIS, not supported since 3.7.2 --> do nothing.";
            break;
        case routing_state_e::RS_RUNNING:
            VSOMEIP_INFO_P << "Set routing to RS_RUNNING";

            // Reset relevant in service info
            for (const auto& its_service : get_offered_services()) {
                for (const auto& its_instance : its_service.second) {
                    if (host_->get_configuration()->is_someip(its_service.first, its_instance.first)) {
                        its_instance.second->set_ttl(DEFAULT_TTL);
                        its_instance.second->set_is_in_mainphase(false);
                    }
                }
            }

            // Trigger initial phase for relevant services
            for (const auto& its_service : get_offered_services()) {
                for (const auto& its_instance : its_service.second) {
                    if (host_->get_configuration()->is_someip(its_service.first, its_instance.first)) {
                        discovery_->offer_service(its_instance.second);
                    }
                }
            }

            if (routing_state_handler_) {
                routing_state_handler_(_routing_state);
            }

            VSOMEIP_INFO_P << "Set routing to RS_RUNNING done";
            break;
        case routing_state_e::RS_DELAYED_RESUME:
            if (routing_state_handler_) {
                routing_state_handler_(_routing_state);
            }
            break;
        default:
            break;
        }
    }
}

connection_control_response_e routing_manager_impl::change_connection_control(connection_control_request_e _control,
                                                                              const boost::asio::ip::address& _guest_address) {
    // to deal with incoming connections
    if (stub_) {
        return stub_->change_connection_control(_control, _guest_address);
    }

    VSOMEIP_ERROR << "rmi::" << __func__ << ": Cannot manage connections for '" << _guest_address << "', no stub";
    return connection_control_response_e::CCR_ERROR_INVALID_PARAMETER;
}

void routing_manager_impl::on_net_interface_or_route_state_changed(bool _is_interface, const std::string& _if, bool _available) {
    auto log_change_message = [&_if, _available, _is_interface](bool _warning) {
        std::stringstream ss;
        ss << (_is_interface ? "Network interface" : "Route") << " \"" << _if << "\" state changed: " << (_available ? "up" : "down");
        if (_warning) {
            VSOMEIP_WARNING << ss.str();
        } else {
            VSOMEIP_INFO << ss.str();
        }
    };

    bool switch_to_resumed = false;

    {
        std::scoped_lock its_lock(on_state_change_mutex_);
        if (_is_interface) {
            if (_available != if_state_running_) {
                log_change_message(_available);
            }
            if_state_running_ = _available;
            // When the interface goes down the sd route is also lost
            if (!if_state_running_ && configuration_->get_sd_wait_route_netlink_notification()) {
                sd_route_set_ = false;
            }
        } else {
            if (_available != sd_route_set_) {
                log_change_message(_available);
            }
            sd_route_set_ = _available;
        }

        if (is_external_routing_ready()) {
            if (!routing_running_) {
                start_ip_routing();
            }

            auto its_routing_state{get_routing_state()};
            if (its_routing_state == routing_state_e::RS_DELAYED_RESUME) {
                switch_to_resumed = true;
            } else if (its_routing_state != routing_state_e::RS_SUSPENDED) {
                init_pending_services();
                VSOMEIP_INFO << VSOMEIP_EXTERNAL_ROUTING_READY_MESSAGE;
            }
        }
    }

    if (switch_to_resumed) {
        set_routing_state(routing_state_e::RS_RESUMED);
    }
}

void routing_manager_impl::start_ip_routing() {
#if defined(_WIN32) || defined(__QNX__)
    if_state_running_ = true;
    sd_route_set_ = true;
#endif

    if (routing_ready_handler_) {
        routing_ready_handler_();
    }

    if (discovery_) {
        if (!is_suspended()) {
            discovery_->start();
        }
    } else {
        init_routing_info();
    }

    routing_running_ = true;
}

void routing_manager_impl::init_pending_services() {
    std::scoped_lock its_lock(pending_sd_offers_mutex_);
    if (!pending_sd_offers_.empty()) {
        for (auto [service, instance] : pending_sd_offers_) {
            init_service_info(service, instance, true);
        }
        pending_sd_offers_.clear();

        VSOMEIP_INFO_P << "Pending services cleared.";
    }
}

bool routing_manager_impl::is_external_routing_ready() const {
    return if_state_running_ && (!configuration_->is_sd_enabled() || (configuration_->is_sd_enabled() && sd_route_set_));
}

bool routing_manager_impl::is_available(service_t _service, instance_t _instance, major_version_t _major) const {
    std::scoped_lock its_lock(local_services_mutex_);
    return local_services_table_.is_available(_service, _instance, _major);
}

void routing_manager_impl::add_requested_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                 minor_version_t _minor) {

    std::scoped_lock ist_lock{requested_services_mutex_};
    requested_services_[_service][_instance][_major][_minor].insert(_client);
}

void routing_manager_impl::remove_requested_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                    minor_version_t _minor) {

    std::scoped_lock ist_lock{requested_services_mutex_};

    using minor_map_t = std::map<minor_version_t, std::set<client_t>>;
    using major_map_t = std::map<major_version_t, minor_map_t>;
    using instance_map_t = std::map<instance_t, major_map_t>;

    auto delete_client = [&_client](minor_map_t::iterator& _minor_iter, const major_map_t::iterator& _parent_major_iter) {
        if (_minor_iter->second.erase(_client)) { // client was requester
            if (_minor_iter->second.empty()) {
                // client was last requester of this minor version
                _minor_iter = _parent_major_iter->second.erase(_minor_iter);
            } else { // there are still other requesters of this minor version
                ++_minor_iter;
            }
        } else { // client wasn't requester
            ++_minor_iter;
        }
    };

    auto handle_minor = [&_minor, &delete_client](major_map_t::iterator& _major_iter,
                                                  const instance_map_t::iterator& _parent_instance_iter) {
        if (_minor == ANY_MINOR) {
            for (auto minor_iter = _major_iter->second.begin(); minor_iter != _major_iter->second.end();) {
                delete_client(minor_iter, _major_iter);
            }
        } else {
            auto found_minor = _major_iter->second.find(_minor);
            if (found_minor != _major_iter->second.end()) {
                delete_client(found_minor, _major_iter);
            }
        }
        if (_major_iter->second.empty()) {
            // client was last requester of this major version
            _major_iter = _parent_instance_iter->second.erase(_major_iter);
        } else {
            ++_major_iter;
        }
    };

    auto found_service = requested_services_.find(_service);
    if (found_service != requested_services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            if (_major == ANY_MAJOR) {
                for (auto major_iter = found_instance->second.begin(); major_iter != found_instance->second.end();) {
                    handle_minor(major_iter, found_instance);
                }
            } else {
                auto found_major = found_instance->second.find(_major);
                if (found_major != found_instance->second.end()) {
                    handle_minor(found_major, found_instance);
                }
            }
            if (found_instance->second.empty()) {
                // client was last requester of this instance
                found_service->second.erase(found_instance);
                if (found_service->second.empty()) {
                    // client was last requester of this service
                    requested_services_.erase(found_service);
                }
            }
        }
    }
}

std::vector<protocol::service> routing_manager_impl::get_requested_services(client_t _client) const {
    std::scoped_lock ist_lock{requested_services_mutex_};
    std::vector<protocol::service> its_requests;
    for (const auto& [service, instances] : requested_services_) {
        for (const auto& [instance, majors] : instances) {
            bool requested = false;
            major_version_t its_major = ANY_MAJOR;
            minor_version_t its_minor = ANY_MINOR;
            for (const auto& [major, minors] : majors) {
                for (const auto& [minor, clients] : minors) {
                    if (clients.find(_client) != clients.end()) {
                        requested = true;
                        its_minor = minor;
                        break;
                    }
                }
                if (requested) {
                    its_major = major;
                    break;
                }
            }
            if (requested) {
                its_requests.emplace_back(service, instance, its_major, its_minor);
                break;
            }
        }
    }
    return its_requests;
}

std::set<client_t> routing_manager_impl::get_requesters(service_t _service, instance_t _instance, major_version_t _major) {

    std::scoped_lock ist_lock{requested_services_mutex_};
    return get_requesters_unlocked(_service, _instance, _major);
}

std::set<client_t> routing_manager_impl::get_requesters_unlocked(service_t _service, instance_t _instance, major_version_t _major) {

    std::set<client_t> its_requesters;

    auto found_service = requested_services_.find(_service);
    if (found_service == requested_services_.end()) {
        found_service = requested_services_.find(ANY_SERVICE);
        if (found_service == requested_services_.end()) {
            return its_requesters;
        }
    }

    auto found_instance = found_service->second.find(_instance);
    if (found_instance == found_service->second.end()) {
        found_instance = found_service->second.find(ANY_INSTANCE);
        if (found_instance == found_service->second.end()) {
            return its_requesters;
        }
    }

    for (const auto& [major, minors_map] : found_instance->second) {
        if (major == _major || _major == DEFAULT_MAJOR || major == ANY_MAJOR) {
            for (const auto& [minor, clients] : minors_map) {
                if (its_requesters.empty()) {
                    its_requesters = clients;
                } else {
                    its_requesters.insert(clients.cbegin(), clients.cend());
                }
            }
        }
    }

    return its_requesters;
}

bool routing_manager_impl::has_requester(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor) {
    std::scoped_lock lock{requested_services_mutex_};
    return has_requester_unlocked(_service, _instance, _major, _minor);
}

bool routing_manager_impl::has_requester_unlocked(service_t _service, instance_t _instance, major_version_t _major,
                                                  minor_version_t _minor) {

    auto found_service = requested_services_.find(_service);
    if (found_service == requested_services_.end()) {
        found_service = requested_services_.find(ANY_SERVICE);
        if (found_service == requested_services_.end()) {
            return false;
        }
    }

    auto found_instance = found_service->second.find(_instance);
    if (found_instance == found_service->second.end()) {
        found_instance = found_service->second.find(ANY_INSTANCE);
        if (found_instance == found_service->second.end()) {
            return false;
        }
    }

    for (const auto& [major, minors_map] : found_instance->second) {
        if (major == _major || _major == DEFAULT_MAJOR || major == ANY_MAJOR || _major == ANY_MAJOR) {
            for (const auto& [minor, clients] : minors_map) {
                if (minor <= _minor || _minor == DEFAULT_MINOR || minor == ANY_MINOR || _minor == ANY_MINOR) {
                    return true;
                }
            }
        }
    }

    return false;
}

std::set<eventgroup_t> routing_manager_impl::get_subscribed_eventgroups(service_t _service, instance_t _instance) {
    std::set<eventgroup_t> its_eventgroups;

    std::scoped_lock its_lock{eventgroups_mutex_};
    const auto search = eventgroups_.find(service_instance_t{_service, _instance});
    if (search != eventgroups_.end()) {
        for (const auto& [eventgroup_id, eventgroup_info] : search->second) {
            for (const auto& its_event : eventgroup_info->get_events()) {
                if (its_event->has_subscriber(eventgroup_id, ANY_CLIENT)) {
                    its_eventgroups.insert(eventgroup_id);
                }
            }
        }
    }

    return its_eventgroups;
}

void routing_manager_impl::clear_targets_and_pending_sub_from_eventgroups(service_t _service, instance_t _instance) {
    std::vector<std::shared_ptr<event>> its_events;
    {
        std::scoped_lock its_lock{eventgroups_mutex_};
        const auto search = eventgroups_.find(service_instance_t{_service, _instance});

        if (search != eventgroups_.end()) {
            for (const auto& [eventgroup_id, eventgroup_info] : search->second) {
                // As the service is gone, all subscriptions to its events
                // do no longer exist and the last received payload is no
                // longer valid.
                for (auto& its_event : eventgroup_info->get_events()) {
                    const auto its_subscribers = its_event->get_subscribers();
                    for (const auto its_subscriber : its_subscribers) {
                        its_event->remove_subscriber(eventgroup_id, its_subscriber);

                        client_t its_client = VSOMEIP_ROUTING_CLIENT; // is_specific_endpoint_client(its_subscriber,
                                                                      // _service, _instance);
                        {
                            std::scoped_lock its_lock_inner{remote_subscription_state_mutex_};
                            const auto its_tuple = std::make_tuple(_service, _instance, eventgroup_id, its_client);
                            remote_subscription_state_.erase(its_tuple);
                        }
                        its_events.push_back(its_event);
                    }
                    // TODO dn: find out why this was commented out
                    // eventgroup_info->clear_targets();
                    // eventgroup_info->clear_pending_subscriptions();
                }
            }
        }
    }
    for (const auto& e : its_events) {
        if (e->is_set()) {
            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(e->get_event()) << "]";
        }
        e->unset_payload(true);
    }
}

void routing_manager_impl::call_sd_endpoint_connected(const boost::system::error_code& _error, service_t _service, instance_t _instance,
                                                      const std::shared_ptr<boardnet_endpoint>& _endpoint,
                                                      std::shared_ptr<boost::asio::steady_timer> _timer) {
    (void)_timer;
    if (_error) {
        return;
    }
    _endpoint->set_established(true);
    if (discovery_) {
        discovery_->on_endpoint_connected(_service, _instance, _endpoint);
    }
}

bool routing_manager_impl::create_placeholder_event_and_subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                                  event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter,
                                                                  client_t _client) {

    bool is_inserted(false);
    // we received a event which was not yet requested/offered
    // create a placeholder field until someone requests/offers this event with
    // full information like eventgroup, field or not etc.
    std::set<eventgroup_t> its_eventgroups({_eventgroup});

    if (const client_t its_local_client(find_local_client(_service, _instance)); its_local_client != VSOMEIP_ROUTING_CLIENT) {
        // received subscription for event of a service instance hosted on
        // this node register with client id of local_client and set shadow to true
        register_event(its_local_client, _service, _instance, _event, its_eventgroups, event_type_e::ET_UNKNOWN,
                       reliability_type_e::RT_UNKNOWN, std::chrono::milliseconds::zero(), false, true, nullptr, false, true, true);
    } else {
        // received subscription for event of a unknown or remote service instance
        std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
        if (its_info && !its_info->is_local()) {
            // remote service, register shadow event with client ID of subscriber
            // which should have called register_event
            register_event(_client, _service, _instance, _event, its_eventgroups, event_type_e::ET_UNKNOWN, reliability_type_e::RT_UNKNOWN,
                           std::chrono::milliseconds::zero(), false, true, nullptr, false, true, true);
        } else {
            VSOMEIP_WARNING_P << "(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                              << "." << hex4(_event) << "] received subscription for unknown service instance.";
        }
    }

    std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
    if (its_event) {
        is_inserted = its_event->add_subscriber(_eventgroup, _filter, _client, false);
    }
    return is_inserted;
}

void routing_manager_impl::handle_subscription_state(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                     event_t _event) {
    // Note: remote_subscription_state_mutex_ is already locked as this
    // method builds a critical section together with insert_subscription
    // from routing_manager_base.
    // Todo: Improve this situation...
    auto its_event = find_event(_service, _instance, _event);
    client_t its_client(VSOMEIP_ROUTING_CLIENT);
    if (its_event && its_event->get_type() == event_type_e::ET_SELECTIVE_EVENT) {
        its_client = _client;
    }

    auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, its_client);
    auto its_state = remote_subscription_state_.find(its_tuple);
    if (its_state != remote_subscription_state_.end()) {
        if (its_state->second == subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED) {
            // Subscription already acknowledged!
            stub_->send_subscribe_ack(_client, _service, _instance, _eventgroup, _event);
        }
    }
}

void routing_manager_impl::register_sd_acceptance_handler(const sd_acceptance_handler_t& _handler) const {
    if (discovery_) {
        discovery_->register_sd_acceptance_handler(_handler);
    }
}

void routing_manager_impl::register_reboot_notification_handler(const reboot_notification_handler_t& _handler) const {
    if (discovery_) {
        discovery_->register_reboot_notification_handler(_handler);
    }
}

void routing_manager_impl::register_routing_ready_handler(const routing_ready_handler_t& _handler) {
    routing_ready_handler_ = _handler;
}

void routing_manager_impl::register_routing_state_handler(const routing_state_handler_t& _handler) {
    routing_state_handler_ = _handler;
}

void routing_manager_impl::sd_acceptance_enabled(const boost::asio::ip::address& _address, const port_range_t& _range, bool _reliable) {
    expire_subscriptions(_address, _range, _reliable);
    expire_services(_address, _range, _reliable);
}

void routing_manager_impl::memory_log_timer_cbk(boost::system::error_code const& _error) {
    if (_error) {
        return;
    }

#if defined(__linux__) || defined(__QNX__)
    static const std::uint32_t its_pagesize = static_cast<std::uint32_t>(getpagesize() / 1024);

    std::FILE* its_file = std::fopen("/proc/self/statm", "r");
    if (!its_file) {
        VSOMEIP_ERROR << "memory_log_timer_cbk: couldn't open: errno " << errno;
        return;
    }
    std::uint64_t its_size(0);
    std::uint64_t its_rsssize(0);
    std::uint64_t its_sharedpages(0);
    std::uint64_t its_text(0);
    std::uint64_t its_lib(0);
    std::uint64_t its_data(0);
    std::uint64_t its_dirtypages(0);

    if (EOF
        == std::fscanf(its_file, "%lu %lu %lu %lu %lu %lu %lu", &its_size, &its_rsssize, &its_sharedpages, &its_text, &its_lib, &its_data,
                       &its_dirtypages)) {
        VSOMEIP_ERROR << "memory_log_timer_cbk: error reading: errno " << errno;
    }
    std::fclose(its_file);

    struct timespec cputs, monots;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cputs);
    clock_gettime(CLOCK_MONOTONIC, &monots);

    VSOMEIP_INFO << "memory usage: "
                 << "VmSize " << its_size * its_pagesize << " kB, "
                 << "VmRSS " << its_rsssize * its_pagesize << " kB, "
                 << "shared pages " << its_sharedpages * its_pagesize << " kB, "
                 << "text " << its_text * its_pagesize << " kB, "
                 << "data " << its_data * its_pagesize << " kB "
                 << "| monotonic time: " << monots.tv_sec << "." << monots.tv_nsec << " cpu time: " << cputs.tv_sec << "." << cputs.tv_nsec;
#endif

    {
        std::scoped_lock its_lock{memory_log_timer_mutex_};
        memory_log_timer_.expires_after(std::chrono::seconds(configuration_->get_log_memory_interval()));
        memory_log_timer_.async_wait(std::bind(&routing_manager_impl::memory_log_timer_cbk, this, std::placeholders::_1));
    }
}

void routing_manager_impl::status_log_timer_cbk(boost::system::error_code const& _error) {
    if (_error) {
        return;
    }
    const uint32_t its_interval = configuration_->get_status_log_interval(host_->get_name(), true);
    VSOMEIP_INFO_P << " ";

    ep_mgr_impl_->print_status();
    {
        std::scoped_lock its_lock{log_timer_mutex_};
        status_log_timer_.expires_after(std::chrono::milliseconds(its_interval));
        status_log_timer_.async_wait([this](boost::system::error_code const& ec) { this->status_log_timer_cbk(ec); });
    }
}

void routing_manager_impl::on_unsubscribe_ack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              remote_subscription_id_t _id) {
    std::shared_ptr<eventgroupinfo> its_info = find_eventgroup(_service, _instance, _eventgroup);
    if (its_info) {
        std::unique_lock<std::mutex> its_update_lock{update_remote_subscription_mutex_};
        const auto its_subscription = its_info->get_remote_subscription(_id);
        if (its_subscription) {
            its_info->remove_remote_subscription(_id);

            if (its_info->get_remote_subscriptions().size() == 0) {
                for (const auto& its_event : its_info->get_events()) {
                    bool has_remote_subscriber(false);
                    for (const auto& its_eventgroup : its_event->get_eventgroups()) {
                        const auto its_eventgroup_info = find_eventgroup(_service, _instance, its_eventgroup);
                        if (its_eventgroup_info && its_eventgroup_info->get_remote_subscriptions().size() > 0) {
                            has_remote_subscriber = true;
                        }
                    }

                    if (!has_remote_subscriber && its_event->is_shadow()) {
                        if (its_event->is_set()) {
                            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "."
                                           << hex4(its_event->get_event()) << "]";
                        }
                        its_event->unset_payload();
                    }
                }
            }
        } else {
            VSOMEIP_ERROR_P << "Unknown StopSubscribe for eventgroup [" << hex4(_service) << "." << hex4(_instance) << "."
                            << hex4(_eventgroup) << "] id=" << hex4(_id);
        }
    } else {
        VSOMEIP_ERROR_P << "Received StopSubscribe for unknown eventgroup: (" << hex4(_client) << "): [" << hex4(_service) << "."
                        << hex4(_instance) << "." << hex4(_eventgroup) << "] id=" << hex4(_id);
    }
}

void routing_manager_impl::send_subscription(const client_t _offering_client, const service_t _service, const instance_t _instance,
                                             const eventgroup_t _eventgroup, const major_version_t _major,
                                             const std::set<client_t>& _clients, const remote_subscription_id_t _id) {
    { // service hosted by local client
        for (const auto its_client : _clients) {
            if (!stub_->send_subscribe(find_routing_endpoint(_offering_client), its_client, _service, _instance, _eventgroup, _major,
                                       ANY_EVENT, nullptr, _id)) {
                try {
                    const auto its_callback = std::bind(&routing_manager_stub_host::on_subscribe_nack,
                                                        std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                                                        its_client, _service, _instance, _eventgroup, true, _id);
                    boost::asio::post(io_, its_callback);
                } catch (const std::exception& e) {
                    VSOMEIP_ERROR_P << e.what();
                }
            }
        }
    }
}

void routing_manager_impl::cleanup_server_endpoint(service_t _service, const std::shared_ptr<boardnet_endpoint>& _endpoint) {
    if (_endpoint) {
        // Clear service_instances_, check whether any service still
        // uses this endpoint and clear server endpoint if no service
        // remains using it
        if (ep_mgr_impl_->remove_instance(_service, _endpoint.get())) {
            if (ep_mgr_impl_->remove_server_endpoint(_endpoint->get_local_port(), _endpoint->is_reliable())) {
                // Stop endpoint (close socket) to release its async_handlers!
                _endpoint->stop(false);
            }
        }
    }
}

pending_remote_offer_id_t routing_manager_impl::pending_remote_offer_add(service_t _service, instance_t _instance) {
    std::scoped_lock its_lock{pending_remote_offers_mutex_};
    if (++pending_remote_offer_id_ == 0) {
        pending_remote_offer_id_++;
    }
    pending_remote_offers_[pending_remote_offer_id_] = std::make_pair(_service, _instance);
    return pending_remote_offer_id_;
}

std::pair<service_t, instance_t> routing_manager_impl::pending_remote_offer_remove(pending_remote_offer_id_t _id) {
    std::scoped_lock its_lock{pending_remote_offers_mutex_};
    std::pair<service_t, instance_t> ret = std::make_pair(ANY_SERVICE, ANY_INSTANCE);
    auto found_si = pending_remote_offers_.find(_id);
    if (found_si != pending_remote_offers_.end()) {
        ret = found_si->second;
        pending_remote_offers_.erase(found_si);
    }
    return ret;
}

void routing_manager_impl::on_resend_provided_events_response(pending_remote_offer_id_t _id) {
    const std::pair<service_t, instance_t> its_service = pending_remote_offer_remove(_id);
    if (its_service.first != ANY_SERVICE) {
        // create server endpoint
        std::shared_ptr<serviceinfo> its_info = find_service(its_service.first, its_service.second);
        if (its_info) {
            its_info->set_ttl(DEFAULT_TTL);
            init_service_info(its_service.first, its_service.second, true);
        }
    }
}

void routing_manager_impl::service_endpoint_connected(service_t _service, instance_t _instance, major_version_t _major,
                                                      minor_version_t _minor, const std::shared_ptr<boardnet_endpoint>& _endpoint) {
    stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, _major, _minor);
    on_availability(_service, _instance, availability_state_e::AS_AVAILABLE, _major, _minor);

    auto its_timer = std::make_shared<boost::asio::steady_timer>(io_);
    its_timer->expires_after(std::chrono::milliseconds(3));

    auto its_me{std::static_pointer_cast<routing_manager_impl>(shared_from_this())};
    its_timer->async_wait([its_me, _service, _instance, _endpoint, its_timer](const boost::system::error_code& _error) {
        its_me->call_sd_endpoint_connected(_error, _service, _instance, _endpoint, its_timer);
    });
}

void routing_manager_impl::service_endpoint_disconnected(service_t _service, instance_t _instance, major_version_t _major,
                                                         minor_version_t _minor, const std::shared_ptr<boardnet_endpoint>& _endpoint) {
    (void)_endpoint;
    on_availability(_service, _instance, availability_state_e::AS_UNAVAILABLE, _major, _minor);
    stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, _major, _minor);

    {
        std::scoped_lock its_lock{remote_subscription_state_mutex_};
        std::set<std::tuple<service_t, instance_t, eventgroup_t, client_t>> its_invalid_remote_subscription_;

        for (const auto& its_state : remote_subscription_state_) {
            if (std::get<0>(its_state.first) == _service && std::get<1>(its_state.first) == _instance) {
                its_invalid_remote_subscription_.insert(its_state.first);
            }
        }

        for (const auto& its_key : its_invalid_remote_subscription_)
            remote_subscription_state_.erase(its_key);
    }

    VSOMEIP_WARNING_P << "Lost connection to remote service: [" << hex4(_service) << "." << hex4(_instance) << "]";
}

void routing_manager_impl::send_unsubscription(client_t _offering_client, service_t _service, instance_t _instance,
                                               eventgroup_t _eventgroup, major_version_t _major, const std::set<client_t>& _removed,
                                               remote_subscription_id_t _id) {

    (void)_major; // TODO: Remove completely?
    {
        for (const auto its_client : _removed) {
            if (!stub_->send_unsubscribe(find_routing_endpoint(_offering_client), its_client, _service, _instance, _eventgroup, ANY_EVENT,
                                         _id)) {
                try {
                    const auto its_callback = std::bind(&routing_manager_stub_host::on_unsubscribe_ack,
                                                        std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                                                        its_client, _service, _instance, _eventgroup, _id);
                    boost::asio::post(io_, its_callback);
                } catch (const std::exception& e) {
                    VSOMEIP_ERROR_P << e.what();
                }
            }
        }
    }
}

void routing_manager_impl::send_expired_subscription(client_t _offering_client, service_t _service, instance_t _instance,
                                                     eventgroup_t _eventgroup, const std::set<client_t>& _removed,
                                                     remote_subscription_id_t _id) {

    {
        for (const auto its_client : _removed) {
            stub_->send_expired_subscription(find_routing_endpoint(_offering_client), its_client, _service, _instance, _eventgroup,
                                             ANY_EVENT, _id);
        }
    }
}

#ifndef VSOMEIP_DISABLE_SECURITY
bool routing_manager_impl::update_security_policy_configuration(uid_t _uid, gid_t _gid, const std::shared_ptr<policy>& _policy,
                                                                const std::shared_ptr<payload>& _payload,
                                                                const security_update_handler_t& _handler) {
    return stub_->update_security_policy_configuration(_uid, _gid, _policy, _payload, _handler);
}

bool routing_manager_impl::remove_security_policy_configuration(uid_t _uid, gid_t _gid, const security_update_handler_t& _handler) {

    return stub_->remove_security_policy_configuration(_uid, _gid, _handler);
}
#endif // !VSOMEIP_DISABLE_SECURITY

bool routing_manager_impl::insert_event_statistics(service_t _service, instance_t _instance, method_t _method, length_t _length) {

    static uint32_t its_max_messages = configuration_->get_statistics_max_messages();
    std::scoped_lock its_lock{message_statistics_mutex_};
    const auto its_tuple = std::make_tuple(_service, _instance, _method);
    const auto its_main_s = message_statistics_.find(its_tuple);
    if (its_main_s != message_statistics_.end()) {
        // increase counter and calculate moving average for payload length
        its_main_s->second.avg_length_ =
                (its_main_s->second.avg_length_ * its_main_s->second.counter_ + _length) / (its_main_s->second.counter_ + 1);
        its_main_s->second.counter_++;

        if (its_tuple == message_to_discard_) {
            // check list for entry with least counter value
            uint32_t its_min_count(0xFFFFFFFF);
            auto its_tuple_to_discard = std::make_tuple(0xFFFF, 0xFFFF, 0xFFFF);
            for (const auto& s : message_statistics_) {
                if (s.second.counter_ < its_min_count) {
                    its_min_count = s.second.counter_;
                    its_tuple_to_discard = s.first;
                }
            }
            if (its_min_count != 0xFFFF && its_min_count < its_main_s->second.counter_) {
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
            if (it != message_statistics_.end() && it->second.counter_ == 1) {
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

void routing_manager_impl::statistics_log_timer_cbk(boost::system::error_code const& _error) {
    if (!_error) {
        uint32_t its_interval = configuration_->get_statistics_interval();
        its_interval = its_interval >= 1000 ? its_interval : 1000;
        uint32_t its_min_freq = configuration_->get_statistics_min_freq();
        std::stringstream its_log;
        {
            std::scoped_lock its_lock{message_statistics_mutex_};
            for (const auto& [key, stats] : message_statistics_) {
                if (stats.counter_ / (its_interval / 1000) > its_min_freq) {
                    uint16_t its_subscribed(0);
                    auto [service, instance, method] = key;
                    std::shared_ptr<event> its_event = find_event(service, instance, method);
                    if (its_event) {
                        if (!its_event->is_provided()) {
                            its_subscribed = static_cast<std::uint16_t>(its_event->get_subscribers().size());
                        }
                    }
                    its_log << hex4(service) << "." << hex4(instance) << "." << hex4(method) << ": #=" << stats.counter_
                            << " L=" << stats.avg_length_ << " S=" << its_subscribed << ", ";
                }
            }

            if (ignored_statistics_counter_) {
                its_log << " #ignored: " << ignored_statistics_counter_;
            }

            message_statistics_.clear();
            message_to_discard_ = std::make_tuple(0x00, 0x00, 0x00);
            ignored_statistics_counter_ = 0;
        }

        if (its_log.str().length() > 0) {
            VSOMEIP_INFO_P << "Received events statistics: [" << its_log.str() << "]";
        }

        {
            std::scoped_lock its_lock{log_timer_mutex_};
            statistics_log_timer_.expires_after(std::chrono::milliseconds(its_interval));
            statistics_log_timer_.async_wait(std::bind(&routing_manager_impl::statistics_log_timer_cbk, this, std::placeholders::_1));
        }
    }
}

bool routing_manager_impl::get_guest(client_t _client, boost::asio::ip::address& _address, port_t& _port) const {
    return ep_mgr_impl_->get_guest(_client, _address, _port);
}

void routing_manager_impl::send_suspend() const {
    stub_->send_suspend();
}

void routing_manager_impl::register_message_acceptance_handler(const message_acceptance_handler_t& _handler) {
    message_acceptance_handler_ = _handler;
}

void routing_manager_impl::remove_subscriptions(port_t _local_port, const boost::asio::ip::address& _remote_address, port_t _remote_port) {

    eventgroups_t its_eventgroups;
    {
        std::scoped_lock its_lock{eventgroups_mutex_};
        its_eventgroups = eventgroups_;
    }

    for (const auto& [key, its_eventgroup] : its_eventgroups) {
        for (const auto& [eventgroup_id, its_info] : its_eventgroup) {
            for (auto its_subscription : its_info->get_remote_subscriptions()) {
                auto its_definition = its_subscription->get_reliable();
                if (its_definition && its_definition->get_address() == _remote_address && its_definition->get_port() == _remote_port
                    && its_definition->get_remote_port() == _local_port) {

                    VSOMEIP_INFO_P << "Removing subscription to [" << hex4(its_info->get_service()) << "." << hex4(its_info->get_instance())
                                   << "." << hex4(its_info->get_eventgroup()) << "] from target " << its_definition->get_address() << ":"
                                   << its_definition->get_port() << " reliable=true";

                    on_remote_unsubscribe(its_subscription);
                }
            }
        }
    }
}

std::shared_ptr<local_endpoint> routing_manager_impl::find_routing_endpoint(client_t _client) const {
    return ep_mgr_impl_->find_routing_endpoint(_client);
}

void routing_manager_impl::try_to_send_before_stop() {
    ep_mgr_impl_->flush_routing_endpoint_queues();
}

const char* routing_manager_impl::routing_state_tostring(routing_state_e _state) {
    switch (_state) {
    case routing_state_e::RS_RUNNING:
        return "RS_RUNNING";
    case routing_state_e::RS_SUSPENDED:
        return "RS_SUSPENDED";
    case routing_state_e::RS_RESUMED:
        return "RS_RESUMED";
    case routing_state_e::RS_SHUTDOWN:
        return "RS_SHUTDOWN";
    case routing_state_e::RS_DELAYED_RESUME:
        return "RS_DELAYED_RESUME";
    case routing_state_e::RS_UNKNOWN:
        return "RS_UNKNOWN";
    default:
        return "Unknown State";
    }
}

void routing_manager_impl::remove_pending_requests(pending_request_removal_type_e _removal_type, client_t _client, service_t _service,
                                                   instance_t _instance) {
    std::scoped_lock its_lock{pending_commands_mutex_};
    remove_pending_requests_unlocked(_removal_type, _client, _service, _instance);
}

void routing_manager_impl::remove_pending_requests_unlocked(pending_request_removal_type_e _removal_type, client_t _client,
                                                            service_t _service, instance_t _instance) {
    bool _remove_offering =
            (_removal_type == pending_request_removal_type_e::OFFERING_ONLY || _removal_type == pending_request_removal_type_e::BOTH);
    bool _remove_requesting =
            (_removal_type == pending_request_removal_type_e::REQUESTING_ONLY || _removal_type == pending_request_removal_type_e::BOTH);

    for (auto iter = pending_requests_.begin(); iter != pending_requests_.end();) {
        const auto& its_key = iter->first;
        auto its_service = its_key.service();
        auto its_instance = its_key.instance();

        // Skip if we're filtering by service and this isn't the one
        if (_service != ANY_SERVICE && its_service != _service) {
            ++iter;
            continue;
        }

        // Skip if we're filtering by instance and this isn't the one
        if (_instance != ANY_INSTANCE && its_instance != _instance) {
            ++iter;
            continue;
        }

        auto& [offering_client, requesting_clients] = iter->second;

        bool should_erase = false;

        // Check if the client is the offering client and we should remove it
        if (_remove_offering && offering_client == _client) {
            // Remove the entire entry since the offering client is gone
            should_erase = true;
        } else if (_remove_requesting && requesting_clients.erase(_client)) {
            // If no more requesting clients, remove the entry
            if (requesting_clients.empty()) {
                should_erase = true;
            }
        }

        if (should_erase) {
            iter = pending_requests_.erase(iter);
        } else {
            ++iter;
        }
    }
}

bool routing_manager_impl::is_valid_client_id(const client_t _client, const message_type_e _type) const {
    // The diagnostic address can be used to identify client ids used by this host.
    const diagnosis_t diag = configuration_->get_diagnosis_address();

    // No point in checking the diagnostics address if the user hasn't defined one.
    if (diag == VSOMEIP_DIAGNOSIS_ADDRESS) {
        return true;
    }
    const diagnosis_t address = (_client & configuration_->get_diagnosis_mask()) >> 8;

    // We only expect responses to requests sent by this host.
    if (_type == message_type_e::MT_RESPONSE) {
        return address == diag;
    }

    // We only expect requests from other hosts.
    if (_type == message_type_e::MT_REQUEST) {
        // Requests must not use client ids from this host, otherwise the response will not be
        // routed back to the remote host.
        return address != diag;
    }

    // Any id is valid or can be ignored in every other situation.
    return true;
}

bool routing_manager_impl::send_event(client_t _client, std::shared_ptr<message> _message, bool _force) {
    return send(_client, _message, _force);
}

services_t routing_manager_impl::get_services_remote() const {
    std::scoped_lock its_lock(services_remote_mutex_);
    return services_remote_;
}

void routing_manager_impl::clear_service_info(service_t _service, instance_t _instance, bool _reliable) {
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (!its_info) {
        return;
    }

    bool deleted_instance(false);
    bool deleted_service(false);
    {
        std::scoped_lock its_lock(services_mutex_);

        // Clear service_info and service_group
        if (!its_info->get_endpoint(!_reliable)) {
            if (1 >= services_[_service].size()) {
                services_.erase(_service);
                deleted_service = true;
            } else {
                services_[_service].erase(_instance);
                deleted_instance = true;
            }
        } else {
            its_info->set_endpoint(nullptr, _reliable);
        }
    }

    if ((deleted_instance || deleted_service) && !its_info->is_local()) {
        std::scoped_lock its_lock(services_remote_mutex_);
        if (deleted_service) {
            services_remote_.erase(_service);
        } else if (deleted_instance) {
            services_remote_[_service].erase(_instance);
        }
    }
}

std::shared_ptr<serviceinfo> routing_manager_impl::find_service(service_t _service, instance_t _instance) const {
    std::shared_ptr<serviceinfo> its_info;
    std::scoped_lock its_lock(services_mutex_);
    auto found_service = services_.find(_service);
    if (found_service != services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            its_info = found_instance->second;
        }
    }
    return its_info;
}

services_t routing_manager_impl::get_services() const {
    std::scoped_lock its_lock(services_mutex_);
    return services_;
}
bool routing_manager_impl::offer_service_base(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                              minor_version_t _minor) {
    (void)_client;

    // Remote route (incoming only)
    auto its_info = find_service(_service, _instance);
    if (its_info) {
        if (!its_info->is_local()) {
            return false;
        } else if (its_info->get_major() == _major && its_info->get_minor() == _minor) {
            its_info->set_ttl(DEFAULT_TTL);
        } else {
            VSOMEIP_ERROR_P << "Service property mismatch (" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":"
                            << static_cast<std::uint32_t>(its_info->get_major()) << "." << its_info->get_minor()
                            << "] passed: " << static_cast<std::uint32_t>(_major) << ":" << _minor;
            return false;
        }
    } else {
        its_info = create_service_info(_service, _instance, _major, _minor, DEFAULT_TTL, true);
    }
    {
        std::scoped_lock its_lock(events_mutex_);
        // Set major version for all registered events of this service and instance
        const auto search = events_.find(service_instance_t{_service, _instance});

        if (search != events_.end()) {
            for (const auto& [event_id, event_ptr] : search->second) {
                event_ptr->set_version(_major);
            }
        }
    }
    return true;
}

std::shared_ptr<serviceinfo> routing_manager_impl::create_service_info(service_t _service, instance_t _instance, major_version_t _major,
                                                                       minor_version_t _minor, ttl_t _ttl, bool _is_local_service) {
    std::shared_ptr<serviceinfo> its_info = std::make_shared<serviceinfo>(_service, _instance, _major, _minor, _ttl, _is_local_service);
    {
        std::scoped_lock its_lock(services_mutex_);
        services_[_service][_instance] = its_info;
    }
    if (!_is_local_service) {
        std::scoped_lock its_lock(services_remote_mutex_);
        services_remote_[_service][_instance] = its_info;
    }
    return its_info;
}

void routing_manager_impl::register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                          const std::set<eventgroup_t>& _eventgroups, const event_type_e _type,
                                          reliability_type_e _reliability, std::chrono::milliseconds _cycle, bool _change_resets_cycle,
                                          bool _update_on_change, epsilon_change_func_t _epsilon_change_func, bool _is_provided,
                                          bool _is_shadow, bool _is_cache_placeholder) {
    std::scoped_lock its_registration_lock(event_registration_mutex_);

    auto determine_event_reliability = [this, &_service, &_instance, &_notifier, &_reliability]() {
        reliability_type_e its_reliability = configuration_->get_event_reliability(_service, _instance, _notifier);
        if (its_reliability != reliability_type_e::RT_UNKNOWN) {
            // event was explicitly configured -> overwrite value passed via API
            return its_reliability;
        } else if (_reliability != reliability_type_e::RT_UNKNOWN) {
            // use value provided via API
            return _reliability;
        } else { // automatic mode, user service' reliability
            return configuration_->get_service_reliability(_service, _instance);
        }
    };

    std::shared_ptr<event> its_event = find_event(_service, _instance, _notifier);
    bool transfer_subscriptions_from_any_event(false);
    if (its_event) {
        if (!its_event->is_cache_placeholder()) {
            if (_type == its_event->get_type() || its_event->get_type() == event_type_e::ET_UNKNOWN) {
                if (_is_provided) {
                    its_event->set_provided(true);
                    its_event->set_reliability(determine_event_reliability());
                }
                if (_is_shadow && _is_provided) {
                    its_event->set_shadow(_is_shadow);
                }
                if (_client == host_->get_client() && _is_provided) {
                    its_event->set_shadow(false);
                    its_event->set_update_on_change(_update_on_change);
                }
                for (auto eg : _eventgroups) {
                    its_event->add_eventgroup(eg);
                }
                transfer_subscriptions_from_any_event = true;
            } else {
                VSOMEIP_ERROR_P << ": Event registration update failed. Specified arguments do not match existing registration.";
            }
        } else {
            // the found event was a placeholder for caching.
            // update it with the real values
            if (_type != event_type_e::ET_FIELD) {
                // don't cache payload for non-fields
                if (its_event->is_set()) {
                    VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "."
                                   << hex4(its_event->get_event()) << "]";
                }
                its_event->unset_payload(true);
            }
            if (_is_shadow && _is_provided) {
                its_event->set_shadow(_is_shadow);
            }
            if (_client == host_->get_client() && _is_provided) {
                its_event->set_shadow(false);
                its_event->set_update_on_change(_update_on_change);
            }
            its_event->set_type(_type);
            its_event->set_reliability(determine_event_reliability());
            its_event->set_provided(_is_provided);
            its_event->set_cache_placeholder(false);
            std::shared_ptr<serviceinfo> its_service = find_service(_service, _instance);
            if (its_service) {
                its_event->set_version(its_service->get_major());
            }
            if (_eventgroups.size() == 0) { // No eventgroup specified
                std::set<eventgroup_t> its_eventgroups;
                its_eventgroups.insert(_notifier);
                its_event->set_eventgroups(its_eventgroups);
            } else {
                for (auto eg : _eventgroups) {
                    its_event->add_eventgroup(eg);
                }
            }

            its_event->set_epsilon_change_function(_epsilon_change_func);
            its_event->set_change_resets_cycle(_change_resets_cycle);
            its_event->set_update_cycle(_cycle);
        }
    } else {
        its_event = std::make_shared<event>(io_, *this, _is_shadow);
        its_event->set_service(_service);
        its_event->set_instance(_instance);
        its_event->set_event(_notifier);
        its_event->set_type(_type);
        its_event->set_reliability(determine_event_reliability());
        its_event->set_provided(_is_provided);
        its_event->set_cache_placeholder(_is_cache_placeholder);
        std::shared_ptr<serviceinfo> its_service = find_service(_service, _instance);
        if (its_service) {
            its_event->set_version(its_service->get_major());
        }

        if (_eventgroups.size() == 0) { // No eventgroup specified
            std::set<eventgroup_t> its_eventgroups;
            its_eventgroups.insert(_notifier);
            its_event->set_eventgroups(its_eventgroups);
        } else {
            its_event->set_eventgroups(_eventgroups);
        }

        if (_is_shadow && !_epsilon_change_func) {
            std::shared_ptr<debounce_filter_impl_t> its_debounce = configuration_->get_default_debounce(_service, _instance, _notifier);
            if (its_debounce) {
                std::stringstream its_debounce_parameters;
                its_debounce_parameters << "(on_change=" << (its_debounce->on_change_ ? "true" : "false") << ", ignore=[ ";
                for (auto i : its_debounce->ignore_)
                    its_debounce_parameters << "(" << i.first << ", " << std::hex << (int)i.second << ") ";
                its_debounce_parameters << "], interval=" << its_debounce->interval_ << ")";

                VSOMEIP_WARNING << "Using debounce configuration for SOME/IP event " << hex4(_service) << "." << hex4(_instance) << "."
                                << hex4(_notifier) << ". Debounce parameters: " << its_debounce_parameters.str();

                _epsilon_change_func = [its_debounce](const std::shared_ptr<payload>& _old, const std::shared_ptr<payload>& _new) {
                    bool is_changed(false), is_elapsed(false);

                    // Check whether we should forward because of changed data
                    if (its_debounce->on_change_) {
                        length_t its_min_length, its_max_length;

                        if (_old->get_length() < _new->get_length()) {
                            its_min_length = _old->get_length();
                            its_max_length = _new->get_length();
                        } else {
                            its_min_length = _new->get_length();
                            its_max_length = _old->get_length();
                        }

                        // Check whether all additional bytes (if any) are excluded
                        for (length_t i = its_min_length; i < its_max_length; i++) {
                            auto j = its_debounce->ignore_.find(i);
                            // A change is detected when an additional byte is not
                            // excluded at all or if its exclusion does not cover all
                            // bits
                            if (j == its_debounce->ignore_.end() || j->second != 0xFF) {
                                is_changed = true;
                                break;
                            }
                        }

                        if (!is_changed) {
                            const byte_t* its_old = _old->get_data();
                            const byte_t* its_new = _new->get_data();
                            for (length_t i = 0; i < its_min_length; i++) {
                                auto j = its_debounce->ignore_.find(i);
                                if (j == its_debounce->ignore_.end()) {
                                    if (its_old[i] != its_new[i]) {
                                        is_changed = true;
                                        break;
                                    }
                                } else if (j->second != 0xFF) {
                                    if ((its_old[i] & ~(j->second)) != (its_new[i] & ~(j->second))) {
                                        is_changed = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (its_debounce->interval_ > -1) {
                        // Check whether we should forward because of the elapsed time since
                        // we did last time
                        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                        std::chrono::steady_clock::time_point last = its_debounce->last_forwarded_.load();
                        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
                        is_elapsed = (last == std::chrono::steady_clock::time_point::max() || elapsed >= its_debounce->interval_);
                        if (is_elapsed || (is_changed && its_debounce->on_change_resets_interval_)) {
                            its_debounce->last_forwarded_.store(now);
                        }
                    }
                    return (is_changed || is_elapsed);
                };

            } else {
                if (_is_shadow) {
                    _epsilon_change_func = [](const std::shared_ptr<payload>& _old, const std::shared_ptr<payload>& _new) {
                        (void)_old;
                        (void)_new;
                        return true;
                    };
                }
            }
        }

        its_event->set_epsilon_change_function(_epsilon_change_func);
        its_event->set_change_resets_cycle(_change_resets_cycle);
        its_event->set_update_cycle(_cycle);
        its_event->set_update_on_change(_update_on_change);

        if (_is_provided) {
            transfer_subscriptions_from_any_event = true;
        }
    }

    if (transfer_subscriptions_from_any_event) {
        // check if someone subscribed to ANY_EVENT and the subscription
        // was stored in the cache placeholder. Move the subscribers
        // into new event
        std::shared_ptr<event> its_any_event = find_event(_service, _instance, ANY_EVENT);
        if (its_any_event) {
            std::set<eventgroup_t> any_events_eventgroups = its_any_event->get_eventgroups();
            for (eventgroup_t eventgroup : _eventgroups) {
                auto found_eg = any_events_eventgroups.find(eventgroup);
                if (found_eg != any_events_eventgroups.end()) {
                    std::set<client_t> its_any_event_subscribers = its_any_event->get_subscribers(eventgroup);
                    for (const client_t subscriber : its_any_event_subscribers) {
                        its_event->add_subscriber(eventgroup, nullptr, subscriber, true);
                    }
                }
            }
        }
    }
    if (!_is_cache_placeholder) {
        its_event->add_ref(_client, _is_provided);
    }

    for (auto eg : _eventgroups) {
        std::shared_ptr<eventgroupinfo> its_eventgroupinfo = find_eventgroup(_service, _instance, eg);
        if (!its_eventgroupinfo) {
            its_eventgroupinfo = std::make_shared<eventgroupinfo>();
            its_eventgroupinfo->set_service(_service);
            its_eventgroupinfo->set_instance(_instance);
            its_eventgroupinfo->set_eventgroup(eg);
            its_eventgroupinfo->set_max_remote_subscribers(configuration_->get_max_remote_subscribers());
            std::scoped_lock its_lock(eventgroups_mutex_);
            eventgroups_[service_instance_t{_service, _instance}][eg] = its_eventgroupinfo;
        }
        its_eventgroupinfo->add_event(its_event);
    }

    std::scoped_lock its_lock(events_mutex_);
    events_[service_instance_t{_service, _instance}][_notifier] = its_event;
}

void routing_manager_impl::unset_all_eventpayloads(service_t _service, instance_t _instance) {
    std::set<std::shared_ptr<event>> its_events;
    {
        std::scoped_lock its_lock(eventgroups_mutex_);
        const auto search = eventgroups_.find(service_instance_t{_service, _instance});

        if (search != eventgroups_.end()) {
            for (const auto& [eventgroup_id, eventgroup_info] : search->second) {
                for (const auto& event : eventgroup_info->get_events()) {
                    its_events.insert(event);
                }
            }
        }
    }

    for (const auto& e : its_events) {
        if (e->is_set()) {
            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(e->get_event()) << "]";
        }
        e->unset_payload(true);
    }
}

void routing_manager_impl::unset_all_eventpayloads(service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    std::set<std::shared_ptr<event>> its_events;
    {
        std::scoped_lock its_lock(eventgroups_mutex_);
        const auto search = eventgroups_.find(service_instance_t{_service, _instance});

        if (search != eventgroups_.end()) {
            const auto found_eventgroup = search->second.find(_eventgroup);
            if (found_eventgroup != search->second.end()) {
                for (const auto& event : found_eventgroup->second->get_events()) {
                    its_events.insert(event);
                }
            }
        }
    }

    for (const auto& e : its_events) {
        if (e->is_set()) {
            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(e->get_event()) << "]";
        }
        e->unset_payload(true);
    }
}

void routing_manager_impl::unregister_event(client_t _client, service_t _service, instance_t _instance, event_t _event, bool _is_provided) {
    (void)_client;
    std::shared_ptr<event> its_unrefed_event;
    {
        std::scoped_lock its_lock(events_mutex_);
        const auto search = events_.find(service_instance_t{_service, _instance});
        if (search != events_.end()) {
            const auto found_event = search->second.find(_event);
            if (found_event != search->second.end()) {
                auto its_event = found_event->second;
                its_event->remove_ref(_client, _is_provided);
                if (!its_event->has_ref()) {
                    its_unrefed_event = its_event;
                    search->second.erase(found_event);
                } else if (_is_provided) {
                    its_event->set_provided(false);
                }
            }
        }
    }
    if (its_unrefed_event) {
        auto its_eventgroups = its_unrefed_event->get_eventgroups();
        for (auto eg : its_eventgroups) {
            std::shared_ptr<eventgroupinfo> its_eventgroup_info = find_eventgroup(_service, _instance, eg);
            if (its_eventgroup_info) {
                its_eventgroup_info->remove_event(its_unrefed_event);
                if (0 == its_eventgroup_info->get_events().size()) {
                    remove_eventgroup_info(_service, _instance, eg);
                }
            }
        }
    }
}

std::set<std::shared_ptr<eventgroupinfo>> routing_manager_impl::find_eventgroups(service_t _service, instance_t _instance) const {

    std::set<std::shared_ptr<eventgroupinfo>> its_eventgroups;

    std::scoped_lock its_lock{eventgroups_mutex_};
    const auto search = eventgroups_.find(service_instance_t{_service, _instance});

    if (search != eventgroups_.end()) {
        for (const auto& [eventgroup_id, eventgroup_info] : search->second) {
            its_eventgroups.insert(eventgroup_info);
        }
    }

    return its_eventgroups;
}
void routing_manager_impl::remove_eventgroup_info(service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    std::scoped_lock its_lock(eventgroups_mutex_);
    const auto search = eventgroups_.find(service_instance_t{_service, _instance});

    if (search != eventgroups_.end()) {
        const auto found_eventgroup = search->second.find(_eventgroup);
        if (found_eventgroup != search->second.end()) {
            search->second.erase(found_eventgroup);
        }
    }
}

std::set<std::shared_ptr<event>> routing_manager_impl::find_events(service_t _service, instance_t _instance,
                                                                   eventgroup_t _eventgroup) const {
    std::scoped_lock its_lock(eventgroups_mutex_);

    const auto search = eventgroups_.find(service_instance_t{_service, _instance});
    if (search != eventgroups_.end()) {
        const auto found_eventgroup = search->second.find(_eventgroup);
        if (found_eventgroup != search->second.end()) {
            return found_eventgroup->second->get_events();
        }
    }

    return std::set<std::shared_ptr<event>>();
}

std::vector<event_t> routing_manager_impl::find_events(service_t _service, instance_t _instance) const {
    std::vector<event_t> its_events;
    std::scoped_lock its_lock(events_mutex_);
    const auto search = events_.find(service_instance_t{_service, _instance});

    if (search != events_.end()) {
        for (const auto& [event_id, event_ptr] : search->second) {
            its_events.push_back(event_id);
        }
    }

    return its_events;
}

bool routing_manager_impl::insert_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                               const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client) {

    bool is_inserted(false);
    if (_event != ANY_EVENT) { // subscribe to specific event
        std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
        if (its_event) {
            is_inserted = its_event->add_subscriber(_eventgroup, _filter, _client, host_->is_routing());
        } else {
            VSOMEIP_WARNING_P << "(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                              << "." << hex4(_event) << "] received subscription for unknown (unrequested /unoffered) event. Creating"
                              << " placeholder event holding subscription until event is requested/offered.";
            is_inserted = create_placeholder_event_and_subscribe(_service, _instance, _eventgroup, _event, _filter, _client);
        }
    } else { // subscribe to all events of the eventgroup
        std::shared_ptr<eventgroupinfo> its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
        bool create_place_holder(false);
        if (its_eventgroup) {
            std::set<std::shared_ptr<event>> its_events = its_eventgroup->get_events();
            if (!its_events.size()) {
                create_place_holder = true;
            } else {
                for (const auto& e : its_events) {
                    is_inserted = e->add_subscriber(_eventgroup, _filter, _client, host_->is_routing()) || is_inserted;
                }
            }
        } else {
            create_place_holder = true;
        }
        if (create_place_holder) {
            VSOMEIP_WARNING_P << ":(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                              << "." << hex4(_event) << "] received subscription for unknown (unrequested /unoffered) eventgroup. Creating"
                              << " placeholder event holding subscription until event is requested/offered.";
            is_inserted = create_placeholder_event_and_subscribe(_service, _instance, _eventgroup, _event, _filter, _client);
        }
    }
    return is_inserted;
}

void routing_manager_impl::clear_shadow_subscriptions(void) {
    std::scoped_lock its_lock(events_mutex_);

    for (const auto& [service_instance_key, eventmap] : events_) {
        for (auto [event_id, event] : eventmap) {
            if (event->is_shadow()) {
                event->clear_subscribers();
            }
        }
    }
}

std::set<std::tuple<service_t, instance_t, eventgroup_t>> routing_manager_impl::get_subscriptions(const client_t _client) {
    std::set<std::tuple<service_t, instance_t, eventgroup_t>> result;
    std::scoped_lock its_lock(events_mutex_);

    for (const auto& [key, eventmap] : events_) {
        for (auto [event_id, event] : eventmap) {
            auto its_eventgroups = event->get_eventgroups(_client);
            for (const auto& e : its_eventgroups) {
                result.insert(std::make_tuple(key.service(), key.instance(), e));
            }
        }
    }

    return result;
}

void routing_manager_impl::notify_one_current_value(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                    event_t _event) {
    if (_event != ANY_EVENT) {
        std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
        if (its_event && its_event->is_field())
            its_event->notify_one(_client, false);
    } else {
        auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
        if (its_eventgroup) {
            std::set<std::shared_ptr<event>> its_events = its_eventgroup->get_events();
            for (const auto& e : its_events) {
                if (e->is_field()) {
                    e->notify_one(_client, false);
                }
            }
        }
    }
}

bool routing_manager_impl::is_subscribe_to_any_event_allowed(const vsomeip_sec_client_t* _sec_client, client_t _client, service_t _service,
                                                             instance_t _instance, eventgroup_t _eventgroup) {

    bool is_allowed(true);

    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        for (const auto& e : its_eventgroup->get_events()) {
            if (VSOMEIP_SEC_OK
                != configuration_->get_security()->is_client_allowed_to_access_member(_sec_client, _service, _instance, e->get_event())) {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(_client)
                                << " : routing_manager_impl::is_subscribe_to_any_event_allowed: "
                                << "subscribes to service/instance/event " << hex4(_service) << "/" << hex4(_instance) << "/"
                                << hex4(e->get_event()) << " which violates the security policy!";
                is_allowed = false;
                break;
            }
        }
    }

    return is_allowed;
}

std::shared_ptr<event> routing_manager_impl::find_event(service_t _service, instance_t _instance, event_t _event) const {
    std::scoped_lock its_lock(events_mutex_);
    std::shared_ptr<event> its_event;

    const auto search = events_.find(service_instance_t{_service, _instance});

    if (search != events_.end()) {
        const auto found_event = search->second.find(_event);
        if (found_event != search->second.end()) {
            its_event = found_event->second;
        }
    }

    return its_event;
}

std::shared_ptr<eventgroupinfo> routing_manager_impl::find_eventgroup(service_t _service, instance_t _instance,
                                                                      eventgroup_t _eventgroup) const {
    std::scoped_lock its_lock(eventgroups_mutex_);

    std::shared_ptr<eventgroupinfo> its_info(nullptr);

    const auto search = eventgroups_.find(service_instance_t{_service, _instance});

    if (search != eventgroups_.end()) {
        const auto found_eventgroup = search->second.find(_eventgroup);
        if (found_eventgroup != search->second.end()) {
            its_info = found_eventgroup->second;
            std::shared_ptr<serviceinfo> its_service_info = find_service(_service, _instance);
            if (its_service_info) {
                std::string its_multicast_address;
                uint16_t its_multicast_port;
                if (configuration_->get_multicast(_service, _instance, _eventgroup, its_multicast_address, its_multicast_port)) {
                    try {
                        its_info->set_multicast(boost::asio::ip::make_address(its_multicast_address), its_multicast_port);
                    } catch (...) {
                        VSOMEIP_ERROR_P << "Eventgroup [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                                        << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                                        << "] is configured as multicast, but no valid multicast address is configured!";
                    }
                }

                // LB: THIS IS STRANGE. A "FIND" - METHOD SHOULD NOT ADD INFORMATION...
                its_info->set_major(its_service_info->get_major());
                its_info->set_ttl(its_service_info->get_ttl());
                its_info->set_threshold(configuration_->get_threshold(_service, _instance, _eventgroup));
            }
        }
    }

    return its_info;
}

void routing_manager_impl::stop_offer_service_base(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                   minor_version_t _minor) {
    (void)_client;
    (void)_major;
    (void)_minor;

    std::map<event_t, std::shared_ptr<event>> events;
    {
        std::scoped_lock its_lock(events_mutex_);
        const auto search = events_.find(service_instance_t{_service, _instance});
        if (search != events_.end()) {
            for (const auto& [event_id, event_ptr] : search->second) {
                events[event_id] = event_ptr;
            }
        }
    }
    for (auto& e : events) {
        if (e.second->is_set()) {
            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(e.first) << "]";
        }
        e.second->unset_payload();
        e.second->clear_subscribers();
    }
}
session_t routing_manager_impl::get_event_session() {
    return host_->get_session(false);
}

bool routing_manager_impl::send_event_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target,
                                         std::shared_ptr<message> _message) {
    return send_to(_client, _target, _message);
}

} // namespace vsomeip_v3

// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

#ifndef WITHOUT_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <boost/asio/steady_timer.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/event.hpp"
#include "../include/eventgroupinfo.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_impl.hpp"
#include "../include/routing_manager_stub.hpp"
#include "../include/serviceinfo.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../endpoints/include/local_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_client_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../endpoints/include/virtual_server_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"
#include "../../service_discovery/include/constants.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../service_discovery/include/service_discovery_impl.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../../plugin/include/plugin_manager.hpp"

#include "../../e2e_protection/include/buffer/buffer.hpp"
#include "../../e2e_protection/include/e2exf/config.hpp"

#include "../../e2e_protection/include/e2e/profile/profile01/profile_01.hpp"
#include "../../e2e_protection/include/e2e/profile/profile01/protector.hpp"
#include "../../e2e_protection/include/e2e/profile/profile01/checker.hpp"

#include "../../e2e_protection/include/e2e/profile/profile_custom/profile_custom.hpp"
#include "../../e2e_protection/include/e2e/profile/profile_custom/protector.hpp"
#include "../../e2e_protection/include/e2e/profile/profile_custom/checker.hpp"

namespace vsomeip {

routing_manager_impl::routing_manager_impl(routing_manager_host *_host) :
        routing_manager_base(_host),
        version_log_timer_(_host->get_io()),
        if_state_running_(false),
#ifndef WITHOUT_SYSTEMD
        watchdog_timer_(_host->get_io()),
#endif
        status_log_timer_(_host->get_io()),
        memory_log_timer_(_host->get_io())
{
}

routing_manager_impl::~routing_manager_impl() {
}

boost::asio::io_service & routing_manager_impl::get_io() {
    return routing_manager_base::get_io();
}

client_t routing_manager_impl::get_client() const {
    return routing_manager_base::get_client();
}

void routing_manager_impl::init() {
    routing_manager_base::init();

    // TODO: Only instantiate the stub if needed
    stub_ = std::make_shared<routing_manager_stub>(this, configuration_);
    stub_->init();

    // We need to be able to send messages to ourself (for delivering events)
    (void)create_local(VSOMEIP_ROUTING_CLIENT);

    if (configuration_->is_sd_enabled()) {
        VSOMEIP_INFO<< "Service Discovery enabled. Trying to load module.";
        auto its_plugin = plugin_manager::get()->get_plugin(
                plugin_type_e::SD_RUNTIME_PLUGIN, VSOMEIP_SD_LIBRARY);
        if (its_plugin) {
            VSOMEIP_INFO << "Service Discovery module loaded.";
            discovery_ = std::dynamic_pointer_cast<sd::runtime>(its_plugin)->create_service_discovery(this);
            discovery_->init();
        }
    }

    if( configuration_->is_e2e_enabled()) {
        VSOMEIP_INFO << "E2E protection enabled.";
        std::map<e2exf::data_identifier, std::shared_ptr<cfg::e2e>> its_e2e_configuration = configuration_->get_e2e_configuration();
        for (auto &identifier : its_e2e_configuration) {
            auto its_cfg = identifier.second;
            if(its_cfg->profile == "CRC8") {
                e2exf::data_identifier its_data_identifier = {its_cfg->service_id, its_cfg->event_id};
                e2e::profile01::profile_config its_profile_config = e2e::profile01::profile_config(its_cfg->crc_offset, its_cfg->data_id,
                    (e2e::profile01::p01_data_id_mode) its_cfg->data_id_mode, its_cfg->data_length, its_cfg->counter_offset, its_cfg->data_id_nibble_offset);
                if ((its_cfg->variant == "protector") || (its_cfg->variant == "both")) {
                    custom_protectors[its_data_identifier] = std::make_shared<e2e::profile01::protector>(its_profile_config);
                }
                if ((its_cfg->variant == "checker") || (its_cfg->variant == "both")) {
                    custom_checkers[its_data_identifier] = std::make_shared<e2e::profile01::profile_01_checker>(its_profile_config);
                }
            } else if(its_cfg->profile == "CRC32") {
                e2exf::data_identifier its_data_identifier = {its_cfg->service_id, its_cfg->event_id};
                e2e::profile_custom::profile_config its_profile_config = e2e::profile_custom::profile_config(its_cfg->crc_offset);

                if ((its_cfg->variant == "protector") || (its_cfg->variant == "both")) {
                    custom_protectors[its_data_identifier] = std::make_shared<e2e::profile_custom::protector>(its_profile_config);
                }
                if ((its_cfg->variant == "checker") || (its_cfg->variant == "both")) {
                    custom_checkers[its_data_identifier]   = std::make_shared<e2e::profile_custom::profile_custom_checker>(its_profile_config);
                }
            }
        }
    }
}

void routing_manager_impl::start() {
#ifndef _WIN32
    netlink_connector_ = std::make_shared<netlink_connector>(host_->get_io(),
            configuration_->get_unicast_address());
    netlink_connector_->register_net_if_changes_handler(
            std::bind(&routing_manager_impl::on_net_if_state_changed,
            this, std::placeholders::_1, std::placeholders::_2));
    netlink_connector_->start();
#else
    start_ip_routing();
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

#ifndef WITHOUT_SYSTEMD
    {
        std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
        watchdog_timer_.expires_from_now(std::chrono::seconds(0));
        watchdog_timer_.async_wait(std::bind(&routing_manager_impl::watchdog_cbk,
        this, std::placeholders::_1));
    }
#endif
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
}

void routing_manager_impl::stop() {
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
#ifndef WITHOUT_SYSTEMD
    {
        std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
        watchdog_timer_.cancel();
    }
    sd_notify(0, "STOPPING=1");
    VSOMEIP_INFO << "Sent STOPPING to systemd watchdog";
#endif

    host_->on_state(state_type_e::ST_DEREGISTERED);

    if (discovery_)
        discovery_->stop();
    stub_->stop();

    for (auto client: get_connected_clients()) {
        if (client != VSOMEIP_ROUTING_CLIENT) {
            remove_local(client);
        }
    }
}

bool routing_manager_impl::offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    VSOMEIP_INFO << "OFFER("
        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance
        << ":" << std::dec << int(_major) << "." << std::dec << _minor << "]";

    if(!handle_local_offer_service(_client, _service, _instance, _major, _minor)) {
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
            discovery_->offer_service(_service, _instance, its_info);
        }
    }

    {
        std::lock_guard<std::mutex> ist_lock(pending_subscription_mutex_);
        std::set<event_t> its_already_subscribed_events;
        for (auto &ps : pending_subscriptions_) {
            if (ps.service_ == _service &&
                    ps.instance_ == _instance && ps.major_ == _major) {
                insert_subscription(ps.service_, ps.instance_,
                        ps.eventgroup_, ps.event_, client_, &its_already_subscribed_events);
            }
        }
        send_pending_subscriptions(_service, _instance, _major);
    }
    stub_->on_offer_service(_client, _service, _instance, _major, _minor);
    on_availability(_service, _instance, true, _major, _minor);
    return true;
}

void routing_manager_impl::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {

    VSOMEIP_INFO << "STOP OFFER("
        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance
        << ":" << std::dec << int(_major) << "." << _minor << "]";

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
}

void routing_manager_impl::request_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        bool _use_exclusive_proxy) {

    VSOMEIP_INFO << "REQUEST("
        << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << ":"
        << std::dec << int(_major) << "." << std::dec << _minor << "]";

    routing_manager_base::request_service(_client, _service, _instance, _major,
            _minor, _use_exclusive_proxy);

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
                its_info->add_client(_client);
                find_or_create_remote_client(_service, _instance, true, VSOMEIP_ROUTING_CLIENT);
                if (_use_exclusive_proxy) {
                    std::shared_ptr<endpoint> its_endpoint = its_info->get_endpoint(true);
                    if(its_endpoint) {
                        find_or_create_remote_client(_service, _instance, true, _client);
                    }
                }
            }
        }
    }

    if (_use_exclusive_proxy) {
        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
        specific_endpoint_clients_[_service][_instance].insert(_client);
    }

    if (_client == get_client()) {
        stub_->create_local_receiver();

        service_data_t request = { _service, _instance, _major, _minor, _use_exclusive_proxy };
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
    if(its_info && !its_info->is_local()) {
        unsubscribe_specific_client_at_sd(_service, _instance, _client);
        if(!its_info->get_requesters_size()) {
            if(discovery_) {
                discovery_->release_service(_service, _instance);
                discovery_->unsubscribe_client(_service, _instance, VSOMEIP_ROUTING_CLIENT);
            }
            clear_client_endpoints(_service, _instance, true);
            clear_client_endpoints(_service, _instance, false);
            its_info->set_endpoint(nullptr, true);
            its_info->set_endpoint(nullptr, false);
            clear_identified_clients(_service, _instance);
            clear_identifying_clients( _service, _instance);
            unset_all_eventpayloads(_service, _instance);
        } else {
            remove_identified_client(_service, _instance, _client);
            remove_identifying_client(_service, _instance, _client);
            remove_specific_client_endpoint(_service, _instance, _client, true);
            remove_specific_client_endpoint(_service, _instance, _client, false);
        }
    } else {
        if(discovery_) {
            discovery_->release_service(_service, _instance);
        }
    }
}

void routing_manager_impl::subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        event_t _event, subscription_type_e _subscription_type) {

    VSOMEIP_INFO << "SUBSCRIBE("
        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << ":"
        << std::hex << std::setw(4) << std::setfill('0') << _event << ":"
        << std::dec << (uint16_t)_major << "]";
    const client_t its_local_client = find_local_client(_service, _instance);
    if (get_client() == its_local_client) {
        auto self = shared_from_this();
        host_->on_subscription(_service, _instance, _eventgroup, _client, true,
            [this, self, _client, _service, _instance, _eventgroup,
                _event, _major, _subscription_type]
                    (const bool _subscription_accepted) {
            (void) find_or_create_local(_client);
            if (!_subscription_accepted) {
                stub_->send_subscribe_nack(_client, _service, _instance, _eventgroup, _event);
                VSOMEIP_INFO << "Subscription request from client: 0x" << std::hex
                             << _client << std::dec << " for eventgroup: 0x" << _eventgroup
                             << " rejected from application handler.";
                return;
            } else {
                stub_->send_subscribe_ack(_client, _service, _instance, _eventgroup, _event);
            }
            routing_manager_base::subscribe(_client, _service, _instance, _eventgroup, _major, _event, _subscription_type);
            send_pending_notify_ones(_service, _instance, _eventgroup, _client);
        });
    } else {
        if (discovery_) {
            client_t subscriber = VSOMEIP_ROUTING_CLIENT;
            if (0 == its_local_client) {
                subscriber = is_specific_endpoint_client(_client, _service, _instance);
                if (subscriber != VSOMEIP_ROUTING_CLIENT) {
                    if (supports_selective(_service, _instance)) {
                        identify_for_subscribe(_client, _service, _instance,
                                _major, _subscription_type);
                    } else {
                        VSOMEIP_INFO << "Subcribe to legacy selective service: " << std::hex
                                     << _service << ":" << _instance << ".";
                    }
                }
            }
            std::unique_lock<std::mutex> eventgroup_lock;
            auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
            if (its_eventgroup) {
                eventgroup_lock = its_eventgroup->get_subscription_lock();
            }
            std::set<event_t> its_already_subscribed_events;
            bool inserted = insert_subscription(_service, _instance, _eventgroup,
                    _event, _client, &its_already_subscribed_events);
            if (inserted) {
                if (0 == its_local_client) {
                    handle_subscription_state(_client, _service, _instance, _eventgroup, _event);
                    if (its_eventgroup) {
                        eventgroup_lock.unlock();
                    }
                    static const ttl_t configured_ttl(configuration_->get_sd_ttl());
                    notify_one_current_value(_client, _service, _instance,
                            _eventgroup, _event, its_already_subscribed_events);
                    discovery_->subscribe(_service, _instance, _eventgroup,
                                          _major, configured_ttl, subscriber, _subscription_type);
                } else {
                    if (is_available(_service, _instance, _major)) {
                        stub_->send_subscribe(find_local(_service, _instance),
                               _client, _service, _instance, _eventgroup, _major, _event, DEFAULT_SUBSCRIPTION);
                    }
                }
            } else if (its_eventgroup) {
                eventgroup_lock.unlock();
            }
            if (get_client() == _client) {
                std::lock_guard<std::mutex> ist_lock(pending_subscription_mutex_);
                subscription_data_t subscription = { _service, _instance, _eventgroup, _major,
                        _event, _subscription_type};
                pending_subscriptions_.insert(subscription);
            }
        } else {
            VSOMEIP_ERROR<< "SOME/IP eventgroups require SD to be enabled!";
        }
    }
}

void routing_manager_impl::unsubscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, event_t _event) {

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
        for (auto e : its_info->get_events()) {
            if (e->get_event() == _event || ANY_EVENT == _event)
                e->remove_subscriber(_eventgroup, _client);
        }
        for (auto e : its_info->get_events()) {
            if (e->has_subscriber(_eventgroup, ANY_CLIENT)) {
                last_subscriber_removed = false;
                break;
            }
        }
    }

    if (discovery_) {
        host_->on_subscription(_service, _instance, _eventgroup, _client, false, [](const bool _subscription_accepted){ (void)_subscription_accepted; });
        if (0 == find_local_client(_service, _instance)) {
            client_t subscriber = is_specific_endpoint_client(_client, _service, _instance);
            if (last_subscriber_removed) {
                unset_all_eventpayloads(_service, _instance, _eventgroup);
            }
            if (subscriber == VSOMEIP_ROUTING_CLIENT && last_subscriber_removed) {
                {
                    auto tuple = std::make_tuple(_service, _instance, _eventgroup, subscriber);
                    std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
                    remote_subscription_state_.erase(tuple);
                }
                // for normal subscribers only unsubscribe via SD  if last
                // subscriber was removed
                discovery_->unsubscribe(_service, _instance, _eventgroup, subscriber);
            } else if (subscriber != VSOMEIP_ROUTING_CLIENT) {
                {
                    auto tuple = std::make_tuple(_service, _instance, _eventgroup, subscriber);
                    std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
                    remote_subscription_state_.erase(tuple);
                }
                // for selective subscribers always unsubscribe at the SD
                discovery_->unsubscribe(_service, _instance, _eventgroup, subscriber);
            }
        } else {
            if (get_client() == _client) {
                std::lock_guard<std::mutex> ist_lock(pending_subscription_mutex_);
                remove_pending_subscription(_service, _instance, _eventgroup, _event);
                stub_->send_unsubscribe(find_local(_service, _instance),
                        _client, _service, _instance, _eventgroup, _event, false);
            }
        }
        clear_multicast_endpoints(_service, _instance);

    } else {
        VSOMEIP_ERROR<< "SOME/IP eventgroups require SD to be enabled!";
    }
}

bool routing_manager_impl::send(client_t _client,
        std::shared_ptr<message> _message, bool _flush) {
    return routing_manager_base::send(_client, _message, _flush);
}

bool routing_manager_impl::send(client_t _client, const byte_t *_data,
        length_t _size, instance_t _instance,
        bool _flush, bool _reliable, bool _is_valid_crc) {
    bool is_sent(false);
    if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
        std::shared_ptr<endpoint> its_target;
        bool is_request = utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS]);
        bool is_notification = utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]);

        client_t its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN],
                _data[VSOMEIP_CLIENT_POS_MAX]);
        service_t its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        method_t its_method = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);

        bool is_service_discovery = (its_service == vsomeip::sd::service
                && its_method == vsomeip::sd::method);

#ifdef USE_DLT
        bool is_response(false);
#endif

        if (is_request) {
            its_target = find_local(its_service, _instance);
        } else if (!is_notification) {
#ifdef USE_DLT
            is_response = true;
#endif
            its_target = find_local(its_client);
        } else if (is_notification && _client) { // Selective notifications!
            if (_client == get_client()) {
#ifdef USE_DLT
                const uint16_t its_data_size
                    = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                tc::trace_header its_header;
                if (its_header.prepare(its_target, true, _instance))
                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                            _data, its_data_size);
#endif
                deliver_message(_data, _size, _instance, _reliable, _is_valid_crc);
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

                tc::trace_header its_header;
                if (its_header.prepare(its_target, true, _instance))
                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                            _data, its_data_size);
            }
#endif
            is_sent = send_local(its_target, get_client(), _data, _size, _instance, _flush, _reliable, VSOMEIP_SEND, _is_valid_crc);
        } else {
            // Check whether hosting application should get the message
            // If not, check routes to external
            if ((its_client == host_->get_client() && !is_request)
                    || (find_local_client(its_service, _instance)
                            == host_->get_client() && is_request)) {
                // TODO: find out how to handle session id here
                is_sent = deliver_message(_data, _size, _instance, _reliable, _is_valid_crc);
            } else {
                e2e_buffer outputBuffer;
                if( configuration_->is_e2e_enabled()) {
                    if ( !is_service_discovery) {
                        service_t its_service = VSOMEIP_BYTES_TO_WORD(
                                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
                        method_t its_method = VSOMEIP_BYTES_TO_WORD(
                                _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
                        if( custom_protectors.count({its_service, its_method})) {
                            outputBuffer.assign(_data, _data + VSOMEIP_PAYLOAD_POS);
                            e2e_buffer inputBuffer(_data + VSOMEIP_PAYLOAD_POS, _data +_size);
                            custom_protectors[{its_service, its_method}]->protect( inputBuffer);
                            outputBuffer.resize(inputBuffer.size() + VSOMEIP_PAYLOAD_POS);
                            std::copy(inputBuffer.begin(), inputBuffer.end(), outputBuffer.begin() + VSOMEIP_PAYLOAD_POS);
                            _data = outputBuffer.data();
                       }
                    }
                }
                if (is_request) {
                    client_t client = is_specific_endpoint_client(its_client, its_service, _instance);
                    its_target = find_or_create_remote_client(its_service, _instance, _reliable, client);
                    if (its_target) {
#ifdef USE_DLT
                        const uint16_t its_data_size
                            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                        tc::trace_header its_header;
                        if (its_header.prepare(its_target, true, _instance))
                            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                    _data, its_data_size);
#endif
                        is_sent = its_target->send(_data, _size, _flush);
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
                            send_local_notification(get_client(), _data, _size, _instance, _flush, _reliable, _is_valid_crc);
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
                                bool is_offered_both(false);
                                if (its_udp_server_endpoint && its_tcp_server_endpoint) {
                                    is_offered_both = true;
                                }
                                if (its_udp_server_endpoint || its_tcp_server_endpoint) {
                                    for (auto its_group : its_event->get_eventgroups()) {
                                        auto its_eventgroup = find_eventgroup(its_service, _instance, its_group);
                                        if (its_eventgroup) {
                                            // Unicast targets
                                            for (const auto &its_remote : its_eventgroup->get_targets()) {
                                                if(its_remote.endpoint_->is_reliable() && its_tcp_server_endpoint) {
                                                    if (!is_offered_both || (is_offered_both && its_event->is_reliable())) {
                                                        its_targets.insert(its_remote.endpoint_);
                                                    }
                                                } else if (its_udp_server_endpoint && !its_eventgroup->is_sending_multicast()) {
                                                    if (!is_offered_both || (is_offered_both && !its_event->is_reliable())) {
                                                        its_targets.insert(its_remote.endpoint_);
                                                    }
                                                }
                                            }
                                            // Send to multicast targets if subscribers are still interested
                                            if (its_eventgroup->is_sending_multicast()) {
                                                for (auto its_multicast_target : its_eventgroup->get_multicast_targets()) {
                                                    if (!is_offered_both || (is_offered_both && !its_event->is_reliable())) {
                                                        its_targets.insert(its_multicast_target.endpoint_);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                for (auto const &target : its_targets) {
                                    if (target->is_reliable()) {
                                        its_tcp_server_endpoint->send_to(target, _data, _size, _flush);
                                    } else {
                                        its_udp_server_endpoint->send_to(target, _data, _size, _flush);
                                    }
#ifdef USE_DLT
                                    has_sent = true;
#endif
                                }
#ifdef USE_DLT
                                if (has_sent) {
                                    const uint16_t its_data_size
                                        = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                                    tc::trace_header its_header;
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
                                // we received a response/error but neither the hosting application
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

                                tc::trace_header its_header;
                                if (its_header.prepare(its_target, true, _instance))
                                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                            _data, its_data_size);
#endif
                                is_sent = its_target->send(_data, _size, _flush);
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
        const std::shared_ptr<endpoint_definition> &_target,
        std::shared_ptr<message> _message, bool _flush) {
    bool is_sent(false);
    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    if (serializer_->serialize(_message.get())) {
        const byte_t *_data = serializer_->get_data();
        length_t _size = serializer_->get_size();
        e2e_buffer outputBuffer;
        if( configuration_->is_e2e_enabled()) {
            service_t its_service = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
            method_t its_method = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
            if( custom_protectors.count({its_service, its_method})) {
                outputBuffer.assign(_data, _data + VSOMEIP_PAYLOAD_POS);
                e2e_buffer inputBuffer(_data + VSOMEIP_PAYLOAD_POS, _data +_size);
                custom_protectors[{its_service, its_method}]->protect( inputBuffer);
                outputBuffer.resize(inputBuffer.size() + VSOMEIP_PAYLOAD_POS);
                std::copy(inputBuffer.begin(), inputBuffer.end(), outputBuffer.begin() + VSOMEIP_PAYLOAD_POS);
                _data = outputBuffer.data();
           }
        }
        is_sent = send_to(_target, _data, _size, _message->get_instance(), _flush);
        serializer_->reset();
    } else {
        VSOMEIP_ERROR<< "routing_manager_impl::send_to: serialization failed.";
    }
    return (is_sent);
}

bool routing_manager_impl::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size, instance_t _instance,
        bool _flush) {
    std::shared_ptr<endpoint> its_endpoint = find_server_endpoint(
            _target->get_remote_port(), _target->is_reliable());

    if (its_endpoint) {
#ifdef USE_DLT
        const uint16_t its_data_size
            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

        tc::trace_header its_header;
        if (its_header.prepare(its_endpoint, true, _instance))
            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                    _data, its_data_size);
#else
        (void) _instance;
#endif
        return its_endpoint->send_to(_target, _data, _size, _flush);
    }
    return false;
}

bool routing_manager_impl::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size, uint16_t _sd_port) {
    std::shared_ptr<endpoint> its_endpoint = find_server_endpoint(
            _sd_port, _target->is_reliable());

    if (its_endpoint) {
#ifdef USE_DLT
        const uint16_t its_data_size
            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

        tc::trace_header its_header;
        if (its_header.prepare(its_endpoint, true, 0x0))
            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                    _data, its_data_size);
#endif
        return its_endpoint->send_to(_target, _data, _size);
    }

    return false;
}

void routing_manager_impl::register_event(
        client_t _client, service_t _service, instance_t _instance,
        event_t _event, const std::set<eventgroup_t> &_eventgroups,
        bool _is_field, std::chrono::milliseconds _cycle,
        bool _change_resets_cycle, epsilon_change_func_t _epsilon_change_func,
        bool _is_provided, bool _is_shadow, bool _is_cache_placeholder) {
    auto its_event = find_event(_service, _instance, _event);
    bool is_first(false);
    if (its_event && !its_event->has_ref(_client, _is_provided)) {
        is_first = true;
    } else {
        is_first = true;
    }
    if (is_first) {
        routing_manager_base::register_event(_client, _service, _instance,
                _event, _eventgroups, _is_field, _cycle, _change_resets_cycle,
                _epsilon_change_func, _is_provided, _is_shadow,
                _is_cache_placeholder);
    }
    VSOMEIP_INFO << "REGISTER EVENT("
        << std::hex << std::setw(4) << std::setfill('0') << _client << "): ["
        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
        << std::hex << std::setw(4) << std::setfill('0') << _event
        << ":is_provider=" << _is_provided << "]";
}

void routing_manager_impl::register_shadow_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, const std::set<eventgroup_t> &_eventgroups,
        bool _is_field, bool _is_provided) {
    routing_manager_base::register_event(_client, _service, _instance,
            _event, _eventgroups, _is_field,
            std::chrono::milliseconds::zero(), false,
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
        bool _force, bool _flush) {
    if (find_local(_client)) {
        routing_manager_base::notify_one(_service, _instance, _event, _payload,
                _client, _force, _flush);
    } else {
        std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
        if (its_event) {
            // Event is valid for service/instance
            bool found_eventgroup(false);
            // Iterate over all groups of the event to ensure at least
            // one valid eventgroup for service/instance exists.
            for (auto its_group : its_event->get_eventgroups()) {
                auto its_eventgroup = find_eventgroup(_service, _instance, its_group);
                if (its_eventgroup) {
                    // Eventgroup is valid for service/instance
                    found_eventgroup = true;
                    break;
                }
            }
            if (found_eventgroup) {
                std::set<std::shared_ptr<endpoint_definition>> its_targets;
                // Now set event's payload!
                // Either with endpoint_definition (remote) or with client (local).
                {
                    std::lock_guard<std::mutex> its_lock(remote_subscribers_mutex_);
                    auto its_service = remote_subscribers_.find(_service);
                    if (its_service != remote_subscribers_.end()) {
                        auto its_instance = its_service->second.find(_instance);
                        if (its_instance != its_service->second.end()) {
                            auto its_subscriber = its_instance->second.find(_client);
                            if (its_subscriber != its_instance->second.end()) {
                                its_targets = its_subscriber->second;
                            }
                        }
                    }
                }
                for (const auto &its_target : its_targets) {
                    its_event->set_payload(_payload, its_target, _force, _flush);
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

void routing_manager_impl::on_error(
        const byte_t *_data, length_t _length, endpoint *_receiver,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {
    instance_t its_instance = 0;
    if (_length >= VSOMEIP_SERVICE_POS_MAX) {
        service_t its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        its_instance = find_instance(its_service, _receiver);
    }
    send_error(return_code_e::E_MALFORMED_MESSAGE, _data, _length, its_instance,
            _receiver->is_reliable(), _receiver, _remote_address, _remote_port);
}

void routing_manager_impl::release_port(uint16_t _port, bool _reliable) {
    std::lock_guard<std::mutex> its_lock(used_client_ports_mutex_);
    used_client_ports_[_reliable].erase(_port);
}

void routing_manager_impl::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination,
        client_t _bound_client,
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
    bool its_is_crc_valid(true);
    instance_t its_instance(0x0);
#ifdef USE_DLT
    bool is_forwarded(true);
#endif
    if (_size >= VSOMEIP_SOMEIP_HEADER_SIZE) {
        its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
                _data[VSOMEIP_SERVICE_POS_MAX]);
        its_instance = find_instance(its_service, _receiver);
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
            if (configuration_->is_security_enabled()) {
                if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
                    client_t requester = VSOMEIP_BYTES_TO_WORD(
                            _data[VSOMEIP_CLIENT_POS_MIN],
                            _data[VSOMEIP_CLIENT_POS_MAX]);
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
                    if (!configuration_->is_client_allowed(requester, its_service, its_instance)) {
                        VSOMEIP_WARNING << std::hex << "Security: Received a remote request "
                                << "from client 0x" << requester << " for service/instance "
                                << its_service << "/" << its_instance
                                << " which violates the security policy ~> Skip message!";
                        return;
                    }
                }
            }
            if( configuration_->is_e2e_enabled()) {
                its_method = VSOMEIP_BYTES_TO_WORD(
                           _data[VSOMEIP_METHOD_POS_MIN],
                           _data[VSOMEIP_METHOD_POS_MAX]);
                if( custom_checkers.count({its_service, its_method})) {
                    e2e_buffer inputBuffer(_data + VSOMEIP_PAYLOAD_POS, _data + _size);
                    e2e::profile_interface::generic_check_status check_status;
                    custom_checkers[{its_service, its_method}]->check( inputBuffer, check_status);

                    if ( check_status != e2e::profile_interface::generic_check_status::E2E_OK ) {
                        VSOMEIP_INFO << std::hex << "E2E protection: CRC check failed for service: " << its_service << " method: " << its_method;
                        its_is_crc_valid = false;
                    }
                }
            }
            if (!deliver_specific_endpoint_message(
                    its_service, its_instance, _data, _size, _receiver)) {
                // set client ID to zero for all messages
                if( utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
                    byte_t *its_data = const_cast<byte_t *>(_data);
                    its_data[VSOMEIP_CLIENT_POS_MIN] = 0x0;
                    its_data[VSOMEIP_CLIENT_POS_MAX] = 0x0;
                }
                // Common way of message handling
#ifdef USE_DLT
                is_forwarded =
#endif
                on_message(its_service, its_instance, _data, _size, _receiver->is_reliable(), its_is_crc_valid);

            }
        }
    }
#ifdef USE_DLT
    if (is_forwarded) {
        const uint16_t its_data_size
            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

        tc::trace_header its_header;
        const boost::asio::ip::address_v4 its_remote_address =
                _remote_address.is_v4() ? _remote_address.to_v4() :
                        boost::asio::ip::address_v4::from_string("6.6.6.6");
        tc::protocol_e its_protocol =
                _receiver->is_local() ? tc::protocol_e::local :
                _receiver->is_reliable() ? tc::protocol_e::tcp :
                    tc::protocol_e::udp;
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
        bool _reliable, bool _is_valid_crc) {
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

    if (its_client == VSOMEIP_ROUTING_CLIENT
            && utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        is_forwarded = deliver_notification(_service, _instance, _data, _size, _reliable, _is_valid_crc);
    } else if (its_client == host_->get_client()) {
        deliver_message(_data, _size, _instance, _reliable, _is_valid_crc);
    } else {
        send(its_client, _data, _size, _instance, true, _reliable, _is_valid_crc); //send to proxy
    }
    return is_forwarded;
}

void routing_manager_impl::on_notification(client_t _client,
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _size, bool _notify_one) {
    event_t its_event_id = VSOMEIP_BYTES_TO_WORD(
                            _data[VSOMEIP_METHOD_POS_MIN],
                            _data[VSOMEIP_METHOD_POS_MAX]);

    std::shared_ptr<event> its_event = find_event(_service, _instance, its_event_id);
    if (its_event) {
        uint32_t its_length = utility::get_payload_size(_data, _size);
        std::shared_ptr<payload> its_payload =
                runtime::get()->create_payload(
                                    &_data[VSOMEIP_PAYLOAD_POS],
                                    its_length);

        if (_notify_one) {
            notify_one(_service, _instance, its_event->get_event(), its_payload, _client, true, true);
        } else {
            if (its_event->is_field()) {
                if (its_event->is_set()) {
                    its_event->set_payload(its_payload, false, true);
                } else {
                    // new subscribers will be notified by SD via sent_initiale_events;
                    if (its_event->get_remote_notification_pending()) {
                        // Set payload first time ~> notify all remote subscriber per unicast (initial field)
                        std::vector<std::unique_lock<std::mutex>> its_locks;
                        std::vector<std::shared_ptr<eventgroupinfo>> its_eventgroupinfos;
                        for (auto its_group : its_event->get_eventgroups()) {
                            auto its_eventgroup = find_eventgroup(_service, _instance, its_group);
                            if (its_eventgroup) {
                                its_locks.push_back(its_eventgroup->get_subscription_lock());
                                its_eventgroupinfos.push_back(its_eventgroup);
                            }
                        }

                        bool is_offered_both(false);
                        if (configuration_->get_reliable_port(_service, _instance) != ILLEGAL_PORT &&
                                configuration_->get_unreliable_port(_service, _instance) != ILLEGAL_PORT) {
                            is_offered_both = true;
                        }
                        for (const auto &its_eventgroup : its_eventgroupinfos) {
                            //Unicast targets
                            for (auto its_remote : its_eventgroup->get_targets()) {
                                if (!is_offered_both) {
                                    its_event->set_payload(its_payload, its_remote.endpoint_, true, true);
                                } else {
                                    bool was_set(false);
                                    if (its_event->is_reliable() && its_remote.endpoint_->is_reliable()) {
                                        its_event->set_payload(its_payload, its_remote.endpoint_, true, true);
                                        was_set = true;
                                    }
                                    if (!its_event->is_reliable() && !its_remote.endpoint_->is_reliable()) {
                                        its_event->set_payload(its_payload, its_remote.endpoint_, true, true);
                                        was_set = true;
                                    }
                                    if (!was_set) {
                                        its_event->set_payload_dont_notify(its_payload);
                                    }
                                }
                            }
                        }
                        its_event->set_remote_notification_pending(false);
                    } else {
                        its_event->set_payload_dont_notify(its_payload);
                    }
                }
            } else {
                its_event->set_payload(its_payload, false, true);
            }
        }
    }
}

void routing_manager_impl::on_connect(std::shared_ptr<endpoint> _endpoint) {
    // Is called when endpoint->connect succeeded!
    struct service_info {
        service_t service_id_;
        instance_t instance_id_;
        major_version_t major_;
        minor_version_t minor_;
        bool reliable_;
        std::shared_ptr<endpoint> endpoint_;
    };
    std::forward_list<struct service_info> services_to_report_;
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        for (auto &its_service : remote_services_) {
            for (auto &its_instance : its_service.second) {
                for (auto &its_client : its_instance.second) {
                    if (its_client.first == VSOMEIP_ROUTING_CLIENT ||
                            its_client.first == get_client()) {
                        auto found_endpoint = its_client.second.find(false);
                        if (found_endpoint != its_client.second.end()) {
                            if (found_endpoint->second.get() == _endpoint.get()) {
                                std::shared_ptr<serviceinfo> its_info(find_service(its_service.first, its_instance.first));
                                if (!its_info) {
                                    return;
                                }
                                services_to_report_.push_front(
                                            { its_service.first,
                                                    its_instance.first,
                                                    its_info->get_major(),
                                                    its_info->get_minor(),
                                                    false, nullptr });
                            }
                        }
                        found_endpoint = its_client.second.find(true);
                        if (found_endpoint != its_client.second.end()) {
                            if (found_endpoint->second.get() == _endpoint.get()) {
                                std::shared_ptr<serviceinfo> its_info(find_service(its_service.first, its_instance.first));
                                if (!its_info) {
                                    return;
                                }
                                services_to_report_.push_front(
                                            { its_service.first,
                                                    its_instance.first,
                                                    its_info->get_major(),
                                                    its_info->get_minor(),
                                                    true, _endpoint });
                            }
                        }
                    }
                }
            }
        }
    }
    for (const auto &s : services_to_report_) {
        on_availability(s.service_id_, s.instance_id_, true, s.major_, s.minor_);
        if (s.reliable_) {
            stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, s.service_id_,
                    s.instance_id_, s.major_, s.minor_);
        }
        std::shared_ptr<boost::asio::steady_timer> its_timer =
                std::make_shared<boost::asio::steady_timer>(io_);
        boost::system::error_code ec;
        its_timer->expires_from_now(std::chrono::milliseconds(3), ec);
        if (!ec) {
            its_timer->async_wait(
                    std::bind(
                            &routing_manager_impl::call_sd_endpoint_connected,
                            std::static_pointer_cast<routing_manager_impl>(
                                    shared_from_this()),
                            std::placeholders::_1, s.service_id_,
                            s.instance_id_, s.endpoint_, its_timer));
        } else {
            VSOMEIP_ERROR<< "routing_manager_impl::on_connect: " << ec.message();
        }
    }
}

void routing_manager_impl::on_disconnect(std::shared_ptr<endpoint> _endpoint) {
    // Is called when endpoint->connect fails!
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    for (auto &its_service : remote_services_) {
        for (auto &its_instance : its_service.second) {
            for (auto &its_client : its_instance.second) {
                if (its_client.first == VSOMEIP_ROUTING_CLIENT ||
                        its_client.first == get_client()) {
                    auto found_endpoint = its_client.second.find(false);
                    if (found_endpoint != its_client.second.end()) {
                        if (found_endpoint->second.get() == _endpoint.get()) {

                            std::shared_ptr<serviceinfo> its_info(find_service(its_service.first, its_instance.first));
                            if(!its_info){
                                return;
                            }
                            on_availability(its_service.first, its_instance.first,
                                    false, its_info->get_major(), its_info->get_minor());
                        }
                    }
                    found_endpoint = its_client.second.find(true);
                    if (found_endpoint != its_client.second.end()) {
                        if (found_endpoint->second.get() == _endpoint.get()) {

                            std::shared_ptr<serviceinfo> its_info(find_service(its_service.first, its_instance.first));
                            if(!its_info){
                                return;
                            }
                            on_availability(its_service.first, its_instance.first,
                                    false, its_info->get_major(), its_info->get_minor());
                        }
                    }
                }
            }
        }
    }
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
    }

    // Trigger "del_routing_info" either over SD or static
    if (discovery_) {
        if (its_info) {
            if (its_info->get_major() == _major && its_info->get_minor() == _minor) {
                its_info->set_ttl(0);
                discovery_->stop_offer_service(_service, _instance, its_info);
            }
        }
    } else {
        del_routing_info(_service, _instance,
                (its_reliable_endpoint != nullptr),
                (its_unreliable_endpoint != nullptr));
    }

    // Cleanup reliable & unreliable server endpoints hold before
    if (its_info) {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        std::shared_ptr<endpoint> its_empty_endpoint;
        bool reliable = true;

        // Loop over reliable/unreliable and cleanup if needed
        for (uint8_t i = 0; i < 2; ++i) {
            std::shared_ptr<endpoint> its_endpoint;
            if (reliable) {
                its_endpoint = its_reliable_endpoint;
            } else {
                its_endpoint = its_unreliable_endpoint;
            }
            if (!its_endpoint) {
                reliable = !reliable;
                continue;
            }

            // Check whether any service still uses this endpoint
            its_endpoint->decrement_use_count();
            bool isLastService = (its_endpoint->get_use_count() == 0);

            // Clear service_instances_
            if (1 >= service_instances_[_service].size()) {
                service_instances_.erase(_service);
            } else {
                service_instances_[_service].erase(its_endpoint.get());
            }

            // Clear server endpoint if no service remains using it
            if (isLastService) {
                uint16_t port = its_endpoint->get_local_port();
                if (server_endpoints_.find(port) != server_endpoints_.end()) {
                    server_endpoints_[port].erase(reliable);
                    if (server_endpoints_[port].find(!reliable) == server_endpoints_[port].end()) {
                        server_endpoints_.erase(port);
                    }
                }

                // Stop endpoint (close socket) to release its async_handlers!
                its_endpoint->stop();
            }

            // Clear service info and service group
            clear_service_info(_service, _instance, reliable);

            // Invert reliable flag and loop again
            reliable = !reliable;
        }
    }
}

bool routing_manager_impl::deliver_message(const byte_t *_data, length_t _size,
        instance_t _instance, bool _reliable, bool _is_valid_crc) {
    bool is_delivered(false);

    auto a_deserializer = get_deserializer();
    a_deserializer->set_data(_data, _size);
    std::shared_ptr<message> its_message(a_deserializer->deserialize_message());
    a_deserializer->reset();
    put_deserializer(a_deserializer);

    if (its_message) {
        its_message->set_instance(_instance);
        its_message->set_reliable(_reliable);
        its_message->set_is_valid_crc(_is_valid_crc);
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
        bool _reliable, bool _is_valid_crc) {
    method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
            _data[VSOMEIP_METHOD_POS_MAX]);

    std::shared_ptr<event> its_event = find_event(_service, _instance, its_method);
    if (its_event) {
        if (!its_event->is_provided()) {
            const uint32_t its_length(utility::get_payload_size(_data, _length));
            std::shared_ptr<payload> its_payload
                = runtime::get()->create_payload(&_data[VSOMEIP_PAYLOAD_POS],
                                                 its_length);
            if (!its_event->set_payload_dont_notify(its_payload)) {
                // do not forward the notification as it was filtered
                return false;
            }
        }

        for (const auto its_local_client : its_event->get_subscribers()) {
            if (its_local_client == host_->get_client()) {
                deliver_message(_data, _length, _instance, _reliable, _is_valid_crc);
            } else {
                std::shared_ptr<endpoint> its_local_target = find_local(its_local_client);
                if (its_local_target) {
                    send_local(its_local_target, VSOMEIP_ROUTING_CLIENT,
                            _data, _length, _instance, true, _reliable, VSOMEIP_SEND, _is_valid_crc);
                }
            }
        }
    }

    return true;
}

std::shared_ptr<eventgroupinfo> routing_manager_impl::find_eventgroup(
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) const {
    return routing_manager_base::find_eventgroup(_service, _instance, _eventgroup);
}

const std::shared_ptr<configuration> routing_manager_impl::get_configuration() const {
    return (host_->get_configuration());
}

std::shared_ptr<endpoint> routing_manager_impl::create_service_discovery_endpoint(
        const std::string &_address, uint16_t _port, bool _reliable) {
    std::shared_ptr<endpoint> its_service_endpoint = find_server_endpoint(_port,
            _reliable);
    if (!its_service_endpoint) {
        try {
            its_service_endpoint = create_server_endpoint(_port, _reliable,
                    true);

            if (its_service_endpoint) {
                sd_info_ = std::make_shared<serviceinfo>(ANY_MAJOR, ANY_MINOR,
                        DEFAULT_TTL, false); // false, because we do _not_ want to announce it...
                sd_info_->set_endpoint(its_service_endpoint, _reliable);
                its_service_endpoint->add_default_target(VSOMEIP_SD_SERVICE,
                        _address, _port);
                its_service_endpoint->join(_address);
            } else {
                VSOMEIP_ERROR<< "Service Discovery endpoint could not be created. "
                "Please check your network configuration.";
            }
        } catch (const std::exception &e) {
            host_->on_error(error_code_e::SERVER_ENDPOINT_CREATION_FAILED);
            VSOMEIP_ERROR << "Service Discovery endpoint could not be created: "
                    << e.what();
        }
    }
    return its_service_endpoint;
}

services_t routing_manager_impl::get_offered_services() const {
    services_t its_services;
    for (auto s : get_services()) {
        for (auto i : s.second) {
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
        for (const auto i : found_service->second) {
            if (i.second->is_local()) {
                its_instances[i.first] = i.second;
            }
        }
    }
    return its_instances;
}

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_remote_client(
        service_t _service, instance_t _instance, bool _reliable, client_t _client) {
    std::shared_ptr<endpoint> its_endpoint;
    bool start_endpoint(false);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        its_endpoint = find_remote_client(_service, _instance, _reliable, _client);
        if (!its_endpoint) {
            its_endpoint = create_remote_client(_service, _instance, _reliable, _client);
            start_endpoint = true;
        }
    }
    if (start_endpoint && its_endpoint
            && configuration_->is_someip(_service, _instance)) {
        its_endpoint->start();
    }
    return its_endpoint;
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
        std::shared_ptr<endpoint> its_reliable_endpoint;
        std::shared_ptr<endpoint> its_unreliable_endpoint;

        uint16_t its_reliable_port = configuration_->get_reliable_port(_service,
                _instance);
        uint16_t its_unreliable_port = configuration_->get_unreliable_port(
                _service, _instance);

        bool is_someip = configuration_->is_someip(_service, _instance);

        // Create server endpoints for local services only
        if (_is_local_service) {
            if (ILLEGAL_PORT != its_reliable_port) {
                its_reliable_endpoint = find_or_create_server_endpoint(
                        its_reliable_port, true, is_someip);
                if (its_reliable_endpoint) {
                    its_info->set_endpoint(its_reliable_endpoint, true);
                    its_reliable_endpoint->increment_use_count();
                    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
                    service_instances_[_service][its_reliable_endpoint.get()] =
                            _instance;
                }
            }

            if (ILLEGAL_PORT != its_unreliable_port) {
                its_unreliable_endpoint = find_or_create_server_endpoint(
                        its_unreliable_port, false, is_someip);
                if (its_unreliable_endpoint) {
                    its_info->set_endpoint(its_unreliable_endpoint, false);
                    its_unreliable_endpoint->increment_use_count();
                    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
                    service_instances_[_service][its_unreliable_endpoint.get()] =
                            _instance;
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
        host_->on_error(error_code_e::CONFIGURATION_MISSING);
    }
}

std::shared_ptr<endpoint> routing_manager_impl::create_client_endpoint(
        const boost::asio::ip::address &_address,
        uint16_t _local_port, uint16_t _remote_port,
        bool _reliable, client_t _client) {
    (void)_client;

    std::shared_ptr<endpoint> its_endpoint;
    try {
        if (_reliable) {
            its_endpoint = std::make_shared<tcp_client_endpoint_impl>(
                    shared_from_this(),
                    boost::asio::ip::tcp::endpoint(
                            (_address.is_v4() ?
                                    boost::asio::ip::tcp::v4() :
                                    boost::asio::ip::tcp::v6()),
                            _local_port),
                    boost::asio::ip::tcp::endpoint(_address, _remote_port),
                    io_,
                    configuration_->get_max_message_size_reliable(
                            _address.to_string(), _remote_port),
                    configuration_->get_buffer_shrink_threshold(),
                    std::chrono::milliseconds(configuration_->get_sd_ttl() * 666),
                    configuration_->get_endpoint_queue_limit(
                            _address.to_string(), _remote_port));

            if (configuration_->has_enabled_magic_cookies(_address.to_string(),
                    _remote_port)) {
                its_endpoint->enable_magic_cookies();
            }
        } else {
            its_endpoint = std::make_shared<udp_client_endpoint_impl>(
                    shared_from_this(),
                    boost::asio::ip::udp::endpoint(
                            (_address.is_v4() ?
                                boost::asio::ip::udp::v4() :
                                boost::asio::ip::udp::v6()),
                            _local_port),
                    boost::asio::ip::udp::endpoint(_address, _remote_port),
                    io_, configuration_->get_endpoint_queue_limit(
                            _address.to_string(), _remote_port));
        }
    } catch (...) {
        host_->on_error(error_code_e::CLIENT_ENDPOINT_CREATION_FAILED);
    }

    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::create_server_endpoint(
        uint16_t _port, bool _reliable, bool _start) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    std::shared_ptr<endpoint> its_endpoint;
    try {
        boost::asio::ip::address its_unicast = configuration_->get_unicast_address();
        if (_start) {
            if (_reliable) {
                its_endpoint = std::make_shared<tcp_server_endpoint_impl>(
                        shared_from_this(),
                        boost::asio::ip::tcp::endpoint(its_unicast, _port), io_,
                        configuration_->get_max_message_size_reliable(
                                its_unicast.to_string(), _port),
                        configuration_->get_buffer_shrink_threshold(),
                        std::chrono::milliseconds(configuration_->get_sd_ttl() * 666),
                        configuration_->get_endpoint_queue_limit(
                                its_unicast.to_string(), _port));
                if (configuration_->has_enabled_magic_cookies(
                        its_unicast.to_string(), _port) ||
                        configuration_->has_enabled_magic_cookies(
                                "local", _port)) {
                    its_endpoint->enable_magic_cookies();
                }
            } else {
                configuration::endpoint_queue_limit_t its_limit =
                        configuration_->get_endpoint_queue_limit(
                                its_unicast.to_string(), _port);
#ifndef _WIN32
                if (its_unicast.is_v4()) {
                    its_unicast = boost::asio::ip::address_v4::any();
                } else if (its_unicast.is_v6()) {
                    its_unicast = boost::asio::ip::address_v6::any();
                }
#endif
                boost::asio::ip::udp::endpoint ep(its_unicast, _port);
                its_endpoint = std::make_shared<udp_server_endpoint_impl>(
                        shared_from_this(), ep, io_, its_limit);
            }

        } else {
            its_endpoint = std::make_shared<virtual_server_endpoint_impl>(
                                its_unicast.to_string(), _port, _reliable);
        }

        if (its_endpoint) {
            server_endpoints_[_port][_reliable] = its_endpoint;
            its_endpoint->start();
        }
    } catch (const std::exception &e) {
        host_->on_error(error_code_e::SERVER_ENDPOINT_CREATION_FAILED);
        VSOMEIP_ERROR << e.what();
    }

    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::find_server_endpoint(
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

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_server_endpoint(
        uint16_t _port, bool _reliable, bool _start) {
    std::shared_ptr<endpoint> its_endpoint = find_server_endpoint(_port,
            _reliable);
    if (!its_endpoint) {
        its_endpoint = create_server_endpoint(_port, _reliable, _start);
    }
    return (its_endpoint);
}

void routing_manager_impl::remove_local(client_t _client) {
    routing_manager_base::remove_local(_client);
    std::forward_list<std::pair<service_t, instance_t>> services_to_release_;
    {
        std::lock_guard<std::mutex> its_lock(requested_services_mutex_);
        auto its_client = requested_services_.find(_client);
        if (its_client != requested_services_.end()) {
            for (auto its_service : its_client->second) {
                for (auto its_instance : its_service.second) {
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

instance_t routing_manager_impl::find_instance(service_t _service,
        endpoint * _endpoint) {
    instance_t its_instance(0xFFFF);
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        auto found_service = service_instances_.find(_service);
        if (found_service != service_instances_.end()) {
            auto found_endpoint = found_service->second.find(_endpoint);
            if (found_endpoint != found_service->second.end()) {
                its_instance = found_endpoint->second;
            }
        }
    }
    return (its_instance);
}

std::shared_ptr<endpoint> routing_manager_impl::create_remote_client(
        service_t _service, instance_t _instance, bool _reliable, client_t _client) {
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
        if (configuration_->get_client_port(_service, _instance, its_remote_port, _reliable,
                used_client_ports_, its_local_port)) {
            if(its_endpoint_def) {
                its_endpoint = create_client_endpoint(
                        its_endpoint_def->get_address(),
                        its_local_port,
                        its_endpoint_def->get_port(),
                        _reliable, _client
                );
            }

            if (its_endpoint) {
                used_client_ports_[_reliable].insert(its_local_port);
                service_instances_[_service][its_endpoint.get()] = _instance;
                remote_services_[_service][_instance][_client][_reliable] = its_endpoint;
                if (_client == VSOMEIP_ROUTING_CLIENT) {
                    client_endpoints_by_ip_[its_endpoint_def->get_address()]
                                           [its_endpoint_def->get_port()]
                                           [_reliable] = its_endpoint;
                    // Set the basic route to the service in the service info
                    auto found_service_info = find_service(_service, _instance);
                    if (found_service_info) {
                        found_service_info->set_endpoint(its_endpoint, _reliable);
                    }
                }
            }
        }
    }
    return its_endpoint;
}


std::shared_ptr<endpoint> routing_manager_impl::find_remote_client(
        service_t _service, instance_t _instance, bool _reliable, client_t _client) {
    std::shared_ptr<endpoint> its_endpoint;
    auto found_service = remote_services_.find(_service);
    if (found_service != remote_services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_client = found_instance->second.find(_client);
            if (found_client != found_instance->second.end()) {
                auto found_reliability = found_client->second.find(_reliable);
                if (found_reliability != found_client->second.end()) {
                    its_endpoint = found_reliability->second;
                }
            }
        }
    }
    if (its_endpoint || _client != VSOMEIP_ROUTING_CLIENT) {
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
                            remote_services_[_service][_instance][_client][_reliable] =
                                    its_endpoint;
                            service_instances_[_service][its_endpoint.get()] = _instance;
                        }
                    }
                }
            }
        }
    }
    return its_endpoint;
}

client_t routing_manager_impl::find_client(
        service_t _service, instance_t _instance,
        const std::shared_ptr<eventgroupinfo> &_eventgroup,
        const std::shared_ptr<endpoint_definition> &_target) const {
    client_t its_client = VSOMEIP_ROUTING_CLIENT;
    if (!_eventgroup->is_multicast())  {
         if (!_target->is_reliable()) {
             uint16_t unreliable_port = configuration_->get_unreliable_port(_service, _instance);
             auto endpoint = find_server_endpoint(unreliable_port, false);
             if (endpoint) {
                 its_client = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)->
                         get_client(_target);
             }
         } else {
             uint16_t reliable_port = configuration_->get_reliable_port(_service, _instance);
             auto endpoint = find_server_endpoint(reliable_port, true);
             if (endpoint) {
                 its_client = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(endpoint)->
                                 get_client(_target);
             }
         }
    }
    return its_client;
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

    {
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
                            is_reliable_known = true;
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
                            is_unreliable_known = true;
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

    bool udp_inserted(false);

    // Add endpoint(s) if necessary
    if (_reliable_port != ILLEGAL_PORT && !is_reliable_known) {
        std::shared_ptr<endpoint_definition> endpoint_def_tcp
            = endpoint_definition::get(_reliable_address, _reliable_port, true, _service, _instance);
        if (_unreliable_port != ILLEGAL_PORT && !is_unreliable_known) {
            std::shared_ptr<endpoint_definition> endpoint_def_udp
                = endpoint_definition::get(_unreliable_address, _unreliable_port, false, _service, _instance);
            {
                std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
                remote_service_info_[_service][_instance][false] = endpoint_def_udp;
                remote_service_info_[_service][_instance][true] = endpoint_def_tcp;
            }
            udp_inserted = true;
        } else {
            std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
            remote_service_info_[_service][_instance][true] = endpoint_def_tcp;
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
                                    find_or_create_remote_client(_service, _instance,
                                            true, VSOMEIP_ROUTING_CLIENT);
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
        auto specific_endpoint_clients = get_specific_endpoint_clients(_service, _instance);
        for (const client_t& c : specific_endpoint_clients) {
            find_or_create_remote_client(_service, _instance, true, c);
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
                            on_availability(_service, _instance,
                                    true, its_info->get_major(), its_info->get_minor());
                            if (!stub_->contained_in_routing_info(
                                    VSOMEIP_ROUTING_CLIENT, _service, _instance,
                                    its_info->get_major(),
                                    its_info->get_minor())) {
                                stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT,
                                        _service, _instance,
                                        its_info->get_major(),
                                        its_info->get_minor());
                                if (discovery_) {
                                    discovery_->on_endpoint_connected(
                                            _service, _instance,
                                            its_info->get_endpoint(true));
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
            {
                std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
                remote_service_info_[_service][_instance][false] = endpoint_def;
            }
            // check if service was requested and increase requester count if necessary
            {
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
                                    its_info->add_client(client_id.first);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (!is_reliable_known) {
            on_availability(_service, _instance, true, _major, _minor);
            stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, _major, _minor);
        }
        if (discovery_) {
            discovery_->on_endpoint_connected(
                    _service, _instance,
                    its_info->get_endpoint(false));
        }
    }
}

void routing_manager_impl::del_routing_info(service_t _service, instance_t _instance,
        bool _has_reliable, bool _has_unreliable) {

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if(!its_info)
        return;

    on_availability(_service, _instance, false, its_info->get_major(), its_info->get_minor());
    stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, its_info->get_major(), its_info->get_minor());
    // Implicit unsubscribe
    {
        std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
        auto found_service = eventgroups_.find(_service);
        if (found_service != eventgroups_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                for (auto &its_eventgroup : found_instance->second) {
                    its_eventgroup.second->clear_targets();
                    its_eventgroup.second->clear_pending_subscriptions();
                }
            }
        }
    }

    clear_identified_clients( _service, _instance);
    clear_identifying_clients( _service, _instance);

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
        clear_client_endpoints(_service, _instance, true);
        clear_remote_service_info(_service, _instance, true);
    }
    if (_has_unreliable) {
        clear_client_endpoints(_service, _instance, false);
        clear_remote_service_info(_service, _instance, false);
    }

    clear_multicast_endpoints(_service, _instance);

    if (_has_reliable)
        clear_service_info(_service, _instance, true);
    if (_has_unreliable)
        clear_service_info(_service, _instance, false);

    // For expired services using only unreliable endpoints that have never been created before
    if (!_has_reliable && !_has_unreliable) {
        clear_remote_service_info(_service, _instance, true);
        clear_remote_service_info(_service, _instance, false);
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

void routing_manager_impl::expire_services(const boost::asio::ip::address &_address) {
    std::map<service_t, std::vector<instance_t> > its_expired_offers;

    for (auto &s : get_services()) {
        for (auto &i : s.second) {
            if (find_local_client(s.first, i.first) != VSOMEIP_ROUTING_CLIENT) {
                continue; //don't expire local services
            }
            bool is_gone(false);
            boost::asio::ip::address its_address;
            std::shared_ptr<client_endpoint> its_client_endpoint =
                    std::dynamic_pointer_cast<client_endpoint>(
                            i.second->get_endpoint(true));
            if (its_client_endpoint) {
                if (its_client_endpoint->get_remote_address(its_address)) {
                    is_gone = (its_address == _address);
                }
            } else {
                its_client_endpoint =
                        std::dynamic_pointer_cast<client_endpoint>(
                                i.second->get_endpoint(false));
                if (its_client_endpoint) {
                    if (its_client_endpoint->get_remote_address(its_address)) {
                        is_gone = (its_address == _address);
                    }
                }
            }

            if (is_gone) {
                if (discovery_)
                    discovery_->unsubscribe_all(s.first, i.first);
                its_expired_offers[s.first].push_back(i.first);
            }
        }
    }

    for (auto &s : its_expired_offers) {
        for (auto &i : s.second) {
            VSOMEIP_INFO << "expire_services for address: " << _address.to_string()
                    << " : delete service/instance "
                    << std::hex << std::setw(4) << std::setfill('0') << s.first
                    << "." << std::hex << std::setw(4) << std::setfill('0') << i;
            del_routing_info(s.first, i, true, true);
        }
    }
}

void routing_manager_impl::expire_subscriptions(const boost::asio::ip::address &_address) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (auto &its_service : eventgroups_) {
        for (auto &its_instance : its_service.second) {
            const client_t its_hosting_client = find_local_client(
                    its_service.first, its_instance.first);
            const bool service_offered_by_host = (its_hosting_client
                    == host_->get_client());
            auto target = find_local(its_hosting_client);

            for (auto &its_eventgroup : its_instance.second) {
                std::set<std::shared_ptr<endpoint_definition>> its_invalid_endpoints;
                for (auto &its_target : its_eventgroup.second->get_targets()) {
                    if (its_target.endpoint_->get_address() == _address)
                        its_invalid_endpoints.insert(its_target.endpoint_);
                }

                for (auto &its_endpoint : its_invalid_endpoints) {
                    its_eventgroup.second->remove_target(its_endpoint);
                    client_t its_client = find_client(its_service.first,
                            its_instance.first, its_eventgroup.second,
                            its_endpoint);
                    clear_remote_subscriber(its_service.first,
                            its_instance.first, its_client, its_endpoint);

                    if (target) {
                        stub_->send_unsubscribe(target, its_client, its_service.first,
                                its_instance.first, its_eventgroup.first, ANY_EVENT, true);
                    }
                    if (service_offered_by_host) {
                        host_->on_subscription(its_service.first,
                                its_instance.first, its_eventgroup.first,
                                its_client, false,
                                [](const bool _subscription_accepted){
                                    (void)_subscription_accepted;
                                });
                    }
                }
                if(its_eventgroup.second->is_multicast() && its_invalid_endpoints.size() &&
                        0 == its_eventgroup.second->get_unreliable_target_count() ) {
                    //clear multicast targets if no subscriber is left for multicast eventgroup
                    its_eventgroup.second->clear_multicast_targets();
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

        if (its_reliable_port != ILLEGAL_PORT
                || its_unreliable_port != ILLEGAL_PORT) {

            add_routing_info(i.first, i.second,
                    DEFAULT_MAJOR, DEFAULT_MINOR, DEFAULT_TTL,
                    its_address, its_reliable_port,
                    its_address, its_unreliable_port);

            if(its_reliable_port != ILLEGAL_PORT) {
                find_or_create_remote_client(i.first, i.second, true, VSOMEIP_ROUTING_CLIENT);
            }
            if(its_unreliable_port != ILLEGAL_PORT) {
                find_or_create_remote_client(i.first, i.second, false, VSOMEIP_ROUTING_CLIENT);
            }
        }
    }
}

remote_subscription_state_e routing_manager_impl::on_remote_subscription(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        const std::shared_ptr<endpoint_definition> &_subscriber,
        const std::shared_ptr<endpoint_definition> &_target,
        ttl_t _ttl, client_t *_client,
        const std::shared_ptr<sd_message_identifier_t> &_sd_message_id) {
    std::shared_ptr<eventgroupinfo> its_eventgroup
        = find_eventgroup(_service, _instance, _eventgroup);
    if (!its_eventgroup) {
        VSOMEIP_ERROR << "REMOTE SUBSCRIBE: attempt to subscribe to unknown eventgroup ["
            << std::hex << std::setw(4) << std::setfill('0') << _service << "."
            << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
            << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
            << " from " << _subscriber->get_address().to_string()
            << ":" << std::dec << _subscriber->get_port()
            << (_subscriber->is_reliable() ? " reliable" : " unreliable");
        return remote_subscription_state_e::SUBSCRIPTION_ERROR;
    }
    // find out client id for selective subscriber
    if (!_subscriber->is_reliable()) {
        uint16_t unreliable_port = configuration_->get_unreliable_port(_service, _instance);
        _subscriber->set_remote_port(unreliable_port);
        if (!its_eventgroup->is_multicast()) {
            auto endpoint = find_server_endpoint(unreliable_port, false);
            if (endpoint) {
                *_client = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)->
                        get_client(_subscriber);
            }
        }
    } else {
        uint16_t reliable_port = configuration_->get_reliable_port(_service, _instance);
        _subscriber->set_remote_port(reliable_port);
        auto endpoint = find_server_endpoint(reliable_port, true);
        if (endpoint) {
            *_client = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(endpoint)->
                    get_client(_subscriber);
        }
    }
    const std::chrono::steady_clock::time_point its_expiration =
            std::chrono::steady_clock::now() + std::chrono::seconds(_ttl);
    if (its_eventgroup->update_target(_subscriber, its_expiration)) {
        return remote_subscription_state_e::SUBSCRIPTION_ACKED;
    } else {
        const pending_subscription_id_t its_subscription_id =
                its_eventgroup->add_pending_subscription(
                        pending_subscription_t(_sd_message_id,
                                _subscriber, _target, _ttl));
        if (host_->get_client() == find_local_client(_service, _instance)) {
            auto self = shared_from_this();
            const client_t its_client = *_client;
            host_->on_subscription(_service, _instance, _eventgroup, *_client, true,
                [this, self, _service, _instance,
                    _eventgroup, its_client, its_subscription_id]
                        (const bool _subscription_accepted) {
                try {
                    if (!_subscription_accepted) {
                        const auto its_callback = std::bind(
                                &routing_manager_stub_host::on_subscribe_nack,
                                std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                                its_client, _service, _instance,
                                _eventgroup, ANY_EVENT, its_subscription_id);
                        io_.post(its_callback);
                    } else {
                        const auto its_callback = std::bind(
                                &routing_manager_stub_host::on_subscribe_ack,
                                std::dynamic_pointer_cast<routing_manager_stub_host>(shared_from_this()),
                                its_client, _service, _instance,
                                _eventgroup, ANY_EVENT, its_subscription_id);
                        io_.post(its_callback);
                    }
                } catch (const std::exception &e) {
                    VSOMEIP_ERROR << __func__ << e.what();
                }
            });
            return remote_subscription_state_e::SUBSCRIPTION_PENDING;
        } else { // service hosted by local client
            stub_->send_subscribe(find_local(_service, _instance), *_client,
                    _service, _instance, _eventgroup,
                    its_eventgroup->get_major(), ANY_EVENT, its_subscription_id);
            return remote_subscription_state_e::SUBSCRIPTION_PENDING;
        }
    }
    return remote_subscription_state_e::SUBSCRIPTION_ERROR;
}

void routing_manager_impl::on_unsubscribe(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<endpoint_definition> _target) {
    std::shared_ptr<eventgroupinfo> its_eventgroup = find_eventgroup(_service,
            _instance, _eventgroup);
    if (its_eventgroup) {
        client_t its_client = find_client(_service, _instance, its_eventgroup, _target);

        its_eventgroup->remove_target(_target);
        clear_remote_subscriber(_service, _instance, its_client, _target);

        stub_->send_unsubscribe(find_local(_service, _instance),
                its_client, _service, _instance, _eventgroup, ANY_EVENT, true);

        host_->on_subscription(_service, _instance, _eventgroup, its_client, false, [](const bool _subscription_accepted){ (void)_subscription_accepted; });

        if (its_eventgroup->get_targets().size() == 0) {
            std::set<std::shared_ptr<event> > its_events
                = its_eventgroup->get_events();
            for (auto e : its_events) {
                if (e->is_shadow()) {
                    e->unset_payload();
                }
            }
        }
        VSOMEIP_INFO << "REMOTE UNSUBSCRIBE("
            << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
            << std::hex << std::setw(4) << std::setfill('0') << _service << "."
            << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
            << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
            << " from " << _target->get_address().to_string()
            << ":" << std::dec <<_target->get_port()
            << (_target->is_reliable() ? " reliable" : " unreliable");

    } else {
        VSOMEIP_ERROR << "REMOTE UNSUBSCRIBE: attempt to subscribe to unknown eventgroup ["
            << std::hex << std::setw(4) << std::setfill('0') << _service << "."
            << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
            << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
            << " from " << _target->get_address().to_string()
            << ":" << std::dec <<_target->get_port()
            << (_target->is_reliable() ? " reliable" : " unreliable");
    }
}

void routing_manager_impl::on_subscribe_ack(service_t _service,
        instance_t _instance, const boost::asio::ip::address &_address,
        uint16_t _port) {

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
        if (!multicast_known) {
            // Save multicast info to be able to delete the endpoint
            // as soon as the instance stops offering its service
            std::shared_ptr<endpoint_definition> endpoint_def =
                    endpoint_definition::get(_address, _port, false, _service, _instance);
            multicast_info[_service][_instance] = endpoint_def;
        }
    }
    const bool is_someip = configuration_->is_someip(_service, _instance);

    // Create multicast endpoint & join multicase group
    std::shared_ptr<endpoint> its_endpoint
        = find_or_create_server_endpoint(_port, false, is_someip);
    if (its_endpoint) {
        if (!multicast_known) {
            std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
            service_instances_[_service][its_endpoint.get()] = _instance;
        }
        its_endpoint->join(_address.to_string());
    } else {
        VSOMEIP_ERROR<<"Could not find/create multicast endpoint!";
    }
}

void routing_manager_impl::on_subscribe_ack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        event_t _event, pending_subscription_id_t _subscription_id) {
    client_t its_client = is_specific_endpoint_client(_client, _service, _instance);
    bool specific_endpoint_client = its_client != VSOMEIP_ROUTING_CLIENT;
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        std::unique_lock<std::mutex> eventgroup_lock(its_eventgroup->get_subscription_lock());
        if (_subscription_id == DEFAULT_SUBSCRIPTION) {
            // ACK coming in via SD from remote or as answer to a subscription
            // of the application hosting the rm_impl to a local service
            auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, its_client);
            std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
            auto its_state = remote_subscription_state_.find(its_tuple);
            if (its_state != remote_subscription_state_.end()) {
                if (its_state->second == subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED) {
                    // Already notified!
                    return;
                }
            }
            remote_subscription_state_[its_tuple] = subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED;
        } else { // ACK sent back from local client as answer to a remote subscription
            if (find_local_client(_service, _instance) == VSOMEIP_ROUTING_CLIENT) {
                // service was stopped while subscription was pending
                // send subscribe_nack back instead
                eventgroup_lock.unlock();
                on_subscribe_nack(_client, _service, _instance, _eventgroup,
                        _event, _subscription_id);
                return;
            }
            if (discovery_) {
                pending_subscription_t its_sd_message_id =
                        its_eventgroup->remove_pending_subscription(
                                _subscription_id);
                if (its_sd_message_id.sd_message_identifier_
                        && its_sd_message_id.subscriber_
                        && its_sd_message_id.target_) {
                    {
                        std::lock_guard<std::mutex> its_lock(remote_subscribers_mutex_);
                        remote_subscribers_[_service][_instance][_client].insert(its_sd_message_id.subscriber_);
                    }
                    const std::chrono::steady_clock::time_point its_expiration =
                            std::chrono::steady_clock::now()
                                    + std::chrono::seconds(
                                            its_sd_message_id.ttl_);
                    // IP address of target is a multicast address if the event is in a multicast eventgroup
                    if (its_eventgroup->is_multicast()
                            && !its_sd_message_id.subscriber_->is_reliable()) {
                        // Event is in multicast eventgroup and subscribe for UDP
                        its_eventgroup->add_target(
                            { its_sd_message_id.target_, its_expiration },
                            { its_sd_message_id.subscriber_, its_expiration });
                    } else {
                        // subscribe for TCP or UDP
                        its_eventgroup->add_target(
                            { its_sd_message_id.subscriber_, its_expiration });
                    }
                    discovery_->remote_subscription_acknowledge(_service,
                            _instance, _eventgroup, _client, true,
                            its_sd_message_id.sd_message_identifier_);

                    VSOMEIP_INFO << "REMOTE SUBSCRIBE("
                        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
                        << " from " << its_sd_message_id.subscriber_->get_address().to_string()
                        << ":" << std::dec << its_sd_message_id.subscriber_->get_port()
                        << (its_sd_message_id.subscriber_->is_reliable() ? " reliable" : " unreliable")
                        << " was accepted";
                }
            }
            return;
        }

        if (specific_endpoint_client) {
            if (_client == get_client()) {
                host_->on_subscription_error(_service, _instance, _eventgroup, 0x0 /*OK*/);
                host_->on_subscription_status(_service, _instance, _eventgroup, _event, 0x0 /*OK*/);
            } else {
                stub_->send_subscribe_ack(_client, _service, _instance, _eventgroup,
                        _event);
            }
        } else {
            std::set<client_t> subscribed_clients;
            for (auto its_event : its_eventgroup->get_events()) {
                for (auto its_client : its_event->get_subscribers()) {
                    subscribed_clients.insert(its_client);
                }
            }
            for (auto its_subscriber : subscribed_clients) {
                if (its_subscriber == get_client()) {
                    host_->on_subscription_error(_service, _instance,
                            _eventgroup, 0x0 /*OK*/);
                    for (auto its_event : its_eventgroup->get_events()) {
                        host_->on_subscription_status(_service, _instance,
                                _eventgroup, its_event->get_event(),
                                0x0 /*OK*/);
                    }
                } else {
                    stub_->send_subscribe_ack(its_subscriber, _service,
                            _instance, _eventgroup, _event);
                }
            }
        }
    }
}

void routing_manager_impl::on_subscribe_nack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        event_t _event, pending_subscription_id_t _subscription_id) {
    client_t its_client = is_specific_endpoint_client(_client, _service, _instance);
    bool specific_endpoint_client = its_client != VSOMEIP_ROUTING_CLIENT;
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        std::unique_lock<std::mutex> eventgroup_lock(its_eventgroup->get_subscription_lock());
        if (_subscription_id == DEFAULT_SUBSCRIPTION) {
            // NACK coming in via SD from remote or as answer to a subscription
            // of the application hosting the rm_impl to a local service
            auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, its_client);
            std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
            auto its_state = remote_subscription_state_.find(its_tuple);
            if (its_state != remote_subscription_state_.end()) {
                if (its_state->second == subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED) {
                    // Already notified!
                    return;
                }
            }
            remote_subscription_state_[its_tuple] = subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED;
        } else { // NACK sent back from local client as answer to a remote subscription
            if (discovery_) {
                pending_subscription_t its_sd_message_id =
                        its_eventgroup->remove_pending_subscription(_subscription_id);
                if (its_sd_message_id.sd_message_identifier_ && its_sd_message_id.subscriber_) {
                    discovery_->remote_subscription_acknowledge(_service, _instance,
                            _eventgroup, _client, false, its_sd_message_id.sd_message_identifier_);
                    VSOMEIP_INFO << "REMOTE SUBSCRIBE("
                        << std::hex << std::setw(4) << std::setfill('0') << _client <<"): ["
                        << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "]"
                        << " from " << its_sd_message_id.subscriber_->get_address().to_string()
                        << ":" << std::dec <<its_sd_message_id.subscriber_->get_port()
                        << (its_sd_message_id.subscriber_->is_reliable() ? " reliable" : " unreliable")
                        << " was not accepted";
                }
            }
            return;
        }
        if (specific_endpoint_client) {
            if (_client == get_client()) {
                host_->on_subscription_error(_service, _instance, _eventgroup, 0x7 /*Rejected*/);
                host_->on_subscription_status(_service, _instance, _eventgroup, _event, 0x7 /*Rejected*/);
            } else {
                stub_->send_subscribe_nack(_client, _service, _instance, _eventgroup,
                        _event);
            }
        } else {
            std::set<client_t> subscribed_clients;
            for (auto its_event : its_eventgroup->get_events()) {
                for (auto its_client : its_event->get_subscribers()) {
                    subscribed_clients.insert(its_client);
                }
            }
            for (auto its_subscriber : subscribed_clients) {
                if (its_subscriber == get_client()) {
                    host_->on_subscription_error(_service, _instance,
                            _eventgroup, 0x7 /*Rejected*/);
                    for (auto its_event : its_eventgroup->get_events()) {
                        host_->on_subscription_status(_service, _instance,
                                _eventgroup, its_event->get_event(),
                                0x7 /*Rejected*/);
                    }
                } else {
                    stub_->send_subscribe_nack(its_subscriber, _service,
                            _instance, _eventgroup, _event);
                }
            }
        }
    }
}

bool routing_manager_impl::deliver_specific_endpoint_message(service_t _service,
        instance_t _instance, const byte_t *_data, length_t _size, endpoint *_receiver) {
    client_t its_client(0x0);

    // Try to deliver specific endpoint message (for selective subscribers)
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        auto found_servic = remote_services_.find(_service);
        if (found_servic != remote_services_.end()) {
            auto found_instance = found_servic->second.find(_instance);
            if (found_instance != found_servic->second.end()) {
                for (auto client_entry : found_instance->second) {
                    if (!client_entry.first) {
                        continue;
                    }
                    auto found_reliability = client_entry.second.find(_receiver->is_reliable());
                    if (found_reliability != client_entry.second.end()) {
                        auto found_enpoint = found_reliability->second;
                        if (found_enpoint.get() == _receiver) {
                            its_client = client_entry.first;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (its_client) {
        if (its_client != get_client()) {
            auto local_endpoint = find_local(its_client);
            if (local_endpoint) {
                send_local(local_endpoint, its_client, _data, _size, _instance, true,
                        _receiver->is_reliable(), VSOMEIP_SEND);
            }
        } else {
            deliver_message(_data, _size, _instance, _receiver->is_reliable());
        }
        return true;
    }

    return false;
}

void routing_manager_impl::clear_client_endpoints(service_t _service, instance_t _instance,
        bool _reliable) {
    auto its_specific_endpoint_clients = get_specific_endpoint_clients(_service, _instance);
    std::shared_ptr<endpoint> endpoint_to_delete;
    bool other_services_reachable_through_endpoint(false);
    std::vector<std::shared_ptr<endpoint>> its_specific_endpoints;
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        // Clear client endpoints for remote services (generic and specific ones)
        if (remote_services_.find(_service) != remote_services_.end()) {
            if (remote_services_[_service].find(_instance) != remote_services_[_service].end()) {
                auto endpoint = remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT][_reliable];
                if (endpoint) {
                    service_instances_[_service].erase(endpoint.get());
                    endpoint_to_delete = endpoint;
                }
                remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT].erase(_reliable);
                auto found_endpoint = remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT].find(
                        !_reliable);
                if (found_endpoint == remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT].end()) {
                    remote_services_[_service][_instance].erase(VSOMEIP_ROUTING_CLIENT);
                }
                // erase specific client endpoints
                for (const client_t &client : its_specific_endpoint_clients) {
                    auto endpoint = remote_services_[_service][_instance][client][_reliable];
                    if (endpoint) {
                        service_instances_[_service].erase(endpoint.get());
                        its_specific_endpoints.push_back(endpoint);
                    }
                    remote_services_[_service][_instance][client].erase(_reliable);
                    auto found_endpoint = remote_services_[_service][_instance][client].find(!_reliable);
                    if (found_endpoint == remote_services_[_service][_instance][client].end()) {
                        remote_services_[_service][_instance].erase(client);
                    }
                }
            }
        }
        if (remote_services_.find(_service) != remote_services_.end()) {
            if (remote_services_[_service].find(_instance) != remote_services_[_service].end()) {
                if (!remote_services_[_service][_instance].size()) {
                    remote_services_[_service].erase(_instance);
                    if (0 >= remote_services_[_service].size()) {
                        remote_services_.erase(_service);
                    }
                }
            }
        }

        if (!service_instances_[_service].size()) {
            service_instances_.erase(_service);
        }

        // Only stop and delete the endpoint if none of the services
        // reachable through it is online anymore.
        if (endpoint_to_delete) {
            for (const auto& service : remote_services_) {
                for (const auto& instance : service.second) {
                    const auto& client = instance.second.find(VSOMEIP_ROUTING_CLIENT);
                    if (client != instance.second.end()) {
                        for (const auto& reliable : client->second) {
                            if (reliable.second == endpoint_to_delete) {
                                other_services_reachable_through_endpoint = true;
                                break;
                            }
                        }
                    }
                    if (other_services_reachable_through_endpoint) { break; }
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
    for (const auto &specific_endpoint : its_specific_endpoints) {
        specific_endpoint->stop();
    }
}

void routing_manager_impl::clear_multicast_endpoints(service_t _service, instance_t _instance) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    // Clear multicast info and endpoint and multicast instance (remote service)
    if (multicast_info.find(_service) != multicast_info.end()) {
        if (multicast_info[_service].find(_instance) != multicast_info[_service].end()) {
            std::string address = multicast_info[_service][_instance]->get_address().to_string();
            uint16_t port = multicast_info[_service][_instance]->get_port();
            std::shared_ptr<endpoint> multicast_endpoint;
            auto found_port = server_endpoints_.find(port);
            if (found_port != server_endpoints_.end()) {
                auto found_unreliable = found_port->second.find(false);
                if (found_unreliable != found_port->second.end()) {
                    multicast_endpoint = found_unreliable->second;
                    multicast_endpoint->leave(address);
                    multicast_endpoint->stop();
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
            // Clear service_instances_ for multicase endpoint
            if (1 >= service_instances_[_service].size()) {
                service_instances_.erase(_service);
            } else if (multicast_endpoint) {
                service_instances_[_service].erase(multicast_endpoint.get());
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
        endpoint *_receiver,
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
        std::lock_guard<std::mutex> its_lock(serialize_mutex_);
        if (serializer_->serialize(error_message.get())) {
            if (_receiver) {
                auto its_endpoint_def = std::make_shared<endpoint_definition>(
                        _remote_address, _remote_port,
                        _receiver->is_reliable());
                its_endpoint_def->set_remote_port(_receiver->get_local_port());
                send_to(its_endpoint_def, serializer_->get_data(),
                        serializer_->get_size(), _instance, true);
            }
            serializer_->reset();
        } else {
            VSOMEIP_ERROR<< "Failed to serialize error message.";
        }
    }
}

void routing_manager_impl::on_identify_response(client_t _client, service_t _service,
        instance_t _instance, bool _reliable) {
    {
        std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
        auto its_service = identifying_clients_.find(_service);
        if (its_service != identifying_clients_.end()) {
            auto its_instance = its_service->second.find(_instance);
            if (its_instance != its_service->second.end()) {
                auto its_reliable = its_instance->second.find(_reliable);
                if (its_reliable != its_instance->second.end()) {
                    its_reliable->second.erase(_client);
                }
            }
        }
        identified_clients_[_service][_instance][_reliable].insert(_client);
    }
    discovery_->send_subscriptions(_service, _instance, _client, _reliable);
}

void routing_manager_impl::identify_for_subscribe(client_t _client,
        service_t _service, instance_t _instance, major_version_t _major,
        subscription_type_e _subscription_type) {
    if (_subscription_type == subscription_type_e::SU_RELIABLE_AND_UNRELIABLE
            || _subscription_type == subscription_type_e::SU_PREFER_UNRELIABLE
            || _subscription_type == subscription_type_e::SU_UNRELIABLE) {
        if (!has_identified(_client, _service, _instance, false)
                && !is_identifying(_client, _service, _instance, false)) {
            if (!send_identify_message(_client, _service, _instance, _major,
                    false) && _subscription_type
                                == subscription_type_e::SU_PREFER_UNRELIABLE) {
                send_identify_message(_client, _service, _instance, _major,
                        true);
            }
        }
    }

    if (_subscription_type == subscription_type_e::SU_RELIABLE_AND_UNRELIABLE
            || _subscription_type == subscription_type_e::SU_PREFER_RELIABLE
            || _subscription_type == subscription_type_e::SU_RELIABLE) {
        if (!has_identified(_client, _service, _instance, true)
                && !is_identifying(_client, _service, _instance, true)) {
            if (!send_identify_message(_client, _service, _instance, _major,
                    true) && _subscription_type
                    == subscription_type_e::SU_PREFER_RELIABLE) {
                send_identify_message(_client, _service, _instance, _major,
                        false);
            }
        }
    }
}

bool routing_manager_impl::send_identify_message(client_t _client,
                                                 service_t _service,
                                                 instance_t _instance,
                                                 major_version_t _major,
                                                 bool _reliable) {
    auto its_endpoint = find_or_create_remote_client(_service, _instance,
            _reliable, _client);
    if (!its_endpoint) {
        VSOMEIP_WARNING << "routing_manager_impl::send_identify_message: "
                << "No " << (_reliable ? "reliable" : "unreliable")
                << " route for identify message to service/instance "
                << std::hex << _service << "/" << _instance << " for client "
                << _client;
        return false;
    }
    {
        std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
        identifying_clients_[_service][_instance][_reliable].insert(_client);
    }

    if (_client == get_client()) {
        send_identify_request(_service, _instance, _major, _reliable);
    } else {
        stub_->send_identify_request_command(find_local(_client),
                _service, _instance, _major, _reliable);
    }

    return true;
}


bool routing_manager_impl::supports_selective(service_t _service, instance_t _instance) {
    bool supports_selective(false);
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    auto its_service = remote_service_info_.find(_service);
    if (its_service != remote_service_info_.end()) {
        auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            for (auto its_reliable : its_instance->second) {
                supports_selective |= configuration_->
                        supports_selective_broadcasts(
                                its_reliable.second->get_address());
            }
        }
    }
    return supports_selective;
}

bool routing_manager_impl::is_identifying(client_t _client, service_t _service,
            instance_t _instance, bool _reliable) {
    if (!supports_selective(_service, _instance)) {
        // For legacy selective services clients can't be identified!
        return false;
    }
    bool is_identifieing(false);
    std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
    auto its_service = identifying_clients_.find(_service);
    if (its_service != identifying_clients_.end()) {
        auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            auto its_reliable = its_instance->second.find(_reliable);
            if (its_reliable != its_instance->second.end()) {
                auto its_client = its_reliable->second.find(_client);
                if (its_client != its_reliable->second.end()) {
                    is_identifieing = true;
                }
            }
        }
    }
    return is_identifieing;
}

bool routing_manager_impl::has_identified(client_t _client, service_t _service,
            instance_t _instance, bool _reliable) {
    if (!supports_selective(_service, _instance)) {
        // For legacy selective services clients can't be identified!
        return true;
    }
    bool has_identified(false);
    std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
    auto its_service = identified_clients_.find(_service);
    if (its_service != identified_clients_.end()) {
        auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            auto its_reliable = its_instance->second.find(_reliable);
            if (its_reliable != its_instance->second.end()) {
                auto its_client = its_reliable->second.find(_client);
                if (its_client != its_reliable->second.end()) {
                    has_identified = true;
                }
            }
        }
    }
    return has_identified;
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
routing_manager_impl::expire_subscriptions() {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    std::chrono::steady_clock::time_point now
        = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point next_expiration
        = std::chrono::steady_clock::now() + std::chrono::hours(24);

    for (auto &its_service : eventgroups_) {
        for (auto &its_instance : its_service.second) {
            const client_t its_hosting_client = find_local_client(
                    its_service.first, its_instance.first);
            for (auto &its_eventgroup : its_instance.second) {
                std::set<std::shared_ptr<endpoint_definition>> its_expired_endpoints;
                for (auto &its_target : its_eventgroup.second->get_targets()) {
                    if (its_target.expiration_ < now) {
                        its_expired_endpoints.insert(its_target.endpoint_);
                    } else if (its_target.expiration_ < next_expiration) {
                        next_expiration = its_target.expiration_;
                    }
                }
                std::shared_ptr<endpoint> target;
                bool service_offered_by_host(false);
                if (its_expired_endpoints.size()) {
                    target = find_local(its_hosting_client);
                    service_offered_by_host = (its_hosting_client == host_->get_client());
                }

                for (auto its_endpoint : its_expired_endpoints) {
                    its_eventgroup.second->remove_target(its_endpoint);

                    client_t its_client
                        = find_client(its_service.first, its_instance.first,
                                      its_eventgroup.second, its_endpoint);
                    clear_remote_subscriber(its_service.first, its_instance.first,
                            its_client, its_endpoint);

                    if (target) {
                        stub_->send_unsubscribe(target, its_client, its_service.first,
                                its_instance.first, its_eventgroup.first, ANY_EVENT, true);
                    }
                    if (service_offered_by_host) {
                        host_->on_subscription(its_service.first,
                                its_instance.first, its_eventgroup.first,
                                its_client, false,
                                [](const bool _subscription_accepted){
                                    (void)_subscription_accepted;
                                });
                    }

                    VSOMEIP_INFO << "Expired subscription ("
                            << std::hex << its_service.first << "."
                            << its_instance .first << "."
                            << its_eventgroup.first << " from "
                            << its_endpoint->get_address() << ":"
                            << std::dec << its_endpoint->get_port()
                            << "(" << std::hex << its_client << ")";
                }
                if(its_eventgroup.second->is_multicast() && its_expired_endpoints.size() &&
                        0 == its_eventgroup.second->get_unreliable_target_count() ) {
                    //clear multicast targets if no unreliable subscriber is left for multicast eventgroup
                    its_eventgroup.second->clear_multicast_targets();
                }
            }
        }
    }

    return next_expiration;
}

void routing_manager_impl::log_version_timer_cbk(boost::system::error_code const & _error) {
    if (!_error) {

#ifndef VSOMEIP_VERSION
#define VSOMEIP_VERSION "unknown version"
#endif

        VSOMEIP_INFO << "vSomeIP " << VSOMEIP_VERSION;
        {
            std::lock_guard<std::mutex> its_lock(version_log_timer_mutex_);
            version_log_timer_.expires_from_now(
                    std::chrono::seconds(configuration_->get_log_version_interval()));
            version_log_timer_.async_wait(std::bind(&routing_manager_impl::log_version_timer_cbk,
                            this, std::placeholders::_1));
        }
    }
}

#ifndef WITHOUT_SYSTEMD
void routing_manager_impl::watchdog_cbk(boost::system::error_code const &_error) {
    if (!_error) {
        static bool is_ready(false);
        static bool has_interval(false);
        static uint64_t its_interval(0);

        if (is_ready) {
            sd_notify(0, "WATCHDOG=1");
            VSOMEIP_INFO << "Triggered systemd watchdog";
        } else {
            is_ready = true;
            sd_notify(0, "READY=1");
            VSOMEIP_INFO << "Sent READY to systemd watchdog";
            if (0 < sd_watchdog_enabled(0, &its_interval)) {
                has_interval = true;
                VSOMEIP_INFO << "systemd watchdog is enabled";
            }
        }

        if (has_interval) {
            std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
            watchdog_timer_.expires_from_now(std::chrono::microseconds(its_interval / 2));
            watchdog_timer_.async_wait(std::bind(&routing_manager_impl::watchdog_cbk,
                    this, std::placeholders::_1));
        }
    }
}
#endif

void routing_manager_impl::clear_remote_service_info(service_t _service, instance_t _instance, bool _reliable) {
    // Clear remote_service_info_
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    if (remote_service_info_.find(_service) != remote_service_info_.end()) {
        if (remote_service_info_[_service].find(_instance) != remote_service_info_[_service].end()) {
            remote_service_info_[_service][_instance].erase(_reliable);
            auto found_endpoint_def = remote_service_info_[_service][_instance].find(!_reliable);
            if (found_endpoint_def == remote_service_info_[_service][_instance].end()) {
                remote_service_info_[_service].erase(_instance);
                if (0 >= remote_service_info_[_service].size()) {
                    remote_service_info_.erase(_service);
                }
            }
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
                std::get<3>(offer), std::get<4>(offer));
    }
}

void routing_manager_impl::remove_specific_client_endpoint(client_t _client, service_t _service,
        instance_t _instance, bool _reliable) {
    client_t its_client = is_specific_endpoint_client(_client, _service, _instance);
    if (its_client != VSOMEIP_ROUTING_CLIENT) {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        if (remote_services_.find(_service) != remote_services_.end()) {
            if (remote_services_[_service].find(_instance) != remote_services_[_service].end()) {
                auto endpoint = remote_services_[_service][_instance][_client][_reliable];
                if (endpoint) {
                    service_instances_[_service].erase(endpoint.get());
                    endpoint->stop();
                }
                remote_services_[_service][_instance][_client].erase(_reliable);
                auto found_endpoint = remote_services_[_service][_instance][_client].find(!_reliable);
                if (found_endpoint == remote_services_[_service][_instance][_client].end()) {
                    remote_services_[_service][_instance].erase(_client);
                }
            }
        }
    }
}

void routing_manager_impl::clear_identified_clients( service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
    auto its_service = identified_clients_.find(_service);
    if (its_service != identified_clients_.end()) {
        auto found_instance = its_service->second.find(_instance);
        if (found_instance != its_service->second.end()) {
            auto found_reliable = found_instance->second.find(true);
            if (found_reliable != found_instance->second.end()) {
                found_reliable->second.clear();
            }
            auto found_unreliable = found_instance->second.find(false);
            if (found_unreliable != found_instance->second.end()) {
                found_unreliable->second.clear();
            }
        }
    }
}

void routing_manager_impl::clear_identifying_clients( service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
    auto its_service = identifying_clients_.find(_service);
    if (its_service != identifying_clients_.end()) {
        auto found_instance = its_service->second.find(_instance);
        if (found_instance != its_service->second.end()) {
            auto found_reliable = found_instance->second.find(true);
            if (found_reliable != found_instance->second.end()) {
                found_reliable->second.clear();
            }
            auto found_unreliable = found_instance->second.find(false);
            if (found_unreliable != found_instance->second.end()) {
                found_unreliable->second.clear();
            }
        }
    }
}

void routing_manager_impl::remove_identified_client(service_t _service, instance_t _instance, client_t _client) {
    std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
    auto its_service = identified_clients_.find(_service);
    if (its_service != identified_clients_.end()) {
        auto found_instance = its_service->second.find(_instance);
        if (found_instance != its_service->second.end()) {
            auto found_reliable = found_instance->second.find(true);
            if (found_reliable != found_instance->second.end()) {
                auto found_client = found_reliable->second.find(_client);
                if(found_client != found_reliable->second.end())
                    found_reliable->second.erase(_client);
            }
            auto found_unreliable = found_instance->second.find(false);
            if (found_unreliable != found_instance->second.end()) {
                auto found_client = found_unreliable->second.find(_client);
                if(found_client != found_unreliable->second.end())
                    found_unreliable->second.erase(_client);
            }
        }
    }
}

void routing_manager_impl::remove_identifying_client(service_t _service, instance_t _instance, client_t _client) {
    std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
    auto its_service = identifying_clients_.find(_service);
    if (its_service != identifying_clients_.end()) {
        auto found_instance = its_service->second.find(_instance);
        if (found_instance != its_service->second.end()) {
            auto found_reliable = found_instance->second.find(true);
            if (found_reliable != found_instance->second.end()) {
                auto found_client = found_reliable->second.find(_client);
                if(found_client != found_reliable->second.end())
                    found_reliable->second.erase(_client);
            }
            auto found_unreliable = found_instance->second.find(false);
            if (found_unreliable != found_instance->second.end()) {
                auto found_client = found_unreliable->second.find(_client);
                if(found_client != found_unreliable->second.end())
                    found_unreliable->second.erase(_client);
            }
        }
    }
}

void routing_manager_impl::unsubscribe_specific_client_at_sd(
        service_t _service, instance_t _instance, client_t _client) {
    client_t subscriber = is_specific_endpoint_client(_client, _service, _instance);
    if (subscriber != VSOMEIP_ROUTING_CLIENT && discovery_) {
        discovery_->unsubscribe_client(_service, _instance, _client);
    }
}

void routing_manager_impl::send_subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        event_t _event, subscription_type_e _subscription_type) {
    (void)_subscription_type;
    auto endpoint = find_local(_service, _instance);
    if (endpoint) {
        stub_->send_subscribe(endpoint, _client,
                _service, _instance, _eventgroup, _major, _event, DEFAULT_SUBSCRIPTION);
    }
}

void routing_manager_impl::set_routing_state(routing_state_e _routing_state) {
    if(discovery_) {
        switch (_routing_state) {
            case vsomeip::routing_state_e::RS_SUSPENDED:
            {
                VSOMEIP_INFO << "set routing to suspend mode";
                // stop processing of incoming SD messages
                discovery_->stop();

                // send StopOffer messages for remotely offered services on this node
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        its_instance.second->set_ttl(0);
                        discovery_->stop_offer_service(its_service.first, its_instance.first, its_instance.second);
                    }
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
                            auto specific_endpoint_clients = get_specific_endpoint_clients(s.first, i.first);
                            for (auto its_client : specific_endpoint_clients) {
                                discovery_->unsubscribe(s.first, i.first, its_eventgroup, its_client);
                            }
                            for (const auto &e : find_events(s.first, i.first, its_eventgroup)) {
                                e->clear_subscribers();
                            }
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
            case vsomeip::routing_state_e::RS_RESUMED:
            {
                VSOMEIP_INFO << "set routing to resume mode";

                // Reset relevant in service info
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        its_instance.second->set_ttl(DEFAULT_TTL);
                        its_instance.second->set_is_in_mainphase(false);
                    }
                }
                // start processing of SD messages (incoming remote offers should lead to new subscribe messages)
                discovery_->start();

                // Trigger initial offer phase for relevant services
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        discovery_->offer_service(its_service.first,
                                its_instance.first, its_instance.second);
                    }
                }
                break;
            }
            case routing_state_e::RS_DIAGNOSIS:
            {
                VSOMEIP_INFO << "set routing to diagnosis mode";
                discovery_->set_diagnosis_mode(true);

                // send StopOffer messages for all someip protocal services
                for (const auto &its_service : get_offered_services()) {
                    for (const auto &its_instance : its_service.second) {
                        if (host_->get_configuration()->is_someip(
                                its_service.first, its_instance.first)) {
                            its_instance.second->set_ttl(0);
                            discovery_->stop_offer_service(
                                    its_service.first, its_instance.first, its_instance.second);
                        }
                    }
                }
                break;
            }
            case routing_state_e::RS_RUNNING:
                VSOMEIP_INFO << "set routing to running mode";

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
                            discovery_->offer_service(its_service.first,
                                    its_instance.first, its_instance.second);
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
}

void routing_manager_impl::on_net_if_state_changed(std::string _if, bool _available) {
    if (_available != if_state_running_) {
        if (_available) {
            VSOMEIP_INFO << "Network interface \"" << _if << "\" is up and running.";
            start_ip_routing();
#ifndef _WIN32
            if (netlink_connector_) {
                netlink_connector_->unregister_net_if_changes_handler();
            }
#endif
        }
    }
}

void routing_manager_impl::start_ip_routing() {
    std::lock_guard<std::mutex> its_lock(pending_sd_offers_mutex_);
    if_state_running_  = true;

    if (discovery_) {
        discovery_->start();
    } else {
        init_routing_info();
    }

    for (auto its_service : pending_sd_offers_) {
        init_service_info(its_service.first, its_service.second, true);
    }
    pending_sd_offers_.clear();

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
            for (auto its_group : found_instance->second) {
                for (auto its_event : its_group.second->get_events()) {
                    if (its_event->has_subscriber(its_group.first, ANY_CLIENT)) {
                        its_eventgroups.insert(its_group.first);
                    }
                }
            }
        }
    }

    return its_eventgroups;
}

void routing_manager_impl::call_sd_endpoint_connected(
        const boost::system::error_code& _error,
        service_t _service, instance_t _instance,
        std::shared_ptr<endpoint> _endpoint,
        std::shared_ptr<boost::asio::steady_timer> _timer) {
    (void)_timer;
    if (_error) {
        return;
    }
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
        register_event(host_->get_client(), _service, _instance, _event,
                its_eventgroups, true, std::chrono::milliseconds::zero(), false,
                nullptr, false, false, true);
    } else if (its_local_client != VSOMEIP_ROUTING_CLIENT) {
        // received subscription for event of a service instance hosted on
        // this node register with client id of local_client and set shadow to true
        register_event(its_local_client, _service, _instance, _event,
                its_eventgroups, true, std::chrono::milliseconds::zero(), false,
                nullptr, false, true, true);
    } else {
        // received subscription for event of a unknown or remote service instance
        std::shared_ptr<serviceinfo> its_info = find_service(_service,
                _instance);
        if (its_info && !its_info->is_local()) {
            // remote service, register shadow event with client ID of subscriber
            // which should have called register_event
            register_event(_client, _service, _instance, _event,
                    its_eventgroups, true, std::chrono::milliseconds::zero(),
                    false, nullptr, false, true, true);
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
        is_inserted = its_event->add_subscriber(_eventgroup, _client);
    }
    return is_inserted;
}

void routing_manager_impl::handle_subscription_state(client_t _client, service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, event_t _event) {

    client_t subscriber = is_specific_endpoint_client(_client, _service, _instance);
    auto its_tuple = std::make_tuple(_service, _instance, _eventgroup, subscriber);

    std::lock_guard<std::mutex> its_lock(remote_subscription_state_mutex_);
    auto its_state = remote_subscription_state_.find(its_tuple);
    if (its_state != remote_subscription_state_.end()) {
        if (its_state->second == subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED) {
            // Subscription already acknowledged!
            if (_client == get_client()) {
                host_->on_subscription_error(_service, _instance, _eventgroup, 0x0 /*OK*/);
                host_->on_subscription_status(_service, _instance, _eventgroup, _event, 0x0 /*OK*/);
            } else {
                stub_->send_subscribe_ack(_client, _service, _instance, _eventgroup, _event);
            }
        }
    } else {
        remote_subscription_state_[its_tuple] = subscription_state_e::IS_SUBSCRIBING;
    }
}

client_t routing_manager_impl::is_specific_endpoint_client(client_t _client,
        service_t _service, instance_t _instance) {
    client_t result = VSOMEIP_ROUTING_CLIENT;
    {
        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
        auto found_service = specific_endpoint_clients_.find(_service);
        if (found_service != specific_endpoint_clients_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_client = found_instance->second.find(_client);
                if(found_client != found_instance->second.end()) {
                    result = _client;
                }
            }
        }
    }
    // A client_t != VSOMEIP_ROUTING_CLIENT implies true
    return result;
}

std::unordered_set<client_t> routing_manager_impl::get_specific_endpoint_clients(
        service_t _service, instance_t _instance) {
    std::unordered_set<client_t> result;
    {
        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
        auto found_service = specific_endpoint_clients_.find(_service);
        if (found_service != specific_endpoint_clients_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                result = found_instance->second;
            }
        }
    }
    return result;
}

void routing_manager_impl::send_initial_events(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        const std::shared_ptr<endpoint_definition> &_subscriber) {
    std::shared_ptr<eventgroupinfo> its_eventgroup = find_eventgroup(_service,
            _instance, _eventgroup);
    if (!its_eventgroup) {
        return;
    }
    bool is_offered_both(false);
    if (configuration_->get_reliable_port(_service, _instance) != ILLEGAL_PORT &&
            configuration_->get_unreliable_port(_service, _instance) != ILLEGAL_PORT) {
        is_offered_both = true;
    }
    // send initial events if we already have a cached field (is_set)
    for (const auto &its_event : its_eventgroup->get_events()) {
        if (its_event->is_field()) {
            if (its_event->is_set()) {
                if (!is_offered_both) {
                    its_event->notify_one(_subscriber, true);
                } else {
                    if (its_event->is_reliable() && _subscriber->is_reliable()) {
                        its_event->notify_one(_subscriber, true);
                    }
                    if (!its_event->is_reliable() && !_subscriber->is_reliable()) {
                        its_event->notify_one(_subscriber, true);
                    }
                }
            } else {
                // received a subscription but can't notify due to missing payload
                its_event->set_remote_notification_pending(true);
            }
        }
    }
}

void routing_manager_impl::memory_log_timer_cbk(
        boost::system::error_code const & _error) {
    if (_error) {
        return;
    }
#ifndef _WIN32
    static const std::uint32_t its_pagesize = getpagesize() / 1024;
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

    // local client endpoints
    {
        std::map<client_t, std::shared_ptr<endpoint>> lces = get_local_endpoints();
        VSOMEIP_INFO << "status local client endpoints: " << std::dec << lces.size();
        for (const auto lce : lces) {
            lce.second->print_status();
        }
    }

    // udp and tcp client endpoints
    {
        client_endpoints_by_ip_t client_endpoints_by_ip;
        remote_services_t remote_services;
        server_endpoints_t server_endpoints;
        {
            std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
            client_endpoints_by_ip = client_endpoints_by_ip_;
            remote_services = remote_services_;
            server_endpoints = server_endpoints_;
        }
        VSOMEIP_INFO << "status start remote client endpoints:";
        std::uint32_t num_remote_client_endpoints(0);
        // normal endpoints
        for (const auto &a : client_endpoints_by_ip) {
            for (const auto p : a.second) {
                for (const auto ru : p.second) {
                    ru.second->print_status();
                    num_remote_client_endpoints++;
                }
            }
        }
        VSOMEIP_INFO << "status end remote client endpoints: " << std::dec
                << num_remote_client_endpoints;

        // selective client endpoints
        VSOMEIP_INFO << "status start selective remote client endpoints:";
        std::uint32_t num_remote_selectiv_client_endpoints(0);
        for (const auto s : remote_services) {
            for (const auto i : s.second) {
                for (const auto c : i.second) {
                    if (c.first != VSOMEIP_ROUTING_CLIENT) {
                        for (const auto ur : c.second) {
                            ur.second->print_status();
                            num_remote_selectiv_client_endpoints++;
                        }
                    }
                }
            }
        }
        VSOMEIP_INFO << "status end selective remote client endpoints: "
                << std::dec << num_remote_selectiv_client_endpoints;

        VSOMEIP_INFO << "status start server endpoints:";
        std::uint32_t num_server_endpoints(1);
        // local server endpoints
        stub_->print_endpoint_status();

        // server endpoints
        for (const auto p : server_endpoints) {
            for (const auto ru : p.second ) {
                ru.second->print_status();
                num_server_endpoints++;
            }
        }
        VSOMEIP_INFO << "status end server endpoints:"
                << std::dec << num_server_endpoints;
    }


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

} // namespace vsomeip

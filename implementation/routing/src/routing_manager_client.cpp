// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if __GNUC__ > 11
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

#if defined(__linux__) || defined(__QNX__)
#include <unistd.h>
#endif

#include <climits>
#include <forward_list>
#include <future>
#include <iomanip>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <boost/asio/post.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/event.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_client.hpp"
#include "../include/routing_client_state_machine.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../endpoints/include/abstract_socket_factory.hpp"
#include "../../endpoints/include/local_server.hpp"
#include "../../endpoints/include/local_endpoint.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/serializer.hpp"
#include "../../protocol/include/assign_client_command.hpp"
#include "../../protocol/include/assign_client_ack_command.hpp"
#include "../../protocol/include/config_command.hpp"
#include "../../protocol/include/deregister_application_command.hpp"
#include "../../protocol/include/distribute_security_policies_command.hpp"
#include "../../protocol/include/dummy_command.hpp"
#include "../../protocol/include/expire_command.hpp"
#include "../../protocol/include/offer_service_command.hpp"
#include "../../protocol/include/offered_services_request_command.hpp"
#include "../../protocol/include/offered_services_response_command.hpp"
#include "../../protocol/include/ping_command.hpp"
#include "../../protocol/include/pong_command.hpp"
#include "../../protocol/include/register_application_command.hpp"
#include "../../protocol/include/register_events_command.hpp"
#include "../../protocol/include/registered_ack_command.hpp"
#include "../../protocol/include/release_service_command.hpp"
#include "../../protocol/include/remove_security_policy_command.hpp"
#include "../../protocol/include/remove_security_policy_response_command.hpp"
#include "../../protocol/include/request_service_command.hpp"
#include "../../protocol/include/resend_provided_events_command.hpp"
#include "../../protocol/include/routing_info_command.hpp"
#include "../../protocol/include/send_command.hpp"
#include "../../protocol/include/stop_offer_service_command.hpp"
#include "../../protocol/include/subscribe_ack_command.hpp"
#include "../../protocol/include/subscribe_command.hpp"
#include "../../protocol/include/subscribe_nack_command.hpp"
#include "../../protocol/include/unregister_event_command.hpp"
#include "../../protocol/include/unsubscribe_ack_command.hpp"
#include "../../protocol/include/unsubscribe_command.hpp"
#include "../../protocol/include/update_security_credentials_command.hpp"
#include "../../protocol/include/update_security_policy_command.hpp"
#include "../../protocol/include/update_security_policy_response_command.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../security/include/policy.hpp"
#include "../../security/include/policy_manager_impl.hpp"
#include "../../security/include/security.hpp"
#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/is_value.hpp"
#include "../../utility/include/service_instance_map.hpp"
#include "../../utility/include/utility.hpp"
#include "../../tracing/include/connector_impl.hpp"

#if defined(__QNX__)
#define HAVE_INET_PTON 1
#include <boost/icl/concept/interval_associator.hpp>
#endif
namespace vsomeip_v3 {

#define VSOMEIP_LOG_PREFIX "rmc"

routing_manager_client::routing_manager_client(routing_manager_host* _host, bool _client_side_logging,
                                               const std::set<std::tuple<service_t, instance_t>>& _client_side_logging_filter) :
    routing_manager_base(_host), keepalive_timer_(io_), status_log_timer_(io_), version_log_timer_(_host->get_io()),
    keepalive_active_(false), keepalive_is_alive_(false), sender_(nullptr), receiver_(nullptr), request_debounce_timer_(io_),
    request_debounce_timer_running_(false), client_side_logging_(_client_side_logging),
    client_side_logging_filter_(_client_side_logging_filter) {

    char its_hostname[1024];
    if (gethostname(its_hostname, sizeof(its_hostname)) == 0) {
        set_client_host(its_hostname);
    }
}

routing_manager_client::~routing_manager_client() { }

void routing_manager_client::init() {
    routing_manager_base::init(std::make_shared<endpoint_manager_base>(this, io_, configuration_));
    sender_debounce_ = timer::create(io_, std::chrono::milliseconds(100), [this, weak_self = weak_from_this()] {
        if (auto self = weak_self.lock(); self) {
            debounce_restart_sender_done();
        }
        return false;
    });
    if (!state_machine_) {
        state_machine_ = routing_client_state_machine::create(
                io_,
                routing_client_state_machine::configuration{std::chrono::seconds(3),
                                                            std::chrono::milliseconds(configuration_->get_shutdown_timeout())},
                [this, weak_self = weak_from_this()] {
                    if (auto self = weak_self.lock(); self) {
                        std::unique_lock<std::recursive_mutex> lock{sender_mutex_};
                        restart_sender(lock);
                    }
                });
    }
}

void routing_manager_client::start() {
    state_machine_->target_running();
    {
        std::scoped_lock its_receiver_lock(receiver_mutex_);
        // NOTE: order matters, `create_local_server` must done first
        // with TCP, following `create_local_client` will use whatever port is established there
        if (!configuration_->is_local_routing() && !receiver_) {
            receiver_ = ep_mgr_->create_local_server(shared_from_this());
        }
    }
    std::unique_lock<std::recursive_mutex> lock{sender_mutex_};
    restart_sender(lock);
    {
        std::scoped_lock its_lock{log_timer_mutex_};
        if (configuration_->get_version_log_interval(host_->get_name(), false) > 0) {
            version_log_timer_.expires_after(std::chrono::seconds(0));
            version_log_timer_.async_wait([this](boost::system::error_code const& ec) { this->version_log_timer_cbk(ec); });
        }
        if (configuration_->get_status_log_interval(host_->get_name(), false) > 0) {
            status_log_timer_.expires_after(std::chrono::seconds(0));
            status_log_timer_.async_wait([this](boost::system::error_code const& ec) { this->status_log_timer_cbk(ec); });
        }
    }
}

void routing_manager_client::status_log_timer_cbk(boost::system::error_code const& _error) {
    if (!_error) {
        const uint32_t its_interval = configuration_->get_status_log_interval(host_->get_name(), false);
        VSOMEIP_INFO_P << " ";
        ep_mgr_->print_status();

        {
            std::scoped_lock its_lock(log_timer_mutex_);
            status_log_timer_.expires_after(std::chrono::milliseconds(its_interval));
            status_log_timer_.async_wait([this](boost::system::error_code const& ec) { this->status_log_timer_cbk(ec); });
        }
    }
}

void routing_manager_client::version_log_timer_cbk(boost::system::error_code const& _error) {
    if (!_error) {
        const uint32_t its_interval = configuration_->get_version_log_interval(host_->get_name(), false);

        VSOMEIP_INFO << "vSomeIP " << VSOMEIP_VERSION << " | ";

        log_network_state(true, false);

        {
            std::scoped_lock its_lock(log_timer_mutex_);
            version_log_timer_.expires_after(std::chrono::milliseconds(its_interval));
            version_log_timer_.async_wait([this](boost::system::error_code const& ec) { this->version_log_timer_cbk(ec); });
        }
    }
}

void routing_manager_client::stop() {
    state_machine_->target_shutdown();
    cancel_keepalive();
    if (state_machine_->await_registered()) {
        {
            // the subsequent deregister_application might lead to a "end of file"
            // error in the sender, which could initiate a reconnection attempt
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->register_error_handler(nullptr);
            }
        }
        // Important to do before deregistration to ensure messages are sent before the application deregisters with the daemon
        try_to_send_before_stop();
        if (state_machine_->start_deregister()) {
            deregister_application();
        } else {
            VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " couldn't initiate deregister application procedure";
        }
        if (!state_machine_->await_deregistered()) {
            VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " couldn't deregister application - timeout";
        }
    }

    {
        std::scoped_lock its_lock(requests_to_debounce_mutex_);
        request_debounce_timer_.cancel();
    }

    {
        std::scoped_lock its_receiver_lock(receiver_mutex_);
        if (receiver_) {
            receiver_->stop();
        }
        receiver_ = nullptr;
    }

    {
        std::scoped_lock its_lock{log_timer_mutex_};
        version_log_timer_.cancel();
        status_log_timer_.cancel();
    }
    {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->stop(false);
        }
        // delete the sender
        sender_ = nullptr;
    }

    ep_mgr_->clear_local_endpoints();
}

std::shared_ptr<configuration> routing_manager_client::get_configuration() const {
    return host_->get_configuration();
}

std::string routing_manager_client::get_env(client_t _client) const {

    std::scoped_lock its_known_clients_lock(known_clients_mutex_);
    return get_env_unlocked(_client);
}

std::string routing_manager_client::get_env_unlocked(client_t _client) const {

    auto find_client = known_clients_.find(_client);
    if (find_client != known_clients_.end()) {
        return find_client->second;
    }
    return "";
}

void routing_manager_client::start_keepalive() {
    std::scoped_lock lk{keepalive_mutex_};
    if (!keepalive_active_ && configuration_->is_local_clients_keepalive_enabled()) {
        VSOMEIP_INFO_P << "Local Clients Keepalive is enabled. Value is " << configuration_->get_local_clients_keepalive_time().count()
                       << " ms.";

        keepalive_active_ = true;
        keepalive_is_alive_ = true;
        keepalive_timer_.expires_after(configuration_->get_local_clients_keepalive_time());
        keepalive_timer_.async_wait([this](boost::system::error_code const&) { this->check_keepalive(); });
    }
}

void routing_manager_client::check_keepalive() {
    bool send_probe{false};
    {
        std::scoped_lock lk{keepalive_mutex_};
        if (keepalive_active_) {
            if (keepalive_is_alive_) {
                keepalive_is_alive_ = false;
                send_probe = true;
                keepalive_timer_.expires_after(configuration_->get_local_clients_keepalive_time());
                keepalive_timer_.async_wait([this](boost::system::error_code const&) { this->check_keepalive(); });
            } else {
                VSOMEIP_WARNING_P << "Client 0x" << hex4(get_client()) << " didn't receive keepalive confirmation from HOST.";
                boost::asio::post(io_, [this] { this->handle_client_error(VSOMEIP_ROUTING_CLIENT); });
            }
        }
    }
    // Can't send with keepalive_mutex_ due to lock inversion
    if (send_probe) {
        ping_host();
    }
}

void routing_manager_client::cancel_keepalive() {
    std::scoped_lock lk{keepalive_mutex_};
    if (keepalive_active_ && configuration_->is_local_clients_keepalive_enabled()) {
        VSOMEIP_INFO_P << " ";
        keepalive_active_ = false;
        keepalive_timer_.cancel();
    }
}

void routing_manager_client::ping_host() {
    protocol::ping_command its_command;
    its_command.set_client(get_client());

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {

        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else {
        VSOMEIP_ERROR_P << "Ping command serialization failed (" << static_cast<int>(its_error) << ")";
    }
}

void routing_manager_client::on_pong(client_t _client) {
    if (_client == VSOMEIP_ROUTING_CLIENT) {
        std::scoped_lock lk{keepalive_mutex_};
        keepalive_is_alive_ = true;
    }
}

bool routing_manager_client::offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                           minor_version_t _minor) {

    if (!routing_manager_base::offer_service(_client, _service, _instance, _major, _minor)) {
        VSOMEIP_WARNING_P << "Service " << hex4(_service) << "." << hex4(_instance) << "." << hex4(_major)
                          << " is already offered by Client 0x" << hex4(_client);
    }
    {
        // order matters:
        // 1. Ensure that it is part of the pending_offers set
        // 2. check for the state
        // otherwise:
        // state might be not be registered, but turn registered just after the check,
        // rushing ahead sending the pending offers that do not contain this offer yet.
        protocol::service offer(_service, _instance, _major, _minor);
        std::scoped_lock its_lock(pending_offers_mutex_);
        pending_offers_.insert(offer);
        if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {
            send_offer_service(_client, _service, _instance, _major, _minor);
        }
    }
    return true;
}

bool routing_manager_client::send_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                minor_version_t _minor) {

    (void)_client;

    protocol::offer_service_command its_offer;
    its_offer.set_client(get_client());
    its_offer.set_service(_service);
    its_offer.set_instance(_instance);
    its_offer.set_major(_major);
    its_offer.set_minor(_minor);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_offer.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_ && sender_->send(&its_buffer[0], uint32_t(its_buffer.size()))) {
            return true;
        }
        VSOMEIP_ERROR_P << "Failure offerring service " << hex4(_service) << "." << hex4(_instance) << "." << hex4(_major);
    } else {
        VSOMEIP_ERROR_P << "Offer_service serialization failed (" << static_cast<int>(its_error) << ")";
    }

    return false;
}

void routing_manager_client::stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                minor_version_t _minor) {

    (void)_client;

    {
        // Hold the mutex to ensure no placeholder event is created in between.
        std::scoped_lock its_lock(stop_mutex_);

        routing_manager_base::stop_offer_service(_client, _service, _instance, _major, _minor);
        clear_remote_subscriber_count(_service, _instance);

        // Note: The last argument does not matter here as a proxy
        //       does not manage endpoints to the external network.
        clear_service_info(_service, _instance, false);
    }

    {
        // order matters:
        // 1. Remove the offer from the set,
        // 2. Send the removal if we are registered
        // otherwise it might happen that we don't send the stop offer,
        // but have not removed the offer when REGISTERED is entered
        std::scoped_lock its_lock(pending_offers_mutex_);
        auto it = pending_offers_.begin();
        while (it != pending_offers_.end()) {
            if (it->service_ == _service && it->instance_ == _instance) {
                break;
            }
            it++;
        }
        if (it != pending_offers_.end()) {
            pending_offers_.erase(it);
        }
    }
    if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {

        protocol::stop_offer_service_command its_command;
        its_command.set_client(get_client());
        its_command.set_service(_service);
        its_command.set_instance(_instance);
        its_command.set_major(_major);
        its_command.set_minor(_minor);

        std::vector<byte_t> its_buffer;
        protocol::error_e its_error;
        its_command.serialize(its_buffer, its_error);

        if (its_error == protocol::error_e::ERROR_OK) {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
            }
        } else {
            VSOMEIP_ERROR_P << "Stop offer serialization failed (" << static_cast<int>(its_error) << ")";
        }
    }
}

void routing_manager_client::request_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                             minor_version_t _minor) {
    routing_manager_base::request_service(_client, _service, _instance, _major, _minor);
    {
        size_t request_debouncing_time = configuration_->get_request_debounce_time(host_->get_name());
        protocol::service request = {_service, _instance, _major, _minor};
        if (!request_debouncing_time) {
            // order matters:
            // 1. Ensure that the set contains this request
            // 2. Try to send if we are registered
            // If exchanged the sending after the registration is going to race with
            // the subsequent logic.
            {
                std::scoped_lock its_lock(requests_mutex_);
                requests_.insert(request);
            }
            if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {
                std::set<protocol::service> requests;
                requests.insert(request);
                send_request_services(requests);
            }
        } else {
            std::scoped_lock its_lock{requests_to_debounce_mutex_};
            requests_to_debounce_.insert(request);
            if (!request_debounce_timer_running_) {
                request_debounce_timer_running_ = true;
                request_debounce_timer_.expires_after(std::chrono::milliseconds(request_debouncing_time));
                request_debounce_timer_.async_wait(std::bind(&routing_manager_client::request_debounce_timeout_cbk,
                                                             std::dynamic_pointer_cast<routing_manager_client>(shared_from_this()),
                                                             std::placeholders::_1));
            }
        }
    }
}

void routing_manager_client::release_service(client_t _client, service_t _service, instance_t _instance) {
    routing_manager_base::release_service(_client, _service, _instance);
    {
        remove_pending_subscription(_service, _instance, 0xFFFF, ANY_EVENT);

        // order matters:
        // 1. Remove the service from the requests
        // 2. Send the release when we are registered
        // otherwise we might not send the release, but request the service
        // when being registered
        {
            std::scoped_lock its_lock(requests_mutex_);
            auto it = requests_.begin();
            while (it != requests_.end()) {
                if (it->service_ == _service && it->instance_ == _instance) {
                    break;
                }
                it++;
            }
            if (it != requests_.end())
                requests_.erase(it);
        }
        bool pending(false);
        {
            std::scoped_lock its_lock(requests_to_debounce_mutex_);
            auto it = requests_to_debounce_.begin();
            while (it != requests_to_debounce_.end()) {
                if (it->service_ == _service && it->instance_ == _instance) {
                    pending = true;
                    break;
                }
                it++;
            }
            if (it != requests_to_debounce_.end())
                requests_to_debounce_.erase(it);
        }
        if (!pending && state_machine_->state() == routing_client_state_e::ST_REGISTERED) {
            send_release_service(_client, _service, _instance);
        }
    }
}

void routing_manager_client::register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                            const std::set<eventgroup_t>& _eventgroups, const event_type_e _type,
                                            reliability_type_e _reliability, std::chrono::milliseconds _cycle, bool _change_resets_cycle,
                                            bool _update_on_change, epsilon_change_func_t _epsilon_change_func, bool _is_provided,
                                            bool _is_shadow, bool _is_cache_placeholder) {

    (void)_is_shadow;
    (void)_is_cache_placeholder;

    bool is_cyclic(_cycle != std::chrono::milliseconds::zero());

    const event_data_t registration = {_service, _instance, _notifier, _type, _reliability, _is_provided, is_cyclic, _eventgroups};
    bool is_first(false);
    {
        std::scoped_lock its_lock(pending_event_registrations_mutex_);
        is_first = pending_event_registrations_.find(registration) == pending_event_registrations_.end();
        if (is_first) {
            pending_event_registrations_.insert(registration);
        }
        bool insert = true;
        if (is_first) {
            for (auto iter = pending_event_registrations_.begin(); iter != pending_event_registrations_.end();) {
                if (iter->service_ == _service && iter->instance_ == _instance && iter->notifier_ == _notifier
                    && iter->is_provided_ == _is_provided && iter->type_ == event_type_e::ET_EVENT
                    && _type == event_type_e::ET_SELECTIVE_EVENT) {
                    iter = pending_event_registrations_.erase(iter);
                    iter = pending_event_registrations_.insert(registration).first;
                    is_first = true;
                    insert = false;
                    break;
                } else {
                    iter++;
                }
            }
            if (insert) {
                pending_event_registrations_.insert(registration);
            }
        }
    }
    if (is_first || _is_provided) {
        routing_manager_base::register_event(_client, _service, _instance, _notifier, _eventgroups, _type, _reliability, _cycle,
                                             _change_resets_cycle, _update_on_change, _epsilon_change_func, _is_provided);
    }
    {
        if (state_machine_->state() == routing_client_state_e::ST_REGISTERED && is_first) {
            send_register_event(get_client(), _service, _instance, _notifier, _eventgroups, _type, _reliability, _is_provided, is_cyclic);
        }
    }
}

void routing_manager_client::unregister_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                              bool _is_provided) {

    routing_manager_base::unregister_event(_client, _service, _instance, _notifier, _is_provided);

    // order matters:
    // 1. Remove the event from the pending events
    // 2. Try to send the unregister command
    // Otherwise we might not send the command, but request it when entering REGISTERED
    {
        std::scoped_lock its_lock(pending_event_registrations_mutex_);
        for (auto iter = pending_event_registrations_.begin(); iter != pending_event_registrations_.end();) {
            if (iter->service_ == _service && iter->instance_ == _instance && iter->notifier_ == _notifier
                && iter->is_provided_ == _is_provided) {
                pending_event_registrations_.erase(iter);
                break;
            } else {
                iter++;
            }
        }
    }
    if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {

        protocol::unregister_event_command its_command;
        its_command.set_client(get_client());
        its_command.set_service(_service);
        its_command.set_instance(_instance);
        its_command.set_event(_notifier);
        its_command.set_provided(_is_provided);

        std::vector<byte_t> its_buffer;
        protocol::error_e its_error;
        its_command.serialize(its_buffer, its_error);
        if (its_error == protocol::error_e::ERROR_OK) {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
            }
        }
    }
}

bool routing_manager_client::is_field(service_t _service, instance_t _instance, event_t _event) const {
    auto event = find_event(_service, _instance, _event);
    if (event && event->is_field()) {
        return true;
    }
    return false;
}

void routing_manager_client::subscribe(client_t _client, const vsomeip_sec_client_t* _sec_client, service_t _service, instance_t _instance,
                                       eventgroup_t _eventgroup, major_version_t _major, event_t _event,
                                       const std::shared_ptr<debounce_filter_impl_t>& _filter) {

    (void)_client;

    // order matters:
    // 1. Add the subscription to the set
    // 2. Try to send the subscription if already registered
    // otherwise the subscription might not be send at all
    {
        std::scoped_lock its_lock{pending_subscription_mutex_};
        subscription_data_t subscription = {_service, _instance, _eventgroup, _major, _event, _filter, *_sec_client};
        pending_subscriptions_.insert(subscription);
    }
    if (state_machine_->state() == routing_client_state_e::ST_REGISTERED && is_available(_service, _instance, _major)) {
        send_subscribe(get_client(), _service, _instance, _eventgroup, _major, _event, _filter);
    }
}

void routing_manager_client::send_subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                            major_version_t _major, event_t _event,
                                            const std::shared_ptr<debounce_filter_impl_t>& _filter) {

    if (_event == ANY_EVENT) {
        auto const sec_client = get_sec_client();
        if (!is_subscribe_to_any_event_allowed(&sec_client, _client, _service, _instance, _eventgroup)) {
            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(_client) << " : routing_manager_proxy::subscribe: "
                            << " isn't allowed to subscribe to service/instance/event " << hex4(_service) << "/" << hex4(_instance)
                            << "/ANY_EVENT which violates the security policy ~> Skip subscribe!";
            return;
        }
    } else {
        auto const sec_client = get_sec_client();
        if (VSOMEIP_SEC_OK
            != configuration_->get_security()->is_client_allowed_to_access_member(&sec_client, _service, _instance, _event)) {
            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(_client) << " : routing_manager_proxy::subscribe: "
                            << " isn't allowed to subscribe to service/instance/event " << hex4(_service) << "/" << hex4(_instance) << "/"
                            << hex4(_event);
            return;
        }
    }

    protocol::subscribe_command its_command;
    its_command.set_client(_client);
    its_command.set_service(_service);
    its_command.set_instance(_instance);
    its_command.set_eventgroup(_eventgroup);
    its_command.set_major(_major);
    its_command.set_event(_event);
    its_command.set_pending_id(PENDING_SUBSCRIPTION_ID);
    its_command.set_filter(_filter);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        client_t its_target_client = find_local_client(_service, _instance);
        if (its_target_client != VSOMEIP_ROUTING_CLIENT) {
            auto its_target = ep_mgr_->find_or_create_local_client(its_target_client);
            if (its_target) {
                its_target->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "No target available to send subscription. Client=0x" << hex4(_client) << " service=" << hex4(_service)
                                << "." << hex4(_instance) << "." << std::setw(2) << static_cast<std::uint16_t>(_major)
                                << " event=" << hex4(_event);
                // if we can not create a connection with this client -> there should be something wrong with the routing info,
                // but we assume the service is offered -> this is an error and we should handle it
                handle_client_error(its_target_client);
            }
        } else {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
            }
        }
    } else {
        VSOMEIP_ERROR_P << "Subscribe command serialization failed (" << static_cast<int>(its_error) << ")";
    }
}

void routing_manager_client::send_subscribe_nack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                 event_t _event, remote_subscription_id_t _id) {

    protocol::subscribe_nack_command its_command;
    its_command.set_client(get_client());
    its_command.set_service(_service);
    its_command.set_instance(_instance);
    its_command.set_eventgroup(_eventgroup);
    its_command.set_subscriber(_subscriber);
    its_command.set_event(_event);
    its_command.set_pending_id(_id);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);
    if (its_error == protocol::error_e::ERROR_OK) {
        if (_subscriber != VSOMEIP_ROUTING_CLIENT && _id == PENDING_SUBSCRIPTION_ID) {
            auto its_target = ep_mgr_->find_local_server_endpoint(_subscriber);
            if (its_target) {
                its_target->send(&its_buffer[0], uint32_t(its_buffer.size()));
                return;
            } else {
                VSOMEIP_WARNING_P << "No target available to send subscription nack. Client=0x" << hex4(_subscriber)
                                  << " service=" << hex4(_service) << "." << hex4(_instance) << " event=" << hex4(_event);
            }
        }
        {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
            }
        }
    } else {
        VSOMEIP_ERROR_P << "Subscribe nack serialization failed (" << static_cast<int>(its_error) << ")";
    }
}

void routing_manager_client::send_subscribe_ack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                event_t _event, remote_subscription_id_t _id) {

    protocol::subscribe_ack_command its_command;
    its_command.set_client(get_client());
    its_command.set_service(_service);
    its_command.set_instance(_instance);
    its_command.set_eventgroup(_eventgroup);
    its_command.set_subscriber(_subscriber);
    its_command.set_event(_event);
    its_command.set_pending_id(_id);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);
    if (its_error == protocol::error_e::ERROR_OK) {
        if (_subscriber != VSOMEIP_ROUTING_CLIENT && _id == PENDING_SUBSCRIPTION_ID) {
            auto its_target = ep_mgr_->find_local_server_endpoint(_subscriber);
            if (its_target) {
                its_target->send(&its_buffer[0], uint32_t(its_buffer.size()));
                return;
            } else {
                VSOMEIP_WARNING_P << "No target available to send subscription ack. Client=0x" << hex4(_subscriber)
                                  << " service=" << hex4(_service) << "." << hex4(_instance) << " event=" << hex4(_event);
            }
        }
        {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
            }
        }
    } else {
        VSOMEIP_ERROR_P << "Subscribe ack serialization failed (" << static_cast<int>(its_error) << ")";
    }
}

void routing_manager_client::unsubscribe(client_t _client, const vsomeip_sec_client_t* _sec_client, service_t _service,
                                         instance_t _instance, eventgroup_t _eventgroup, event_t _event) {

    (void)_client;
    (void)_sec_client;

    {
        remove_pending_subscription(_service, _instance, _eventgroup, _event);

        if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {

            protocol::unsubscribe_command its_command;
            its_command.set_client(_client);
            its_command.set_service(_service);
            its_command.set_instance(_instance);
            its_command.set_eventgroup(_eventgroup);
            its_command.set_major(ANY_MAJOR);
            its_command.set_event(_event);
            its_command.set_pending_id(PENDING_SUBSCRIPTION_ID);

            std::vector<byte_t> its_buffer;
            protocol::error_e its_error;
            its_command.serialize(its_buffer, its_error);

            if (its_error == protocol::error_e::ERROR_OK) {

                auto its_target = ep_mgr_->find_local_client(_service, _instance);
                if (its_target) {
                    its_target->send(&its_buffer[0], uint32_t(its_buffer.size()));
                } else {
                    if (_client != VSOMEIP_ROUTING_CLIENT) {
                        VSOMEIP_WARNING_P << "Could not find endpoint for client 0x" << hex4(_client) << ", sending to the router";
                    }
                    std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
                    if (sender_) {
                        sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
                    } else {
                        VSOMEIP_ERROR_P << "Failed due to a missing sender";
                    }
                }
            } else {

                VSOMEIP_ERROR_P << "Unsubscribe serialization failed (" << static_cast<int>(its_error) << ")";
            }
        }
    }
}

bool routing_manager_client::send(client_t _client, const byte_t* _data, length_t _size, instance_t _instance, bool _reliable,
                                  client_t _bound_client, const vsomeip_sec_client_t* _sec_client, uint8_t _status_check,
                                  bool _sent_from_remote, bool _force) {

    (void)_bound_client;
    (void)_sec_client;
    (void)_sent_from_remote;
    bool is_sent{false};
    bool has_remote_subscribers{false};
    if (auto const state = state_machine_->state(); state != routing_client_state_e::ST_REGISTERED) {
        VSOMEIP_WARNING_P << "(" << hex4(get_client()) << "): Dropping message for client: " << hex4(_client)
                          << ", due to unexpected state: " << state;
        return false;
    }
    if (client_side_logging_) {
        if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
            service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
            if (client_side_logging_filter_.empty() || (1 == client_side_logging_filter_.count(std::make_tuple(its_service, ANY_INSTANCE)))
                || (1 == client_side_logging_filter_.count(std::make_tuple(its_service, _instance)))) {
                method_t its_method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);
                session_t its_session = bithelper::read_uint16_be(&_data[VSOMEIP_SESSION_POS_MIN]);
                client_t its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);
                VSOMEIP_INFO_P << "(" << hex4(get_client()) << "): [" << hex4(its_service) << "." << hex4(_instance) << "."
                               << hex4(its_method) << ":" << hex4(its_session) << ":" << hex4(its_client) << "] "
                               << "type=" << std::hex << static_cast<std::uint32_t>(_data[VSOMEIP_MESSAGE_TYPE_POS])
                               << " thread=" << std::hex << std::this_thread::get_id();
            }
        } else {
            VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << ": message too short to log: " << std::dec << _size;
        }
    }
    if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
        std::shared_ptr<local_endpoint> its_target;
        if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            // Request
            service_t its_service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
            client_t its_client = find_local_client(its_service, _instance);
            if (its_client != VSOMEIP_ROUTING_CLIENT) {
                if (is_client_known(its_client)) {
                    its_target = ep_mgr_->find_or_create_local_client(its_client);
                }
            }
        } else if (!utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            // Response
            client_t its_client = bithelper::read_uint16_be(&_data[VSOMEIP_CLIENT_POS_MIN]);
            if (its_client != VSOMEIP_ROUTING_CLIENT) {
                its_target = ep_mgr_->find_local_server_endpoint(its_client);
            }
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) && _client == VSOMEIP_ROUTING_CLIENT) {
            // notify
            std::scoped_lock lock{sender_mutex_};
            /**
             * Note: The sender_ is passed in in case the routing client subscribed with its actual id. We need
             * to explicitly pass it in, because the router uses the routing connection for everything. Before
             * the refactoring the router would have send everything over one sender, but would have accepted
             * a connection for the sub_ack from the client (meaning the client would have found an endpoint for this id).
             * BUT: in case the router subscribed on behalf of a boardnet client it would have used VSOMEIP_ROUTING_CLIENT as an id.
             * But the base class has a dedicated case for this id in which it does not send anything out.
             * Note: the _client == VSOMEIP_ROUTING_CLIENT needs to be read as "broadcast this event".
             * For these reasons one should neither do an early return here (as the broadcast should also mean: Send it also to the router).
             * All this is somewhat questionable, but in the course of the sender/receiver -> client/server arch change,
             * this should not be reworked.
             **/
            has_remote_subscribers =
                    send_local_notification(get_client(), _data, _size, _instance, _reliable, _status_check, _force, sender_);
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) && _client != VSOMEIP_ROUTING_CLIENT) {
            // notify_one
            its_target = ep_mgr_->find_local_server_endpoint(_client);
            if (its_target) {
                is_sent = send_local(its_target, get_client(), _data, _size, _instance, _reliable, protocol::id_e::SEND_ID, _status_check);
                if (is_sent) {
                    trace::header its_header;
                    if (its_header.prepare(its_target, true, _instance))
                        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
                }

                return is_sent;
            }
        }
        // If no direct endpoint could be found
        // or for notifications ~> route to routing_manager_stub
        bool message_to_stub(false);
        if (!its_target) {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                its_target = sender_;
                message_to_stub = true;
            } else {
                VSOMEIP_WARNING_P << "No connection to router. Message will be dropped";
                return false;
            }
        }

        bool send(true);
        protocol::id_e its_command(protocol::id_e::SEND_ID);

        if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            if (_client != VSOMEIP_ROUTING_CLIENT) {
                its_command = protocol::id_e::NOTIFY_ONE_ID;
            } else {
                its_command = protocol::id_e::NOTIFY_ID;
                // Do we need to deliver a notification to the routing manager?
                // Only for services which already have remote clients subscribed to
                send = has_remote_subscribers;
            }
        }
        if (send) {
            auto its_client{its_command == protocol::id_e::NOTIFY_ONE_ID ? _client : get_client()};
            is_sent = send_local(its_target, its_client, _data, _size, _instance, _reliable, its_command, _status_check);
            if (is_sent && !utility::is_notification(VSOMEIP_MESSAGE_TYPE_POS) && !message_to_stub) {
                trace::header its_header;
                if (its_header.prepare(its_target, true, _instance))
                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data, _size);
            }
        }
    }
    return is_sent;
}

bool routing_manager_client::send_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target,
                                     std::shared_ptr<message> _message) {

    (void)_client;
    (void)_target;
    (void)_message;

    return false;
}

bool routing_manager_client::send_to(const std::shared_ptr<endpoint_definition>& _target, const byte_t* _data, uint32_t _size,
                                     instance_t _instance) {

    (void)_target;
    (void)_data;
    (void)_size;
    (void)_instance;

    return false;
}

void routing_manager_client::on_message(const byte_t* _data, length_t _size, boardnet_endpoint* _receiver, bool _is_multicast,
                                        client_t _bound_client, const vsomeip_sec_client_t* _sec_client,
                                        const boost::asio::ip::address& _remote_address, std::uint16_t _remote_port) {

    (void)_receiver;
    (void)_is_multicast;
    (void)_remote_address;
    (void)_remote_port;

    protocol::id_e its_id;
    client_t its_client;
    service_t its_service;
    instance_t its_instance;
    eventgroup_t its_eventgroup;
    event_t its_event;
    major_version_t its_major;
    client_t routing_host_id = configuration_->get_id(configuration_->get_routing_host_name());
    client_t its_subscriber;
    remote_subscription_id_t its_pending_id(PENDING_SUBSCRIPTION_ID);
    std::uint32_t its_remote_subscriber_count(0);
#ifndef VSOMEIP_DISABLE_SECURITY
    bool is_internal_policy_update(false);
#endif // !VSOMEIP_DISABLE_SECURITY
    std::vector<byte_t> its_buffer(_data, _data + _size);
    protocol::error_e its_error;

    auto its_policy_manager = configuration_->get_policy_manager();
    if (!its_policy_manager)
        return;

    protocol::dummy_command its_dummy_command;
    its_dummy_command.deserialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        its_id = its_dummy_command.get_id();
        its_client = its_dummy_command.get_client();

        bool is_from_routing(false);
        if (configuration_->is_security_enabled()) {
            if (configuration_->is_local_routing()) {
                // if security is enabled, client ID of routing must be configured
                // and credential passing is active. Otherwise bound client is zero by default
                is_from_routing = (_bound_client == routing_host_id || _bound_client == VSOMEIP_ROUTING_CLIENT);
            } else {
                is_from_routing = (_remote_address == configuration_->get_routing_host_address()
                                   && _remote_port == configuration_->get_routing_host_port());
            }
        } else {
            is_from_routing = (its_client == routing_host_id);
        }

        if (configuration_->is_security_enabled() && configuration_->is_local_routing() && !is_from_routing
            && _bound_client != its_client) {
            VSOMEIP_WARNING_P << "Client 0x" << hex4(get_client()) << " received a message with command " << static_cast<int>(its_id)
                              << " from " << hex4(its_client) << " which doesn't match the bound client " << hex4(_bound_client)
                              << " ~> skip message!";
            return;
        }

        switch (its_id) {
        case protocol::id_e::SEND_ID: {
            protocol::send_command its_send_command(protocol::id_e::SEND_ID);
            its_send_command.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {

                auto a_deserializer = get_deserializer();
                a_deserializer->set_data(its_send_command.get_message());
                std::shared_ptr<message_impl> its_message = a_deserializer->deserialize_message();
                a_deserializer->reset();
                put_deserializer(a_deserializer);

                if (its_message) {
                    its_message->set_instance(its_send_command.get_instance());
                    its_message->set_reliable(its_send_command.is_reliable());
                    its_message->set_check_result(its_send_command.get_status());
                    if (_sec_client)
                        its_message->set_sec_client(*_sec_client);
                    its_message->set_env(get_env(_bound_client));

                    if (!is_from_routing) {
                        if (utility::is_request(its_message->get_message_type())) {
                            if (configuration_->is_security_enabled() && configuration_->is_local_routing()
                                && its_message->get_client() != _bound_client) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client())
                                                << " received a request from client 0x" << hex4(its_message->get_client())
                                                << " to service/instance/method " << hex4(its_message->get_service()) << "/"
                                                << hex4(its_message->get_instance()) << "/" << hex4(its_message->get_method())
                                                << " which doesn't match the bound client 0x" << hex4(_bound_client) << " ~> skip message!";
                                return;
                            }
                            if (VSOMEIP_SEC_OK
                                != configuration_->get_security()->is_client_allowed_to_access_member(
                                        _sec_client, its_message->get_service(), its_message->get_instance(), its_message->get_method())) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(its_message->get_client())
                                                << " : routing_manager_client::on_message: " << hex4(its_message->get_client())
                                                << "isn't allowed to send a request to service/instance/method "
                                                << hex4(its_message->get_service()) << "/" << hex4(its_message->get_instance()) << "/"
                                                << hex4(its_message->get_method()) << " ~> Skip message!";
                                return;
                            }
                        } else { // Notification or Response
                            // By analysis we know that this branch is never taken,
                            // but sonarQube files the unchecked usage as a bug
                            // -> introduce a check just for sonarQube
                            if (!_sec_client) {
                                return;
                            }
                            // TODO for external security ports are checked.
                            // Notification and responses were originally send out by the "sender".
                            // With the refactoring towards client-server we have to temporarily "lie"
                            // to security about the port we received the message from...
                            auto sec_client = *_sec_client;
                            sec_client.port = htons(ntohs(_sec_client->port) - 1);

                            // Verifies security offer rule for messages (notifications and
                            // responses)
                            bool is_offer_access_ok =
                                    (configuration_->is_security_external()
                                     && VSOMEIP_SEC_OK
                                             == configuration_->get_security()->is_client_allowed_to_offer(
                                                     &sec_client, its_message->get_service(), its_message->get_instance()));

                            if (!is_offer_access_ok && configuration_->is_security_external()) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client())
                                                << " : routing_manager_client::on_message: received a "
                                                << (utility::is_notification(its_message->get_message_type()) ? "notification" : "response")
                                                << " from client 0x" << hex4(_bound_client)
                                                << " which does not offer service/instance/method " << hex4(its_message->get_service())
                                                << "/" << hex4(its_message->get_instance()) << "/" << hex4(its_message->get_method())
                                                << " ~> Skip message!";
                                return;
                            }

                            bool is_intern_resp_allowed = (!configuration_->is_security_external()
                                                           && is_response_allowed(_bound_client, its_message->get_service(),
                                                                                  its_message->get_instance(), its_message->get_method()));

                            if (is_intern_resp_allowed || is_offer_access_ok) {
                                const bool is_notification = utility::is_notification(its_message->get_message_type());

                                if (is_notification) {
                                    auto const sec_client = get_sec_client();
                                    const bool is_access_member_ok = (VSOMEIP_SEC_OK
                                                                      == configuration_->get_security()->is_client_allowed_to_access_member(
                                                                              &sec_client, its_message->get_service(),
                                                                              its_message->get_instance(), its_message->get_method()));

                                    if (!is_access_member_ok) {
                                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(its_message->get_client())
                                                        << " : routing_manager_client::on_message: " << hex4(get_client())
                                                        << " : routing_manager_client::on_message: isn't allowed to receive a "
                                                        << " notification from service/instance/method " << hex4(its_message->get_service())
                                                        << "/" << hex4(its_message->get_instance()) << "/"
                                                        << hex4(its_message->get_method()) << " respectively from client 0x"
                                                        << hex4(_bound_client) << " ~> Skip message!";
                                        return;
                                    }
                                    cache_event_payload(its_message);
                                }
                            } else {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client())
                                                << " : routing_manager_client::on_message: received a "
                                                << (utility::is_notification(its_message->get_message_type()) ? "notification" : "response")
                                                << " from client 0x" << hex4(_bound_client)
                                                << " which does not offer service/instance/method " << hex4(its_message->get_service())
                                                << "/" << hex4(its_message->get_instance()) << "/" << hex4(its_message->get_method())
                                                << " ~> Skip message!";
                                return;
                            }
                        }
                    } else {
                        if (!configuration_->is_remote_access_allowed()) {
                            // if the message is from routing manager, check if
                            // policy allows remote requests.
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client())
                                            << " : routing_manager_client::on_message: Security: Remote clients via routing manager with "
                                            << "client ID 0x" << hex4(its_client)
                                            << " are not allowed to communicate with service/instance/method "
                                            << hex4(its_message->get_service()) << "/" << hex4(its_message->get_instance()) << "/"
                                            << hex4(its_message->get_method()) << " respectively with client 0x" << hex4(get_client())
                                            << " ~> Skip message!";
                            return;
                        } else if (utility::is_notification(its_message->get_message_type())) {
                            // As subscription is sent on eventgroup level, incoming remote event
                            // ID's need to be checked as well if remote clients are allowed and the
                            // local policy only allows specific events in the eventgroup to be
                            // received.

                            auto const sec_client = get_sec_client();
                            if (VSOMEIP_SEC_OK
                                != configuration_->get_security()->is_client_allowed_to_access_member(
                                        &sec_client, its_message->get_service(), its_message->get_instance(), its_message->get_method())) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client())
                                                << " : routing_manager_client::on_message: "
                                                << " isn't allowed to receive a notification from service/instance/event "
                                                << hex4(its_message->get_service()) << "/" << hex4(its_message->get_instance()) << "/"
                                                << hex4(its_message->get_method())
                                                << " respectively from remote clients via routing manager with client ID 0x"
                                                << hex4(routing_host_id) << " ~> Skip message!";
                                return;
                            }
                            cache_event_payload(its_message);
                        }
                    }

                    if (client_side_logging_
                        && (client_side_logging_filter_.empty()
                            || (1 == client_side_logging_filter_.count(std::make_tuple(its_message->get_service(), ANY_INSTANCE)))
                            || (1
                                == client_side_logging_filter_.count(
                                        std::make_tuple(its_message->get_service(), its_message->get_instance()))))) {
                        trace::header its_header;
                        if (its_header.prepare(nullptr, false, its_send_command.get_instance(), trace::protocol_e::unknown)) {
                            uint32_t its_message_size = its_send_command.get_size();
                            if (its_message_size >= uint32_t{vsomeip_v3::protocol::SEND_COMMAND_HEADER_SIZE})
                                its_message_size -= uint32_t{vsomeip_v3::protocol::SEND_COMMAND_HEADER_SIZE};
                            else
                                its_message_size = 0;

                            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, &_data[vsomeip_v3::protocol::SEND_COMMAND_HEADER_SIZE],
                                       its_message_size);
                        }
                    }

                    host_->on_message(std::move(its_message));

                } else
                    VSOMEIP_ERROR_P << "Routing proxy: SomeIP-Header deserialization failed!";
            } else
                VSOMEIP_ERROR_P << "Send command deserialization failed (" << static_cast<int>(its_error) << ")";
            break;
        }

        case protocol::id_e::ASSIGN_CLIENT_ACK_ID: {
            client_t its_assigned_client(VSOMEIP_CLIENT_UNSET);
            protocol::assign_client_ack_command its_ack_command;
            its_ack_command.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK)
                its_assigned_client = its_ack_command.get_assigned();

            on_client_assign_ack(its_assigned_client);
            break;
        }

        case protocol::id_e::ROUTING_INFO_ID:
            if (!configuration_->is_security_enabled() || is_from_routing) {
                on_routing_info(_data, _size);
            } else {
                VSOMEIP_WARNING_P << "Security: Client 0x" << hex4(get_client())
                                  << " received an routing info from a client which isn't the routing manager: Skip message!";
            }
            break;

        case protocol::id_e::PING_ID: {
            protocol::ping_command its_command;
            its_command.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {
                send_pong();
                VSOMEIP_TRACE << "PING(" << hex4(get_client()) << ")";
            } else {
                VSOMEIP_ERROR_P << "Ping command deserialization failed (" << static_cast<int>(its_error) << ")";
            }
            break;
        }

        case protocol::id_e::PONG_ID: {
            protocol::pong_command its_command;
            its_command.deserialize(its_buffer, its_error);

            if (its_error == protocol::error_e::ERROR_OK) {
                on_pong(its_client);
                VSOMEIP_TRACE << "PONG(" << hex4(get_client()) << ")";
            } else {
                VSOMEIP_ERROR_P << "Pong command deserialization failed (" << static_cast<int>(its_error) << ")";
            }

            break;
        }

        case protocol::id_e::SUBSCRIBE_ID: {
            protocol::subscribe_command its_subscribe_command;
            its_subscribe_command.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {

                its_service = its_subscribe_command.get_service();
                its_instance = its_subscribe_command.get_instance();
                its_eventgroup = its_subscribe_command.get_eventgroup();
                its_major = its_subscribe_command.get_major();
                its_event = its_subscribe_command.get_event();
                its_pending_id = its_subscribe_command.get_pending_id();
                auto its_filter = its_subscribe_command.get_filter();

                if (its_pending_id != PENDING_SUBSCRIPTION_ID) {
                    auto its_info = find_service(its_service, its_instance);
                    if (its_info) {
                        // Remote subscriber: Notify routing manager initially + count subscribes
                        auto self = shared_from_this();
                        host_->on_subscription(
                                its_service, its_instance, its_eventgroup, its_client, _sec_client, get_env(its_client), true,
                                [this, self, its_client, its_service, its_instance, its_eventgroup, its_event, its_filter, its_pending_id,
                                 its_major](const bool _subscription_accepted) {
                                    std::uint32_t its_count(0);
                                    if (_subscription_accepted) {
                                        insert_subscription(its_service, its_instance, its_eventgroup, its_event, its_filter,
                                                            VSOMEIP_ROUTING_CLIENT);
                                        // NOTE: order matters, send ACK _after_ inserting the subscription
                                        send_subscribe_ack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                           its_pending_id);
                                        notify_remote_initially(its_service, its_instance, its_eventgroup);

                                        its_count = get_remote_subscriber_count(its_service, its_instance, its_eventgroup, true);
                                    } else {
                                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                            its_pending_id);
                                    }
                                    VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                                 << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event) << ":"
                                                 << static_cast<uint16_t>(its_major) << "] "
                                                 << (_subscription_accepted ? "accepted." : "not accepted.")
                                                 << " id=" << hex4(its_pending_id) << std::dec << " subscribers=" << its_count;
                                });
                    } else {
                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event, its_pending_id);
                    }
                } else { // local subscription
                    if (!is_from_routing) {
                        if (its_event == ANY_EVENT) {
                            if (!is_subscribe_to_any_event_allowed(_sec_client, its_client, its_service, its_instance, its_eventgroup)) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(its_client)
                                                << " : routing_manager_client::on_message: isn't allowed to subscribe to"
                                                << " service/instance/event " << hex4(its_service) << "/" << hex4(its_instance)
                                                << "/ANY_EVENT which violates the security policy ~> Skip subscribe!";
                                return;
                            }
                        } else {
                            if (VSOMEIP_SEC_OK
                                != configuration_->get_security()->is_client_allowed_to_access_member(_sec_client, its_service,
                                                                                                      its_instance, its_event)) {
                                VSOMEIP_WARNING
                                        << "vSomeIP Security: Client 0x" << hex4(its_client) << " : routing_manager_client::on_message: "
                                        << " subscribes to service/instance/event " << hex4(its_service) << "/" << hex4(its_instance) << "/"
                                        << its_event << " which violates the security policy ~> Skip subscribe!";
                                return;
                            }
                        }
                    } else {
                        if (!configuration_->is_remote_access_allowed()) {
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(its_client)
                                            << " : routing_manager_client::on_message: " << hex4(its_client)
                                            << "Routing manager with client ID 0x" << hex4(its_client)
                                            << " isn't allowed to subscribe to service/instance/event " << hex4(its_service) << "/"
                                            << hex4(its_instance) << "/" << its_event << " respectively to client 0x" << hex4(get_client())
                                            << " ~> Skip Subscribe!";
                            return;
                        }
                    }

                    auto self = shared_from_this();
                    auto its_env = get_env(its_client);

                    auto its_info = find_service(its_service, its_instance);
                    if (its_info) {
                        host_->on_subscription(
                                its_service, its_instance, its_eventgroup, its_client, _sec_client, its_env, true,
                                [this, self, its_client, its_filter, its_pending_id, its_env, its_service, its_instance, its_eventgroup,
                                 its_event, its_major](const bool _subscription_accepted) {
                                    if (!_subscription_accepted) {
                                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                            PENDING_SUBSCRIPTION_ID);
                                    } else {

                                        insert_subscription(its_service, its_instance, its_eventgroup, its_event, its_filter, its_client);
                                        // NOTE: order matters, send ACK _after_ inserting the subscription
                                        send_subscribe_ack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                           PENDING_SUBSCRIPTION_ID);
                                        notify_one_current_value(its_client, its_service, its_instance, its_eventgroup, its_event);
                                    }

                                    VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                                 << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event) << ":"
                                                 << std::dec << static_cast<uint16_t>(its_major) << "] " << std::boolalpha
                                                 << (its_pending_id != PENDING_SUBSCRIPTION_ID)
                                                 << (_subscription_accepted ? " accepted" : "not accepted");
                                });
                    } else {
                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event, PENDING_SUBSCRIPTION_ID);
                    }
                }
                VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                             << hex4(its_eventgroup) << ":" << hex4(its_event) << ":" << std::dec << static_cast<uint16_t>(its_major)
                             << "] " << std::boolalpha << (its_pending_id != PENDING_SUBSCRIPTION_ID);
            } else {
                VSOMEIP_ERROR_P << "Subscribe command deserialization failed (" << static_cast<int>(its_error) << ")";
            }
            break;
        }

        case protocol::id_e::UNSUBSCRIBE_ID: {
            protocol::unsubscribe_command its_unsubscribe;
            its_unsubscribe.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {

                its_client = its_unsubscribe.get_client();
                its_service = its_unsubscribe.get_service();
                its_instance = its_unsubscribe.get_instance();
                its_eventgroup = its_unsubscribe.get_eventgroup();
                its_event = its_unsubscribe.get_event();
                its_pending_id = its_unsubscribe.get_pending_id();

                host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, _sec_client, get_env(its_client), false,
                                       [](const bool _subscription_accepted) { (void)_subscription_accepted; });
                if (its_pending_id == PENDING_SUBSCRIPTION_ID) {
                    // Local subscriber: withdraw subscription
                    routing_manager_base::unsubscribe(its_client, _sec_client, its_service, its_instance, its_eventgroup, its_event);
                } else {
                    // Remote subscriber: withdraw subscription only if no more remote subscriber
                    // exists
                    its_remote_subscriber_count = get_remote_subscriber_count(its_service, its_instance, its_eventgroup, false);
                    if (!its_remote_subscriber_count) {
                        routing_manager_base::unsubscribe(VSOMEIP_ROUTING_CLIENT, nullptr, its_service, its_instance, its_eventgroup,
                                                          its_event);
                    }
                    send_unsubscribe_ack(its_service, its_instance, its_eventgroup, its_pending_id);
                }
                VSOMEIP_INFO << "UNSUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                             << hex4(its_eventgroup) << "." << hex4(its_event) << "] id=" << hex4(its_pending_id)
                             << " subscribers=" << std::dec << its_remote_subscriber_count;
            } else
                VSOMEIP_ERROR_P << "Unsubscribe command deserialization failed (" << static_cast<int>(its_error) << ")";
            break;
        }

        case protocol::id_e::EXPIRE_ID: {
            protocol::expire_command its_expire;
            its_expire.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {

                its_client = its_expire.get_client();
                its_service = its_expire.get_service();
                its_instance = its_expire.get_instance();
                its_eventgroup = its_expire.get_eventgroup();
                its_event = its_expire.get_event();
                its_pending_id = its_expire.get_pending_id();

                host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, _sec_client, get_env(its_client), false,
                                       [](const bool _subscription_accepted) { (void)_subscription_accepted; });
                if (its_pending_id == PENDING_SUBSCRIPTION_ID) {
                    // Local subscriber: withdraw subscription
                    routing_manager_base::unsubscribe(its_client, _sec_client, its_service, its_instance, its_eventgroup, its_event);
                } else {
                    // Remote subscriber: withdraw subscription only if no more remote subscriber
                    // exists
                    its_remote_subscriber_count = get_remote_subscriber_count(its_service, its_instance, its_eventgroup, false);
                    if (!its_remote_subscriber_count) {
                        routing_manager_base::unsubscribe(VSOMEIP_ROUTING_CLIENT, nullptr, its_service, its_instance, its_eventgroup,
                                                          its_event);
                    }
                }
                VSOMEIP_INFO << "EXPIRED SUBSCRIPTION(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance)
                             << "." << hex4(its_eventgroup) << "." << hex4(its_event) << "] " << std::boolalpha
                             << (its_pending_id != PENDING_SUBSCRIPTION_ID) << " " << std::dec << its_remote_subscriber_count;
            } else
                VSOMEIP_ERROR_P << "Expire deserialization failed (" << static_cast<int>(its_error) << ")";
            break;
        }

        case protocol::id_e::SUBSCRIBE_NACK_ID: {
            protocol::subscribe_nack_command its_subscribe_nack;
            its_subscribe_nack.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {

                its_service = its_subscribe_nack.get_service();
                its_instance = its_subscribe_nack.get_instance();
                its_eventgroup = its_subscribe_nack.get_eventgroup();
                its_subscriber = its_subscribe_nack.get_subscriber();
                its_event = its_subscribe_nack.get_event();

                on_subscribe_nack(its_subscriber, its_service, its_instance, its_eventgroup, its_event);
                VSOMEIP_INFO << "SUBSCRIBE NACK(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                             << hex4(its_eventgroup) << "." << hex4(its_event) << "]";
            } else
                VSOMEIP_ERROR_P << "Subscribe nack command deserialization failed (" << static_cast<int>(its_error) << ")";
            break;
        }

        case protocol::id_e::SUBSCRIBE_ACK_ID: {
            protocol::subscribe_ack_command its_subscribe_ack;
            its_subscribe_ack.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {

                its_service = its_subscribe_ack.get_service();
                its_instance = its_subscribe_ack.get_instance();
                its_eventgroup = its_subscribe_ack.get_eventgroup();
                its_subscriber = its_subscribe_ack.get_subscriber();
                its_event = its_subscribe_ack.get_event();

                on_subscribe_ack(its_subscriber, its_service, its_instance, its_eventgroup, its_event);
                VSOMEIP_INFO << "SUBSCRIBE ACK(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                             << hex4(its_eventgroup) << "." << hex4(its_event) << "]";
            } else
                VSOMEIP_ERROR_P << "Subscribe ack command deserialization failed (" << static_cast<int>(its_error) << ")";
            break;
        }

        case protocol::id_e::OFFERED_SERVICES_RESPONSE_ID: {
            protocol::offered_services_response_command its_response;
            its_response.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {
                if (!configuration_->is_security_enabled() || is_from_routing) {
                    on_offered_services_info(its_response);
                } else {
                    VSOMEIP_WARNING << std::hex << "Security: Client 0x" << get_client()
                                    << " received an offered services info from a client which isn't the routing manager"
                                    << " : Skip message!";
                }
            } else
                VSOMEIP_ERROR_P << "Offered services response command deserialization failed (" << static_cast<int>(its_error) << ")";
            break;
        }
        case protocol::id_e::RESEND_PROVIDED_EVENTS_ID: {
            protocol::resend_provided_events_command its_command;
            its_command.deserialize(its_buffer, its_error);
            if (its_error == protocol::error_e::ERROR_OK) {

                resend_provided_event_registrations();
                send_resend_provided_event_response(its_command.get_remote_offer_id());

                VSOMEIP_INFO << "RESEND_PROVIDED_EVENTS(" << hex4(its_command.get_client()) << ")";
            } else
                VSOMEIP_ERROR_P << "Resend provided events command deserialization failed (" << static_cast<int>(its_error) << ")";
            break;
        }
        case protocol::id_e::SUSPEND_ID: {
            on_suspend(); // cleanup remote subscribers
            break;
        }
#ifndef VSOMEIP_DISABLE_SECURITY
        case protocol::id_e::UPDATE_SECURITY_POLICY_INT_ID:
            is_internal_policy_update = true;
            [[gnu::fallthrough]];
        case protocol::id_e::UPDATE_SECURITY_POLICY_ID: {
            if (!configuration_->is_security_enabled() || is_from_routing) {
                protocol::update_security_policy_command its_command(is_internal_policy_update);
                its_command.deserialize(its_buffer, its_error);
                if (its_error == protocol::error_e::ERROR_OK) {
                    auto its_policy = its_command.get_policy();
                    uid_t its_uid;
                    gid_t its_gid;
                    if (its_policy->get_uid_gid(its_uid, its_gid)) {
                        if (is_internal_policy_update || its_policy_manager->is_policy_update_allowed(its_uid, its_policy)) {
                            its_policy_manager->update_security_policy(its_uid, its_gid, its_policy);
                            send_update_security_policy_response(its_command.get_update_id());
                        }
                    } else {
                        VSOMEIP_ERROR << "vSomeIP Security: Policy has no valid uid/gid!";
                    }
                } else {
                    VSOMEIP_ERROR << "vSomeIP Security: Policy deserialization failed!";
                }
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client()) << " : routing_manager_client::on_message: "
                                << " received a security policy update from a client which isn't the routing manager"
                                << " : Skip message!";
            }
            break;
        }

        case protocol::id_e::REMOVE_SECURITY_POLICY_ID: {
            if (!configuration_->is_security_enabled() || is_from_routing) {
                protocol::remove_security_policy_command its_command;
                its_command.deserialize(its_buffer, its_error);
                if (its_error == protocol::error_e::ERROR_OK) {

                    uid_t its_uid(its_command.get_uid());
                    gid_t its_gid(its_command.get_gid());

                    if (its_policy_manager->is_policy_removal_allowed(its_uid)) {
                        its_policy_manager->remove_security_policy(its_uid, its_gid);
                        send_remove_security_policy_response(its_command.get_update_id());
                    }
                } else
                    VSOMEIP_ERROR_P << "Remove security policy command deserialization failed (" << static_cast<int>(its_error) << ")";
            } else
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client()) << " : routing_manager_client::on_message: "
                                << "received a security policy removal from a client which isn't the routing manager"
                                << " : Skip message!";
            break;
        }

        case protocol::id_e::DISTRIBUTE_SECURITY_POLICIES_ID: {
            if (!configuration_->is_security_enabled() || is_from_routing) {
                protocol::distribute_security_policies_command its_command;
                its_command.deserialize(its_buffer, its_error);
                if (its_error == protocol::error_e::ERROR_OK) {
                    for (auto p : its_command.get_policies()) {
                        uid_t its_uid;
                        gid_t its_gid;
                        p->get_uid_gid(its_uid, its_gid);
                        if (its_policy_manager->is_policy_update_allowed(its_uid, p))
                            its_policy_manager->update_security_policy(its_uid, its_gid, p);
                    }
                } else
                    VSOMEIP_ERROR_P << "Distribute security policies command deserialization failed (" << static_cast<int>(its_error)
                                    << ")";
            } else
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client()) << " : routing_manager_client::on_message: "
                                << " received a security policy distribution command from a client which isn't the routing manager"
                                << " : Skip message!";
            break;
        }

        case protocol::id_e::UPDATE_SECURITY_CREDENTIALS_ID: {
            if (!configuration_->is_security_enabled() || is_from_routing) {
                protocol::update_security_credentials_command its_command;
                its_command.deserialize(its_buffer, its_error);
                if (its_error == protocol::error_e::ERROR_OK) {
                    on_update_security_credentials(its_command);
                } else
                    VSOMEIP_ERROR_P << "Update security credentials command deserialization failed (" << static_cast<int>(its_error) << ")";
            } else
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << hex4(get_client()) << " : routing_manager_client::on_message: "
                                << "received a security credential update from a client which isn't the routing manager"
                                << " : Skip message!";

            break;
        }
#endif // !VSOMEIP_DISABLE_SECURITY
        case protocol::id_e::CONFIG_ID: {
            protocol::config_command its_command;
            protocol::error_e its_command_error;
            its_command.deserialize(its_buffer, its_command_error);
            if (its_command_error != protocol::error_e::ERROR_OK) {
                VSOMEIP_ERROR_P << "Config command deserialization failed (" << std::dec << static_cast<int>(its_command_error) << ")";
                break;
            }
            if (its_command.contains("hostname")) {
                add_known_client(its_command.get_client(), its_command.at("hostname"));
            }
            break;
        }
        default:
            break;
        }
    } else
        VSOMEIP_ERROR_P << "Dummy command deserialization failed (" << static_cast<int>(its_error) << ")";
}

void routing_manager_client::on_routing_info(const byte_t* _data, uint32_t _size) {
    auto its_policy_manager = configuration_->get_policy_manager();
    if (!its_policy_manager)
        return;

    std::vector<byte_t> its_buffer(_data, _data + _size);
    protocol::error_e its_error;

    protocol::routing_info_command its_command;
    its_command.deserialize(its_buffer, its_error);
    if (its_error != protocol::error_e::ERROR_OK) {
        VSOMEIP_ERROR_P << "Deserializing routing info command failed (" << static_cast<int>(its_error) << ")";
        return;
    }

    for (const auto& e : its_command.get_entries()) {
        auto its_client = e.get_client();
        switch (e.get_type()) {
        case protocol::routing_info_entry_type_e::RIE_ADD_CLIENT: {
            if (its_client == get_client()) {
#if defined(__linux__) || defined(__QNX__)
                auto const sec_client = get_sec_client();
                if (!its_policy_manager->check_credentials(get_client(), &sec_client)) {
                    VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(get_client())
                                  << " : routing_manager_client::on_routing_info: RIE_ADD_CLIENT: isn't allowed"
                                  << " to use the client endpoint due to credential check failed!";
                    deregister_application();
                    host_->on_state(state_type_e::ST_DEREGISTERED);
                    return;
                }
#endif

                // when changing the state we need to ensure that the debounce timer is not dispatching + altering the request set
                if (std::unique_lock its_lock(requests_to_debounce_mutex_); state_machine_->registered()) {
                    if (send_registered_ack()) {
                        VSOMEIP_INFO << "Application/Client " << hex4(get_client()) << " (" << host_->get_name() << ") is registered.";

                        start_keepalive();
                        if (!send_pending_commands()) {
                            VSOMEIP_WARNING_P << "Application/Client 0x" << hex4(get_client()) << " (" << host_->get_name()
                                              << ") Could not send pending offers";
                        }
                        its_lock.unlock();
                        // inform host about its own registration state changes
                        host_->on_state(state_type_e::ST_REGISTERED);
                    } else {
                        VSOMEIP_ERROR_P << "Failure registering client 0x" << hex4(get_client()) << " (" << host_->get_name() << ")";
                        // since we are not communicating REGISTERED to the host, neither should the state machine remain in this state
                        state_machine_->deregistered();
                    }
                } else {
                    VSOMEIP_INFO_P << "Application/client 0x" << hex4(get_client()) << " (" << host_->get_name()
                                   << ") Could not enter the registered state due to state: " << state_machine_->state();
                }
            }
            break;
        }

        case protocol::routing_info_entry_type_e::RIE_DELETE_CLIENT: {
            if (its_client == get_client()) {
                its_policy_manager->remove_client_to_sec_client_mapping(its_client);
                VSOMEIP_INFO_P << "Application/Client 0x" << hex4(get_client()) << " (" << host_->get_name() << ") is deregistered.";

                // inform host about its own registration state changes
                host_->on_state(state_type_e::ST_DEREGISTERED);
                state_machine_->deregistered();
            }
            break;
        }

        case protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE: {
            add_known_client(its_client, "");
            boost::asio::ip::address its_address = e.get_address();
            port_t its_port = e.get_port();
            if (!its_address.is_unspecified()) {
                // remove client (and endpoints!) at same address/port
                // as address/port are unique and that definitely means the client no longer exists
                if (client_t old_client = get_guest_by_address(its_address, its_port);
                    old_client != VSOMEIP_CLIENT_UNSET && old_client != its_client) {
                    VSOMEIP_INFO_P << "Old client 0x" << hex4(old_client) << " removed due to new client 0x" << hex4(its_client) << " @ "
                                   << std::dec << its_address.to_string() + ":" << its_port;

                    // also removes guest
                    remove_local(old_client, true);
                }

                add_guest(its_client, its_address, its_port);
            }

            for (const auto& s : e.get_services()) {

                const auto its_service(s.service_);
                const auto its_instance(s.instance_);
                const auto its_major(s.major_);
                const auto its_minor(s.minor_);

                {
                    std::scoped_lock its_lock(local_services_mutex_);
                    local_services_[its_service][its_instance] = std::make_tuple(its_major, its_minor, its_client);
                }
                { send_pending_subscriptions(its_service, its_instance, its_major); }
                host_->on_availability(its_service, its_instance, availability_state_e::AS_AVAILABLE, its_major, its_minor);
                VSOMEIP_INFO << "ON_AVAILABLE(" << hex4(get_client()) << "): [" << hex4(its_service) << "." << hex4(its_instance) << ":"
                             << std::dec << int(its_major) << "." << std::dec << its_minor << "]";
            }
            break;
        }

        case protocol::routing_info_entry_type_e::RIE_DELETE_SERVICE_INSTANCE: {
            for (const auto& s : e.get_services()) {
                const auto its_service(s.service_);
                const auto its_instance(s.instance_);
                const auto its_major(s.major_);
                const auto its_minor(s.minor_);

                {
                    std::scoped_lock its_lock(local_services_mutex_);
                    auto found_service = local_services_.find(its_service);
                    if (found_service != local_services_.end()) {
                        found_service->second.erase(its_instance);
                        // move previously offering client to history
                        local_services_history_[its_service][its_instance].insert(its_client);
                        if (found_service->second.size() == 0) {
                            local_services_.erase(its_service);
                        }
                    }
                }
                on_stop_offer_service(its_service, its_instance, its_major, its_minor);
                host_->on_availability(its_service, its_instance, availability_state_e::AS_UNAVAILABLE, its_major, its_minor);
                VSOMEIP_INFO << "ON_UNAVAILABLE(" << hex4(get_client()) << "): [" << hex4(its_service) << "." << hex4(its_instance) << ":"
                             << std::dec << static_cast<int>(its_major) << "." << std::dec << its_minor << "]";
            }
            break;
        }

        default:
            VSOMEIP_ERROR_P << "Unknown routing info entry type (" << static_cast<int>(e.get_type()) << ")";
            break;
        }
    }
}

void routing_manager_client::on_offered_services_info(protocol::offered_services_response_command& _command) {

    std::vector<std::pair<service_t, instance_t>> its_offered_services_info;

    for (const auto& s : _command.get_services())
        its_offered_services_info.push_back(std::make_pair(s.service_, s.instance_));

    host_->on_offered_services_info(its_offered_services_info);
}

void routing_manager_client::reconnect(const std::map<client_t, std::string>& _clients) {
    auto its_policy_manager = configuration_->get_policy_manager();
    if (!its_policy_manager)
        return;

    // inform host about its own registration state changes
    host_->on_state(state_type_e::ST_DEREGISTERED);

    // Clear boardnet subscriptions
    clear_remote_subscriptions();

    // Remove all local connections/endpoints
    for (const auto& c : _clients) {
        if (c.first != VSOMEIP_ROUTING_CLIENT) {
            remove_local(c.first, true);
        }
    }

    VSOMEIP_INFO_P << "Application/Client 0x" << hex4(get_client()) << ": Reconnecting to routing manager.";

    {
        std::scoped_lock lock(receiver_mutex_);
        if (!configuration_->is_local_routing()) {
            // tcp needs to claim a port to ensure that the sender is not
            // blocking a wrong port
            if (receiver_) {
                // stop accepting connections + stop existing connections,
                // but don't close the socket to not free the claimed port.
                receiver_->halt();
            } else {
                // TODO this might end up looping forever, if the network is assumed
                // to be down. How to break out of this loop in case of "stop" ?
                receiver_ = ep_mgr_->create_local_server(shared_from_this());
            }
        } else {
            // But the actual reconnect
            // can lead to a change in the client id, for which reason the
            // uds receiver should only be set up, once the "new" client id
            // is known.
            if (receiver_) {
                receiver_->stop();
                receiver_ = nullptr;
            }
        }
    }
    state_machine_->deregistered();
}

bool routing_manager_client::is_local_client(client_t _client) const {
    return ep_mgr_->find_local_server_endpoint(_client) != nullptr;
}

void routing_manager_client::register_application() {

    if (!receiver_) {
        VSOMEIP_ERROR_P << "Cannot register. Local server endpoint does not exist.";
        return;
    }

    auto its_configuration(get_configuration());
    if (its_configuration->is_local_routing()) {
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " Registering to routing manager @ " << its_configuration->get_network()
                       << "-0";
    } else {
        auto its_routing_address(its_configuration->get_routing_host_address());
        auto its_routing_port(its_configuration->get_routing_host_port());
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " Registering to routing manager @ " << its_routing_address.to_string()
                       << ":" << std::dec << its_routing_port;
    }

    protocol::register_application_command its_command;
    its_command.set_client(get_client());
    its_command.set_port(receiver_->get_local_port());

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            if (!state_machine_->start_registration()) {
                VSOMEIP_INFO_P << "Interrupting the application registration for Client 0x" << hex4(get_client());
                return;
            }
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else {
        VSOMEIP_ERROR_P << "Register application command serialization failed(" << static_cast<int>(its_error) << ")";
    }
}

void routing_manager_client::deregister_application() {

    protocol::deregister_application_command its_command;
    its_command.set_client(get_client());

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        if (auto state = state_machine_->state();
            is_value(state).any_of(routing_client_state_e::ST_REGISTERED, routing_client_state_e::ST_DEREGISTERING)) {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
            }
        } else {
            VSOMEIP_WARNING_P << "Deregister command for Client 0x" << hex4(get_client())
                              << " not dispatched due to unexpected state: " << state;
        }
    } else
        VSOMEIP_ERROR_P << "Deregister application command serialization failed(" << static_cast<int>(its_error) << ")";
}

void routing_manager_client::send_pong() const {

    protocol::pong_command its_command;
    its_command.set_client(get_client());

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        if (auto state = state_machine_->state();
            is_value(state).any_of(routing_client_state_e::ST_REGISTERED, routing_client_state_e::ST_REGISTERING,
                                   routing_client_state_e::ST_ASSIGNED, routing_client_state_e::ST_ASSIGNING)) {
            std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
            }
        } else {
            VSOMEIP_WARNING_P << "Pong command for Client 0x" << hex4(get_client()) << get_client()
                              << " not dispatched due to unexpected state: " << state;
        }
    } else
        VSOMEIP_ERROR_P << "Pong command serialization failed(" << static_cast<int>(its_error) << ")";
}

bool routing_manager_client::send_request_services(const std::set<protocol::service>& _requests) {
    if (!_requests.size()) {
        return true;
    }

    protocol::request_service_command its_command;
    its_command.set_client(get_client());
    its_command.set_services(_requests);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);
    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_ && sender_->send(&its_buffer[0], uint32_t(its_buffer.size()))) {
            return true;
        }
        VSOMEIP_ERROR_P << "Failed to send requested services";
    } else {
        VSOMEIP_ERROR_P << "Request service serialization failed (" << static_cast<int>(its_error) << ")";
    }

    return false;
}

void routing_manager_client::send_release_service(client_t _client, service_t _service, instance_t _instance) {

    (void)_client;

    protocol::release_service_command its_command;
    its_command.set_client(get_client());
    its_command.set_service(_service);
    its_command.set_instance(_instance);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);
    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    }
}

bool routing_manager_client::send_pending_event_registrations(client_t _client) {

    protocol::register_events_command its_command;
    its_command.set_client(_client);
    bool sent{true};

    std::scoped_lock its_lock(pending_event_registrations_mutex_);
    auto it = pending_event_registrations_.begin();
    while (it != pending_event_registrations_.end()) {
        for (; it != pending_event_registrations_.end(); it++) {
            protocol::register_event reg(it->service_, it->instance_, it->notifier_, it->type_, it->is_provided_, it->reliability_,
                                         it->is_cyclic_, static_cast<uint16_t>(it->eventgroups_.size()), it->eventgroups_);
            if (!its_command.add_registration(reg)) {
                break;
            }
        }

        std::vector<byte_t> its_buffer;
        protocol::error_e its_error;
        its_command.serialize(its_buffer, its_error);

        if (its_error == protocol::error_e::ERROR_OK) {
            std::scoped_lock its_sender_lock{sender_mutex_};
            if (!(sender_ && sender_->send(&its_buffer[0], uint32_t(its_buffer.size())))) {
                VSOMEIP_ERROR_P << "Failed to send pending registration to host";
                sent = false;
            }
        } else {
            VSOMEIP_ERROR_P << "Register event command serialization failed (" << static_cast<int>(its_error) << ")";
            sent = false;
        }

        if (!sent) {
            break;
        }
    }

    return sent;
}

void routing_manager_client::send_register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                                 const std::set<eventgroup_t>& _eventgroups, const event_type_e _type,
                                                 reliability_type_e _reliability, bool _is_provided, bool _is_cyclic) {

    (void)_client;

    protocol::register_events_command its_command;
    its_command.set_client(get_client());

    protocol::register_event reg(_service, _instance, _notifier, _type, _is_provided, _reliability, _is_cyclic,
                                 static_cast<uint16_t>(_eventgroups.size()), _eventgroups);

    if (!its_command.add_registration(reg)) {
        VSOMEIP_ERROR_P << "Register event command is too long.";
    }

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }

        if (_is_provided) {
            VSOMEIP_INFO << "REGISTER EVENT(" << hex4(get_client()) << "): [" << hex4(_service) << "." << hex4(_instance) << "."
                         << hex4(_notifier) << ":is_provider=" << std::boolalpha << _is_provided << "]";
        }
    } else
        VSOMEIP_ERROR_P << "Register event command serialization failed (" << static_cast<int>(its_error) << ")";
}

void routing_manager_client::on_subscribe_ack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              event_t _event) {
    (void)_client;

    if (_event == ANY_EVENT) {
        auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
        if (its_eventgroup) {
            for (const auto& its_event : its_eventgroup->get_events()) {
                host_->on_subscription_status(_service, _instance, _eventgroup, its_event->get_event(), 0x0 /*OK*/);
            }
        }
    } else {
        host_->on_subscription_status(_service, _instance, _eventgroup, _event, 0x0 /*OK*/);
    }
}

void routing_manager_client::on_subscribe_nack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                               event_t _event) {
    (void)_client;
    if (_event == ANY_EVENT) {
        auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
        if (its_eventgroup) {
            for (const auto& its_event : its_eventgroup->get_events()) {
                host_->on_subscription_status(_service, _instance, _eventgroup, its_event->get_event(), 0x7 /*Rejected*/);
            }
        }
    } else {
        host_->on_subscription_status(_service, _instance, _eventgroup, _event, 0x7 /*Rejected*/);
    }
}

void routing_manager_client::cache_event_payload(const std::shared_ptr<message>& _message) {
    const service_t its_service(_message->get_service());
    const instance_t its_instance(_message->get_instance());
    const method_t its_method(_message->get_method());
    std::shared_ptr<event> its_event = find_event(its_service, its_instance, its_method);
    if (its_event) {
        if (its_event->is_field()) {
            its_event->prepare_update_payload(_message->get_payload(), true);
            its_event->update_payload();
        }
    } else {
        // we received a event which was not yet requested
        std::set<eventgroup_t> its_eventgroups;
        // create a placeholder field until someone requests this event with
        // full information like eventgroup, field or not etc.
        routing_manager_base::register_event(host_->get_client(), its_service, its_instance, its_method, its_eventgroups,
                                             event_type_e::ET_UNKNOWN, reliability_type_e::RT_UNKNOWN, std::chrono::milliseconds::zero(),
                                             false, true, nullptr, false, false, true);
        std::shared_ptr<event> its_event_inner = find_event(its_service, its_instance, its_method);
        if (its_event_inner) {
            its_event_inner->prepare_update_payload(_message->get_payload(), true);
            its_event_inner->update_payload();
        }
    }
}

void routing_manager_client::on_stop_offer_service(service_t _service, instance_t _instance, major_version_t _major,
                                                   minor_version_t _minor) {
    (void)_major;
    (void)_minor;
    std::map<event_t, std::shared_ptr<event>> events;
    {
        std::scoped_lock its_lock{events_mutex_};
        const auto search = events_.find(service_instance_t{_service, _instance});

        if (search != events_.end()) {
            for (const auto& [event_id, event_ptr] : search->second) {
                events[event_id] = event_ptr;
            }
        }
    }
    for (auto& e : events) {
        if (e.second->is_set()) {
            VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << _instance << "." << e.first << "]";
        }
        e.second->unset_payload();
    }
}

bool routing_manager_client::send_pending_commands() {
    {
        std::scoped_lock its_lock(pending_offers_mutex_);
        for (auto& po : pending_offers_) {
            if (!send_offer_service(get_client(), po.service_, po.instance_, po.major_, po.minor_)) {
                return false;
            }
        }
    }

    {
        std::scoped_lock its_lock(requests_mutex_);
        return send_pending_event_registrations(get_client()) && send_request_services(requests_);
    }
}

void routing_manager_client::init_receiver([[maybe_unused]] std::unique_lock<std::mutex> const& _receive_lock) {
    if (!receiver_) {
        receiver_ = ep_mgr_->create_local_server(shared_from_this());
    } else {
        std::uint16_t its_port = receiver_->get_local_port();
        if (its_port != ILLEGAL_PORT)
            VSOMEIP_INFO_P << "Reusing local server endpoint @" << its_port << " endpoint: " << receiver_;
    }
}

void routing_manager_client::notify_remote_initially(service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        auto service_info = find_service(_service, _instance);
        for (const auto& e : its_eventgroup->get_events()) {
            if (e->is_field() && e->is_set()) {
                std::shared_ptr<message> its_notification = runtime::get()->create_notification();
                its_notification->set_service(_service);
                its_notification->set_instance(_instance);
                its_notification->set_method(e->get_event());
                its_notification->set_payload(e->get_payload());
                if (service_info) {
                    its_notification->set_interface_version(service_info->get_major());
                }

                std::shared_ptr<serializer> its_serializer(get_serializer());
                if (its_serializer->serialize(its_notification.get())) {
                    {
                        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
                        if (sender_) {
                            send_local(sender_, VSOMEIP_ROUTING_CLIENT, its_serializer->get_data(), its_serializer->get_size(), _instance,
                                       false, protocol::id_e::NOTIFY_ID, 0);
                        } else {
                            VSOMEIP_ERROR_P << "Failed due to a missing sender";
                        }
                    }
                    its_serializer->reset();
                    put_serializer(its_serializer);
                } else {
                    VSOMEIP_ERROR_P << "Failed to serialize message. Check message size!";
                }
            }
        }
    }
}

uint32_t routing_manager_client::get_remote_subscriber_count(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                             bool _increment) {
    std::scoped_lock its_lock(remote_subscriber_count_mutex_);
    uint32_t count(0);
    bool found(false);
    auto found_service = remote_subscriber_count_.find(_service);
    if (found_service != remote_subscriber_count_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_group = found_instance->second.find(_eventgroup);
            if (found_group != found_instance->second.end()) {
                found = true;
                if (_increment) {
                    found_group->second = found_group->second + 1;
                } else {
                    if (found_group->second > 0) {
                        found_group->second = found_group->second - 1;
                    }
                }
                count = found_group->second;
            }
        }
    }
    if (!found) {
        if (_increment) {
            remote_subscriber_count_[_service][_instance][_eventgroup] = 1;
            count = 1;
        }
    }
    return count;
}

void routing_manager_client::clear_remote_subscriber_count(service_t _service, instance_t _instance) {
    std::scoped_lock its_lock(remote_subscriber_count_mutex_);
    auto found_service = remote_subscriber_count_.find(_service);
    if (found_service != remote_subscriber_count_.end()) {
        if (found_service->second.erase(_instance)) {
            if (!found_service->second.size()) {
                remote_subscriber_count_.erase(found_service);
            }
        }
    }
}

bool routing_manager_client::send_registered_ack() {

    protocol::registered_ack_command its_command;
    its_command.set_client(get_client());

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {

        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_ && sender_->send(&its_buffer[0], uint32_t(its_buffer.size()))) {
            return true;
        }
        VSOMEIP_ERROR_P << "Failed sending registered ack";
    } else {
        VSOMEIP_ERROR_P << "Registered ack command serialization failed (" << static_cast<int>(its_error) << ")";
    }

    return false;
}

bool routing_manager_client::is_client_known(client_t _client) {

    std::scoped_lock its_lock(known_clients_mutex_);
    return (known_clients_.find(_client) != known_clients_.end());
}

bool routing_manager_client::create_placeholder_event_and_subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                                    event_t _notifier,
                                                                    const std::shared_ptr<debounce_filter_impl_t>& _filter,
                                                                    client_t _client) {

    std::scoped_lock its_lock(stop_mutex_);

    bool is_inserted(false);

    if (find_service(_service, _instance)) {
        // We received an event for an existing service which was not yet
        // requested/offered. Create a placeholder field until someone
        // requests/offers this event with full information like eventgroup,
        // field/event, etc.
        std::set<eventgroup_t> its_eventgroups({_eventgroup});
        // routing_manager_client: Always register with own client id and shadow = false
        routing_manager_base::register_event(host_->get_client(), _service, _instance, _notifier, its_eventgroups, event_type_e::ET_UNKNOWN,
                                             reliability_type_e::RT_UNKNOWN, std::chrono::milliseconds::zero(), false, true, nullptr, false,
                                             false, true);

        std::shared_ptr<event> its_event = find_event(_service, _instance, _notifier);
        if (its_event) {
            is_inserted = its_event->add_subscriber(_eventgroup, _filter, _client, false);
        }
    }

    return is_inserted;
}

void routing_manager_client::request_debounce_timeout_cbk(boost::system::error_code const& _error) {
    std::scoped_lock its_lock{requests_to_debounce_mutex_, requests_mutex_};
    if (!_error) {
        if (requests_to_debounce_.size()) {
            if (auto state = state_machine_->state(); state == routing_client_state_e::ST_REGISTERED) {
                send_request_services(requests_to_debounce_);
                requests_.insert(requests_to_debounce_.begin(), requests_to_debounce_.end());
                requests_to_debounce_.clear();
            } else {
                request_debounce_timer_.expires_after(
                        std::chrono::milliseconds(configuration_->get_request_debounce_time(host_->get_name())));
                request_debounce_timer_.async_wait(std::bind(&routing_manager_client::request_debounce_timeout_cbk,
                                                             std::dynamic_pointer_cast<routing_manager_client>(shared_from_this()),
                                                             std::placeholders::_1));
                return;
            }
        }
    }
    request_debounce_timer_running_ = false;
}

void routing_manager_client::register_client_error_handler(client_t _client, const std::shared_ptr<local_endpoint>& _endpoint) {

    _endpoint->register_error_handler(std::bind(&routing_manager_client::handle_client_error, this, _client));
}

void routing_manager_client::handle_client_error(client_t _client) {

    if (_client != VSOMEIP_ROUTING_CLIENT) {
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " handles a client error 0x" << hex4(_client) << " not reconnecting";

        // First ensure that the connection is dropped, before enforcing a
        // reconnect from the client. Otherwise a client subscribe might
        // be handled by a partially cleaned-up connection
        std::set<protocol::service> requested_services;
        remove_local(_client, true, get_subscriptions(_client), &requested_services);

        // Request the host these services again.
        if (auto state = state_machine_->state(); state == routing_client_state_e::ST_REGISTERED) {
            send_request_services(requested_services);
        }

    } else {
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " handles a client error 0x" << hex4(_client)
                       << " with host, will reconnect";
        std::map<client_t, std::string> its_known_clients;
        {
            std::scoped_lock its_lock(known_clients_mutex_);
            its_known_clients = known_clients_;
        }
        cancel_keepalive();
        reconnect(its_known_clients);
    }
}

void routing_manager_client::send_get_offered_services_info(client_t _client, offer_type_e _offer_type) {

    protocol::offered_services_request_command its_command;
    its_command.set_client(_client);
    its_command.set_offer_type(_offer_type);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else
        VSOMEIP_ERROR_P << "Offered service request command serialization failed (" << static_cast<int>(its_error) << ")";
}

void routing_manager_client::send_unsubscribe_ack(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                  remote_subscription_id_t _id) {

    protocol::unsubscribe_ack_command its_command;
    its_command.set_client(get_client());
    its_command.set_service(_service);
    its_command.set_instance(_instance);
    its_command.set_eventgroup(_eventgroup);
    its_command.set_pending_id(_id);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else
        VSOMEIP_ERROR_P << "Unsubscribe ack command serialization failed (" << static_cast<int>(its_error) << ")";
}

void routing_manager_client::resend_provided_event_registrations() {
    std::scoped_lock its_lock(pending_event_registrations_mutex_);
    for (const event_data_t& ed : pending_event_registrations_) {
        if (ed.is_provided_) {
            send_register_event(get_client(), ed.service_, ed.instance_, ed.notifier_, ed.eventgroups_, ed.type_, ed.reliability_,
                                ed.is_provided_, ed.is_cyclic_);
        }
    }
}

void routing_manager_client::send_resend_provided_event_response(pending_remote_offer_id_t _id) {

    protocol::resend_provided_events_command its_command;
    its_command.set_client(get_client());
    its_command.set_remote_offer_id(_id);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else
        VSOMEIP_ERROR_P << "Resend provided event command serialization failed (" << static_cast<int>(its_error) << ")";
}

#ifndef VSOMEIP_DISABLE_SECURITY
void routing_manager_client::send_update_security_policy_response(pending_security_update_id_t _update_id) {

    protocol::update_security_policy_response_command its_command;
    its_command.set_client(get_client());
    its_command.set_update_id(_update_id);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else
        VSOMEIP_ERROR_P << "Update security policy response command serialization failed (" << static_cast<int>(its_error) << ")";
}

void routing_manager_client::send_remove_security_policy_response(pending_security_update_id_t _update_id) {

    protocol::remove_security_policy_response_command its_command;
    its_command.set_client(get_client());
    its_command.set_update_id(_update_id);

    std::vector<byte_t> its_buffer;
    protocol::error_e its_error;
    its_command.serialize(its_buffer, its_error);

    if (its_error == protocol::error_e::ERROR_OK) {
        std::scoped_lock<std::recursive_mutex> its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(&its_buffer[0], uint32_t(its_buffer.size()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else
        VSOMEIP_ERROR_P << "Update security policy response command serialization failed (" << static_cast<int>(its_error) << ")";
}

void routing_manager_client::on_update_security_credentials(const protocol::update_security_credentials_command& _command) {

    auto its_policy_manager = configuration_->get_policy_manager();
    if (!its_policy_manager)
        return;

    for (const auto& c : _command.get_credentials()) {
        std::shared_ptr<policy> its_policy(std::make_shared<policy>());
        boost::icl::interval_set<gid_t> its_gid_set;
        uid_t its_uid(c.first);
        gid_t its_gid(c.second);

        its_gid_set.insert(its_gid);

        its_policy->credentials_ += std::make_pair(boost::icl::interval<uid_t>::closed(its_uid, its_uid), its_gid_set);
        its_policy->allow_who_ = true;
        its_policy->allow_what_ = true;

        its_policy_manager->add_security_credentials(its_uid, its_gid, its_policy, get_client());
    }
}
#endif

void routing_manager_client::on_client_assign_ack(const client_t& _client) {

    if (_client == VSOMEIP_CLIENT_UNSET) {
        VSOMEIP_ERROR_P << "(" << host_->get_name() << ":" << hex4(_client) << ") Invalid clientID";
        return;
    }
    // order matters:
    // 0. call host (while unlocked to avoid lock inversion)
    host_->set_client(_client);
#ifdef __linux__
    auto its_policy_manager = configuration_->get_policy_manager();
    if (!its_policy_manager)
        return;

    auto const sec_client = get_sec_client();
    its_policy_manager->store_client_to_sec_client_mapping(_client, &sec_client);
    its_policy_manager->store_sec_client_to_client_mapping(&sec_client, _client);
    // TODO why is there no logic to remove this mapping
    // when there was some problem with the registration?
#endif

    // order matters:
    // 1. lock the receiver mutex,
    // 2. try to transition the state machine
    // this ensures that th receiver init does counter act the potentially
    // interleaving stopping of the receiver within the ::stop method.
    bool is_started{false};
    std::unique_lock its_lock{receiver_mutex_};
    if (!state_machine_->assigned(_client)) {
        VSOMEIP_INFO_P << "Not starting the application registration for Client 0x" << hex4(_client);
        its_lock.unlock();
        host_->set_client(VSOMEIP_CLIENT_UNSET);
        return;
    }
    init_receiver(its_lock);
    {
        if (receiver_) {
            receiver_->set_id(_client);
            receiver_->start();
            VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " (" << host_->get_name()
                           << ") successfully connected to routing  ~> registering..";
            register_application();

            is_started = true;
        }
    }
    if (!is_started) {
        VSOMEIP_WARNING_P << ": (" << host_->get_name() << ":" << hex4(_client) << ") Receiver not started. Restarting";
        state_machine_->deregistered();
        its_lock.unlock();
        host_->set_client(VSOMEIP_CLIENT_UNSET);
    }
}

void routing_manager_client::on_suspend() {

    VSOMEIP_INFO_P << "Application 0x" << hex4(host_->get_client());
    clear_remote_subscriptions();
}

void routing_manager_client::clear_remote_subscriptions() {
    std::scoped_lock its_lock(remote_subscriber_count_mutex_);

    // Unsubscribe everything that is left over.
    for (const auto& s : remote_subscriber_count_) {
        for (const auto& i : s.second) {
            for (const auto& e : i.second)
                routing_manager_base::unsubscribe(VSOMEIP_ROUTING_CLIENT, nullptr, s.first, i.first, e.first, ANY_EVENT);
        }
    }

    // Remove all entries.
    remote_subscriber_count_.clear();
}

void routing_manager_client::restart_sender([[maybe_unused]] std::unique_lock<std::recursive_mutex> const& _sender_mutex) {
    cancel_keepalive();
    if (sender_) {
        sender_->stop(true);
        sender_ = nullptr;
    }
    if (sender_debounce_active_) {
        start_sender_after_debounce_ = true;
        VSOMEIP_INFO_P << "The restart of the sender is debounced";
        return;
    }
    start_sender_after_debounce_ = false;
    if (!state_machine_->start_assignment()) {
        VSOMEIP_WARNING_P << "(" << hex4(get_client()) << ") Non-Deregistered State Set (" << state_machine_->state() << "). Returning";
        return;
    }
    sender_ = ep_mgr_->create_local_client(VSOMEIP_ROUTING_CLIENT);
    if (sender_) {
        sender_->start();
        sender_debounce_active_ = true;
        sender_debounce_->start();
    } else {
        VSOMEIP_ERROR_P << "Failed due to a missing sender";
    }
}

void routing_manager_client::debounce_restart_sender_done() {
    std::unique_lock<std::recursive_mutex> its_sender_lock(sender_mutex_);
    sender_debounce_active_ = false;
    if (start_sender_after_debounce_) {
        restart_sender(its_sender_lock);
    }
}

void routing_manager_client::try_to_send_before_stop() {
    {
        std::scoped_lock its_lock(sender_mutex_);
        if (sender_) {
            sender_->flush_queue();
        }
    }

    ep_mgr_->flush_local_endpoint_queues();
}

} // namespace vsomeip_v3

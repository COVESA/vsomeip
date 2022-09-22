// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <climits>
#include <iomanip>
#include <mutex>
#include <unordered_set>
#include <future>
#include <forward_list>

#include <vsomeip/constants.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/event.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_proxy.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../security/include/policy.hpp"
#include "../../security/include/security_impl.hpp"

#include "../../endpoints/include/local_client_endpoint_impl.hpp"
#include "../../endpoints/include/local_server_endpoint_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/serializer.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#ifdef USE_DLT
#include "../../tracing/include/connector_impl.hpp"
#endif

namespace vsomeip_v3 {

routing_manager_proxy::routing_manager_proxy(routing_manager_host *_host,
            bool _client_side_logging,
            const std::set<std::tuple<service_t, instance_t> > & _client_side_logging_filter) :
        routing_manager_base(_host),
        is_connected_(false),
        is_started_(false),
        state_(inner_state_type_e::ST_DEREGISTERED),
        sender_(nullptr),
        receiver_(nullptr),
        register_application_timer_(io_),
        request_debounce_timer_ (io_),
        request_debounce_timer_running_(false),
        client_side_logging_(_client_side_logging),
        client_side_logging_filter_(_client_side_logging_filter)
{
}

routing_manager_proxy::~routing_manager_proxy() {
}

void routing_manager_proxy::init() {
    routing_manager_base::init(std::make_shared<endpoint_manager_base>(this, io_, configuration_));
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        sender_ = ep_mgr_->create_local(VSOMEIP_ROUTING_CLIENT);
    }
}

void routing_manager_proxy::start() {
    is_started_ = true;
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (!sender_) {
            // application has been stopped and started again
            sender_ = ep_mgr_->create_local(VSOMEIP_ROUTING_CLIENT);
        }
        if (sender_) {
            sender_->start();
        }
    }
}

void routing_manager_proxy::stop() {
    std::unique_lock<std::mutex> its_lock(state_mutex_);
    if (state_ == inner_state_type_e::ST_REGISTERING) {
        register_application_timer_.cancel();
    }

    const std::chrono::milliseconds its_timeout(configuration_->get_shutdown_timeout());
    while (state_ == inner_state_type_e::ST_REGISTERING) {
        std::cv_status status = state_condition_.wait_for(its_lock, its_timeout);
        if (status == std::cv_status::timeout) {
            VSOMEIP_WARNING << std::hex << client_ << " registering timeout on stop";
            break;
        }
    }

    if (state_ == inner_state_type_e::ST_REGISTERED) {
        deregister_application();
        // Waiting de-register acknowledge to synchronize shutdown
        while (state_ == inner_state_type_e::ST_REGISTERED) {
            std::cv_status status = state_condition_.wait_for(its_lock, its_timeout);
            if (status == std::cv_status::timeout) {
                VSOMEIP_WARNING << std::hex << client_ << " couldn't deregister application - timeout";
                break;
            }
        }
    }
    is_started_ = false;
    its_lock.unlock();

    {
        std::lock_guard<std::mutex> its_lock(request_timer_mutex_);
        request_debounce_timer_.cancel();
    }

    if (receiver_) {
        receiver_->stop();
    }
    receiver_ = nullptr;

    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->stop();
        }
        // delete the sender
        sender_ = nullptr;
    }

    for (const auto& client : ep_mgr_->get_connected_clients()) {
        if (client != VSOMEIP_ROUTING_CLIENT) {
            remove_local(client, true);
        }
    }

    std::stringstream its_client;
    its_client << utility::get_base_path(configuration_) << std::hex << client_;
#ifdef _WIN32
    ::_unlink(its_client.str().c_str());
#else
    if (-1 == ::unlink(its_client.str().c_str())) {
        VSOMEIP_ERROR<< "routing_manager_proxy::stop unlink failed ("
                << its_client.str() << "): "<< std::strerror(errno);
    }
#endif
}

std::shared_ptr<configuration> routing_manager_proxy::get_configuration() const {
    return host_->get_configuration();
}

bool routing_manager_proxy::offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {

    if(!routing_manager_base::offer_service(_client, _service, _instance, _major, _minor)) {
        VSOMEIP_WARNING << "routing_manager_proxy::offer_service,"
                << "routing_manager_base::offer_service returned false";
        return false;
    }
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            send_offer_service(_client, _service, _instance, _major, _minor);
        }
        service_data_t offer = { _service, _instance, _major, _minor };
        pending_offers_.insert(offer);
    }
    return true;
}

void routing_manager_proxy::send_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {
    (void)_client;

    byte_t its_command[VSOMEIP_OFFER_SERVICE_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_OFFER_SERVICE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_OFFER_SERVICE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4] = _major;
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 5], &_minor,
            sizeof(_minor));

    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {

    (void)_client;

    {
        // Hold the mutex to ensure no placeholder event is created inbetween.
        std::lock_guard<std::mutex> its_lock(stop_mutex_);

        routing_manager_base::stop_offer_service(_client, _service, _instance, _major, _minor);
        clear_remote_subscriber_count(_service, _instance);

        // Note: The last argument does not matter here as a proxy
        //       does not manage endpoints to the external network.
        clear_service_info(_service, _instance, false);
    }

    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            byte_t its_command[VSOMEIP_STOP_OFFER_SERVICE_COMMAND_SIZE];
            uint32_t its_size = VSOMEIP_STOP_OFFER_SERVICE_COMMAND_SIZE
                    - VSOMEIP_COMMAND_HEADER_SIZE;

            its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_STOP_OFFER_SERVICE;
            std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
                    sizeof(client_));
            std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                    sizeof(its_size));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                    sizeof(_service));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                    sizeof(_instance));
            its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4] = _major;
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 5], &_minor,
                    sizeof(_minor));

            {
                std::lock_guard<std::mutex> its_lock(sender_mutex_);
                if (sender_) {
                    sender_->send(its_command, sizeof(its_command));
                }
            }
        }
        auto it = pending_offers_.begin();
        while (it != pending_offers_.end()) {
            if (it->service_ == _service
             && it->instance_ == _instance) {
                break;
            }
            it++;
        }
        if (it != pending_offers_.end()) pending_offers_.erase(it);
    }
}

void routing_manager_proxy::request_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {
    routing_manager_base::request_service(_client,
            _service, _instance, _major, _minor);
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        size_t request_debouncing_time = configuration_->get_request_debouncing(host_->get_name());
        service_data_t request = { _service, _instance, _major, _minor };
        if (!request_debouncing_time) {
            if (state_ == inner_state_type_e::ST_REGISTERED) {
                std::set<service_data_t> requests;
                requests.insert(request);
                send_request_services(requests);
            }
            requests_.insert(request);
        } else {
            requests_to_debounce_.insert(request);
            std::lock_guard<std::mutex> its_lock(request_timer_mutex_);
            if (!request_debounce_timer_running_) {
                request_debounce_timer_running_ = true;
                request_debounce_timer_.expires_from_now(std::chrono::milliseconds(request_debouncing_time));
                request_debounce_timer_.async_wait(
                        std::bind(
                                &routing_manager_proxy::request_debounce_timeout_cbk,
                                std::dynamic_pointer_cast<routing_manager_proxy>(shared_from_this()),
                                std::placeholders::_1));
            }
        }
    }
}

void routing_manager_proxy::release_service(client_t _client,
        service_t _service, instance_t _instance) {
    routing_manager_base::release_service(_client, _service, _instance);
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        remove_pending_subscription(_service, _instance, 0xFFFF, ANY_EVENT);

        auto it = requests_to_debounce_.begin();
        while (it != requests_to_debounce_.end()) {
            if (it->service_ == _service
             && it->instance_ == _instance) {
                break;
            }
            it++;
        }
        if (it != requests_to_debounce_.end()) {
            requests_to_debounce_.erase(it);
        } else if (state_ == inner_state_type_e::ST_REGISTERED) {
            send_release_service(_client, _service, _instance);
        }

        {
            auto it = requests_.begin();
            while (it != requests_.end()) {
                if (it->service_ == _service
                 && it->instance_ == _instance) {
                    break;
                }
                it++;
            }
            if (it != requests_.end()) requests_.erase(it);
        }
    }
}

void routing_manager_proxy::register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _notifier,
        const std::set<eventgroup_t> &_eventgroups, const event_type_e _type,
        reliability_type_e _reliability,
        std::chrono::milliseconds _cycle, bool _change_resets_cycle,
        bool _update_on_change,  epsilon_change_func_t _epsilon_change_func,
        bool _is_provided, bool _is_shadow, bool _is_cache_placeholder) {
    (void)_is_shadow;
    (void)_is_cache_placeholder;

    const event_data_t registration = {
            _service,
            _instance,
            _notifier,
            _type,
            _reliability,
            _is_provided,
            _eventgroups
    };
    bool is_first(false);
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        is_first = pending_event_registrations_.find(registration)
                                        == pending_event_registrations_.end();
#ifndef VSOMEIP_ENABLE_COMPAT
        if (is_first) {
            pending_event_registrations_.insert(registration);
        }
#else
        bool insert = true;
        if (is_first) {
            for (auto iter = pending_event_registrations_.begin();
                    iter != pending_event_registrations_.end();) {
                if (iter->service_ == _service
                        && iter->instance_ == _instance
                        && iter->notifier_ == _notifier
                        && iter->is_provided_ == _is_provided
                        && iter->type_ == event_type_e::ET_EVENT
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
#endif
    }
    if (is_first || _is_provided) {
        routing_manager_base::register_event(_client,
                _service, _instance,
                _notifier,
                _eventgroups, _type, _reliability,
                _cycle, _change_resets_cycle, _update_on_change,
                _epsilon_change_func,
                _is_provided);
    }
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED && is_first) {
            send_register_event(client_, _service, _instance,
                    _notifier, _eventgroups, _type, _reliability, _is_provided);
        }
    }
}

void routing_manager_proxy::unregister_event(client_t _client,
        service_t _service, instance_t _instance, event_t _notifier,
        bool _is_provided) {

    routing_manager_base::unregister_event(_client, _service, _instance,
            _notifier, _is_provided);

    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            byte_t its_command[VSOMEIP_UNREGISTER_EVENT_COMMAND_SIZE];
            uint32_t its_size = VSOMEIP_UNREGISTER_EVENT_COMMAND_SIZE
                    - VSOMEIP_COMMAND_HEADER_SIZE;

            its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_UNREGISTER_EVENT;
            std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
                    sizeof(client_));
            std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                    sizeof(its_size));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                    sizeof(_service));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                    sizeof(_instance));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_notifier,
                    sizeof(_notifier));
            its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6]
                        = static_cast<byte_t>(_is_provided);

            {
                std::lock_guard<std::mutex> its_lock(sender_mutex_);
                if (sender_) {
                    sender_->send(its_command, sizeof(its_command));
                }
            }
        }

        for (auto iter = pending_event_registrations_.begin();
                iter != pending_event_registrations_.end(); ) {
            if (iter->service_ == _service
                    && iter->instance_ == _instance
                    && iter->notifier_ == _notifier
                    && iter->is_provided_ == _is_provided) {
                pending_event_registrations_.erase(iter);
                break;
            } else {
                iter++;
            }
        }
    }
}

bool routing_manager_proxy::is_field(service_t _service, instance_t _instance,
        event_t _event) const {
    auto event = find_event(_service, _instance, _event);
    if (event && event->is_field()) {
        return true;
    }
    return false;
}

void routing_manager_proxy::subscribe(client_t _client, uid_t _uid, gid_t _gid, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        event_t _event) {
    (void)_uid;
    (void)_gid;
    {
        credentials_t its_credentials = std::make_pair(own_uid_, own_gid_);
        if (_event == ANY_EVENT) {
           if (!is_subscribe_to_any_event_allowed(its_credentials, _client, _service, _instance, _eventgroup)) {
               VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << _client
                       << " : routing_manager_proxy::subscribe: "
                       << " isn't allowed to subscribe to service/instance/event "
                       << _service << "/" << _instance << "/ANY_EVENT"
                       << " which violates the security policy ~> Skip subscribe!";
               return;
           }
        } else {
            auto its_security = security_impl::get();
            if (!its_security)
                return;
            if (!its_security->is_client_allowed(own_uid_, own_gid_,
                    _client, _service, _instance, _event)) {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << _client
                        << " : routing_manager_proxy::subscribe: "
                        << " isn't allowed to subscribe to service/instance/event "
                        << _service << "/" << _instance
                        << "/" << _event;
                return;
            }
        }

        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED && is_available(_service, _instance, _major)) {
            send_subscribe(client_, _service, _instance, _eventgroup, _major, _event );
        }
        subscription_data_t subscription = { _service, _instance, _eventgroup, _major, _event, _uid, _gid};
        pending_subscriptions_.insert(subscription);
    }
}

void routing_manager_proxy::send_subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        event_t _event) {
    (void)_client;

    byte_t its_command[VSOMEIP_SUBSCRIBE_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_SUBSCRIBE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
            sizeof(_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
            sizeof(_eventgroup));
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6] = _major;
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 7], &_event,
            sizeof(_event));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 9],
            &PENDING_SUBSCRIPTION_ID, sizeof(PENDING_SUBSCRIPTION_ID));

    client_t target_client = find_local_client(_service, _instance);
    if (target_client != VSOMEIP_ROUTING_CLIENT) {
        auto its_target = ep_mgr_->find_or_create_local(target_client);
        its_target->send(its_command, sizeof(its_command));
    } else {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::send_subscribe_nack(client_t _subscriber,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        event_t _event, remote_subscription_id_t _id) {
    byte_t its_command[VSOMEIP_SUBSCRIBE_NACK_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_SUBSCRIBE_NACK_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    client_t its_client = get_client();
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE_NACK;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &its_client,
            sizeof(its_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
            sizeof(_eventgroup));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &_subscriber,
            sizeof(_subscriber));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8], &_event,
            sizeof(_event));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 10], &_id,
            sizeof(_id));

    if (_subscriber != VSOMEIP_ROUTING_CLIENT
            && _id == PENDING_SUBSCRIPTION_ID) {
        auto its_target = ep_mgr_->find_local(_subscriber);
        if (its_target) {
            its_target->send(its_command, sizeof(its_command));
            return;
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::send_subscribe_ack(client_t _subscriber,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        event_t _event, remote_subscription_id_t _id) {
    byte_t its_command[VSOMEIP_SUBSCRIBE_ACK_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_SUBSCRIBE_ACK_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    client_t its_client = get_client();
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE_ACK;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &its_client,
            sizeof(its_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
            sizeof(_eventgroup));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &_subscriber,
            sizeof(_subscriber));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8], &_event,
            sizeof(_event));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 10], &_id,
            sizeof(_id));

    if (_subscriber != VSOMEIP_ROUTING_CLIENT
            && _id == PENDING_SUBSCRIPTION_ID) {
        auto its_target = ep_mgr_->find_local(_subscriber);
        if (its_target) {
            its_target->send(its_command, sizeof(its_command));
            return;
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::unsubscribe(client_t _client, uid_t _uid, gid_t _gid,
    service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event) {
    (void)_client;
    (void)_uid;
    (void)_gid;
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        remove_pending_subscription(_service, _instance, _eventgroup, _event);

        if (state_ == inner_state_type_e::ST_REGISTERED) {
            byte_t its_command[VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE];
            uint32_t its_size = VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE
                    - VSOMEIP_COMMAND_HEADER_SIZE;

            its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_UNSUBSCRIBE;
            std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
                    sizeof(_client));
            std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                    sizeof(its_size));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                    sizeof(_service));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                    sizeof(_instance));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                    sizeof(_eventgroup));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &_event,
                    sizeof(_event));
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                    &PENDING_SUBSCRIPTION_ID, sizeof(PENDING_SUBSCRIPTION_ID));

            auto its_target = ep_mgr_->find_local(_service, _instance);
            if (its_target) {
                its_target->send(its_command, sizeof(its_command));
            } else {
                std::lock_guard<std::mutex> its_lock(sender_mutex_);
                if (sender_) {
                    sender_->send(its_command, sizeof(its_command));
                }
            }
        }
    }
}

bool routing_manager_proxy::send(client_t _client, const byte_t *_data,
        length_t _size, instance_t _instance,
        bool _reliable,
        client_t _bound_client,
        credentials_t _credentials,
        uint8_t _status_check,
        bool _sent_from_remote) {
    (void)_client;
    (void)_bound_client;
    (void)_credentials;
    (void)_sent_from_remote;
    bool is_sent(false);
    bool has_remote_subscribers(false);
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ != inner_state_type_e::ST_REGISTERED) {
            return false;
        }
    }
    if (client_side_logging_) {
        if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
            service_t its_service = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_SERVICE_POS_MIN],
                    _data[VSOMEIP_SERVICE_POS_MAX]);
            if (client_side_logging_filter_.empty()
                || (1 == client_side_logging_filter_.count(std::make_tuple(its_service, ANY_INSTANCE)))
                || (1 == client_side_logging_filter_.count(std::make_tuple(its_service, _instance)))) {
                method_t its_method = VSOMEIP_BYTES_TO_WORD(
                        _data[VSOMEIP_METHOD_POS_MIN],
                        _data[VSOMEIP_METHOD_POS_MAX]);
                session_t its_session = VSOMEIP_BYTES_TO_WORD(
                        _data[VSOMEIP_SESSION_POS_MIN],
                        _data[VSOMEIP_SESSION_POS_MAX]);
                client_t its_client = VSOMEIP_BYTES_TO_WORD(
                        _data[VSOMEIP_CLIENT_POS_MIN],
                        _data[VSOMEIP_CLIENT_POS_MAX]);
                VSOMEIP_INFO << "routing_manager_proxy::send: ("
                    << std::hex << std::setw(4) << std::setfill('0') << client_ <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_method << ":"
                    << std::hex << std::setw(4) << std::setfill('0') << its_session << ":"
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "] "
                    << "type=" << std::hex << static_cast<std::uint32_t>(_data[VSOMEIP_MESSAGE_TYPE_POS])
                    << " thread=" << std::hex << std::this_thread::get_id();
            }
        } else {
            VSOMEIP_ERROR << "routing_manager_proxy::send: ("
                << std::hex << std::setw(4) << std::setfill('0') << client_
                <<"): message too short to log: " << std::dec << _size;
        }
    }
    if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
        std::shared_ptr<endpoint> its_target;
        if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            // Request
            service_t its_service = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_SERVICE_POS_MIN],
                    _data[VSOMEIP_SERVICE_POS_MAX]);
            client_t its_client = find_local_client(its_service, _instance);
            if (its_client != VSOMEIP_ROUTING_CLIENT) {
                if (is_client_known(its_client)) {
                    its_target = ep_mgr_->find_or_create_local(its_client);
                }
            }
        } else if (!utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            // Response
            client_t its_client = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_CLIENT_POS_MIN],
                    _data[VSOMEIP_CLIENT_POS_MAX]);
            if (its_client != VSOMEIP_ROUTING_CLIENT) {
                if (is_client_known(its_client)) {
                    its_target = ep_mgr_->find_or_create_local(its_client);
                }
            }
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) &&
                _client == VSOMEIP_ROUTING_CLIENT) {
            // notify
            has_remote_subscribers = send_local_notification(get_client(), _data, _size,
                    _instance, _reliable, _status_check);
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) &&
                _client != VSOMEIP_ROUTING_CLIENT) {
            // notify_one
            its_target = ep_mgr_->find_local(_client);
            if (its_target) {
#ifdef USE_DLT
                const uint16_t its_data_size
                    = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                trace::header its_header;
                if (its_header.prepare(nullptr, true, _instance))
                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                            _data, its_data_size);
#endif
                return send_local(its_target, get_client(), _data, _size,
                        _instance, _reliable, VSOMEIP_SEND, _status_check);
            }
        }
        // If no direct endpoint could be found
        // or for notifications ~> route to routing_manager_stub
#ifdef USE_DLT
        bool message_to_stub(false);
#endif
        if (!its_target) {
            std::lock_guard<std::mutex> its_lock(sender_mutex_);
            if (sender_) {
                its_target = sender_;
#ifdef USE_DLT
                message_to_stub = true;
#endif
            } else {
                return false;
            }
        }

        bool send(true);
        uint8_t command = VSOMEIP_SEND;

        if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            if (_client != VSOMEIP_ROUTING_CLIENT) {
                command = VSOMEIP_NOTIFY_ONE;
            } else {
                command = VSOMEIP_NOTIFY;
                // Do we need to deliver a notification to the routing manager?
                // Only for services which already have remote clients subscribed to
                send = has_remote_subscribers;
            }
        }
#ifdef USE_DLT
        else if (!message_to_stub) {
            const uint16_t its_data_size
                = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

            trace::header its_header;
            if (its_header.prepare(nullptr, true, _instance))
                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                        _data, its_data_size);
        }
#endif
        if (send) {
            is_sent = send_local(its_target,
                    (command == VSOMEIP_NOTIFY_ONE ? _client : get_client()),
                    _data, _size, _instance, _reliable, command, _status_check);
        }
    }
    return (is_sent);
}

bool routing_manager_proxy::send_to(const client_t _client,
        const std::shared_ptr<endpoint_definition> &_target,
        std::shared_ptr<message> _message) {
    (void)_client;
    (void)_target;
    (void)_message;
    return (false);
}

bool routing_manager_proxy::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size, instance_t _instance) {
    (void)_target;
    (void)_data;
    (void)_size;
    (void)_instance;
    return (false);
}

void routing_manager_proxy::on_connect(const std::shared_ptr<endpoint>& _endpoint) {
    _endpoint->set_connected(true);
    _endpoint->set_established(true);
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (_endpoint != sender_) {
            return;
        }
    }
    is_connected_ = true;
    assign_client();
}

void routing_manager_proxy::on_disconnect(const std::shared_ptr<endpoint>& _endpoint) {

    bool is_disconnected((_endpoint == sender_));
    if (is_disconnected) {
        {
            std::lock_guard<std::mutex> its_lock(sender_mutex_);
            is_connected_ = false;
        }

        VSOMEIP_INFO << "routing_manager_proxy::on_disconnect: Client 0x" << std::hex
                << get_client() << " calling host_->on_state "
                << "with DEREGISTERED";
        host_->on_state(state_type_e::ST_DEREGISTERED);
    }
}

void routing_manager_proxy::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination,
        client_t _bound_client,
        credentials_t _credentials,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {
    (void)_receiver;
    (void)_destination;
    (void)_remote_address;
    (void)_remote_port;
#if 0
    std::stringstream msg;
    msg << "rmp::on_message: ";
    for (length_t i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    byte_t its_command;
    client_t its_client;
    length_t its_length;
    service_t its_service;
    instance_t its_instance;
    eventgroup_t its_eventgroup;
    event_t its_event;
    major_version_t its_major;
    client_t routing_host_id = configuration_->get_id(configuration_->get_routing_host());
    client_t its_subscriber;
    remote_subscription_id_t its_subscription_id(PENDING_SUBSCRIPTION_ID);
    std::uint32_t its_remote_subscriber_count(0);
    bool is_internal_policy_update(false);

    std::uint32_t its_sender_uid = std::get<0>(_credentials);
    std::uint32_t its_sender_gid = std::get<1>(_credentials);

    auto its_security = security_impl::get();
    if (!its_security)
        return;

    if (_size > VSOMEIP_COMMAND_SIZE_POS_MAX) {
        its_command = _data[VSOMEIP_COMMAND_TYPE_POS];
        std::memcpy(&its_client, &_data[VSOMEIP_COMMAND_CLIENT_POS],
                sizeof(its_client));
        std::memcpy(&its_length, &_data[VSOMEIP_COMMAND_SIZE_POS_MIN],
                sizeof(its_length));

        bool message_from_routing(false);
        if (its_security->is_enabled()) {
            // if security is enabled, client ID of routing must be configured
            // and credential passing is active. Otherwise bound client is zero by default
            message_from_routing = (_bound_client == routing_host_id);
        } else {
            message_from_routing = (its_client == routing_host_id);
        }

        if (its_security->is_enabled() && !message_from_routing &&
                _bound_client != its_client) {
            VSOMEIP_WARNING << std::hex << "Client " << std::setw(4) << std::setfill('0') << get_client()
                    << " received a message with command " << (uint32_t)its_command
                    << " from " << std::setw(4) << std::setfill('0')
                    << its_client << " which doesn't match the bound client "
                    << std::setw(4) << std::setfill('0') << _bound_client
                    << " ~> skip message!";
            return;
        }

        switch (its_command) {
        case VSOMEIP_SEND: {
            if (_size < VSOMEIP_SEND_COMMAND_SIZE + VSOMEIP_FULL_HEADER_SIZE) {
                VSOMEIP_WARNING << "Received a SEND command with too small size -> skip!";
                break;
            }
            instance_t its_instance;
            bool its_reliable;
            uint8_t its_check_status;
            std::memcpy(&its_instance,&_data[VSOMEIP_SEND_COMMAND_INSTANCE_POS_MIN],
                        sizeof(instance_t));
            std::memcpy(&its_reliable, &_data[VSOMEIP_SEND_COMMAND_RELIABLE_POS],
                        sizeof(its_reliable));
            std::memcpy(&its_check_status, &_data[VSOMEIP_SEND_COMMAND_CHECK_STATUS_POS],
                        sizeof(its_check_status));

            // reduce by size of instance, flush, reliable, client and is_valid_crc flag
            const std::uint32_t its_message_size = its_length -
                    (VSOMEIP_SEND_COMMAND_SIZE - VSOMEIP_COMMAND_HEADER_SIZE);

            if (its_message_size !=
                    VSOMEIP_BYTES_TO_LONG(_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN],
                                          _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 1],
                                          _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 2],
                                          _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 3])
                    + VSOMEIP_SOMEIP_HEADER_SIZE) {
                VSOMEIP_WARNING << "Received a SEND command containing message with invalid size -> skip!";
                break;
            }

            auto a_deserializer = get_deserializer();
            a_deserializer->set_data(&_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS],
                    its_message_size);
            std::shared_ptr<message_impl> its_message(a_deserializer->deserialize_message());
            a_deserializer->reset();
            put_deserializer(a_deserializer);

            if (its_message) {
                its_message->set_instance(its_instance);
                its_message->set_reliable(its_reliable);
                its_message->set_check_result(its_check_status);
                its_message->set_uid(std::get<0>(_credentials));
                its_message->set_gid(std::get<1>(_credentials));

                if (!message_from_routing) {
                    if (utility::is_notification(its_message->get_message_type())) {
                        if (!is_response_allowed(_bound_client, its_message->get_service(),
                                its_message->get_instance(), its_message->get_method())) {
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                    << " : routing_manager_proxy::on_message: "
                                    << " received a notification from client 0x" << _bound_client
                                    << " which does not offer service/instance/event "
                                    << its_message->get_service() << "/" << its_message->get_instance()
                                    << "/" << its_message->get_method()
                                    << " ~> Skip message!";
                            return;
                        } else {
                            if (!its_security->is_client_allowed(own_uid_, own_gid_,
                                    get_client(), its_message->get_service(),
                                    its_message->get_instance(), its_message->get_method())) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                        << " : routing_manager_proxy::on_message: "
                                        << " isn't allowed to receive a notification from service/instance/event "
                                        << its_message->get_service() << "/" << its_message->get_instance()
                                        << "/" << its_message->get_method()
                                        << " respectively from client 0x" << _bound_client
                                        << " ~> Skip message!";
                                return;
                            }
                            cache_event_payload(its_message);
                        }
                    } else if (utility::is_request(its_message->get_message_type())) {
                        if (its_security->is_enabled()
                                && its_message->get_client() != _bound_client) {
                            VSOMEIP_WARNING << std::hex << "vSomeIP Security: Client 0x" << std::setw(4) << std::setfill('0') << get_client()
                                    << " received a request from client 0x" << std::setw(4) << std::setfill('0')
                                    << its_message->get_client() << " to service/instance/method "
                                    << its_message->get_service() << "/" << its_message->get_instance()
                                    << "/" << its_message->get_method() << " which doesn't match the bound client 0x"
                                    << std::setw(4) << std::setfill('0') << _bound_client
                                    << " ~> skip message!";
                            return;
                        }

                        if (!its_security->is_client_allowed(its_sender_uid, its_sender_gid,
                                its_message->get_client(), its_message->get_service(),
                                its_message->get_instance(), its_message->get_method())) {
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_message->get_client()
                                    << " : routing_manager_proxy::on_message: "
                                    << "isn't allowed to send a request to service/instance/method "
                                    << its_message->get_service() << "/" << its_message->get_instance()
                                    << "/" << its_message->get_method()
                                    << " ~> Skip message!";
                            return;
                        }
                    } else { // response
                        if (!is_response_allowed(_bound_client, its_message->get_service(),
                                its_message->get_instance(), its_message->get_method())) {
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                    << " : routing_manager_proxy::on_message: "
                                    << " received a response from client 0x" << _bound_client
                                    << " which does not offer service/instance/method "
                                    << its_message->get_service() << "/" << its_message->get_instance()
                                    << "/" << its_message->get_method()
                                    << " ~> Skip message!";
                            return;
                        } else {
                            if (!its_security->is_client_allowed(own_uid_, own_gid_,
                                        get_client(), its_message->get_service(),
                                        its_message->get_instance(), its_message->get_method())) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                        << " : routing_manager_proxy::on_message: "
                                        << " isn't allowed to receive a response from service/instance/method "
                                        << its_message->get_service() << "/" << its_message->get_instance()
                                        << "/" << its_message->get_method()
                                        << " respectively from client 0x" << _bound_client
                                        << " ~> Skip message!";
                                return;
                            }
                        }
                    }
                } else {
                    if (!its_security->is_remote_client_allowed()) {
                        // if the message is from routing manager, check if
                        // policy allows remote requests.
                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                << " : routing_manager_proxy::on_message: "
                                << std::hex << "Security: Remote clients via routing manager with client ID 0x" << its_client
                                << " are not allowed to communicate with service/instance/method "
                                << its_message->get_service() << "/" << its_message->get_instance()
                                << "/" << its_message->get_method()
                                << " respectively with client 0x" << get_client()
                                << " ~> Skip message!";
                        return;
                    } else if (utility::is_notification(its_message->get_message_type())) {
                        // As subscription is sent on eventgroup level, incoming remote event ID's
                        // need to be checked as well if remote clients are allowed
                        // and the local policy only allows specific events in the eventgroup to be received.
                        if (!its_security->is_client_allowed(own_uid_, own_gid_,
                                get_client(), its_message->get_service(),
                                its_message->get_instance(), its_message->get_method())) {
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                    << " : routing_manager_proxy::on_message: "
                                    << " isn't allowed to receive a notification from service/instance/event "
                                    << its_message->get_service() << "/" << its_message->get_instance()
                                    << "/" << its_message->get_method()
                                    << " respectively from remote clients via routing manager with client ID 0x"
                                    << routing_host_id
                                    << " ~> Skip message!";
                            return;
                        }
                    }
                }
#ifdef USE_DLT
                if (client_side_logging_
                    && (client_side_logging_filter_.empty()
                        || (1 == client_side_logging_filter_.count(std::make_tuple(its_message->get_service(), ANY_INSTANCE)))
                        || (1 == client_side_logging_filter_.count(std::make_tuple(its_message->get_service(), its_message->get_instance()))))) {
                    trace::header its_header;
                    if (its_header.prepare(nullptr, false, its_instance))
                        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                &_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS],
                                static_cast<std::uint16_t>(its_message_size));
                }
#endif

                host_->on_message(std::move(its_message));
            } else {
                VSOMEIP_ERROR << "Routing proxy: on_message: "
                              << "SomeIP-Header deserialization failed!";
            }
            break;
        }

        case VSOMEIP_ASSIGN_CLIENT_ACK: {
            if (_size != VSOMEIP_ASSIGN_CLIENT_ACK_COMMAND_SIZE) {
                VSOMEIP_WARNING << "Received a VSOMEIP_ASSIGN_CLIENT_ACK command with wrong size ~> skip!";
                break;
            }
            client_t its_assigned_client(VSOMEIP_CLIENT_UNSET);
            std::memcpy(&its_assigned_client,
                        &_data[VSOMEIP_COMMAND_PAYLOAD_POS], sizeof(client_));
            on_client_assign_ack(its_assigned_client);
            break;
        }
        case VSOMEIP_ROUTING_INFO:
            if (_size - VSOMEIP_COMMAND_HEADER_SIZE != its_length) {
                VSOMEIP_WARNING << "Received a ROUTING_INFO command with invalid size -> skip!";
                break;
            }
            if (!its_security->is_enabled() || message_from_routing) {
                on_routing_info(&_data[VSOMEIP_COMMAND_PAYLOAD_POS], its_length);
            } else {
                VSOMEIP_WARNING << "routing_manager_proxy::on_message: "
                        << std::hex << "Security: Client 0x" << get_client()
                        << " received an routing info from a client which isn't the routing manager"
                        << " : Skip message!";
            }
            break;

        case VSOMEIP_PING:
            if (_size != VSOMEIP_PING_COMMAND_SIZE) {
                VSOMEIP_WARNING << "Received a PING command with wrong size ~> skip!";
                break;
            }
            send_pong();
            VSOMEIP_TRACE << "PING("
                << std::hex << std::setw(4) << std::setfill('0') << client_ << ")";
            break;

        case VSOMEIP_SUBSCRIBE:
            if (_size != VSOMEIP_SUBSCRIBE_COMMAND_SIZE) {
                VSOMEIP_WARNING << "Received a SUBSCRIBE command with wrong size ~> skip!";
                break;
            }
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));
            std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                    sizeof(its_major));
            std::memcpy(&its_event, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                    sizeof(its_event));
            std::memcpy(&its_subscription_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 9],
                    sizeof(its_subscription_id));
            {
                std::unique_lock<std::recursive_mutex> its_lock(incoming_subscriptions_mutex_);
                if (its_subscription_id != PENDING_SUBSCRIPTION_ID) {
                    its_lock.unlock();
#ifdef VSOMEIP_ENABLE_COMPAT
                    routing_manager_base::set_incoming_subscription_state(its_client, its_service,
                            its_instance, its_eventgroup, its_event, subscription_state_e::IS_SUBSCRIBING);
#endif
                    // Remote subscriber: Notify routing manager initially + count subscribes
                    auto self = shared_from_this();
                    host_->on_subscription(its_service, its_instance, its_eventgroup,
                        its_client, its_sender_uid, its_sender_gid, true,
                        [this, self, its_client, its_service, its_instance,
                            its_eventgroup, its_event, its_subscription_id, its_major]
                                (const bool _subscription_accepted){
                        if(_subscription_accepted) {
                            send_subscribe_ack(its_client, its_service, its_instance,
                                           its_eventgroup, its_event, its_subscription_id);
                        } else {
                            send_subscribe_nack(its_client, its_service, its_instance,
                                           its_eventgroup, its_event, its_subscription_id);
                        }
                        std::set<event_t> its_already_subscribed_events;
                        bool inserted = insert_subscription(its_service, its_instance, its_eventgroup,
                                its_event, VSOMEIP_ROUTING_CLIENT, &its_already_subscribed_events);
                        if (inserted) {
                            notify_remote_initially(its_service, its_instance, its_eventgroup,
                                    its_already_subscribed_events);
                        }
#ifdef VSOMEIP_ENABLE_COMPAT
                        send_pending_notify_ones(its_service, its_instance, its_eventgroup, its_client, true);
#endif
                        std::uint32_t its_count = get_remote_subscriber_count(
                                its_service, its_instance, its_eventgroup, true);
                        VSOMEIP_INFO << "SUBSCRIBE("
                            << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                            << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                            << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                            << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << ":"
                            << std::hex << std::setw(4) << std::setfill('0') << its_event << ":"
                            << std::dec << (uint16_t)its_major << "]"
                            << (bool)(its_subscription_id != PENDING_SUBSCRIPTION_ID) << " "
                            << std::dec << its_count;
#ifdef VSOMEIP_ENABLE_COMPAT
                        routing_manager_base::erase_incoming_subscription_state(its_client, its_service,
                                its_instance, its_eventgroup, its_event);
#endif
                    });
                } else if (is_client_known(its_client)) {
                    its_lock.unlock();
                    if (!message_from_routing) {
                        if (its_event == ANY_EVENT) {
                           if (!is_subscribe_to_any_event_allowed(_credentials, its_client, its_service, its_instance, its_eventgroup)) {
                               VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_client
                                       << " : routing_manager_proxy::on_message: "
                                       << " isn't allowed to subscribe to service/instance/event "
                                       << its_service << "/" << its_instance << "/ANY_EVENT"
                                       << " which violates the security policy ~> Skip subscribe!";
                               return;
                           }
                        } else {
                            if (!its_security->is_client_allowed(its_sender_uid, its_sender_gid,
                                    its_client, its_service, its_instance, its_event)) {
                                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_client
                                        << " : routing_manager_proxy::on_message: "
                                        << " subscribes to service/instance/event "
                                        << its_service << "/" << its_instance << "/" << its_event
                                        << " which violates the security policy ~> Skip subscribe!";
                                return;
                            }
                        }
                    } else {
                        if (!its_security->is_remote_client_allowed()) {
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_client
                                    << " : routing_manager_proxy::on_message: "
                                    << std::hex << "Routing manager with client ID 0x"
                                    << its_client
                                    << " isn't allowed to subscribe to service/instance/event "
                                    << its_service << "/" << its_instance
                                    << "/" << its_event
                                    << " respectively to client 0x" << get_client()
                                    << " ~> Skip Subscribe!";
                            return;
                        }
                    }

                    // Local & already known subscriber: create endpoint + send (N)ACK + insert subscription
#ifdef VSOMEIP_ENABLE_COMPAT
                    routing_manager_base::set_incoming_subscription_state(its_client, its_service,
                            its_instance, its_eventgroup, its_event, subscription_state_e::IS_SUBSCRIBING);
#endif
                    (void) ep_mgr_->find_or_create_local(its_client);
                    auto self = shared_from_this();
                    host_->on_subscription(its_service, its_instance,
                            its_eventgroup, its_client, its_sender_uid, its_sender_gid, true,
                            [this, self, its_client, its_sender_uid, its_sender_gid, its_service,
                                its_instance, its_eventgroup, its_event, its_major]
                                    (const bool _subscription_accepted) {
                        if (!_subscription_accepted) {
                            send_subscribe_nack(its_client, its_service, its_instance,
                                    its_eventgroup, its_event, PENDING_SUBSCRIPTION_ID);
                        } else {
                            send_subscribe_ack(its_client, its_service, its_instance,
                                    its_eventgroup, its_event, PENDING_SUBSCRIPTION_ID);
                            routing_manager_base::subscribe(its_client, its_sender_uid, its_sender_gid,
                                its_service, its_instance, its_eventgroup, its_major, its_event);
#ifdef VSOMEIP_ENABLE_COMPAT
                            send_pending_notify_ones(its_service, its_instance, its_eventgroup, its_client);
#endif
                        }
#ifdef VSOMEIP_ENABLE_COMPAT
                        routing_manager_base::erase_incoming_subscription_state(its_client, its_service,
                                its_instance, its_eventgroup, its_event);
#endif
                    });
                } else {
                    // Local & not yet known subscriber ~> set pending until subscriber gets known!
                    subscription_data_t subscription = { its_service, its_instance,
                            its_eventgroup, its_major, its_event, its_sender_uid, its_sender_gid };
                    pending_incoming_subscripitons_[its_client].insert(subscription);
                }
            }
            if (its_subscription_id == PENDING_SUBSCRIPTION_ID) { // local subscription
                VSOMEIP_INFO << "SUBSCRIBE("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << ":"
                    << std::hex << std::setw(4) << std::setfill('0') << its_event << ":"
                    << std::dec << (uint16_t)its_major << "]";
            }
            break;

        case VSOMEIP_UNSUBSCRIBE:
            if (_size != VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE) {
                VSOMEIP_WARNING << "Received an UNSUBSCRIBE command with wrong ~> skip!";
                break;
            }
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));
            std::memcpy(&its_event, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                    sizeof(its_event));
            std::memcpy(&its_subscription_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                    sizeof(its_subscription_id));
            host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, its_sender_uid, its_sender_gid, false, [](const bool _subscription_accepted){ (void)_subscription_accepted; });
            if (its_subscription_id == PENDING_SUBSCRIPTION_ID) {
                // Local subscriber: withdraw subscription
                routing_manager_base::unsubscribe(its_client, its_sender_uid, its_sender_gid, its_service, its_instance, its_eventgroup, its_event);
            } else {
                // Remote subscriber: withdraw subscription only if no more remote subscriber exists
                its_remote_subscriber_count = get_remote_subscriber_count(its_service,
                        its_instance, its_eventgroup, false);
                if (!its_remote_subscriber_count) {
                    routing_manager_base::unsubscribe(VSOMEIP_ROUTING_CLIENT, ANY_UID, ANY_GID, its_service,
                            its_instance, its_eventgroup, its_event);
                }
                send_unsubscribe_ack(its_service, its_instance, its_eventgroup,
                                     its_subscription_id);
            }
            VSOMEIP_INFO << "UNSUBSCRIBE("
                << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_event << "] "
                << (bool)(its_subscription_id != PENDING_SUBSCRIPTION_ID) << " "
                << std::dec << its_remote_subscriber_count;
            break;

        case VSOMEIP_SUBSCRIBE_NACK:
            if (_size != VSOMEIP_SUBSCRIBE_NACK_COMMAND_SIZE) {
                VSOMEIP_WARNING << "Received a VSOMEIP_SUBSCRIBE_NACK command with wrong size ~> skip!";
                break;
            }
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));
            std::memcpy(&its_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                    sizeof(its_subscriber));
            std::memcpy(&its_event, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                                sizeof(its_event));

            on_subscribe_nack(its_subscriber, its_service, its_instance, its_eventgroup, its_event);
            VSOMEIP_INFO << "SUBSCRIBE NACK("
                << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_event << "]";
            break;

        case VSOMEIP_SUBSCRIBE_ACK:
            if (_size != VSOMEIP_SUBSCRIBE_ACK_COMMAND_SIZE) {
                VSOMEIP_WARNING << "Received a VSOMEIP_SUBSCRIBE_ACK command with wrong size ~> skip!";
                break;
            }
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));
            std::memcpy(&its_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                    sizeof(its_subscriber));
            std::memcpy(&its_event, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                    sizeof(its_event));

            on_subscribe_ack(its_subscriber, its_service, its_instance, its_eventgroup, its_event);
            VSOMEIP_INFO << "SUBSCRIBE ACK("
                << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_event << "]";
            break;

        case VSOMEIP_OFFERED_SERVICES_RESPONSE:
            if (_size - VSOMEIP_COMMAND_HEADER_SIZE != its_length) {
                VSOMEIP_WARNING << "Received a VSOMEIP_OFFERED_SERVICES_RESPONSE command with invalid size -> skip!";
                break;
            }
            if (!its_security->is_enabled() || message_from_routing) {
                on_offered_services_info(&_data[VSOMEIP_COMMAND_PAYLOAD_POS], its_length);
            } else {
                VSOMEIP_WARNING << std::hex << "Security: Client 0x" << get_client()
                        << " received an offered services info from a client which isn't the routing manager"
                        << " : Skip message!";
            }
            break;
        case VSOMEIP_RESEND_PROVIDED_EVENTS: {
            if (_size != VSOMEIP_RESEND_PROVIDED_EVENTS_COMMAND_SIZE) {
                VSOMEIP_WARNING << "Received a RESEND_PROVIDED_EVENTS command with wrong size ~> skip!";
                break;
            }
            pending_remote_offer_id_t its_pending_remote_offer_id(0);
            std::memcpy(&its_pending_remote_offer_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(pending_remote_offer_id_t));
            resend_provided_event_registrations();
            send_resend_provided_event_response(its_pending_remote_offer_id);
            VSOMEIP_INFO << "RESEND_PROVIDED_EVENTS("
                    << std::hex << std::setw(4) << std::setfill('0')
                    << its_client << ")";
            break;
        }
        case VSOMEIP_UPDATE_SECURITY_POLICY_INT:
            is_internal_policy_update = true;
            /* Fallthrough */
        case VSOMEIP_UPDATE_SECURITY_POLICY: {
            if (_size < VSOMEIP_COMMAND_HEADER_SIZE + sizeof(pending_security_update_id_t) ||
                    _size - VSOMEIP_COMMAND_HEADER_SIZE != its_length) {
                VSOMEIP_WARNING << "vSomeIP Security: Received a VSOMEIP_UPDATE_SECURITY_POLICY command with wrong size -> skip!";
                break;
            }
            if (!its_security->is_enabled() || message_from_routing) {
                pending_security_update_id_t its_update_id(0);

                std::memcpy(&its_update_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(pending_security_update_id_t));

                std::shared_ptr<policy> its_policy(std::make_shared<policy>());
                const byte_t *its_policy_data = _data + (VSOMEIP_COMMAND_PAYLOAD_POS +
                                                    sizeof(pending_security_update_id_t));

                uint32_t its_policy_size = uint32_t(_size - (VSOMEIP_COMMAND_PAYLOAD_POS
                        + sizeof(pending_security_update_id_t)));

                bool is_valid = its_policy->deserialize(its_policy_data, its_policy_size);
                if (is_valid) {
                    uint32_t its_uid;
                    uint32_t its_gid;
                    is_valid = its_policy->get_uid_gid(its_uid, its_gid);
                    if (is_valid) {
                        if (is_internal_policy_update
                                || its_security->is_policy_update_allowed(its_uid, its_policy)) {
                            its_security->update_security_policy(its_uid, its_gid, its_policy);
                            send_update_security_policy_response(its_update_id);
                        }
                    } else {
                        VSOMEIP_ERROR << "vSomeIP Security: Policy has no valid uid/gid!";
                    }
                } else {
                    VSOMEIP_ERROR << "vSomeIP Security: Policy deserialization failed!";
                }
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                        << " : routing_manager_proxy::on_message: "
                        << " received a security policy update from a client which isn't the routing manager"
                        << " : Skip message!";
            }
            break;
        }
        case VSOMEIP_REMOVE_SECURITY_POLICY: {
            if (_size != VSOMEIP_REMOVE_SECURITY_POLICY_COMMAND_SIZE) {
                VSOMEIP_WARNING << "vSomeIP Security: Received a VSOMEIP_REMOVE_SECURITY_POLICY command with wrong size ~> skip!";
                break;
            }
            if (!its_security->is_enabled() || message_from_routing) {
                pending_security_update_id_t its_update_id(0);
                uint32_t its_uid(ANY_UID);
                uint32_t its_gid(ANY_GID);

                std::memcpy(&its_update_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(pending_security_update_id_t));
                std::memcpy(&its_uid, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(uint32_t));
                std::memcpy(&its_gid, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                        sizeof(uint32_t));
                if (its_security->is_policy_removal_allowed(its_uid)) {
                    its_security->remove_security_policy(its_uid, its_gid);
                    send_remove_security_policy_response(its_update_id);
                }
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                        << " : routing_manager_proxy::on_message: "
                        << "received a security policy removal from a client which isn't the routing manager"
                        << " : Skip message!";
            }
            break;
        }
        case VSOMEIP_DISTRIBUTE_SECURITY_POLICIES: {
            if (_size < VSOMEIP_COMMAND_HEADER_SIZE ||
                    _size - VSOMEIP_COMMAND_HEADER_SIZE != its_length) {
                VSOMEIP_WARNING << "vSomeIP Security: Received a VSOMEIP_DISTRIBUTE_SECURITY_POLICIES command with wrong size -> skip!";
                break;
            }
            if (!its_security->is_enabled() || message_from_routing) {
                uint32_t its_policy_count(0);
                uint32_t its_policy_size(0);
                const byte_t* buffer_ptr = 0;

                if (VSOMEIP_COMMAND_PAYLOAD_POS + sizeof(uint32_t) * 2 <= _size) {
                    std::memcpy(&its_policy_count, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(uint32_t));

                    // skip policy count field
                    buffer_ptr = _data + (VSOMEIP_COMMAND_PAYLOAD_POS +
                            sizeof(uint32_t));

                    for (uint32_t i = 0; i < its_policy_count; i++) {
                        uint32_t its_uid(0);
                        uint32_t its_gid(0);
                        std::shared_ptr<policy> its_policy(std::make_shared<policy>());
                        // length field of next (UID/GID + policy)
                        if (buffer_ptr + sizeof(uint32_t) <= _data + _size) {
                            std::memcpy(&its_policy_size, buffer_ptr,
                                    sizeof(uint32_t));
                            buffer_ptr += sizeof(uint32_t);

                            if (buffer_ptr + its_policy_size <= _data + _size) {
                                if (its_security->parse_policy(buffer_ptr, its_policy_size, its_uid, its_gid, its_policy)) {
                                    if (its_security->is_policy_update_allowed(its_uid, its_policy)) {
                                        its_security->update_security_policy(its_uid, its_gid, its_policy);
                                    }
                                } else {
                                    VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client() << " could not parse policy!";
                                }
                            }
                        }
                    }
                }
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                        << " : routing_manager_proxy::on_message: "
                        << " received a security policy distribution command from a client which isn't the routing manager"
                        << " : Skip message!";
            }
            break;
        }
        case VSOMEIP_UPDATE_SECURITY_CREDENTIALS: {
            if (_size < VSOMEIP_COMMAND_HEADER_SIZE ||
                    _size - VSOMEIP_COMMAND_HEADER_SIZE != its_length) {
                VSOMEIP_WARNING << "vSomeIP Security: Received a VSOMEIP_UPDATE_SECURITY_CREDENTIALS command with wrong size -> skip!";
                break;
            }
            if (!its_security->is_enabled() || message_from_routing) {
                on_update_security_credentials(&_data[VSOMEIP_COMMAND_PAYLOAD_POS], its_length);
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << get_client()
                        << " : routing_manager_proxy::on_message: "
                        << "received a security credential update from a client which isn't the routing manager"
                        << " : Skip message!";
            }
            break;
        }
        default:
            break;
        }
    }
}

void routing_manager_proxy::on_routing_info(const byte_t *_data,
        uint32_t _size) {
#if 0
    std::stringstream msg;
    msg << "rmp::on_routing_info(" << std::hex << client_ << "): ";
    for (uint32_t i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    auto its_security = security_impl::get();
    if (!its_security)
        return;

    uint32_t i = 0;
    while (i + sizeof(uint32_t) + sizeof(routing_info_entry_e) <= _size) {
        routing_info_entry_e routing_info_entry;
        std::memcpy(&routing_info_entry, &_data[i], sizeof(routing_info_entry_e));
        i += uint32_t(sizeof(routing_info_entry_e));

        uint32_t its_client_size;
        std::memcpy(&its_client_size, &_data[i], sizeof(uint32_t));
        i += uint32_t(sizeof(uint32_t));

        if (its_client_size + i > _size) {
            VSOMEIP_WARNING << "Client 0x" << std::hex << get_client() << " : "
                    << "Processing of routing info failed due to bad length fields!";
            return;
        }

        if (i + sizeof(client_t) <= _size) {
            client_t its_client;
            std::memcpy(&its_client, &_data[i], sizeof(client_t));
            i += uint32_t(sizeof(client_t));

            if (routing_info_entry == routing_info_entry_e::RIE_ADD_CLIENT) {
                {
                    std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
                    known_clients_.insert(its_client);
                }
                if (its_client == get_client()) {
                    VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
                                << " (" << host_->get_name() << ") is registered.";

#ifndef _WIN32
                    if (!its_security->check_credentials(get_client(), own_uid_, own_gid_)) {
                        VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << std::hex << get_client()
                                << " : routing_manager_proxy::on_routing_info: RIE_ADD_CLIENT: isn't allowed"
                                << " to use the server endpoint due to credential check failed!";
                        deregister_application();
                        host_->on_state(static_cast<state_type_e>(inner_state_type_e::ST_DEREGISTERED));
                        return;
                    }
#endif
                    {
                        std::lock_guard<std::mutex> its_lock(state_mutex_);
                        if (state_ == inner_state_type_e::ST_REGISTERING) {
                            boost::system::error_code ec;
                            register_application_timer_.cancel(ec);
                            send_registered_ack();
                            send_pending_commands();
                            state_ = inner_state_type_e::ST_REGISTERED;
                            // Notify stop() call about clean deregistration
                            state_condition_.notify_one();
                        }
                    }

                    // inform host about its own registration state changes
                    if (state_ == inner_state_type_e::ST_REGISTERED)
                        host_->on_state(static_cast<state_type_e>(inner_state_type_e::ST_REGISTERED));

                }
            } else if (routing_info_entry == routing_info_entry_e::RIE_DEL_CLIENT) {
                {
                    std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
                    known_clients_.erase(its_client);
                }
                if (its_client == get_client()) {
                    its_security->remove_client_to_uid_gid_mapping(its_client);
                    VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
                                << " (" << host_->get_name() << ") is deregistered.";

                    // inform host about its own registration state changes
                    host_->on_state(static_cast<state_type_e>(inner_state_type_e::ST_DEREGISTERED));

                    {
                        std::lock_guard<std::mutex> its_lock(state_mutex_);
                        state_ = inner_state_type_e::ST_DEREGISTERED;
                        // Notify stop() call about clean deregistration
                        state_condition_.notify_one();
                    }
                } else if (its_client != VSOMEIP_ROUTING_CLIENT) {
                    remove_local(its_client, true);
                }
            }

            uint32_t j = 0;
            while (j + sizeof(uint32_t) <= its_client_size) {
                uint32_t its_services_size;
                std::memcpy(&its_services_size, &_data[i + j], sizeof(uint32_t));
                j += uint32_t(sizeof(uint32_t));

                if (its_services_size >= sizeof(service_t) + sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t)) {
                    its_services_size -= uint32_t(sizeof(service_t));

                    service_t its_service;
                    std::memcpy(&its_service, &_data[i + j], sizeof(service_t));
                    j += uint32_t(sizeof(service_t));

                    while (its_services_size >= sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t)) {
                        instance_t its_instance;
                        std::memcpy(&its_instance, &_data[i + j], sizeof(instance_t));
                        j += uint32_t(sizeof(instance_t));

                        major_version_t its_major;
                        std::memcpy(&its_major, &_data[i + j], sizeof(major_version_t));
                        j += uint32_t(sizeof(major_version_t));

                        minor_version_t its_minor;
                        std::memcpy(&its_minor, &_data[i + j], sizeof(minor_version_t));
                        j += uint32_t(sizeof(minor_version_t));

                        if (routing_info_entry == routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE) {
                            {
                                std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
                                known_clients_.insert(its_client);
                            }
                            {
                                std::lock_guard<std::mutex> its_lock(local_services_mutex_);
                                local_services_[its_service][its_instance] = std::make_tuple(its_major, its_minor, its_client);
                            }
                            {
                                std::lock_guard<std::mutex> its_lock(state_mutex_);
                                send_pending_subscriptions(its_service, its_instance, its_major);
                            }
                            host_->on_availability(its_service, its_instance, true, its_major, its_minor);
                            VSOMEIP_INFO << "ON_AVAILABLE("
                                << std::hex << std::setw(4) << std::setfill('0') << get_client() <<"): ["
                                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                                << std::hex << std::setw(4) << std::setfill('0') << its_instance
                                << ":" << std::dec << int(its_major) << "." << std::dec << its_minor << "]";
                        } else if (routing_info_entry == routing_info_entry_e::RIE_DEL_SERVICE_INSTANCE) {
                            {
                                std::lock_guard<std::mutex> its_lock(local_services_mutex_);
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
                            host_->on_availability(its_service, its_instance, false, its_major, its_minor);
                            VSOMEIP_INFO << "ON_UNAVAILABLE("
                                << std::hex << std::setw(4) << std::setfill('0') << get_client() <<"): ["
                                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                                << std::hex << std::setw(4) << std::setfill('0') << its_instance
                                << ":" << std::dec << int(its_major) << "." << std::dec << its_minor << "]";
                        }

                        its_services_size -= uint32_t(sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t) );
                    }
                }
            }

            i += j;
        }
    }
    {
        struct subscription_info {
            service_t service_id_;
            instance_t instance_id_;
            eventgroup_t eventgroup_id_;
            client_t client_id_;
            major_version_t major_;
            event_t event_;
            uid_t uid_;
            gid_t gid_;
        };
        std::lock_guard<std::recursive_mutex> its_lock(incoming_subscriptions_mutex_);
        std::forward_list<struct subscription_info> subscription_actions;
        if (pending_incoming_subscripitons_.size()) {
            {
                std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
                for (const client_t client : known_clients_) {
                    auto its_client = pending_incoming_subscripitons_.find(client);
                    if (its_client != pending_incoming_subscripitons_.end()) {
                        for (const auto& subscription : its_client->second) {
                            subscription_actions.push_front(
                                { subscription.service_, subscription.instance_,
                                        subscription.eventgroup_, client,
                                        subscription.major_, subscription.event_,
                                        subscription.uid_, subscription.gid_ });
                        }
                    }
                }
            }
            for (const subscription_info &si : subscription_actions) {
#ifdef VSOMEIP_ENABLE_COMPAT
                routing_manager_base::set_incoming_subscription_state(si.client_id_, si.service_id_, si.instance_id_,
                        si.eventgroup_id_, si.event_, subscription_state_e::IS_SUBSCRIBING);
#endif
                (void) ep_mgr_->find_or_create_local(si.client_id_);
                auto self = shared_from_this();
                host_->on_subscription(
                        si.service_id_, si.instance_id_, si.eventgroup_id_,
                        si.client_id_, si.uid_, si.gid_, true,
                        [this, self, si](const bool _subscription_accepted) {
                    if (!_subscription_accepted) {
                        send_subscribe_nack(si.client_id_, si.service_id_,
                                si.instance_id_, si.eventgroup_id_, si.event_, PENDING_SUBSCRIPTION_ID);
                    } else {
                        send_subscribe_ack(si.client_id_, si.service_id_,
                                si.instance_id_, si.eventgroup_id_, si.event_, PENDING_SUBSCRIPTION_ID);
                        routing_manager_base::subscribe(si.client_id_, si.uid_, si.gid_,
                                si.service_id_, si.instance_id_, si.eventgroup_id_,
                                si.major_, si.event_);
#ifdef VSOMEIP_ENABLE_COMPAT
                        send_pending_notify_ones(si.service_id_,
                                si.instance_id_, si.eventgroup_id_, si.client_id_);
#endif
                    }
#ifdef VSOMEIP_ENABLE_COMPAT
                    routing_manager_base::erase_incoming_subscription_state(si.client_id_, si.service_id_,
                            si.instance_id_, si.eventgroup_id_, si.event_);
#endif
                    {
                        std::lock_guard<std::recursive_mutex> its_lock2(incoming_subscriptions_mutex_);
                        pending_incoming_subscripitons_.erase(si.client_id_);
                    }
                });
            }
        }
    }
}

void routing_manager_proxy::on_offered_services_info(const byte_t *_data,
        uint32_t _size) {
#if 0
    std::stringstream msg;
    msg << "rmp::on_offered_services_info(" << std::hex << client_ << "): ";
    for (uint32_t i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif

    std::vector<std::pair<service_t, instance_t>> its_offered_services_info;

    uint32_t i = 0;
    while (i + sizeof(uint32_t) + sizeof(routing_info_entry_e) <= _size) {
        routing_info_entry_e routing_info_entry;
        std::memcpy(&routing_info_entry, &_data[i], sizeof(routing_info_entry_e));
        i += uint32_t(sizeof(routing_info_entry_e));

        uint32_t its_service_entry_size;
        std::memcpy(&its_service_entry_size, &_data[i], sizeof(uint32_t));
        i += uint32_t(sizeof(uint32_t));

        if (its_service_entry_size + i > _size) {
            VSOMEIP_WARNING << "Client 0x" << std::hex << get_client() << " : "
                    << "Processing of offered services info failed due to bad length fields!";
            return;
        }

        if (its_service_entry_size >= sizeof(service_t) + sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t)) {
            service_t its_service;
            std::memcpy(&its_service, &_data[i], sizeof(service_t));
            i += uint32_t(sizeof(service_t));

            instance_t its_instance;
            std::memcpy(&its_instance, &_data[i], sizeof(instance_t));
            i += uint32_t(sizeof(instance_t));

            major_version_t its_major;
            std::memcpy(&its_major, &_data[i], sizeof(major_version_t));
            i += uint32_t(sizeof(major_version_t));

            minor_version_t its_minor;
            std::memcpy(&its_minor, &_data[i], sizeof(minor_version_t));
            i += uint32_t(sizeof(minor_version_t));

            its_offered_services_info.push_back(std::make_pair(its_service, its_instance));
        }
    }
    host_->on_offered_services_info(its_offered_services_info);
}

void routing_manager_proxy::reconnect(const std::unordered_set<client_t> &_clients) {
    auto its_security = security_impl::get();
    if (!its_security)
        return;

    // inform host about its own registration state changes
    host_->on_state(static_cast<state_type_e>(inner_state_type_e::ST_DEREGISTERED));

    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        state_ = inner_state_type_e::ST_DEREGISTERED;
        // Notify stop() call about clean deregistration
        state_condition_.notify_one();
    }


    // Remove all local connections/endpoints
    for (const auto& its_client : _clients) {
        if (its_client != VSOMEIP_ROUTING_CLIENT) {
            remove_local(its_client, true);
        }
    }

    VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
            <<": Reconnecting to routing manager.";

#ifndef _WIN32
    if (!its_security->check_credentials(get_client(), own_uid_, own_gid_)) {
        VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << std::hex << get_client()
                << " :  routing_manager_proxy::reconnect: isn't allowed"
                << " to use the server endpoint due to credential check failed!";
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->stop();
        }
        return;
    }
#endif

    std::lock_guard<std::mutex> its_lock(sender_mutex_);
    if (sender_) {
        sender_->restart();
    }
}

void routing_manager_proxy::assign_client() {
    std::vector<byte_t> its_command;

    std::string its_name(host_->get_name());
    uint32_t its_size(static_cast<uint32_t>(its_name.size()));
    its_command.resize(7 + its_name.size());

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_ASSIGN_CLIENT;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    if (0 < its_name.size())
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], its_name.c_str(),
                its_name.size());

    std::lock_guard<std::mutex> its_state_lock(state_mutex_);
    if (is_connected_) {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            if (state_ != inner_state_type_e::ST_DEREGISTERED)
                return;
            state_ = inner_state_type_e::ST_ASSIGNING;

            sender_->send(&its_command[0], static_cast<uint32_t>(its_command.size()));

            boost::system::error_code ec;
            register_application_timer_.cancel(ec);
            register_application_timer_.expires_from_now(std::chrono::milliseconds(10000));
            register_application_timer_.async_wait(
                    std::bind(
                            &routing_manager_proxy::assign_client_timeout_cbk,
                            std::dynamic_pointer_cast<routing_manager_proxy>(shared_from_this()),
                            std::placeholders::_1));
        }
    }
}

void routing_manager_proxy::register_application() {
    byte_t its_command[] = {
            VSOMEIP_REGISTER_APPLICATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));

    if (is_connected_) {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            state_ = inner_state_type_e::ST_REGISTERING;
            sender_->send(its_command, sizeof(its_command));

            register_application_timer_.cancel();
            register_application_timer_.expires_from_now(std::chrono::milliseconds(1000));
            register_application_timer_.async_wait(
                    std::bind(
                            &routing_manager_proxy::register_application_timeout_cbk,
                            std::dynamic_pointer_cast<routing_manager_proxy>(shared_from_this()),
                            std::placeholders::_1));
        }
    }
}

void routing_manager_proxy::deregister_application() {
    std::vector<byte_t> its_command(VSOMEIP_COMMAND_HEADER_SIZE, 0);
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_DEREGISTER_APPLICATION;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));
    if (is_connected_)
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(&its_command[0], uint32_t(its_command.size()));
        }
    }
}

void routing_manager_proxy::send_pong() const {
    byte_t its_pong[] = {
    VSOMEIP_PONG, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

    std::memcpy(&its_pong[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_t));

    if (is_connected_) {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_pong, sizeof(its_pong));
        }
    }
}

void routing_manager_proxy::send_request_services(std::set<service_data_t>& _requests) {
    if (!_requests.size()) {
        return;
    }
    size_t its_size = (VSOMEIP_REQUEST_SERVICE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE) * _requests.size();
    if (its_size > (std::numeric_limits<std::uint32_t>::max)()) {
        VSOMEIP_ERROR<< "routing_manager_proxy::send_request_services too many"
                << " requests (" << std::dec << its_size << "), returning.";
        return;
    }

    std::vector<byte_t> its_command(its_size + VSOMEIP_COMMAND_HEADER_SIZE);
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_REQUEST_SERVICE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS],
            &client_, sizeof(client_));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN],
            &its_size, sizeof(std::uint32_t));

    uint32_t entry_size = (sizeof(service_t) + sizeof(instance_t)
            + sizeof(major_version_t) + sizeof(minor_version_t));

    unsigned int i = 0;
    for (auto its_service : _requests) {
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + (i * entry_size)],
                &its_service.service_, sizeof(its_service.service_));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2 + (i * entry_size)],
                &its_service.instance_, sizeof(its_service.instance_));
        its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4 + (i * entry_size)] = its_service.major_;
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 5 + (i * entry_size)],
                &its_service.minor_, sizeof(its_service.minor_));
        ++i;
    }

    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(&its_command[0],
                    static_cast<std::uint32_t>(its_size + VSOMEIP_COMMAND_HEADER_SIZE));
        }
    }
}

void routing_manager_proxy::send_release_service(client_t _client, service_t _service,
        instance_t _instance) {
    (void)_client;
    byte_t its_command[VSOMEIP_RELEASE_SERVICE_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_RELEASE_SERVICE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_RELEASE_SERVICE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));

    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::send_register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _notifier,
        const std::set<eventgroup_t> &_eventgroups, const event_type_e _type,
        reliability_type_e _reliability,
        bool _is_provided) {

    std::size_t its_eventgroups_size = (_eventgroups.size() * sizeof(eventgroup_t)) +
            VSOMEIP_REGISTER_EVENT_COMMAND_SIZE;
    if (its_eventgroups_size > (std::numeric_limits<std::uint32_t>::max)()) {
        VSOMEIP_ERROR<< "routing_manager_proxy::send_register_event too many"
                << " eventgroups (" << std::dec << its_eventgroups_size << "), returning.";
        return;
    }
    byte_t *its_command = new byte_t[its_eventgroups_size];
    uint32_t its_size = static_cast<std::uint32_t>(its_eventgroups_size)
            - VSOMEIP_COMMAND_HEADER_SIZE;

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_REGISTER_EVENT;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_notifier,
            sizeof(_notifier));
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6]
                = static_cast<byte_t>(_type);
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 7]
                = static_cast<byte_t>(_is_provided);
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8]
                = static_cast<byte_t>(_reliability);

    std::size_t i = 9;
    for (auto eg : _eventgroups) {
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + i], &eg,
            sizeof(eventgroup_t));
        i += sizeof(eventgroup_t);
    }

    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, static_cast<std::uint32_t>(its_eventgroups_size));
        }
    }

    if (_is_provided) {
        VSOMEIP_INFO << "REGISTER EVENT("
            << std::hex << std::setw(4) << std::setfill('0') << client_ << "): ["
            << std::hex << std::setw(4) << std::setfill('0') << _service << "."
            << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
            << std::hex << std::setw(4) << std::setfill('0') << _notifier
            << ":is_provider=" << _is_provided << "]";
    }

    delete[] its_command;
}

void routing_manager_proxy::on_subscribe_ack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event) {
    (void)_client;
#if 0
    VSOMEIP_ERROR << "routing_manager_proxy::" << __func__
            << "(" << std::hex << host_->get_client() << "):"
            << "event="
            << std::hex << _service << "."
            << std::hex << _instance << "."
            << std::hex << _eventgroup << "."
            << std::hex << _event;
#endif
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

void routing_manager_proxy::on_subscribe_nack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event) {
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

void routing_manager_proxy::cache_event_payload(
        const std::shared_ptr<message> &_message) {
    const service_t its_service(_message->get_service());
    const instance_t its_instance(_message->get_instance());
    const method_t its_method(_message->get_method());
    std::shared_ptr<event> its_event = find_event(its_service, its_instance, its_method);
    if (its_event) {
        if (its_event->is_field()) {
            its_event->set_payload_dont_notify(_message->get_payload());
        }
    } else {
        // we received a event which was not yet requested
        std::set<eventgroup_t> its_eventgroups;
        // create a placeholder field until someone requests this event with
        // full information like eventgroup, field or not etc.
        routing_manager_base::register_event(host_->get_client(),
                its_service, its_instance,
                its_method,
                its_eventgroups, event_type_e::ET_UNKNOWN,
                reliability_type_e::RT_UNKNOWN,
                std::chrono::milliseconds::zero(), false, true,
                nullptr,
                false, false, true);
        std::shared_ptr<event> its_event = find_event(its_service, its_instance, its_method);
        if (its_event) {
                its_event->set_payload_dont_notify(_message->get_payload());
        }
    }

}

void routing_manager_proxy::on_stop_offer_service(service_t _service,
                                                  instance_t _instance,
                                                  major_version_t _major,
                                                  minor_version_t _minor) {
    (void) _major;
    (void) _minor;
    std::map<event_t, std::shared_ptr<event> > events;
    {
        std::lock_guard<std::mutex> its_lock(events_mutex_);
        auto its_events_service = events_.find(_service);
        if (its_events_service != events_.end()) {
            auto its_events_instance = its_events_service->second.find(_instance);
            if (its_events_instance != its_events_service->second.end()) {
                for (auto &e : its_events_instance->second)
                    events[e.first] = e.second;
            }
        }
    }
    for (auto &e : events) {
        e.second->unset_payload();
    }
}

void routing_manager_proxy::send_pending_commands() {
    for (auto &po : pending_offers_)
        send_offer_service(client_,
                po.service_, po.instance_,
                po.major_, po.minor_);

    for (auto &per : pending_event_registrations_)
        send_register_event(client_,
                per.service_, per.instance_,
                per.notifier_,
                per.eventgroups_, per.type_, per.reliability_,
                per.is_provided_);

    send_request_services(requests_);
}

void routing_manager_proxy::init_receiver() {
#ifndef _WIN32
    auto its_security = security_impl::get();
    if (!its_security)
        return;

    its_security->store_client_to_uid_gid_mapping(get_client(), own_uid_, own_gid_);
    its_security->store_uid_gid_to_client_mapping(own_uid_, own_gid_, get_client());
#endif
    receiver_ = ep_mgr_->create_local_server(shared_from_this());
}

void routing_manager_proxy::notify_remote_initially(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, const std::set<event_t> &_events_to_exclude) {
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        auto service_info = find_service(_service, _instance);
        for (const auto &e : its_eventgroup->get_events()) {
            if (e->is_field() && e->is_set()
                    && _events_to_exclude.find(e->get_event())
                            == _events_to_exclude.end()) {
                std::shared_ptr<message> its_notification
                    = runtime::get()->create_notification();
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
                        std::lock_guard<std::mutex> its_lock(sender_mutex_);
                        if (sender_) {
                            send_local(sender_, VSOMEIP_ROUTING_CLIENT, its_serializer->get_data(),
                                    its_serializer->get_size(), _instance, false, VSOMEIP_NOTIFY);
                        }
                    }
                    its_serializer->reset();
                    put_serializer(its_serializer);
                } else {
                    VSOMEIP_ERROR << "Failed to serialize message. Check message size!";
                }
            }
        }
    }

}

uint32_t routing_manager_proxy::get_remote_subscriber_count(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, bool _increment) {
    std::lock_guard<std::mutex> its_lock(remote_subscriber_count_mutex_);
    uint32_t count (0);
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

void routing_manager_proxy::clear_remote_subscriber_count(
        service_t _service, instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(remote_subscriber_count_mutex_);
    auto found_service = remote_subscriber_count_.find(_service);
    if (found_service != remote_subscriber_count_.end()) {
        if (found_service->second.erase(_instance)) {
            if (!found_service->second.size()) {
                remote_subscriber_count_.erase(found_service);
            }
        }
    }
}

void
routing_manager_proxy::assign_client_timeout_cbk(
        boost::system::error_code const &_error) {
    if (!_error) {
        bool register_again(false);
        {
            std::lock_guard<std::mutex> its_lock(state_mutex_);
            if (state_ != inner_state_type_e::ST_REGISTERED) {
                state_ = inner_state_type_e::ST_DEREGISTERED;
                register_again = true;
            }
        }
        if (register_again) {
            std::lock_guard<std::mutex> its_lock(sender_mutex_);
            VSOMEIP_WARNING << std::hex << "Client 0x" << get_client()
                    << " request client timeout! Trying again...";

            if (sender_) {
                sender_->restart();
            }
        }
    }
}

void routing_manager_proxy::register_application_timeout_cbk(
        boost::system::error_code const &_error) {

    bool register_again(false);
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (!_error && state_ != inner_state_type_e::ST_REGISTERED) {
            state_ = inner_state_type_e::ST_DEREGISTERED;
            register_again = true;
        }
    }
    if (register_again) {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        VSOMEIP_WARNING << std::hex << "Client 0x" << get_client()
            << " register timeout! Trying again...";

        if (sender_)
            sender_->restart();
    }
}

void routing_manager_proxy::send_registered_ack() {
    byte_t its_command[VSOMEIP_COMMAND_HEADER_SIZE] = {
            VSOMEIP_REGISTERED_ACK, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    client_t client = get_client();
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client,
            sizeof(client));
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, VSOMEIP_COMMAND_HEADER_SIZE);
        }
    }
}

bool routing_manager_proxy::is_client_known(client_t _client) {
    std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
    return (known_clients_.find(_client) != known_clients_.end());
}

bool routing_manager_proxy::create_placeholder_event_and_subscribe(
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, event_t _notifier, client_t _client) {

    std::lock_guard<std::mutex> its_lock(stop_mutex_);

    bool is_inserted(false);

    if (find_service(_service, _instance)) {
        // We received an event for an existing service which was not yet
        // requested/offered. Create a placeholder field until someone
        // requests/offers this event with full information like eventgroup,
        // field/event, etc.
        std::set<eventgroup_t> its_eventgroups({ _eventgroup });
        // routing_manager_proxy: Always register with own client id and shadow = false
        routing_manager_base::register_event(host_->get_client(),
                _service, _instance, _notifier,
                its_eventgroups, event_type_e::ET_UNKNOWN, reliability_type_e::RT_UNKNOWN,
                std::chrono::milliseconds::zero(), false, true, nullptr, false, false,
                true);

        std::shared_ptr<event> its_event = find_event(_service, _instance, _notifier);
        if (its_event) {
            is_inserted = its_event->add_subscriber(_eventgroup, _client, false);
        }
    }

    return is_inserted;
}

void routing_manager_proxy::request_debounce_timeout_cbk(
        boost::system::error_code const &_error) {
    std::lock_guard<std::mutex> its_lock(state_mutex_);
    if (!_error) {
        if (requests_to_debounce_.size()) {
            if (state_ == inner_state_type_e::ST_REGISTERED) {
                send_request_services(requests_to_debounce_);
                requests_.insert(requests_to_debounce_.begin(),
                        requests_to_debounce_.end());
                requests_to_debounce_.clear();
            } else {
                {
                    std::lock_guard<std::mutex> its_lock(request_timer_mutex_);
                    request_debounce_timer_running_ = true;
                    request_debounce_timer_.expires_from_now(std::chrono::milliseconds(
                            configuration_->get_request_debouncing(host_->get_name())));
                    request_debounce_timer_.async_wait(
                            std::bind(
                                    &routing_manager_proxy::request_debounce_timeout_cbk,
                                    std::dynamic_pointer_cast<routing_manager_proxy>(shared_from_this()),
                                    std::placeholders::_1));
                    return;
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(request_timer_mutex_);
        request_debounce_timer_running_ = false;
    }
}

void routing_manager_proxy::register_client_error_handler(client_t _client,
        const std::shared_ptr<endpoint> &_endpoint) {
    _endpoint->register_error_handler(
            std::bind(&routing_manager_proxy::handle_client_error, this, _client));
}

void routing_manager_proxy::handle_client_error(client_t _client) {
    if (_client != VSOMEIP_ROUTING_CLIENT) {
        VSOMEIP_INFO << "Client 0x" << std::hex << get_client()
                << " handles a client error(" << std::hex << _client << ")";
        remove_local(_client, true);
    } else {
        bool should_reconnect(true);
        {
            std::unique_lock<std::mutex> its_lock(state_mutex_);
            should_reconnect = is_started_;
        }
        if (should_reconnect) {
            std::unordered_set<client_t> its_known_clients;
            {
                std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
                its_known_clients = known_clients_;
            }
           reconnect(its_known_clients);
        }
    }
}

void routing_manager_proxy::send_get_offered_services_info(client_t _client, offer_type_e _offer_type) {
    (void)_client;

    byte_t its_command[VSOMEIP_OFFERED_SERVICES_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_OFFERED_SERVICES_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_OFFERED_SERVICES_REQUEST;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
            sizeof(_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_offer_type,
                sizeof(_offer_type));

    std::lock_guard<std::mutex> its_lock(sender_mutex_);
    if (sender_) {
        sender_->send(its_command, sizeof(its_command));
    }
}

void routing_manager_proxy::send_unsubscribe_ack(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        remote_subscription_id_t _id) {
    byte_t its_command[VSOMEIP_UNSUBSCRIBE_ACK_COMMAND_SIZE];
    const std::uint32_t its_size = VSOMEIP_UNSUBSCRIBE_ACK_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    const client_t its_client = get_client();
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_UNSUBSCRIBE_ACK;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &its_client,
            sizeof(its_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
            sizeof(_eventgroup));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &_id,
            sizeof(_id));

    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::resend_provided_event_registrations() {
    std::lock_guard<std::mutex> its_lock(state_mutex_);
    for (const event_data_t& ed : pending_event_registrations_) {
        if (ed.is_provided_) {
            send_register_event(client_, ed.service_, ed.instance_,
                    ed.notifier_, ed.eventgroups_, ed.type_, ed.reliability_,
                    ed.is_provided_);
        }
    }
}

void routing_manager_proxy::send_resend_provided_event_response(pending_remote_offer_id_t _id) {
    byte_t its_command[VSOMEIP_RESEND_PROVIDED_EVENTS_COMMAND_SIZE];
    const std::uint32_t its_size = VSOMEIP_RESEND_PROVIDED_EVENTS_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    const client_t its_client = get_client();
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_RESEND_PROVIDED_EVENTS;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &its_client,
            sizeof(its_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_id,
            sizeof(pending_remote_offer_id_t));
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::send_update_security_policy_response(pending_security_update_id_t _update_id) {
    byte_t its_command[VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE];
    const std::uint32_t its_size = VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    const client_t its_client = get_client();
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &its_client,
            sizeof(its_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_update_id,
            sizeof(pending_security_update_id_t));
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::send_remove_security_policy_response(pending_security_update_id_t _update_id) {
    byte_t its_command[VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE];
    const std::uint32_t its_size = VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    const client_t its_client = get_client();
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &its_client,
            sizeof(its_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_update_id,
            sizeof(pending_security_update_id_t));
    {
        std::lock_guard<std::mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::on_update_security_credentials(const byte_t *_data, uint32_t _size) {
    auto its_security = security_impl::get();
    if (!its_security)
        return;

    uint32_t i = 0;
    while ( (i + sizeof(uint32_t) + sizeof(uint32_t)) <= _size) {
        std::shared_ptr<policy> its_policy(std::make_shared<policy>());

        boost::icl::interval_set<uint32_t> its_gid_set;
        uint32_t its_uid, its_gid;

        std::memcpy(&its_uid, &_data[i], sizeof(uint32_t));
        i += uint32_t(sizeof(uint32_t));
        std::memcpy(&its_gid, &_data[i], sizeof(uint32_t));
        i += uint32_t(sizeof(uint32_t));

        its_gid_set.insert(its_gid);

        its_policy->credentials_ += std::make_pair(
                boost::icl::interval<uid_t>::closed(its_uid, its_uid), its_gid_set);
        its_policy->allow_who_ = true;
        its_policy->allow_what_ = true;

        its_security->add_security_credentials(its_uid, its_gid, its_policy, get_client());
    }
}

void routing_manager_proxy::on_client_assign_ack(const client_t &_client) {
    std::lock_guard<std::mutex> its_lock(state_mutex_);
    if (state_ == inner_state_type_e::ST_ASSIGNING) {
        if (_client != VSOMEIP_CLIENT_UNSET) {
            state_ = inner_state_type_e::ST_ASSIGNED;

            boost::system::error_code ec;
            register_application_timer_.cancel(ec);
            host_->set_client(_client);
            client_ = _client;

            if (is_started_) {
                init_receiver();
                if (receiver_) {
                    receiver_->start();

                    VSOMEIP_INFO << std::hex << "Client " << client_
                        << " (" << host_->get_name()
                        << ") successfully connected to routing  ~> registering..";
                    register_application();
                } else {
                    state_ = inner_state_type_e::ST_DEREGISTERED;

                    host_->set_client(VSOMEIP_CLIENT_UNSET);
                    client_ = VSOMEIP_CLIENT_UNSET;

                    sender_->restart();
                }
            }
        } else {
            VSOMEIP_ERROR << "Didn't receive valid clientID! Won't register application.";
        }
    } else {
        VSOMEIP_WARNING << "Client " << std::hex << client_
                << " received another client identifier ("
                << std::hex << _client
                << "). Ignoring it. ("
                << (int)state_ << ")";
    }
}

}  // namespace vsomeip_v3

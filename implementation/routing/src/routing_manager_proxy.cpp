// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <climits>
#include <iomanip>
#include <mutex>
#include <unordered_set>
#include <future>
#include <forward_list>

#ifndef WIN32
// for umask
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <vsomeip/constants.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/event.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_proxy.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/local_client_endpoint_impl.hpp"
#include "../../endpoints/include/local_server_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

routing_manager_proxy::routing_manager_proxy(routing_manager_host *_host) :
        routing_manager_base(_host),
        is_connected_(false),
        is_started_(false),
        state_(inner_state_type_e::ST_DEREGISTERED),
        sender_(0),
        receiver_(0),
        register_application_timer_(io_),
        logger_(logger::get())
{
}

routing_manager_proxy::~routing_manager_proxy() {
}

void routing_manager_proxy::init() {
    routing_manager_base::init();

    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        sender_ = create_local(VSOMEIP_ROUTING_CLIENT);
    }

    init_receiver();
}

void routing_manager_proxy::start() {
    is_started_ = true;

    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (!sender_) {
            // application has been stopped and started again
            sender_ = create_local(VSOMEIP_ROUTING_CLIENT);
        }
    }
    if (!receiver_) {
        // application has been stopped and started again
        init_receiver();
    }

    if (sender_) {
        sender_->start();
    }

    if (receiver_)
        receiver_->start();
}

void routing_manager_proxy::stop() {
    std::unique_lock<std::mutex> its_lock(state_mutex_);
    if (state_ == inner_state_type_e::ST_REGISTERING) {
        register_application_timer_.cancel();
    }
    while (state_ == inner_state_type_e::ST_REGISTERING) {
        std::cv_status status = state_condition_.wait_for(its_lock, std::chrono::milliseconds(1000));
        if (status == std::cv_status::timeout) {
            VSOMEIP_WARNING << std::hex << client_ << " registering timeout on stop";
            break;
        }
    }

    if (state_ == inner_state_type_e::ST_REGISTERED) {
        deregister_application();
        // Waiting de-register acknowledge to synchronize shutdown
        while (state_ == inner_state_type_e::ST_REGISTERED) {
            std::cv_status status = state_condition_.wait_for(its_lock, std::chrono::milliseconds(1000));
            if (status == std::cv_status::timeout) {
                VSOMEIP_WARNING << std::hex << client_ << " couldn't deregister application - timeout";
                break;
            }
        }
    }
    is_started_ = false;
    its_lock.unlock();

    if (receiver_) {
        receiver_->stop();
    }

    if (sender_) {
        sender_->stop();
    }

    for (auto client: get_connected_clients()) {
        if (client != VSOMEIP_ROUTING_CLIENT) {
            remove_local(client);
        }
    }

    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        // delete the sender
        sender_ = nullptr;
    }
    receiver_ = nullptr;
    std::stringstream its_client;
    its_client << VSOMEIP_BASE_PATH << std::hex << client_;
#ifdef WIN32
    ::_unlink(its_client.str().c_str());
#else
    if (-1 == ::unlink(its_client.str().c_str())) {
        VSOMEIP_ERROR<< "routing_manager_proxy::stop unlink failed ("
                << its_client.str() << "): "<< std::strerror(errno);
    }
#endif
}

const std::shared_ptr<configuration> routing_manager_proxy::get_configuration() const {
    return host_->get_configuration();
}

bool routing_manager_proxy::offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    if(!routing_manager_base::offer_service(_client, _service, _instance, _major, _minor)) {
        VSOMEIP_WARNING << "routing_manager_proxy::offer_service,"
                << "routing_manager_base::offer_service returned false";
    }
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            send_offer_service(_client, _service, _instance, _major, _minor);
        }
        service_data_t offer = { _service, _instance, _major, _minor, false };
        pending_offers_.insert(offer);
    }
    return true;
}

void routing_manager_proxy::send_offer_service(client_t _client,
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor) {
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
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {
    (void)_client;

    routing_manager_base::stop_offer_service(_client, _service, _instance, _major, _minor);

    // Reliable/Unreliable unimportant as routing_proxy does not
    // create server endpoints which needs to be freed
    clear_service_info(_service, _instance, false);

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
                std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
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
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, bool _use_exclusive_proxy) {
    routing_manager_base::request_service(_client, _service, _instance, _major,
            _minor, _use_exclusive_proxy);
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            send_request_service(_client, _service, _instance, _major, _minor,
                    _use_exclusive_proxy);
        }
        service_data_t request = { _service, _instance, _major, _minor, _use_exclusive_proxy };
        pending_requests_.insert(request);
    }
}

void routing_manager_proxy::release_service(client_t _client,
        service_t _service, instance_t _instance) {
    routing_manager_base::release_service(_client, _service, _instance);
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            send_release_service(_client, _service, _instance);
        }
        auto it = pending_requests_.begin();
        while (it != pending_requests_.end()) {
            if (it->service_ == _service
             && it->instance_ == _instance) {
                break;
            }
            it++;
        }
        if (it != pending_requests_.end()) pending_requests_.erase(it);
    }
}

void routing_manager_proxy::register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, const std::set<eventgroup_t> &_eventgroups,
        bool _is_field,
        std::chrono::milliseconds _cycle, bool _change_resets_cycle,
        epsilon_change_func_t _epsilon_change_func,
        bool _is_provided, bool _is_shadow, bool _is_cache_placeholder) {

    (void)_is_shadow;
    (void)_is_cache_placeholder;

    routing_manager_base::register_event(_client, _service, _instance,
            _event,_eventgroups, _is_field,
            _cycle, _change_resets_cycle,
            _epsilon_change_func,
            _is_provided);

    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            send_register_event(client_, _service, _instance,
                    _event, _eventgroups, _is_field, _is_provided);
        }
        event_data_t registration = {
                _service,
                _instance,
                _event,
                _is_field,
                _is_provided,
                _eventgroups
        };
        pending_event_registrations_.insert(registration);
    }
}

void routing_manager_proxy::unregister_event(client_t _client,
        service_t _service, instance_t _instance, event_t _event,
        bool _is_provided) {

    routing_manager_base::unregister_event(_client, _service, _instance,
            _event, _is_provided);

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
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_event,
                    sizeof(_event));
            its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6]
                        = static_cast<byte_t>(_is_provided);

            {
                std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
                if (sender_) {
                    sender_->send(its_command, sizeof(its_command));
                }
            }
        }
        auto it = pending_event_registrations_.begin();
        while (it != pending_event_registrations_.end()) {
            if (it->service_ == _service
                    && it->instance_ == _instance
                    && it->event_ == _event) {
                break;
            }
            it++;
        }
        if (it != pending_event_registrations_.end())
            pending_event_registrations_.erase(it);
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

void routing_manager_proxy::subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        subscription_type_e _subscription_type) {
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED && is_available(_service, _instance, _major)) {
            send_subscribe(_client, _service, _instance, _eventgroup, _major,
                    _subscription_type);
        }
        eventgroup_data_t subscription = { _service, _instance, _eventgroup, _major,
                _subscription_type};
        pending_subscriptions_.insert(subscription);
    }
}

void routing_manager_proxy::send_subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        subscription_type_e _subscription_type) {
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
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 7] = 0; // local subscriber
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8], &_subscription_type,
                sizeof(_subscription_type));

    client_t target_client = find_local_client(_service, _instance);
    if (target_client != VSOMEIP_ROUTING_CLIENT) {
        auto its_target = find_or_create_local(target_client);
        its_target->send(its_command, sizeof(its_command));
    } else {
        {
            std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
            if (sender_) {
                sender_->send(its_command, sizeof(its_command));
            }
        }
    }
}

void routing_manager_proxy::send_subscribe_nack(client_t _subscriber,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
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

    auto its_target = find_local(_subscriber);
    if (its_target) {
        its_target->send(its_command, sizeof(its_command));
    } else {
        {
            std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
            if (sender_) {
                sender_->send(its_command, sizeof(its_command));
            }
        }
    }
}

void routing_manager_proxy::send_subscribe_ack(client_t _subscriber,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
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

    auto its_target = find_local(_subscriber);
    if (its_target) {
        its_target->send(its_command, sizeof(its_command));
    } else {
        {
            std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
            if (sender_) {
                sender_->send(its_command, sizeof(its_command));
            }
        }
    }
}

void routing_manager_proxy::unsubscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    (void)_client;
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            bool is_remote(false);
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
            std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &is_remote,
                                sizeof(is_remote));

            auto its_target = find_local(_service, _instance);
            if (its_target) {
                its_target->send(its_command, sizeof(its_command));
            } else {
                {
                    std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
                    if (sender_) {
                        sender_->send(its_command, sizeof(its_command));
                    }
                };
            }
        }
        remove_pending_subscription(_service, _instance);
    }
}

bool routing_manager_proxy::send(client_t _client, const byte_t *_data,
        length_t _size, instance_t _instance,
        bool _flush,
        bool _reliable) {
    (void)_client;
    bool is_sent(false);
    bool has_remote_subscribers(false);
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
                    its_target = find_or_create_local(its_client);
                }
            }
        } else if (!utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            // Response
            client_t its_client = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_CLIENT_POS_MIN],
                    _data[VSOMEIP_CLIENT_POS_MAX]);
            if (its_client != VSOMEIP_ROUTING_CLIENT) {
                if (is_client_known(its_client)) {
                    its_target = find_or_create_local(its_client);
                }
            }
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) &&
                _client == VSOMEIP_ROUTING_CLIENT) {
            // notify
            has_remote_subscribers = send_local_notification(get_client(), _data, _size,
                    _instance, _flush, _reliable);
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) &&
                _client != VSOMEIP_ROUTING_CLIENT) {
            // notify_one
            its_target = find_local(_client);
            if (its_target) {
#ifdef USE_DLT
                const uint16_t its_data_size
                    = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                tc::trace_header its_header;
                if (its_header.prepare(nullptr, true))
                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                            _data, its_data_size);
#endif
                return send_local(its_target, get_client(), _data, _size,
                        _instance, _flush, _reliable, VSOMEIP_SEND);
            }
        }
        // If no direct endpoint could be found/is connected
        // or for notifications ~> route to routing_manager_stub
        if (!its_target || !its_target->is_connected()) {
            if (sender_) {
                its_target = sender_;
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
        else if (its_target != sender_) {
            const uint16_t its_data_size
                = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

            tc::trace_header its_header;
            if (its_header.prepare(nullptr, true))
                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                        _data, its_data_size);
        }
#endif
        if (send) {
            is_sent = send_local(its_target,
                    (command == VSOMEIP_NOTIFY_ONE ? _client : get_client()),
                    _data, _size, _instance, _flush, _reliable, command);
        }
    }
    return (is_sent);
}

bool routing_manager_proxy::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        std::shared_ptr<message> _message,
        bool _flush) {
    (void)_target;
    (void)_message;
    (void)_flush;
    return (false);
}

bool routing_manager_proxy::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size,
        bool _flush) {
    (void)_target;
    (void)_data;
    (void)_size;
    (void)_flush;
    return (false);
}

void routing_manager_proxy::notify(
        service_t _service, instance_t _instance, event_t _event,
        std::shared_ptr<payload> _payload, bool _force, bool _flush) {
    {
        std::lock_guard<std::mutex> its_lock(state_mutex_);
        if (state_ == inner_state_type_e::ST_REGISTERED) {
            routing_manager_base::notify(_service, _instance, _event, _payload, _force, _flush);
        }

        if (is_field(_service, _instance, _event)){
            std::shared_ptr<message> its_notification
                = runtime::get()->create_notification();
            its_notification->set_service(_service);
            its_notification->set_instance(_instance);
            its_notification->set_method(_event);
            its_notification->set_payload(_payload);
            auto service_info = find_service(_service, _instance);
            if (service_info) {
                its_notification->set_interface_version(service_info->get_major());
            }
            pending_notifications_[_service][_instance][_event] = its_notification;
        }
    }
}

void routing_manager_proxy::on_connect(std::shared_ptr<endpoint> _endpoint) {
    if (_endpoint != sender_) {
        return;
    }
    is_connected_ = true;
    if (is_connected_ && is_started_) {
        VSOMEIP_INFO << std::hex << "Client " << client_
                << " successfully connected to routing  ~> registering..";
        register_application();
    }
}

void routing_manager_proxy::on_disconnect(std::shared_ptr<endpoint> _endpoint) {
    is_connected_ = !(_endpoint == sender_);
    if (!is_connected_) {
        host_->on_state(state_type_e::ST_DEREGISTERED);
    }
}

void routing_manager_proxy::on_error(
        const byte_t *_data, length_t _length, endpoint *_receiver,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {

    // Implement me when needed

    (void)(_data);
    (void)(_length);
    (void)(_receiver);
    (void)(_remote_address);
    (void)(_remote_port);
}

void routing_manager_proxy::release_port(uint16_t _port, bool _reliable) {
    (void)_port;
    (void)_reliable;
    // intentionally empty
}

void routing_manager_proxy::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination,
        client_t _bound_client,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {
    (void)_receiver;
    (void)_destination;
    (void)_remote_address;
    (void)_remote_port;
#if 0
    std::stringstream msg;
    msg << "rmp::on_message: ";
    for (int i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    byte_t its_command;
    client_t its_client;
    length_t its_length;
    service_t its_service;
    instance_t its_instance;
    eventgroup_t its_eventgroup;
    major_version_t its_major;
    bool is_remote_subscriber;
    client_t routing_host_id = configuration_->get_id(configuration_->get_routing_host());

    if (_size > VSOMEIP_COMMAND_SIZE_POS_MAX) {
        its_command = _data[VSOMEIP_COMMAND_TYPE_POS];
        std::memcpy(&its_client, &_data[VSOMEIP_COMMAND_CLIENT_POS],
                sizeof(its_client));
        std::memcpy(&its_length, &_data[VSOMEIP_COMMAND_SIZE_POS_MIN],
                sizeof(its_length));

        if (configuration_->is_security_enabled() && _bound_client != routing_host_id &&
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
            instance_t its_instance;
            std::memcpy(&its_instance,
                    &_data[_size - sizeof(instance_t) - sizeof(bool)
                            - sizeof(bool)], sizeof(instance_t));
            bool its_reliable;
            std::memcpy(&its_reliable, &_data[_size - sizeof(bool)],
                            sizeof(its_reliable));
            // reduce by size of instance, flush and reliable flag
            const std::uint32_t its_message_size = its_length -
                    static_cast<uint32_t>(sizeof(its_instance)
                    + sizeof(bool) + sizeof(bool));

            auto a_deserializer = get_deserializer();
            a_deserializer->set_data(&_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    its_message_size);
            std::shared_ptr<message> its_message(a_deserializer->deserialize_message());
            a_deserializer->reset();
            put_deserializer(a_deserializer);

            if (its_message) {
                its_message->set_instance(its_instance);
                its_message->set_reliable(its_reliable);
                if (utility::is_notification(its_message->get_message_type())) {
                    if (!configuration_->is_client_allowed(get_client(), its_message->get_service(),
                            its_message->get_instance())) {
                        VSOMEIP_WARNING << std::hex << "Security: Client 0x" << get_client()
                                << " isn't allow receive a notification from to service/instance "
                                << its_message->get_service() << "/" << its_message->get_instance()
                                << " respectively from client 0x" << its_client
                                << " : Skip message!";
                        return;
                    }
                    cache_event_payload(its_message);
                } else if (utility::is_request(its_message->get_message_type())) {
                    if (!configuration_->is_client_allowed(its_message->get_client(),
                            its_message->get_service(), its_message->get_instance())) {
                        VSOMEIP_WARNING << std::hex << "Security: Client 0x" << its_message->get_client()
                                << " isn't allow to send a request to service/instance "
                                << its_message->get_service() << "/" << its_message->get_instance()
                                << " : Skip message!";
                        return;
                    }
                } else { // response
                    if (!configuration_->is_client_allowed(get_client(), its_message->get_service(),
                            its_message->get_instance())) {
                        VSOMEIP_WARNING << std::hex << "Security: Client 0x" << get_client()
                                << " isn't allow receive a response from to service/instance "
                                << its_message->get_service() << "/" << its_message->get_instance()
                                << " respectively from client 0x" << its_client
                                << " : Skip message!";
                        return;
                    }
                }
                host_->on_message(std::move(its_message));
            } else {
                VSOMEIP_ERROR << "Routing proxy: on_message: "
                              << "SomeIP-Header deserialization failed!";
            }
        }
            break;

        case VSOMEIP_ROUTING_INFO:
            if (!configuration_->is_security_enabled() ||_bound_client == routing_host_id) {
                on_routing_info(&_data[VSOMEIP_COMMAND_PAYLOAD_POS], its_length);
            } else {
                VSOMEIP_WARNING << std::hex << "Security: Client 0x" << get_client()
                        << " received an routing info from a client which isn't the routing manager"
                        << " : Skip message!";
            }
            break;

        case VSOMEIP_PING:
            send_pong();
            VSOMEIP_TRACE << "PING("
                << std::hex << std::setw(4) << std::setfill('0') << client_ << ")";
            break;

        case VSOMEIP_SUBSCRIBE:
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));
            std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                    sizeof(its_major));
            std::memcpy(&is_remote_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                    sizeof(is_remote_subscriber));

            if (is_remote_subscriber) {
                // Remote subscriber: Notify routing manager initially + count subscribes
                (void)host_->on_subscription(its_service, its_instance,
                        its_eventgroup, its_client, true);
                bool inserted = insert_subscription(its_service, its_instance, its_eventgroup,
                        VSOMEIP_ROUTING_CLIENT);
                if (inserted) {
                    notify_remote_initally(its_service, its_instance, its_eventgroup);
                }
                (void)get_remote_subscriber_count(its_service, its_instance, its_eventgroup, true);
            } else if (is_client_known(its_client)) {
                if (!configuration_->is_client_allowed(its_client,
                                        its_service, its_instance)) {
                    VSOMEIP_WARNING << "Security: Client " << std::hex
                            << its_client << " subscribes to service/instance "
                            << its_service << "/" << its_instance
                            << " which violates the security policy ~> Skip subscribe!";
                    return;
                }

                // Local & already known subscriber: create endpoint + send (N)ACK + insert subscription
                (void) find_or_create_local(its_client);
                bool subscription_accepted = host_->on_subscription(its_service, its_instance,
                        its_eventgroup, its_client, true);
                if (!subscription_accepted) {
                    send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup);
                } else {
                    routing_manager_base::subscribe(its_client, its_service, its_instance,
                            its_eventgroup, its_major, subscription_type_e::SU_RELIABLE_AND_UNRELIABLE);
                    send_subscribe_ack(its_client, its_service, its_instance, its_eventgroup);
                    send_pending_notify_ones(its_service, its_instance, its_eventgroup, its_client);
                }
            } else {
                // Local & not yet known subscriber ~> set pending until subscriber gets known!
                eventgroup_data_t subscription = { its_service, its_instance,
                        its_eventgroup, its_major,
                        subscription_type_e::SU_RELIABLE_AND_UNRELIABLE};
                std::lock_guard<std::mutex> its_lock(pending_ingoing_subscripitons_mutex_);
                pending_ingoing_subscripitons_[its_client].insert(subscription);
            }
            VSOMEIP_INFO << "SUBSCRIBE("
                << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << ":"
                << std::dec << (uint16_t)its_major << "]";
            break;

        case VSOMEIP_UNSUBSCRIBE:
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));
            std::memcpy(&is_remote_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                    sizeof(is_remote_subscriber));
            host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, false);
            if (!is_remote_subscriber) {
                // Local subscriber: withdraw subscription
                routing_manager_base::unsubscribe(its_client, its_service, its_instance, its_eventgroup);
            } else {
                // Remote subscriber: withdraw subscription only if no more remote subscriber exists
                if (!get_remote_subscriber_count(its_service, its_instance, its_eventgroup, false)) {
                    routing_manager_base::unsubscribe(VSOMEIP_ROUTING_CLIENT, its_service,
                            its_instance, its_eventgroup);
                }
            }
            VSOMEIP_INFO << "UNSUBSCRIBE("
                << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]";
            break;

        case VSOMEIP_SUBSCRIBE_NACK:
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));

            on_subscribe_nack(its_client, its_service, its_instance, its_eventgroup);
            VSOMEIP_INFO << "SUBSCRIBE NACK("
                << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]";
            break;

        case VSOMEIP_SUBSCRIBE_ACK:
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));

            on_subscribe_ack(its_client, its_service, its_instance, its_eventgroup);
            VSOMEIP_INFO << "SUBSCRIBE ACK("
                << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]";
            break;

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
    inner_state_type_e its_state(inner_state_type_e::ST_DEREGISTERED);
    bool restart_sender(_size == 1); // 1 indicates a routing master stop
    std::map<service_t, std::map<instance_t, std::tuple< major_version_t, minor_version_t, client_t> > > old_local_services;
    std::unordered_set<client_t> clients_to_delete;
    struct service_info {
        service_t service_id_;
        instance_t instance_id_;
        major_version_t major_;
        minor_version_t minor_;
    };
    std::forward_list<struct service_info> services_to_remove;
    std::forward_list<struct service_info> services_to_add;
    {
        std::lock_guard<std::mutex> its_lock(local_services_mutex_);
        old_local_services = local_services_;
        local_services_.clear();
        std::unordered_set<client_t> known_clients;

        uint32_t i = 0;
        while (i + sizeof(uint32_t) <= _size) {
            uint32_t its_client_size;
            std::memcpy(&its_client_size, &_data[i], sizeof(uint32_t));
            i += uint32_t(sizeof(uint32_t));

            if (i + sizeof(client_t) <= _size) {
                client_t its_client;
                std::memcpy(&its_client, &_data[i], sizeof(client_t));
                i += uint32_t(sizeof(client_t));

                if (its_client == client_) {
                    its_state = inner_state_type_e::ST_REGISTERED;
                }
                known_clients.insert(its_client);

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

                            local_services_[its_service][its_instance] = std::make_tuple(its_major, its_minor, its_client);

                            its_services_size -= uint32_t(sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t) );
                        }
                    }
                }

                i += j;
            }
        }
        // Which clients are no longer needed?!
        for (const auto client : get_connected_clients()) {
            if (known_clients.find(client) == known_clients.end()) {
                clients_to_delete.insert(client);
            }
        }
        {
            std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
            known_clients_ = known_clients;
        }

        // Check for services that are no longer available
        for (const auto &i : old_local_services) {
            auto found_service = local_services_.find(i.first);
            if (found_service != local_services_.end()) {
                for (const auto &j : i.second) {
                    auto found_instance = found_service->second.find(j.first);
                    if (found_instance == found_service->second.end()) {
                        services_to_remove.push_front(
                                    { i.first, j.first, std::get<0>(j.second),
                                            std::get<1>(j.second) });
                    }
                }
            } else {
                for (const auto &j : i.second) {
                    services_to_remove.push_front(
                                { i.first, j.first, std::get<0>(j.second),
                                        std::get<1>(j.second) });
                }
            }
        }

        // Check for services that are newly available
        for (const auto &i : local_services_) {
            auto found_service = old_local_services.find(i.first);
            if (found_service != old_local_services.end()) {
                for (const auto &j : i.second) {
                    auto found_instance = found_service->second.find(j.first);
                    if (found_instance == found_service->second.end()) {
                        services_to_add.push_front(
                                    { i.first, j.first, std::get<0>(j.second),
                                            std::get<1>(j.second) });
                    }
                }
            } else {
                for (const auto &j : i.second) {
                    services_to_add.push_front(
                                { i.first, j.first, std::get<0>(j.second),
                                        std::get<1>(j.second) });
                }
            }
        }
    }

    if (state_ != its_state) {
        VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
                     << (its_state == inner_state_type_e::ST_REGISTERED ?
                             " is registered." : " is deregistered.");

        // inform host about its own registration state changes
        host_->on_state(static_cast<state_type_e>(its_state));

        {
            std::lock_guard<std::mutex> its_lock(state_mutex_);
            if (its_state == inner_state_type_e::ST_REGISTERED) {
                boost::system::error_code ec;
                register_application_timer_.cancel(ec);
                send_registered_ack();
                send_pending_commands();
            }
            state_ = its_state;
        }

        // Notify stop() call about clean deregistration
        state_condition_.notify_one();
    }

    // Report services that are no longer available
    for (const service_info &sr : services_to_remove) {
        on_stop_offer_service(sr.service_id_, sr.instance_id_, sr.major_, sr.minor_);
        host_->on_availability(sr.service_id_, sr.instance_id_, false, sr.major_, sr.minor_);
    }
    // Report services that are newly available
    for (const service_info &sa : services_to_add) {
        {
            std::lock_guard<std::mutex> its_lock(state_mutex_);
            send_pending_subscriptions(sa.service_id_, sa.instance_id_, sa.major_);
        }
        host_->on_availability(sa.service_id_, sa.instance_id_, true, sa.major_, sa.minor_);
    }

    {
        struct subscription_info {
            service_t service_id_;
            instance_t instance_id_;
            eventgroup_t eventgroup_id_;
            client_t client_id_;
            major_version_t major_;
        };
        std::lock_guard<std::mutex> its_lock(pending_ingoing_subscripitons_mutex_);
        std::forward_list<struct subscription_info> subscription_actions;
        if (pending_ingoing_subscripitons_.size()) {
            {
                std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
                for (const client_t client : known_clients_) {
                    auto its_client = pending_ingoing_subscripitons_.find(client);
                    if (its_client != pending_ingoing_subscripitons_.end()) {
                        for (const auto subscription : its_client->second) {
                            subscription_actions.push_front(
                                { subscription.service_, subscription.instance_,
                                        subscription.eventgroup_, client,
                                        subscription.major_ });
                        }
                    }
                }
            }
            for (const subscription_info &si : subscription_actions) {
                bool subscription_accepted = host_->on_subscription(
                        si.service_id_, si.instance_id_, si.eventgroup_id_,
                        si.client_id_, true);
                (void) find_or_create_local(si.client_id_);
                if (!subscription_accepted) {
                    send_subscribe_nack(si.client_id_, si.service_id_,
                            si.instance_id_, si.eventgroup_id_);
                } else {
                    routing_manager_base::subscribe(si.client_id_,
                            si.service_id_, si.instance_id_, si.eventgroup_id_,
                            si.major_,
                            subscription_type_e::SU_RELIABLE_AND_UNRELIABLE);
                    send_subscribe_ack(si.client_id_, si.service_id_,
                            si.instance_id_, si.eventgroup_id_);
                    send_pending_notify_ones(si.service_id_,
                            si.instance_id_, si.eventgroup_id_, si.client_id_);
                }
                pending_ingoing_subscripitons_.erase(si.client_id_);
            }
        }
    }

    if (clients_to_delete.size()) {
        std::async(std::launch::async, [this, clients_to_delete, restart_sender] () {
            for (const auto client : clients_to_delete) {
                if (client != VSOMEIP_ROUTING_CLIENT) {
                    remove_local(client);
                }
            }
        });
    }

    if (restart_sender && is_started_) {
        VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
                <<": Reconnecting to routing manager.";
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->start();
        }
    }
}

void routing_manager_proxy::register_application() {
    byte_t its_command[] = {
            VSOMEIP_REGISTER_APPLICATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));
    std::memset(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], 0,
            sizeof(uint32_t));

    if (is_connected_) {
        std::lock_guard<std::mutex> its_state_lock(state_mutex_);
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            {
                state_ = inner_state_type_e::ST_REGISTERING;
            }
            sender_->send(its_command, sizeof(its_command));

            register_application_timer_.cancel();
            register_application_timer_.expires_from_now(std::chrono::milliseconds(1000));
            register_application_timer_.async_wait(
                    std::bind(
                            &routing_manager_proxy::register_application_timeout_cbk,
                            this, std::placeholders::_1));
        }
    }
}

void routing_manager_proxy::deregister_application() {
    uint32_t its_size = sizeof(client_);

    std::vector<byte_t> its_command(VSOMEIP_COMMAND_HEADER_SIZE + its_size);
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_DEREGISTER_APPLICATION;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    if (is_connected_)
    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
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
        {
            std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
            if (sender_) {
                sender_->send(its_pong, sizeof(its_pong));
            }
        }
    }
}

void routing_manager_proxy::send_request_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major,
        minor_version_t _minor, bool _use_exclusive_proxy) {
    (void)_client;

    byte_t its_command[VSOMEIP_REQUEST_SERVICE_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_REQUEST_SERVICE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_REQUEST_SERVICE;
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
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 9], &_use_exclusive_proxy,
            sizeof(_use_exclusive_proxy));

    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
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
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, sizeof(its_command));
        }
    }
}

void routing_manager_proxy::send_register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, const std::set<eventgroup_t> &_eventgroups,
        bool _is_field, bool _is_provided) {

    uint32_t its_eventgroups_size = uint32_t(_eventgroups.size() * sizeof(eventgroup_t));
    byte_t *its_command = new byte_t[VSOMEIP_REGISTER_EVENT_COMMAND_SIZE + its_eventgroups_size];
    uint32_t its_size = VSOMEIP_REGISTER_EVENT_COMMAND_SIZE
                        + its_eventgroups_size
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
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_event,
            sizeof(_event));
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6]
                = static_cast<byte_t>(_is_field);
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 7]
                = static_cast<byte_t>(_is_provided);

    std::size_t i = 8;
    for (auto eg : _eventgroups) {
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + i], &eg,
            sizeof(eventgroup_t));
        i += sizeof(eventgroup_t);
    }

    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command,
                          uint32_t(VSOMEIP_REGISTER_EVENT_COMMAND_SIZE
                                   + its_eventgroups_size));
        }
    }

    delete[] its_command;
}

void routing_manager_proxy::on_subscribe_ack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    (void)_client;
    host_->on_subscription_error(_service, _instance, _eventgroup, 0x0 /*OK*/);
}

void routing_manager_proxy::on_subscribe_nack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    (void)_client;
    host_->on_subscription_error(_service, _instance, _eventgroup, 0x7 /*Rejected*/);
}

void routing_manager_proxy::on_identify_response(client_t _client, service_t _service,
        instance_t _instance, bool _reliable) {
    static const uint32_t size = uint32_t(VSOMEIP_COMMAND_HEADER_SIZE + sizeof(service_t) + sizeof(instance_t)
            + sizeof(bool));
    byte_t its_command[size];
    uint32_t its_size = size - VSOMEIP_COMMAND_HEADER_SIZE;
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_ID_RESPONSE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
            sizeof(_client));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_reliable,
            sizeof(_reliable));
    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, size);
        }
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
        routing_manager_base::register_event(host_->get_client(), its_service,
                its_instance, its_method, its_eventgroups, true,
                std::chrono::milliseconds::zero(), false,
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
        send_offer_service(client_, po.service_, po.instance_,
                po.major_, po.minor_);

    for (auto &per : pending_event_registrations_)
        send_register_event(client_, per.service_, per.instance_,
                per.event_, per.eventgroups_,
                per.is_field_, per.is_provided_);

    for (auto &s : pending_notifications_) {
        for (auto &i : s.second) {
            for (auto &pn : i.second) {
                routing_manager_base::notify(s.first, i.first,
                        pn.first, pn.second->get_payload(), false, true); // TODO: Use _flush argument to send all events at once
            }
        }
    }

    for (auto &po : pending_requests_) {
        send_request_service(client_, po.service_, po.instance_,
                po.major_, po.minor_, po.use_exclusive_proxy_);
    }
}

void routing_manager_proxy::init_receiver() {
    std::stringstream its_client;
    its_client << VSOMEIP_BASE_PATH << std::hex << client_;
#ifdef WIN32
    ::_unlink(its_client.str().c_str());
    int port = VSOMEIP_INTERNAL_BASE_PORT + client_;
#else
    if (!check_credentials(get_client(), getuid(), getgid())) {
        VSOMEIP_ERROR << "routing_manager_proxy::init_receiver: "
                << std::hex << "Client " << get_client() << " isn't allow"
                << " to create a server endpoint due to credential check failed!";
        return;
    }
    if (-1 == ::unlink(its_client.str().c_str()) && errno != ENOENT) {
        VSOMEIP_ERROR << "routing_manager_proxy::init_receiver unlink failed ("
                << its_client.str() << "): "<< std::strerror(errno);
    }
    const mode_t previous_mask(::umask(static_cast<mode_t>(configuration_->get_umask())));
#endif
    try {
        receiver_ = std::make_shared<local_server_endpoint_impl>(shared_from_this(),
#ifdef WIN32
                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
#else
                boost::asio::local::stream_protocol::endpoint(its_client.str()),
#endif
                io_, configuration_->get_max_message_size_local(),
                configuration_->get_buffer_shrink_threshold());
#ifdef WIN32
        VSOMEIP_INFO << "Listening at " << port;
#else
        VSOMEIP_INFO << "Listening at " << its_client.str();
#endif
    } catch (const std::exception &e) {
        host_->on_error(error_code_e::SERVER_ENDPOINT_CREATION_FAILED);
        VSOMEIP_ERROR << "Client ID: " << std::hex << client_ << ": " << e.what();
    }
#ifndef WIN32
    ::umask(previous_mask);
#endif
}

void routing_manager_proxy::notify_remote_initally(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup) {
    auto its_eventgroup = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        std::set<std::shared_ptr<event> > its_events
            = its_eventgroup->get_events();
        for (auto e : its_events) {
            if (e->is_field() && e->is_set()) {
                std::shared_ptr<message> its_notification
                    = runtime::get()->create_notification();
                its_notification->set_service(_service);
                its_notification->set_instance(_instance);
                its_notification->set_method(e->get_event());
                its_notification->set_payload(e->get_payload());
                auto service_info = find_service(_service, _instance);
                if (service_info) {
                    its_notification->set_interface_version(service_info->get_major());
                }
                std::lock_guard<std::mutex> its_lock(serialize_mutex_);
                if (serializer_->serialize(its_notification.get())) {
                    {
                        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
                        if (sender_) {
                            send_local(sender_, VSOMEIP_ROUTING_CLIENT, serializer_->get_data(),
                                    serializer_->get_size(), _instance, true, false, VSOMEIP_NOTIFY);
                        }
                    }
                    serializer_->reset();
                } else {
                    VSOMEIP_ERROR << "Failed to serialize message. Check message size!";
                }
            }
        }
    }

}

uint32_t routing_manager_proxy::get_remote_subscriber_count(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, bool _increment) {
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

void routing_manager_proxy::register_application_timeout_cbk(
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
            std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
            VSOMEIP_WARNING << std::hex << "Client 0x" << get_client() << " register timeout!"
                    << " : Restart route to stub!";
            if (sender_) {
                sender_->start();
            }
        }
    }
}

void routing_manager_proxy::send_registered_ack() {
    byte_t its_command[VSOMEIP_COMMAND_HEADER_SIZE];
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_REGISTERED_ACK;
    client_t client = get_client();
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client,
            sizeof(client));
    std::memset(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], 0,
            sizeof(uint32_t));
    {
        std::lock_guard<std::recursive_mutex> its_lock(sender_mutex_);
        if (sender_) {
            sender_->send(its_command, VSOMEIP_COMMAND_HEADER_SIZE);
        }
    }
}

bool routing_manager_proxy::is_client_known(client_t _client) {
    std::lock_guard<std::mutex> its_lock(known_clients_mutex_);
    return (known_clients_.find(_client) != known_clients_.end());
}
}  // namespace vsomeip

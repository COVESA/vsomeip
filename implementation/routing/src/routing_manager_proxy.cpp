// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <climits>
#include <iomanip>
#include <mutex>
#include <unordered_set>
#include <future>

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
        state_(state_type_e::ST_DEREGISTERED),
        sender_(0),
        receiver_(0)
{
}

routing_manager_proxy::~routing_manager_proxy() {
}

void routing_manager_proxy::init() {
    routing_manager_base::init();

    std::stringstream its_sender_path;
    sender_ = create_local(VSOMEIP_ROUTING_CLIENT);

    std::stringstream its_client;
    its_client << VSOMEIP_BASE_PATH << std::hex << client_;
#ifdef WIN32
    ::_unlink(its_client.str().c_str());
    int port = VSOMEIP_INTERNAL_BASE_PORT + client_;
#else
    ::unlink(its_client.str().c_str());
    const mode_t previous_mask(::umask(static_cast<mode_t>(configuration_->get_umask())));
#endif
    receiver_ = std::make_shared<local_server_endpoint_impl>(shared_from_this(),
#ifdef WIN32
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
#else
            boost::asio::local::stream_protocol::endpoint(its_client.str()),
#endif
            io_, configuration_->get_max_message_size_local());

#ifdef WIN32
    VSOMEIP_DEBUG << "Listening at " << port;
#else
    ::umask(previous_mask);
    VSOMEIP_DEBUG<< "Listening at " << its_client.str();
#endif
}

void routing_manager_proxy::start() {
    is_started_ = true;

    if (!sender_) {
        // application has been stopped and started again
        sender_ = create_local(VSOMEIP_ROUTING_CLIENT);
    }
    if (sender_) {
        sender_->start();
    }

    if (receiver_)
        receiver_->start();
}

void routing_manager_proxy::stop() {
    deregister_application();

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

    // delete the sender
    sender_ = nullptr;
    std::stringstream its_client;
    its_client << VSOMEIP_BASE_PATH << std::hex << client_;
#ifdef WIN32
    ::_unlink(its_client.str().c_str());
#else
    ::unlink(its_client.str().c_str());
#endif

    is_started_ = false;
}

void routing_manager_proxy::offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {

    routing_manager_base::offer_service(_client, _service, _instance, _major, _minor);

    if (is_connected_) {
        send_offer_service(_client, _service, _instance, _major, _minor);
    }
    service_data_t offer = { _service, _instance, _major, _minor, false };
    std::lock_guard<std::mutex> its_lock(pending_mutex_);
    pending_offers_.insert(offer);
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

    sender_->send(its_command, sizeof(its_command));
}

void routing_manager_proxy::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {
    (void)_client;

    routing_manager_base::stop_offer_service(_client, _service, _instance, _major, _minor);

    // Reliable/Unreliable unimportant as routing_proxy does not
    // create server endpoints which needs to be freed
    clear_service_info(_service, _instance, false);

    if (is_connected_) {
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

        sender_->send(its_command, sizeof(its_command));
    }
    std::lock_guard<std::mutex> its_lock(pending_mutex_);
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

void routing_manager_proxy::request_service(client_t _client,
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, bool _use_exclusive_proxy) {
    routing_manager_base::request_service(_client, _service, _instance, _major,
            _minor, _use_exclusive_proxy);
    send_request_service(_client, _service, _instance, _major, _minor,
            _use_exclusive_proxy);
}

void routing_manager_proxy::release_service(client_t _client,
        service_t _service, instance_t _instance) {
    routing_manager_base::release_service(_client, _service, _instance);
    send_release_service(_client, _service, _instance);
}

void routing_manager_proxy::register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, const std::set<eventgroup_t> &_eventgroups,
        bool _is_field, bool _is_provided, bool _is_shadow, bool _is_cache_placeholder) {

    (void)_is_shadow;
    (void)_is_cache_placeholder;

    routing_manager_base::register_event(_client, _service, _instance,
            _event,_eventgroups, _is_field, _is_provided);

    send_register_event(client_, _service, _instance,
            _event, _eventgroups, _is_field, _is_provided);
}

void routing_manager_proxy::unregister_event(client_t _client,
        service_t _service, instance_t _instance, event_t _event,
        bool _is_provided) {

    routing_manager_base::unregister_event(_client, _service, _instance,
            _event, _is_provided);

    if (is_connected_) {
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

        sender_->send(its_command, sizeof(its_command));
    }
    std::lock_guard<std::mutex> its_lock(pending_mutex_);
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

    if (is_connected_ && is_available(_service, _instance, _major)) {
        send_subscribe(_client, _service, _instance, _eventgroup, _major,
                _subscription_type);
    }
    eventgroup_data_t subscription = { _service, _instance, _eventgroup, _major,
            _subscription_type};
    std::lock_guard<std::mutex> its_lock(pending_mutex_);
    pending_subscriptions_.insert(subscription);
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
        sender_->send(its_command, sizeof(its_command));
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
        sender_->send(its_command, sizeof(its_command));
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
        sender_->send(its_command, sizeof(its_command));
    }
}

void routing_manager_proxy::unsubscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    (void)_client;

    if (is_connected_) {
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

        auto its_target = find_local(_service, _instance);
        if (its_target) {
            its_target->send(its_command, sizeof(its_command));
        } else {
            sender_->send(its_command, sizeof(its_command));
        }
    }
    std::lock_guard<std::mutex> its_lock(pending_mutex_);
    auto it = pending_subscriptions_.begin();
    while (it != pending_subscriptions_.end()) {
        if (it->service_ == _service
         && it->instance_ == _instance) {
            break;
        }
        it++;
    }
    if (it != pending_subscriptions_.end()) pending_subscriptions_.erase(it);
}

bool routing_manager_proxy::send(client_t _client, const byte_t *_data,
        length_t _size, instance_t _instance,
        bool _flush,
        bool _reliable,
        bool _initial) {
	bool is_sent(false);
    if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
        std::shared_ptr<endpoint> its_target;
        if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            // Request
            service_t its_service = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_SERVICE_POS_MIN],
                    _data[VSOMEIP_SERVICE_POS_MAX]);
            std::lock_guard<std::mutex> its_lock(send_mutex_);
            client_t its_client = find_local_client(its_service, _instance);
            if (its_client != VSOMEIP_ROUTING_CLIENT) {
                if (known_clients_.find(its_client) != known_clients_.end()) {
                    its_target = find_or_create_local(its_client);
                }
            }
        } else if (!utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            // Response
            client_t its_client = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_CLIENT_POS_MIN],
                    _data[VSOMEIP_CLIENT_POS_MAX]);
            std::lock_guard<std::mutex> its_lock(send_mutex_);
            if (its_client != VSOMEIP_ROUTING_CLIENT) {
                if (known_clients_.find(its_client) != known_clients_.end()) {
                    its_target = find_or_create_local(its_client);
                }
            }
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) &&
                _client == VSOMEIP_ROUTING_CLIENT) {
            // notify
            send_local_notification(get_client(), _data, _size,
                    _instance, _flush, _reliable, _initial);
        } else if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) &&
                _client != VSOMEIP_ROUTING_CLIENT) {
            // notify_one
            its_target = find_local(_client);
            if (its_target) {
                return send_local(its_target, get_client(), _data, _size,
                        _instance, _flush, _reliable, VSOMEIP_SEND, false, _initial);
            }
        }
        // If no direct endpoint could be found/is connected
        // or for notifications ~> route to routing_manager_stub
        if (!its_target || !its_target->is_connected()) {
            its_target = sender_;
        }
#ifdef USE_DLT
        else {
            uint16_t its_data_size
                = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

            tc::trace_header its_header;
            if (its_header.prepare(its_target, true))
                tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                        _data, its_data_size);
        }
#endif
        uint8_t command = utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) ?
                                            VSOMEIP_NOTIFY : VSOMEIP_SEND;
        if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) && _client) {
            command = VSOMEIP_NOTIFY_ONE;
        }
        is_sent = send_local(its_target, _client, _data, _size, _instance, _flush, _reliable, command, false, _initial);
    }
    return (is_sent);
}

bool routing_manager_proxy::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        std::shared_ptr<message> _message) {
    (void)_target;
    (void)_message;
    return (false);
}

bool routing_manager_proxy::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size) {
    (void)_target;
    (void)_data;
    (void)_size;
    return (false);
}

void routing_manager_proxy::notify(
        service_t _service, instance_t _instance, event_t _event,
        std::shared_ptr<payload> _payload) {
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
    if (is_connected_) {
        routing_manager_base::notify(_service, _instance, _event, _payload);
    } else if (is_field(_service, _instance, _event)){
        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        pending_notifications_[_service][_instance][_event] = its_notification;
    }
}

void routing_manager_proxy::on_connect(std::shared_ptr<endpoint> _endpoint) {
    if (_endpoint != sender_) {
        return;
    }
    is_connected_ = true;
    if (is_connected_ && is_started_) {
        VSOMEIP_DEBUG << std::hex << "Client " << client_
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

void routing_manager_proxy::on_error(const byte_t *_data, length_t _length,
        endpoint *_receiver) {

    // Implement me when needed

    (void)(_data);
    (void)(_length);
    (void)(_receiver);
}

void routing_manager_proxy::release_port(uint16_t _port, bool _reliable) {
	(void)_port;
	(void)_reliable;
	// intentionally empty
}

void routing_manager_proxy::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination) {
    (void)_receiver;
    (void)_destination;
#if 0
    std::stringstream msg;
    msg << "rmp::on_message: ";
    for (int i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    byte_t its_command;
    client_t its_client;
    length_t its_length;
    service_t its_service;
    instance_t its_instance;
    eventgroup_t its_eventgroup;
    major_version_t its_major;
    bool subscription_accepted;

    if (_size > VSOMEIP_COMMAND_SIZE_POS_MAX) {
        its_command = _data[VSOMEIP_COMMAND_TYPE_POS];
        std::memcpy(&its_client, &_data[VSOMEIP_COMMAND_CLIENT_POS],
                sizeof(its_client));
        std::memcpy(&its_length, &_data[VSOMEIP_COMMAND_SIZE_POS_MIN],
                sizeof(its_length));

        switch (its_command) {
        case VSOMEIP_SEND: {
            instance_t its_instance;
            std::memcpy(&its_instance,
                    &_data[_size - sizeof(instance_t) - sizeof(bool)
                            - sizeof(bool)  - sizeof(bool)], sizeof(instance_t));
            bool its_reliable;
            std::memcpy(&its_reliable, &_data[_size - sizeof(bool) - sizeof(bool)],
                            sizeof(its_reliable));
            deserializer_->set_data(&_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    its_length);
            std::shared_ptr<message> its_message(
                    deserializer_->deserialize_message());
            if (its_message) {
                its_message->set_instance(its_instance);
                its_message->set_reliable(its_reliable);
                if(its_message->get_message_type() == message_type_e::MT_NOTIFICATION) {
                    bool its_initial(false);
                    std::memcpy(&its_initial, &_data[_size - sizeof(bool)], sizeof(its_initial));
                    its_message->set_initial(its_initial);
                    cache_event_payload(its_message);
                }
                host_->on_message(its_message);
            } else {
                VSOMEIP_ERROR << "Routing proxy: on_message: "
                              << "SomeIP-Header deserialization failed!";
            }
            deserializer_->reset();
        }
            break;

        case VSOMEIP_ROUTING_INFO:
            on_routing_info(&_data[VSOMEIP_COMMAND_PAYLOAD_POS], its_length);
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
            bool is_remote_subscriber;
            std::memcpy(&is_remote_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                    sizeof(is_remote_subscriber));

            if (is_remote_subscriber || known_clients_.find(its_client) != known_clients_.end()) {
                subscription_accepted = host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, true);
                if (!is_remote_subscriber) {
                    (void) find_or_create_local(its_client);
                    if (!subscription_accepted) {
                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup);
                    } else {
                        routing_manager_base::subscribe(its_client, its_service, its_instance, its_eventgroup,
                                its_major, subscription_type_e::SU_RELIABLE_AND_UNRELIABLE);
                        send_subscribe_ack(its_client, its_service, its_instance, its_eventgroup);
                    }
                }
            } else {
                if (!is_remote_subscriber) {
                    eventgroup_data_t subscription = { its_service, its_instance,
                            its_eventgroup, its_major,
                            subscription_type_e::SU_RELIABLE_AND_UNRELIABLE};
                    pending_ingoing_subscripitons_[its_client].insert(subscription);
                }
            }
            VSOMEIP_DEBUG << "SUBSCRIBE("
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
            host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, false);
            routing_manager_base::unsubscribe(its_client, its_service, its_instance, its_eventgroup);
            VSOMEIP_DEBUG << "UNSUBSCRIBE("
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
            VSOMEIP_DEBUG << "SUBSCRIBE NACK("
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
            VSOMEIP_DEBUG << "SUBSCRIBE ACK("
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
    VSOMEIP_DEBUG << msg.str();
#endif
    state_type_e its_state(state_type_e::ST_DEREGISTERED);
    bool restart_sender(_size == 0);
    std::map<service_t, std::map<instance_t, std::tuple< major_version_t, minor_version_t, client_t> > > old_local_services;
    std::unordered_set<client_t> clients_to_delete;
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
                    its_state = state_type_e::ST_REGISTERED;
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
        for (auto client : get_connected_clients()) {
            if (known_clients.find(client) == known_clients.end()) {
                clients_to_delete.insert(client);
            }
        }
        known_clients_ = known_clients;
    }

    // inform host about its own registration state changes
    if (state_ != its_state) {
        if (its_state == state_type_e::ST_REGISTERED) {
            VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
                         << " is registered.";
        } else {
            VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
                         << " is deregistered.";
        }
        host_->on_state(its_state);
        state_ = its_state;
    }

    // Check for services that are no longer available
    for (auto i : old_local_services) {
        auto found_service = local_services_.find(i.first);
        if (found_service != local_services_.end()) {
            for (auto j : i.second) {
                auto found_instance = found_service->second.find(j.first);
                if (found_instance == found_service->second.end()) {
                    auto version_info = j.second;
                    on_stop_offer_service(i.first, j.first, std::get<0>(version_info), std::get<1>(version_info));
                    host_->on_availability(i.first, j.first, false, std::get<0>(version_info), std::get<1>(version_info));
                }
            }
        } else {
            for (auto j : i.second) {
                on_stop_offer_service(i.first, j.first, std::get<0>(j.second), std::get<1>(j.second));
                host_->on_availability(i.first, j.first, false, std::get<0>(j.second), std::get<1>(j.second));
            }
        }
    }

    // Check for services that are newly available
    for (auto i : local_services_) {
        auto found_service = old_local_services.find(i.first);
        if (found_service != old_local_services.end()) {
            for (auto j : i.second) {
                auto found_instance = found_service->second.find(j.first);
                if (found_instance == found_service->second.end()) {
                    send_pending_subscriptions(i.first, j.first, std::get<0>(j.second));
                    host_->on_availability(i.first, j.first, true, std::get<0>(j.second), std::get<1>(j.second));
                }
            }
        } else {
            for (auto j : i.second) {
                send_pending_subscriptions(i.first, j.first, std::get<0>(j.second));
                host_->on_availability(i.first, j.first, true, std::get<0>(j.second), std::get<1>(j.second));
            }
        }
    }

    for (client_t client : known_clients_) {
        auto its_client = pending_ingoing_subscripitons_.find(client);
        if (its_client != pending_ingoing_subscripitons_.end()) {
            for (auto subscription : its_client->second) {
                bool subscription_accepted = host_->on_subscription(subscription.service_, subscription.instance_, subscription.eventgroup_, client, true);
                (void) find_or_create_local(client);
                if (!subscription_accepted) {
                    send_subscribe_nack(client, subscription.service_, subscription.instance_, subscription.eventgroup_);
                } else {
                    routing_manager_base::subscribe(client, subscription.service_, subscription.instance_, subscription.eventgroup_,
                            subscription.major_, subscription_type_e::SU_RELIABLE_AND_UNRELIABLE);
                    send_subscribe_ack(client, subscription.service_, subscription.instance_, subscription.eventgroup_);
                }
            }
        }
        pending_ingoing_subscripitons_.erase(client);
    }

    if (clients_to_delete.size() || restart_sender) {
        std::async(std::launch::async, [this, clients_to_delete, restart_sender] () {
            for (auto client : clients_to_delete) {
                if (client != VSOMEIP_ROUTING_CLIENT) {
                    remove_local(client);
                }
            }
            if (restart_sender && is_started_ && sender_) {
                VSOMEIP_INFO << std::hex << "Application/Client " << get_client()
                        <<": Reconnecting to routing manager.";
                sender_->start();
            }
        });
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
        (void)sender_->send(its_command, sizeof(its_command));

        for (auto &po : pending_offers_)
            send_offer_service(client_, po.service_, po.instance_,
                    po.major_, po.minor_);

        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        for (auto &per : pending_event_registrations_)
            send_register_event(client_, per.service_, per.instance_,
                    per.event_, per.eventgroups_,
                    per.is_field_, per.is_provided_);

        for (auto &s : pending_notifications_) {
            for (auto &i : s.second) {
                for (auto &pn : i.second) {
                    routing_manager_base::notify(s.first, i.first,
                            pn.first, pn.second->get_payload());
                }
            }
        }

        for (auto &po : pending_requests_) {
            send_request_service(client_, po.service_, po.instance_,
                    po.major_, po.minor_, po.use_exclusive_proxy_);
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
        (void)sender_->send(&its_command[0], uint32_t(its_command.size()));
}

void routing_manager_proxy::send_pong() const {
    byte_t its_pong[] = {
    VSOMEIP_PONG, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

    std::memcpy(&its_pong[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_t));

    if (is_connected_)
        sender_->send(its_pong, sizeof(its_pong));
}

void routing_manager_proxy::send_request_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major,
        minor_version_t _minor, bool _use_exclusive_proxy) {
    (void)_client;

    if (is_connected_) {
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

        sender_->send(its_command, sizeof(its_command));
    } else {
        service_data_t request = { _service, _instance, _major, _minor, _use_exclusive_proxy };
        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        pending_requests_.insert(request);
    }
}

void routing_manager_proxy::send_release_service(client_t _client, service_t _service,
        instance_t _instance) {
    (void)_client;
    byte_t its_command[VSOMEIP_RELEASE_SERVICE_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_RELEASE_SERVICE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    if (is_connected_) {
        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_RELEASE_SERVICE;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
                sizeof(client_));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));

        sender_->send(its_command, sizeof(its_command));
    }
}

void routing_manager_proxy::send_register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, const std::set<eventgroup_t> &_eventgroups,
        bool _is_field, bool _is_provided) {
    if (is_connected_) {
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

        sender_->send(its_command,
                      uint32_t(VSOMEIP_REGISTER_EVENT_COMMAND_SIZE
                               + its_eventgroups_size));

        delete[] its_command;
    } else {
        event_data_t registration = {
                _service,
                _instance,
                _event,
                _is_field,
                _is_provided,
                _eventgroups
        };
        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        pending_event_registrations_.insert(registration);
    }
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
    sender_->send(its_command, size);
}

bool routing_manager_proxy::queue_message(const byte_t *_data, uint32_t _size) const {
    std::shared_ptr<local_server_endpoint_impl> its_server_endpoint
        = std::dynamic_pointer_cast<local_server_endpoint_impl>(receiver_);
    return its_server_endpoint->queue_message(_data, _size);
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
                its_instance, its_method, its_eventgroups, true, false, false, true);
        std::shared_ptr<event> its_event = find_event(its_service, its_instance, its_method);
        if (its_event) {
                its_event->set_payload_dont_notify(_message->get_payload());
        }
    }

}

void routing_manager_proxy::send_pending_subscriptions(service_t _service,
        instance_t _instance, major_version_t _major) {
    for (auto &ps : pending_subscriptions_) {
        if (ps.service_ == _service &&
                ps.instance_ == _instance && ps.major_ == _major) {
            send_subscribe(client_, ps.service_, ps.instance_,
                    ps.eventgroup_, ps.major_, ps.subscription_type_);
        }
    }
}

void routing_manager_proxy::on_stop_offer_service(service_t _service,
                                                  instance_t _instance,
                                                  major_version_t _major,
                                                  minor_version_t _minor) {
    (void) _major;
    (void) _minor;
    {
        std::lock_guard<std::mutex> its_lock(events_mutex_);
        auto its_events_service = events_.find(_service);
        if (its_events_service != events_.end()) {
            auto its_events_instance = its_events_service->second.find(_instance);
            if (its_events_instance != its_events_service->second.end()) {
                for (auto &e : its_events_instance->second)
                    e.second->unset_payload();
            }
        }
    }
}

}  // namespace vsomeip

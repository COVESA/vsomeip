// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <mutex>

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
        io_(_host->get_io()),
        is_connected_(false),
        is_started_(false),
        state_(state_type_e::ST_DEREGISTERED),
        host_(_host),
        client_(_host->get_client()),
        configuration_(host_->get_configuration()),
        serializer_(std::make_shared<serializer>()),
        deserializer_(std::make_shared<deserializer>()),
        sender_(0),
        receiver_(0) {
}

routing_manager_proxy::~routing_manager_proxy() {
}

boost::asio::io_service & routing_manager_proxy::get_io() {
    return (io_);
}

client_t routing_manager_proxy::get_client() const {
    return client_;
}

void routing_manager_proxy::init() {
    serializer_->create_data(configuration_->get_max_message_size_local());

    std::stringstream its_sender_path;
    sender_ = create_local(VSOMEIP_ROUTING_CLIENT);

    std::stringstream its_client;
    its_client << VSOMEIP_BASE_PATH << std::hex << client_;
#ifdef WIN32
    ::_unlink(its_client.str().c_str());
    int port = VSOMEIP_INTERNAL_BASE_PORT + client_;
#else
    ::unlink(its_client.str().c_str());
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
    VSOMEIP_DEBUG<< "Listening at " << its_client.str();
#endif
}

void routing_manager_proxy::start() {
    if (sender_)
        sender_->start();

    if (receiver_)
        receiver_->start();

    if (is_connected_) {
        register_application();
    }

    is_started_ = true;
}

void routing_manager_proxy::stop() {
    deregister_application();

    if (receiver_)
        receiver_->stop();

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

    if (is_connected_) {
        send_offer_service(_client, _service, _instance, _major, _minor);
    } else {
        service_data_t offer = { _service, _instance, _major, _minor, false };
        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        pending_offers_.insert(offer);
    }
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
        service_t _service, instance_t _instance) {
    (void)_client;

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

        sender_->send(its_command, sizeof(its_command));
    } else {
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
}

void routing_manager_proxy::request_service(client_t _client,
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, bool _use_exclusive_proxy) {
    send_request_service(_client, _service, _instance, _major, _minor,
            _use_exclusive_proxy);
}

void routing_manager_proxy::release_service(client_t _client,
        service_t _service, instance_t _instance) {
    (void)_client;
    (void)_service;
    (void)_instance;
}

void routing_manager_proxy::register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, std::set<eventgroup_t> _eventgroups,
        bool _is_field, bool _is_provided) {
    (void)_client;

    if (_is_field)
        fields_[_service][_instance].insert(_event);

    send_register_event(client_, _service, _instance,
            _event, _eventgroups, _is_field, _is_provided);
}

void routing_manager_proxy::unregister_event(client_t _client,
        service_t _service, instance_t _instance, event_t _event,
        bool _is_provided) {
    (void)_client;
    auto find_service = fields_.find(_service);
    if (find_service != fields_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
            find_instance->second.erase(_event);
            if (find_instance->second.size() == 0)
                find_service->second.erase(find_instance);
        }
        if (find_service->second.size() == 0)
            fields_.erase(find_service);
    }

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
        its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 5]
                    = static_cast<byte_t>(_is_provided);

        sender_->send(its_command, sizeof(its_command));
    } else {
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
}
bool routing_manager_proxy::is_field(service_t _service, instance_t _instance,
        event_t _event) const {
    auto find_service = fields_.find(_service);
    if (find_service != fields_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end())
            return (find_instance->second.find(_event) != find_instance->second.end());
    }
    return false;
}

void routing_manager_proxy::subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major) {
    if (is_connected_) {
        send_subscribe(_client, _service, _instance, _eventgroup, _major);
    } else {
        eventgroup_data_t subscription = { _service, _instance, _eventgroup, _major };
        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        pending_subscriptions_.insert(subscription);
    }
}

void routing_manager_proxy::send_subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major) {
    (void)_client;

    byte_t its_command[VSOMEIP_SUBSCRIBE_COMMAND_SIZE];
    uint32_t its_size = VSOMEIP_SUBSCRIBE_COMMAND_SIZE
            - VSOMEIP_COMMAND_HEADER_SIZE;

    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
            sizeof(client_));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
            sizeof(its_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
            sizeof(_service));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
            sizeof(_instance));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
            sizeof(_eventgroup));
    its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6] = _major;

    sender_->send(its_command, sizeof(its_command));
}

void routing_manager_proxy::unsubscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    (void)_client;

    if (is_connected_) {
        byte_t its_command[VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE];
        uint32_t its_size = VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE
                - VSOMEIP_COMMAND_HEADER_SIZE;

        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_UNSUBSCRIBE;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &client_,
                sizeof(client_));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                sizeof(_eventgroup));

        sender_->send(its_command, sizeof(its_command));
    } else {
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
}

bool routing_manager_proxy::send(client_t its_client,
        std::shared_ptr<message> _message,
        bool _flush) {
    bool is_sent(false);

    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    if (serializer_->serialize(_message.get())) {
        is_sent = send(its_client, serializer_->get_data(),
                serializer_->get_size(), _message->get_instance(),
                _flush, _message->is_reliable());
        serializer_->reset();
    } else {
        VSOMEIP_ERROR << "Failed to serialize message. Check message size!";
    }
    return (is_sent);
}

bool routing_manager_proxy::send(client_t _client, const byte_t *_data,
        length_t _size, instance_t _instance,
        bool _flush,
        bool _reliable) {
    bool is_sent(false);

    std::shared_ptr<endpoint> its_target;
    if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
        if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            service_t its_service = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_SERVICE_POS_MIN],
                    _data[VSOMEIP_SERVICE_POS_MAX]);
            std::lock_guard<std::mutex> its_lock(send_mutex_);
            its_target = find_local(its_service, _instance);
        } else {
            client_t its_client = VSOMEIP_BYTES_TO_WORD(
                    _data[VSOMEIP_CLIENT_POS_MIN],
                    _data[VSOMEIP_CLIENT_POS_MAX]);
            std::lock_guard<std::mutex> its_lock(send_mutex_);
            its_target = find_local(its_client);
        }

        // If no direct endpoint could be found, route to stub
        if (!its_target)
            its_target = sender_;

        std::vector<byte_t> its_command(
                VSOMEIP_COMMAND_HEADER_SIZE + _size + sizeof(instance_t)
                        + sizeof(bool) + sizeof(bool));
        its_command[VSOMEIP_COMMAND_TYPE_POS]
                    = utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) ?
                            VSOMEIP_NOTIFY : VSOMEIP_SEND;

        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
                sizeof(client_t));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &_size,
                sizeof(_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], _data, _size);
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size],
                &_instance, sizeof(instance_t));
        its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size + sizeof(instance_t)] =
                _flush;
        its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size + sizeof(instance_t)
                + sizeof(bool)] = _reliable;
        is_sent = its_target->send(&its_command[0], uint32_t(its_command.size()));
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
    if (is_connected_) {
        send(VSOMEIP_ROUTING_CLIENT, its_notification, true);
    } else if (is_field(_service, _instance, _event)) {
        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        pending_notifications_[_service][_instance][_event] = its_notification;
    }
}

void routing_manager_proxy::notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload, client_t _client) {

    std::shared_ptr<message> its_notification
        = runtime::get()->create_notification();
    its_notification->set_service(_service);
    its_notification->set_instance(_instance);
    its_notification->set_method(_event);
    its_notification->set_payload(_payload);
    its_notification->set_client(_client);
    send(VSOMEIP_ROUTING_CLIENT, its_notification, true);
}

void routing_manager_proxy::on_connect(std::shared_ptr<endpoint> _endpoint) {
    is_connected_ = is_connected_ || (_endpoint == sender_);
    if (is_connected_ && is_started_) {
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


void routing_manager_proxy::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver) {
    (void)_receiver;
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
    ttl_t its_ttl;

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
                            - sizeof(bool)], sizeof(instance_t));
            bool its_reliable;
            std::memcpy(&its_reliable, &_data[_size - sizeof(bool)],
                            sizeof(its_reliable));
            deserializer_->set_data(&_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    its_length);
            std::shared_ptr<message> its_message(
                    deserializer_->deserialize_message());
            if (its_message) {
                its_message->set_instance(its_instance);
                its_message->set_reliable(its_reliable);
                host_->on_message(its_message);
            } else {
                VSOMEIP_ERROR << "Deserialization of vSomeIP message failed";
            }
            deserializer_->reset();
        }
            break;

        case VSOMEIP_ROUTING_INFO:
            on_routing_info(&_data[VSOMEIP_COMMAND_PAYLOAD_POS], its_length);
            break;

        case VSOMEIP_PING:
            send_pong();
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
            std::memcpy(&its_ttl, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                    sizeof(its_ttl));

            host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, true);
            break;

        case VSOMEIP_UNSUBSCRIBE:
            std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                    sizeof(its_service));
            std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                    sizeof(its_instance));
            std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                    sizeof(its_eventgroup));
            host_->on_subscription(its_service, its_instance, its_eventgroup, its_client, false);
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
    for (int i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    state_type_e its_state(state_type_e::ST_DEREGISTERED);
    std::map<service_t, std::map<instance_t, client_t> > old_local_services;
    {
        std::lock_guard<std::mutex> its_lock(local_services_mutex_);
        old_local_services = local_services_;
        local_services_.clear();

        uint32_t i = 0;
        while (i + sizeof(uint32_t) <= _size) {
            uint32_t its_client_size;
            std::memcpy(&its_client_size, &_data[i], sizeof(uint32_t));
            i += uint32_t(sizeof(uint32_t));

            if (i + sizeof(client_t) <= _size) {
                client_t its_client;
                std::memcpy(&its_client, &_data[i], sizeof(client_t));
                i += uint32_t(sizeof(client_t));

                if (its_client != client_) {
                    (void) find_or_create_local(its_client);
                } else {
                    its_state = state_type_e::ST_REGISTERED;
                }

                uint32_t j = 0;
                while (j + sizeof(uint32_t) <= its_client_size) {
                    uint32_t its_services_size;
                    std::memcpy(&its_services_size, &_data[i + j], sizeof(uint32_t));
                    j += uint32_t(sizeof(uint32_t));

                    if (its_services_size >= sizeof(service_t) + sizeof(instance_t)) {
                        its_services_size -= uint32_t(sizeof(service_t));

                        service_t its_service;
                        std::memcpy(&its_service, &_data[i + j], sizeof(service_t));
                        j += uint32_t(sizeof(service_t));

                        while (its_services_size >= sizeof(instance_t)) {
                            instance_t its_instance;
                            std::memcpy(&its_instance, &_data[i + j], sizeof(instance_t));
                            j += uint32_t(sizeof(instance_t));

                            if (its_client != client_)
                                local_services_[its_service][its_instance] = its_client;

                            its_services_size -= uint32_t(sizeof(instance_t));
                        }
                    }
                }

                i += j;
            }
        }
    }

    // inform host about its own registration state changes
    if (state_ != its_state) {
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
                    host_->on_availability(i.first, j.first, false);
                }
            }
        } else {
            for (auto j : i.second) {
                host_->on_availability(i.first, j.first, false);
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
                    host_->on_availability(i.first, j.first, true);
                }
            }
        } else {
            for (auto j : i.second) {
                host_->on_availability(i.first, j.first, true);
            }
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
        (void)sender_->send(its_command, sizeof(its_command));

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
                    send(VSOMEIP_ROUTING_CLIENT, pn.second, true);
                }
            }
        }

        for (auto &po : pending_requests_) {
            send_request_service(client_, po.service_, po.instance_,
                    po.major_, po.minor_, po.use_exclusive_proxy_);
        }

        std::lock_guard<std::mutex> its_lock(pending_mutex_);
        for (auto &ps : pending_subscriptions_)
            send_subscribe(client_, ps.service_, ps.instance_,
                    ps.eventgroup_, ps.major_);

        pending_offers_.clear();
        pending_requests_.clear();
        pending_notifications_.clear();
        pending_subscriptions_.clear();
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

std::shared_ptr<endpoint> routing_manager_proxy::find_local(client_t _client) {
    std::shared_ptr<endpoint> its_endpoint;
    auto found_endpoint = local_endpoints_.find(_client);
    if (found_endpoint != local_endpoints_.end()) {
        its_endpoint = found_endpoint->second;
    }
    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_proxy::create_local(
        client_t _client) {
    std::stringstream its_path;
    its_path << VSOMEIP_BASE_PATH << std::hex << _client;

#ifdef WIN32
    boost::asio::ip::address address = boost::asio::ip::address::from_string("127.0.0.1");
    int port = VSOMEIP_INTERNAL_BASE_PORT + _client;
    VSOMEIP_DEBUG<< "Connecting to ["
        << std::hex << _client << "] at " << port;
#else
    VSOMEIP_DEBUG<< "Connecting to ["
            << std::hex << _client << "] at " << its_path.str();
#endif

    std::shared_ptr<endpoint> its_endpoint = std::make_shared<
            local_client_endpoint_impl>(shared_from_this(),
#ifdef WIN32
            boost::asio::ip::tcp::endpoint(address, port),
#else
            boost::asio::local::stream_protocol::endpoint(its_path.str()),
#endif
            io_, configuration_->get_max_message_size_local());

    local_endpoints_[_client] = its_endpoint;

    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_proxy::find_or_create_local(
        client_t _client) {
    std::shared_ptr<endpoint> its_endpoint(find_local(_client));
    if (0 == its_endpoint) {
        its_endpoint = create_local(_client);
        its_endpoint->start();
    }
    return (its_endpoint);
}

void routing_manager_proxy::remove_local(client_t _client) {
    std::shared_ptr<endpoint> its_endpoint(find_local(_client));
    if (its_endpoint)
        its_endpoint->stop();
    local_endpoints_.erase(_client);
}

std::shared_ptr<endpoint> routing_manager_proxy::find_local(service_t _service,
        instance_t _instance) {
    client_t its_client(0);
    std::lock_guard<std::mutex> its_lock(local_services_mutex_);
    auto found_service = local_services_.find(_service);
    if (found_service != local_services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            its_client = found_instance->second;
        }
    }
    return (find_local(its_client));
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

void routing_manager_proxy::send_register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, std::set<eventgroup_t> _eventgroups,
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

}  // namespace vsomeip

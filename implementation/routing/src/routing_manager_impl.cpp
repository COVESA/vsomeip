// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <memory>
#include <sstream>

#include <vsomeip/configuration.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/logger.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/event.hpp"
#include "../include/eventgroupinfo.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_impl.hpp"
#include "../include/routing_manager_stub.hpp"
#include "../include/servicegroup.hpp"
#include "../include/serviceinfo.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../endpoints/include/local_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_client_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"
#include "../../service_discovery/include/constants.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../service_discovery/include/service_discovery_impl.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

routing_manager_impl::routing_manager_impl(routing_manager_host *_host) :
        host_(_host), io_(_host->get_io()), deserializer_(
                std::make_shared<deserializer>()), serializer_(
                std::make_shared<serializer>()), configuration_(
                host_->get_configuration()) {
}

routing_manager_impl::~routing_manager_impl() {
}

boost::asio::io_service & routing_manager_impl::get_io() {
    return (io_);
}

client_t routing_manager_impl::get_client() const {
    return host_->get_client();
}

void routing_manager_impl::init() {
    uint32_t its_max_message_size = VSOMEIP_MAX_LOCAL_MESSAGE_SIZE;
    if (VSOMEIP_MAX_TCP_MESSAGE_SIZE > its_max_message_size)
        its_max_message_size = VSOMEIP_MAX_TCP_MESSAGE_SIZE;
    if (VSOMEIP_MAX_UDP_MESSAGE_SIZE > its_max_message_size)
        its_max_message_size = VSOMEIP_MAX_UDP_MESSAGE_SIZE;

    serializer_->create_data(its_max_message_size);

    // TODO: Only instantiate the stub if needed
    stub_ = std::make_shared<routing_manager_stub>(this);
    stub_->init();

    // We need to be able to send messages to ourself (for delivering events)
    (void)create_local(VSOMEIP_ROUTING_CLIENT);

    if (configuration_->is_service_discovery_enabled()) {
        VSOMEIP_INFO<< "Service Discovery enabled. Trying to load module.";
        std::shared_ptr<sd::runtime> *its_runtime =
                static_cast<std::shared_ptr<sd::runtime> *>(utility::load_library(
                        VSOMEIP_SD_LIBRARY,
                        VSOMEIP_SD_RUNTIME_SYMBOL_STRING));

        if (its_runtime && (*its_runtime)) {
            VSOMEIP_INFO << "Service Discovery module loaded.";
            discovery_ = (*its_runtime)->create_service_discovery(this);
            discovery_->init();
        }

        init_event_routing_info();
    } else {
        init_routing_info();
    }
}

void routing_manager_impl::start() {
    stub_->start();
    if (discovery_)
        discovery_->start();

    host_->on_event(event_type_e::ET_REGISTERED);
}

void routing_manager_impl::stop() {
    host_->on_event(event_type_e::ET_DEREGISTERED);

    if (discovery_)
        discovery_->stop();
    stub_->stop();
}

void routing_manager_impl::offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        ttl_t _ttl) {
    std::shared_ptr<serviceinfo> its_info;
    {
        std::lock_guard<std::mutex> its_lock(local_mutex_);
        local_services_[_service][_instance] = _client;

        // Remote route (incoming only)
        its_info = find_service(_service, _instance);
        if (its_info) {
            if (its_info->get_major() == _major
                    && its_info->get_minor() == _minor) {
                its_info->set_ttl(_ttl);
            } else {
                host_->on_error(error_code_e::SERVICE_PROPERTY_MISMATCH);
            }
        } else {
            its_info = create_service(_service, _instance, _major, _minor, _ttl);
        }
    }

    if (discovery_ && its_info) {
        std::string group_name("default");
        if (its_info->get_group())
            group_name = its_info->get_group()->get_name();
        discovery_->on_offer_change(group_name);
    }

    stub_->on_offer_service(_client, _service, _instance);
    host_->on_availability(_service, _instance, true);
}

void routing_manager_impl::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance) {
    on_stop_offer_service(_service, _instance);
    stub_->on_stop_offer_service(_client, _service, _instance);
}

void routing_manager_impl::request_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        ttl_t _ttl, bool _has_selective) {

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info) {
        if ((_major < ANY_MAJOR && _major > its_info->get_major())
                || (_major == its_info->get_major()
                        && _minor < ANY_MINOR && _minor > its_info->get_minor())
                || (_ttl > its_info->get_ttl())) {
            host_->on_error(error_code_e::SERVICE_PROPERTY_MISMATCH);
        } else {
            its_info->add_client(_client);
        }
    } else {
        if (discovery_)
            discovery_->request_service(_service, _instance, _major, _minor,
                    _ttl);
    }

    // TODO: Mutex?!
    if (_has_selective) {
        specific_endpoint_clients.insert(_client);
    }
}

void routing_manager_impl::release_service(client_t _client, service_t _service,
        instance_t _instance) {
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info) {
        its_info->remove_client(_client);
    } else {
        if (discovery_)
            discovery_->release_service(_service, _instance);
    }
}

void routing_manager_impl::subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        ttl_t _ttl) {
    if (discovery_) {
        if (!host_->on_subscription(_service, _instance, _eventgroup, _client, true)) {
            VSOMEIP_INFO << "Subscription request for eventgroup " << _eventgroup
                    << " rejected from application handler";
            return;
        }
        eventgroup_clients_[_service][_instance][_eventgroup].insert(_client);
        if (0 == find_local_client(_service, _instance)) {
            client_t subscriber = VSOMEIP_ROUTING_CLIENT;
            // subscriber != VSOMEIP_ROUTING_CLIENT implies to use its own endpoint
            auto its_selective = specific_endpoint_clients.find(_client);
            if (its_selective != specific_endpoint_clients.end()) {
                subscriber = _client;
            }
            discovery_->subscribe(_service, _instance, _eventgroup, _major,
                    _ttl, subscriber);
        } else {
            send_subscribe(_client, _service, _instance, _eventgroup, _major, _ttl);

            std::shared_ptr<eventgroupinfo> its_eventgroup
                = find_eventgroup(_service, _instance, _eventgroup);
            if (its_eventgroup) {
                std::set<std::shared_ptr<event> > its_events
                    = its_eventgroup->get_events();
                for (auto e : its_events) {
                    if (e->is_field())
                        e->notify_one(_client);
                }
            }
        }
    } else {
        VSOMEIP_ERROR<< "SOME/IP eventgroups require SD to be enabled!";
    }
}

void routing_manager_impl::unsubscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    if (discovery_) {
        auto found_service = eventgroup_clients_.find(_service);
        if (found_service != eventgroup_clients_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_eventgroup = found_instance->second.find(
                        _eventgroup);
                if (found_eventgroup != found_instance->second.end()) {
                    found_eventgroup->second.erase(_client);
                    if (0 == found_eventgroup->second.size()) {
                        eventgroup_clients_.erase(_eventgroup);
                    }
                }
            }
        }
        host_->on_subscription(_service, _instance, _eventgroup, _client, false);

        if (0 == find_local_client(_service, _instance)) {
            discovery_->unsubscribe(_service, _instance, _eventgroup);
        } else {
            send_unsubscribe(_client, _service, _instance, _eventgroup);
        }
    } else {
        VSOMEIP_ERROR<< "SOME/IP eventgroups require SD to be enabled!";
    }
}

bool routing_manager_impl::send(client_t its_client,
        std::shared_ptr<message> _message, bool _flush) {
    bool is_sent(false);

    if (utility::is_request(_message->get_message_type())) {
        _message->set_client(its_client);
    }

    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    if (serializer_->serialize(_message.get())) {
        is_sent = send(its_client, serializer_->get_data(),
                serializer_->get_size(), _message->get_instance(),
                _flush, _message->is_reliable());
        serializer_->reset();
    }

    return (is_sent);
}

bool routing_manager_impl::send(client_t _client, const byte_t *_data,
        length_t _size, instance_t _instance,
        bool _flush, bool _reliable) {
    bool is_sent(false);

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

    if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
        if (is_request) {
            its_target = find_local(its_service, _instance);
        } else if (!is_notification) {
            its_target = find_local(its_client);
        } else if (is_notification && _client) {
            // Selective notifications!
            its_target = find_local(_client);
        }

        if (its_target) {
            is_sent = send_local(its_target, _client, _data, _size, _instance, _flush, _reliable);
        } else {
            // Check whether hosting application should get the message
            // If not, check routes to external
            if ((its_client == host_->get_client() && !is_request)
                    || (find_local_client(its_service, _instance)
                            == host_->get_client() && is_request)) {
                // TODO: find out how to handle session id here
                is_sent = deliver_message(_data, _size, _instance, _reliable);
            } else {
                if (is_request) {
                    client_t client = VSOMEIP_ROUTING_CLIENT;
                    if (specific_endpoint_clients.find(its_client) != specific_endpoint_clients.end()) {
                        client = its_client;
                    }
                    its_target = find_or_create_remote_client(its_service, _instance, _reliable, client);
                    if (its_target) {
                        is_sent = its_target->send(_data, _size, _flush);
                    } else {
                        VSOMEIP_ERROR<< "Routing error. Client from remote service could not be found!";
                    }
                } else {
                    std::shared_ptr<serviceinfo> its_info(find_service(its_service, _instance));
                    if (its_info) {
                        its_target = its_info->get_endpoint(_reliable);
                        if (is_notification && !is_service_discovery) {
                            method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
                                    _data[VSOMEIP_METHOD_POS_MAX]);
                            std::shared_ptr<event> its_event = find_event(its_service, _instance, its_method);
                            if (its_event) {
                                std::vector< byte_t > its_data;

                                for (auto its_group : its_event->get_eventgroups()) {
                                    // local
                                    auto its_local_clients = find_local_clients(its_service, _instance, its_group);
                                    for (auto its_local_client : its_local_clients) {
                                        // If we also want to receive the message, send it to the routing manager
                                        // We cannot call deliver_message in this case as this would end in receiving
                                        // an answer before the call to send has finished.
                                        if (its_local_client == host_->get_client()) {
                                            its_local_client = VSOMEIP_ROUTING_CLIENT;
                                        }

                                        std::shared_ptr<endpoint> its_local_target = find_local(its_local_client);
                                        if (its_local_target) {
                                            send_local(its_local_target, _client, _data, _size, _instance, _flush, _reliable);
                                        }
                                    }

                                    if (its_target) {
                                        // remote
                                        auto its_eventgroup = find_eventgroup(its_service, _instance, its_group);
                                        if (its_eventgroup) {
                                            for (auto its_remote : its_eventgroup->get_targets()) {
                                                its_target->send_to(its_remote, _data, _size);
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (its_target) {
                                is_sent = its_target->send(_data, _size, _flush);
                            } else {
                                VSOMEIP_ERROR << "Routing error. Endpoint for service ["
                                        << std::hex << its_service << "." << _instance
                                        << "] could not be found!";
                            }
                        }
                    } else {
                        if (!is_notification) {
                            VSOMEIP_ERROR << "Routing error. Not hosting service ["
                                    << std::hex << its_service << "." << _instance
                                    << "]";
                        }
                    }
                }
            }
        }
    }

    return (is_sent);
}

bool routing_manager_impl::send_local(
        std::shared_ptr<endpoint>& _target, client_t _client,
        const byte_t *_data, uint32_t _size,
        instance_t _instance,
        bool _flush, bool _reliable) const {

    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);

    std::vector<byte_t> its_command(
            VSOMEIP_COMMAND_HEADER_SIZE + _size + sizeof(instance_t)
                    + sizeof(bool) + sizeof(bool));
    its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SEND;
    std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
            sizeof(client_t));
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &_size,
            sizeof(_size));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], _data,
            _size);
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size],
            &_instance, sizeof(instance_t));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size
            + sizeof(instance_t)], &_flush, sizeof(bool));
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size
            + sizeof(instance_t) + sizeof(bool)], &_reliable, sizeof(bool));

    return _target->send(&its_command[0], its_command.size(), _flush);
}

bool routing_manager_impl::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        std::shared_ptr<message> _message) {
    bool is_sent(false);
    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    if (serializer_->serialize(_message.get())) {
        is_sent = send_to(_target,
                serializer_->get_data(), serializer_->get_size());
        serializer_->reset();
    } else {
        VSOMEIP_ERROR<< "routing_manager_impl::send_to: serialization failed.";
    }
    return (is_sent);
}

bool routing_manager_impl::send_to(
        const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size) {
    std::shared_ptr<endpoint> its_endpoint = find_or_create_server_endpoint(
            _target->get_remote_port(), _target->is_reliable());

    return (its_endpoint && its_endpoint->send_to(_target, _data, _size));
}

void routing_manager_impl::notify(
        service_t _service, instance_t _instance, event_t _event,
        std::shared_ptr<payload> _payload) {
    std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
    if (its_event) {
        its_event->set_payload(_payload);
    } else {
        VSOMEIP_ERROR << "routing_manager_impl::notify: event ["
            << std::hex << _service << "." << _instance << "." << _event
            << "] is unknown.";
    }
}

void routing_manager_impl::notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload, client_t _client) {

    std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
    if (its_event) {
        for (auto its_group : its_event->get_eventgroups()) {
            auto its_eventgroup = find_eventgroup(_service, _instance, its_group);
            if (its_eventgroup) {
                auto its_subscriber = remote_subscriber_map_.find(_client);
                if (its_subscriber != remote_subscriber_map_.end()) {
                    its_event->set_payload(_payload, its_subscriber->second);
                } else {
                    its_event->set_payload(_payload, _client);
                }
            }
        }
    } else {
        VSOMEIP_ERROR << "routing_manager_impl::notify: event ["
            << std::hex << _service << "." << _instance << "." << _event
            << "] is unknown.";
    }
}

void routing_manager_impl::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver) {
#if 0
    std::stringstream msg;
    msg << "rmi::on_message: ";
    for (uint32_t i = 0; i < _size; ++i)
    msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    service_t its_service;
    instance_t its_instance;
    message_type_e its_message_type;

    if (_size >= VSOMEIP_SOMEIP_HEADER_SIZE) {
        its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
                _data[VSOMEIP_SERVICE_POS_MAX]);
        if (its_service == VSOMEIP_SD_SERVICE) {
            if (discovery_)
                discovery_->on_message(_data, _size);
        } else {
            its_instance = find_instance(its_service, _receiver);
            its_message_type = static_cast<message_type_e>(_data[VSOMEIP_MESSAGE_TYPE_POS]);
            if (its_instance == 0xFFFF) {
                VSOMEIP_WARNING << "Receiving endpoint is not configured for service 0x"
                        << std::hex << its_service;
                return;
            }

            // TODO: move specific endpoint handling to to a method!
            auto found_service = remote_services_.find(its_service);
            if (found_service != remote_services_.end()) {
                auto found_instance = found_service->second.find(its_instance);
                if (found_instance != found_service->second.end()) {
                    for (auto client_entry : found_instance->second) {
                        client_t client = client_entry.first;
                        if (!client) {
                            continue;
                        }
                        auto found_reliability = client_entry.second.find(_receiver->is_reliable());
                        if (found_reliability != client_entry.second.end()) {
                            auto found_enpoint = found_reliability->second;
                            if (found_enpoint.get() == _receiver) {
                                auto local_endpoint = find_local(client);
                                if (client != get_client()) {
                                    send_local(local_endpoint, client, _data, _size, its_instance, true, _receiver->is_reliable());
                                } else {
                                    deliver_message(_data, _size, its_instance, _receiver->is_reliable());
                                }
                                return;
                            }
                        }
                    }
                }
            }
            on_message(its_service, its_instance, _data, _size, _receiver->is_reliable());
        }
    }
}

void routing_manager_impl::on_message(
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _size,
        bool _reliable) {
    method_t its_method;
    client_t its_client;
    session_t its_session;

    its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN],
                                       _data[VSOMEIP_CLIENT_POS_MAX]);

    its_method = VSOMEIP_BYTES_TO_WORD(
            _data[VSOMEIP_METHOD_POS_MIN],
            _data[VSOMEIP_METHOD_POS_MAX]);

    if (utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]) && its_client) {
        // targeted notification
        // reset client_id/subscriber/target field
        const_cast<byte_t *>(_data)[VSOMEIP_CLIENT_POS_MIN] = 0;
        const_cast<byte_t *>(_data)[VSOMEIP_CLIENT_POS_MAX] = 0;

        its_method = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_METHOD_POS_MIN],
                _data[VSOMEIP_METHOD_POS_MAX]);

        auto it_subscriber = remote_subscriber_map_.find(its_client);
        if (it_subscriber != remote_subscriber_map_.end()) {
            send_to(it_subscriber->second, _data, _size);
        } else {
            if (its_client == host_->get_client()) {
                deliver_message(_data, _size, _instance, _reliable);
            } else {
                send(its_client, _data, _size, _instance, true, _reliable);
            }
        }
        return;
    }

    if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        std::shared_ptr<event> its_event
            = find_event(_service, _instance, its_method);
        if (its_event) {
            uint32_t its_length = utility::get_payload_size(_data, _size);
            if (its_length > 0) { // set
                std::shared_ptr<payload> its_payload =
                        runtime::get()->create_payload(
                                &_data[VSOMEIP_PAYLOAD_POS],
                                its_length);
                its_event->set_payload(its_payload);
            }

            if (!utility::is_request_no_return(
                    _data[VSOMEIP_MESSAGE_TYPE_POS])) {
                std::shared_ptr<message> its_response =
                        runtime::get()->create_message();
                its_session = VSOMEIP_BYTES_TO_WORD(
                        _data[VSOMEIP_SESSION_POS_MIN],
                        _data[VSOMEIP_SESSION_POS_MAX]);

                its_response->set_service(_service);
                its_response->set_method(its_method);
                its_response->set_client(its_client);
                its_response->set_session(its_session);

                if (its_event->is_field()) {
                    its_response->set_message_type(
                            message_type_e::MT_RESPONSE);
                    its_response->set_payload(its_event->get_payload());
                } else {
                    its_response->set_message_type(message_type_e::MT_ERROR);
                }

                std::lock_guard<std::mutex> its_lock(serialize_mutex_);
                if (serializer_->serialize(its_response.get())) {
                    send(its_client,
                        serializer_->get_data(), serializer_->get_size(),
                        _instance,
                        true, its_event->is_reliable());
                } else {
                    VSOMEIP_ERROR << "routing_manager_impl::on_message: serialization error.";
                }
                serializer_->reset();
            }
            return;
        } else {
            its_client = find_local_client(_service, _instance);
        }
    } else {
        its_client = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_CLIENT_POS_MIN],
                _data[VSOMEIP_CLIENT_POS_MAX]);
    }

    if ((its_client == VSOMEIP_ROUTING_CLIENT
            && utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS]))
        || its_client == host_->get_client()) {
        deliver_message(_data, _size, _instance, _reliable);
    } else {
        send(its_client, _data, _size, _instance, true, _reliable);
    }
}

void routing_manager_impl::on_connect(std::shared_ptr<endpoint> _endpoint) {
    for (auto &its_service : remote_services_) {
        for (auto &its_instance : its_service.second) {
            auto found_endpoint = its_instance.second.find(false);
            if (found_endpoint != its_instance.second.end()) {
                host_->on_availability(its_service.first, its_instance.first,
                        true);
            } else {
                found_endpoint = its_instance.second.find(true);
                if (found_endpoint != its_instance.second.end()) {
                    host_->on_availability(its_service.first,
                            its_instance.first, true);
                }
            }
        }
    }
}

void routing_manager_impl::on_disconnect(std::shared_ptr<endpoint> _endpoint) {
    for (auto &its_service : remote_services_) {
        for (auto &its_instance : its_service.second) {
            auto found_endpoint = its_instance.second.find(false);
            if (found_endpoint != its_instance.second.end()) {
                host_->on_availability(its_service.first, its_instance.first,
                        false);
            } else {
                found_endpoint = its_instance.second.find(true);
                if (found_endpoint != its_instance.second.end()) {
                    host_->on_availability(its_service.first,
                            its_instance.first, false);
                }
            }
        }
    }
}

void routing_manager_impl::on_stop_offer_service(service_t _service,
        instance_t _instance) {

    for (auto &s : events_)
        for (auto &i : s.second)
            for (auto &e : i.second)
                e.second->unset_payload();

    host_->on_availability(_service, _instance, false);

    if (discovery_) {
        auto found_service = services_.find(_service);
        if (found_service != services_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                found_instance->second->set_ttl(0);
                discovery_->on_offer_change(
                        found_instance->second->get_group()->get_name());
            }
        }
    } else {
        // TODO: allow to withdraw a service on one endpoint only
        del_routing_info(_service, _instance, false);
        del_routing_info(_service, _instance, true);
    }
}

bool routing_manager_impl::deliver_message(const byte_t *_data, length_t _size,
        instance_t _instance, bool _reliable) {
    bool is_sent(false);
    deserializer_->set_data(_data, _size);
    std::shared_ptr<message> its_message(deserializer_->deserialize_message());
    if (its_message) {
        its_message->set_instance(_instance);
        its_message->set_reliable(_reliable);
        host_->on_message(its_message);
        is_sent = true;
    } else {
        // TODO: send error "Malformed Message"
        //send_error();
    }
    return (is_sent);
}

const std::map<std::string, std::shared_ptr<servicegroup> > &
routing_manager_impl::get_servicegroups() const {
    return (servicegroups_);
}

std::shared_ptr<eventgroupinfo> routing_manager_impl::find_eventgroup(
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) const {
    std::shared_ptr<eventgroupinfo> its_info(nullptr);
    auto found_service = eventgroups_.find(_service);
    if (found_service != eventgroups_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                its_info = found_eventgroup->second;
                std::shared_ptr<serviceinfo> its_service_info
                    = find_service(_service, _instance);
                if (its_service_info) {
                    if (_eventgroup
                            == its_service_info->get_multicast_group()) {
                        try {
                            boost::asio::ip::address its_multicast_address =
                                    boost::asio::ip::address::from_string(
                                            its_service_info->get_multicast_address());
                            uint16_t its_multicast_port =
                                    its_service_info->get_multicast_port();
                            its_info->set_multicast(its_multicast_address,
                                    its_multicast_port);
                        }
                        catch (...) {
                            VSOMEIP_ERROR << "Eventgroup ["
                                    << std::hex << std::setw(4) << std::setfill('0')
                                    << _service << "." << _instance << "." << _eventgroup
                                    << "] is configured as multicast, but no valid "
                                       "multicast address is configured!";
                        }
                    }
                    its_info->set_major(its_service_info->get_major());
                    its_info->set_ttl(its_service_info->get_ttl());
                }
            }
        }
    }
    return (its_info);
}

std::shared_ptr<configuration> routing_manager_impl::get_configuration() const {
    return (host_->get_configuration());
}

void routing_manager_impl::create_service_discovery_endpoint(
        const std::string &_address, uint16_t _port, bool _reliable) {
    std::shared_ptr<endpoint> its_service_endpoint = find_server_endpoint(_port,
            _reliable);
    if (!its_service_endpoint) {
        its_service_endpoint = create_server_endpoint(_port, _reliable);

        if (its_service_endpoint) {
            std::shared_ptr<serviceinfo> its_info(
                    std::make_shared<serviceinfo>(ANY_MAJOR, ANY_MINOR, ANY_TTL));
            its_info->set_endpoint(its_service_endpoint, _reliable);

            // routing info
            services_[VSOMEIP_SD_SERVICE][VSOMEIP_SD_INSTANCE] = its_info;

            its_service_endpoint->add_multicast(VSOMEIP_SD_SERVICE,
                    VSOMEIP_SD_METHOD, _address, _port);
            its_service_endpoint->join(_address);
            its_service_endpoint->start();
        } else {
            VSOMEIP_ERROR << "Service Discovery endpoint could not be created. "
                    "Please check your network configuration.";
        }
    }
}

services_t routing_manager_impl::get_offered_services(
        const std::string &_name) const {
    services_t its_offers;
    auto find_servicegroup = servicegroups_.find(_name);
    if (find_servicegroup != servicegroups_.end()) {
        if (find_servicegroup->second->is_local()) {
            return (find_servicegroup->second->get_services());
        }
    }

    return (its_offers);
}

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_remote_client(
        service_t _service, instance_t _instance, bool _reliable, client_t _client) {
    std::shared_ptr<endpoint> its_endpoint = find_remote_client(_service, _instance,
            _reliable, _client);
    // Create specific IP-client-endpoint for selective clients
    if (!its_endpoint &&
            specific_endpoint_clients.find(_client) != specific_endpoint_clients.end()) {

        its_endpoint = create_remote_client(_service, _instance, _reliable, _client);
    }
    return its_endpoint;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<serviceinfo> routing_manager_impl::find_service(
        service_t _service, instance_t _instance) const {
    std::shared_ptr<serviceinfo> its_info;
    auto found_service = services_.find(_service);
    if (found_service != services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            its_info = found_instance->second;
        }
    }
    return (its_info);
}

std::shared_ptr<serviceinfo> routing_manager_impl::create_service(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, ttl_t _ttl) {

    std::shared_ptr<serviceinfo> its_info;
    if (configuration_) {
        its_info = std::make_shared<serviceinfo>(_major, _minor, _ttl);

        uint16_t its_reliable_port = configuration_->get_reliable_port(_service,
                _instance);
        uint16_t its_unreliable_port = configuration_->get_unreliable_port(
                _service, _instance);

        its_info->set_multicast_address(
                configuration_->get_multicast_address(_service, _instance));
        its_info->set_multicast_port(
                configuration_->get_multicast_port(_service, _instance));
        its_info->set_multicast_group(
                configuration_->get_multicast_group(_service, _instance));

        std::shared_ptr<endpoint> its_reliable_endpoint;
        std::shared_ptr<endpoint> its_unreliable_endpoint;

        if (ILLEGAL_PORT != its_reliable_port) {
            its_reliable_endpoint = find_or_create_server_endpoint(
                    its_reliable_port,
                    true);
            its_info->set_endpoint(its_reliable_endpoint, true);

            // TODO: put this in a method and check whether an assignment already exists!
            service_instances_[_service][its_reliable_endpoint.get()] =
                    _instance;
        }

        if (ILLEGAL_PORT != its_unreliable_port) {
            its_unreliable_endpoint = find_or_create_server_endpoint(
                    its_unreliable_port, false);
            its_info->set_endpoint(its_unreliable_endpoint, false);

            service_instances_[_service][its_unreliable_endpoint.get()] =
                    _instance;
        }

        if (ILLEGAL_PORT != its_reliable_port
                || ILLEGAL_PORT != its_unreliable_port) {
            std::string its_servicegroup = configuration_->get_group(_service,
                    _instance);
            auto found_servicegroup = servicegroups_.find(its_servicegroup);
            if (found_servicegroup == servicegroups_.end()) {
                servicegroups_[its_servicegroup] =
                        std::make_shared<servicegroup>(its_servicegroup,
                                configuration_->is_local_servicegroup(its_servicegroup));
            }
            servicegroups_[its_servicegroup]->add_service(_service, _instance,
                    its_info);
            services_[_service][_instance] = its_info;
        } else {
            VSOMEIP_DEBUG << "Port configuration missing. Assuming internal service.";
        }
    } else {
        host_->on_error(error_code_e::CONFIGURATION_MISSING);
    }

    return (its_info);
}

std::shared_ptr<endpoint> routing_manager_impl::create_client_endpoint(
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _reliable, client_t _client) {
    std::shared_ptr<endpoint> its_endpoint;
    try {
        if (_reliable) {
            its_endpoint = std::make_shared<tcp_client_endpoint_impl>(
                    shared_from_this(),
                    boost::asio::ip::tcp::endpoint(_address, _port), io_);

            if (configuration_->has_enabled_magic_cookies(_address.to_string(),
                    _port)) {
                its_endpoint->enable_magic_cookies();
            }
        } else {
            its_endpoint = std::make_shared<udp_client_endpoint_impl>(
                    shared_from_this(),
                    boost::asio::ip::udp::endpoint(_address, _port), io_);
        }

        client_endpoints_[_address][_port][_client][_reliable] = its_endpoint;
        its_endpoint->start();
    } catch (...) {
        host_->on_error(error_code_e::CLIENT_ENDPOINT_CREATION_FAILED);
    }

    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::find_client_endpoint(
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _reliable, client_t _client) {
    std::shared_ptr<endpoint> its_endpoint;
    auto found_address = client_endpoints_.find(_address);
    if (found_address != client_endpoints_.end()) {
        auto found_port = found_address->second.find(_port);
        if (found_port != found_address->second.end()) {
            auto found_client = found_port->second.find(_client);
            if (found_client != found_port->second.end()) {
                auto found_endpoint = found_client->second.find(_reliable);
                if (found_endpoint != found_client->second.end()) {
                    its_endpoint = found_endpoint->second;
                }
            }
        }
    }
    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_client_endpoint(
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _reliable, client_t _client) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    std::shared_ptr<endpoint> its_endpoint = find_client_endpoint(_address,
            _port, _reliable, _client);
    if (0 == its_endpoint) {
        its_endpoint = create_client_endpoint(_address, _port, _reliable, _client);
    }
    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::create_server_endpoint(
        uint16_t _port, bool _reliable) {
    std::shared_ptr<endpoint> its_endpoint;

    try {
        boost::asio::ip::address its_unicast = configuration_->get_unicast();
        if (_reliable) {
            its_endpoint = std::make_shared<tcp_server_endpoint_impl>(
                    shared_from_this(),
                    boost::asio::ip::tcp::endpoint(its_unicast, _port), io_);
            if (configuration_->has_enabled_magic_cookies(
                    its_unicast.to_string(), _port)) {
                its_endpoint->enable_magic_cookies();
            }
        } else {
            // TODO: this should be more specific.
            if (its_unicast.is_v4()) {
                its_unicast = boost::asio::ip::address_v4::any();
            } else {
                // TODO: how is "ANY" specified in IPv6?
            }
            boost::asio::ip::udp::endpoint ep(its_unicast, _port);
            its_endpoint = std::make_shared<udp_server_endpoint_impl>(
                    shared_from_this(),
                    ep, io_);
        }

        if (its_endpoint) {
            server_endpoints_[_port][_reliable] = its_endpoint;
            its_endpoint->start();
        }
    } catch (std::exception &e) {
        host_->on_error(error_code_e::SERVER_ENDPOINT_CREATION_FAILED);
        VSOMEIP_ERROR << e.what();
    }

    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::find_server_endpoint(
        uint16_t _port, bool _reliable) {
    std::shared_ptr<endpoint> its_endpoint;
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
        uint16_t _port, bool _reliable) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    std::shared_ptr<endpoint> its_endpoint = find_server_endpoint(_port,
            _reliable);
    if (0 == its_endpoint) {
        its_endpoint = create_server_endpoint(_port, _reliable);
    }
    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::find_local(client_t _client) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    std::shared_ptr<endpoint> its_endpoint;
    auto found_endpoint = local_clients_.find(_client);
    if (found_endpoint != local_clients_.end()) {
        its_endpoint = found_endpoint->second;
    }
    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::create_local(client_t _client) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);

    std::stringstream its_path;
    its_path << VSOMEIP_BASE_PATH << std::hex << _client;

#ifdef WIN32
    boost::asio::ip::address address = boost::asio::ip::address::from_string("127.0.0.1");
    int port = 51235 + _client;
#endif

    std::shared_ptr<endpoint> its_endpoint = std::make_shared<
        local_client_endpoint_impl>(shared_from_this(),
#ifdef WIN32
        boost::asio::ip::tcp::endpoint(address, port)
#else
        boost::asio::local::stream_protocol::endpoint(its_path.str())
#endif
    , io_);
    local_clients_[_client] = its_endpoint;
    its_endpoint->start();
    return (its_endpoint);
}

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_local(
        client_t _client) {
    std::shared_ptr<endpoint> its_endpoint(find_local(_client));
    if (!its_endpoint) {
        its_endpoint = create_local(_client);
    }
    return (its_endpoint);
}

void routing_manager_impl::remove_local(client_t _client) {
    {
        std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
        std::shared_ptr<endpoint> its_endpoint = find_local(_client);
        its_endpoint->stop();
        local_clients_.erase(_client);
    }
    {
        std::lock_guard<std::mutex> its_lock(local_mutex_);
        // Finally remove all services that are implemented by the client.
        std::set<std::pair<service_t, instance_t>> its_services;
        for (auto& s : local_services_) {
            for (auto& i : s.second) {
                if (i.second == _client)
                    its_services.insert({ s.first, i.first });
            }
        }

        for (auto& si : its_services) {
            local_services_[si.first].erase(si.second);
            if (local_services_[si.first].size() == 0)
                local_services_.erase(si.first);
        }
    }
}

std::shared_ptr<endpoint> routing_manager_impl::find_local(service_t _service,
        instance_t _instance) {
    client_t client = find_local_client(_service, _instance);
    if (client) {
        return find_local(client);
    }
    return nullptr;
}

client_t routing_manager_impl::find_local_client(service_t _service,
        instance_t _instance) {
    std::lock_guard<std::mutex> its_lock(local_mutex_);
    client_t its_client(0);
    auto found_service = local_services_.find(_service);
    if (found_service != local_services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            its_client = found_instance->second;
        }
    }
    return (its_client);
}

std::set<client_t> routing_manager_impl::find_local_clients(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    std::set<client_t> its_clients;
    auto found_service = eventgroup_clients_.find(_service);
    if (found_service != eventgroup_clients_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                its_clients = found_eventgroup->second;
            }
        }
    }
    return (its_clients);
}

instance_t routing_manager_impl::find_instance(service_t _service,
        endpoint * _endpoint) {
    instance_t its_instance(0xFFFF);
    auto found_service = service_instances_.find(_service);
    if (found_service != service_instances_.end()) {
        auto found_endpoint = found_service->second.find(_endpoint);
        if (found_endpoint != found_service->second.end()) {
            its_instance = found_endpoint->second;
        }
    }
    return (its_instance);
}

std::shared_ptr<endpoint> routing_manager_impl::create_remote_client(
        service_t _service, instance_t _instance, bool _reliable, client_t _client) {
    std::shared_ptr<endpoint> its_endpoint;
    auto found_service = remote_services_.find(_service);
    if (found_service != remote_services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            for (auto its_client : found_instance->second) {
                auto found_reliability = its_client.second.find(_reliable);
                if (found_reliability != its_client.second.end()) {
                    std::shared_ptr<endpoint> endpoint = found_reliability->second;
                    boost::asio::ip::address address;
                    endpoint->get_remote_address(address);
                    its_endpoint = find_or_create_client_endpoint(address,
                            endpoint->get_remote_port(), _reliable, _client);
                    break;
                }
            }
        }
    }
    if (its_endpoint) {
        remote_services_[_service][_instance][_client][_reliable] = its_endpoint;
        service_instances_[_service][its_endpoint.get()] = _instance;
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
    return its_endpoint;
}

std::shared_ptr<event> routing_manager_impl::find_event(service_t _service,
        instance_t _instance, event_t _event) const {
    std::shared_ptr<event> its_event;
    auto find_service = events_.find(_service);
    if (find_service != events_.end()) {
        auto find_instance = find_service->second.find(_instance);
        if (find_instance != find_service->second.end()) {
            auto find_event = find_instance->second.find(_event);
            if (find_event != find_instance->second.end()) {
                its_event = find_event->second;
            }
        }
    }
    return (its_event);
}

std::set<std::shared_ptr<event> > routing_manager_impl::find_events(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    std::set<std::shared_ptr<event> > its_events;
    auto found_service = eventgroups_.find(_service);
    if (found_service != eventgroups_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                return (found_eventgroup->second->get_events());
            }
        }
    }
    return (its_events);
}

void routing_manager_impl::add_routing_info(service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        ttl_t _ttl, const boost::asio::ip::address &_address, uint16_t _port,
        bool _reliable) {
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (!its_info) {
        its_info = create_service(_service, _instance, _major, _minor, _ttl);
        services_[_service][_instance] = its_info;
    }

    // If host doesn't have a specific endpoint we create it here
    if (specific_endpoint_clients.find(get_client()) == specific_endpoint_clients.end()) {
        std::shared_ptr<endpoint> its_endpoint(
                find_or_create_client_endpoint(_address, _port, _reliable, VSOMEIP_ROUTING_CLIENT));
        its_info->set_endpoint(its_endpoint, _reliable);

        remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT][_reliable] = its_endpoint;
        service_instances_[_service][its_endpoint.get()] = _instance;

        if (its_endpoint->is_connected()) {
            host_->on_availability(_service, _instance, true);
        }
    }

    // Create all specific endpoints for proxies probably like them
    for (client_t client : specific_endpoint_clients) {
        std::shared_ptr<endpoint> its_endpoint(
                find_or_create_client_endpoint(_address, _port, _reliable, client));

        if (client == get_client()) {
            its_info->set_endpoint(its_endpoint, _reliable);
        }

        remote_services_[_service][_instance][client][_reliable] = its_endpoint;
        service_instances_[_service][its_endpoint.get()] = _instance;

        if (its_endpoint->is_connected()) {
            host_->on_availability(_service, _instance, true);
        }
    }

    stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance);
}

void routing_manager_impl::del_routing_info(service_t _service,
        instance_t _instance,
        bool _reliable) {
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info) {
        std::shared_ptr<endpoint> its_empty_endpoint;

        // TODO: only tell the application if the service is completely gone
        host_->on_availability(_service, _instance, false);
        stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service,
                _instance);

        // Implicit unsubscribe
        auto found_service = eventgroups_.find(_service);
        if (found_service != eventgroups_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                for (auto &its_eventgroup : found_instance->second) {
                    its_eventgroup.second->clear_targets();
                }
            }
        }

        std::shared_ptr<endpoint> its_endpoint = its_info->get_endpoint(
                _reliable);
        if (its_endpoint) {
            if (1 >= service_instances_[_service].size()) {
                service_instances_.erase(_service);
            } else {
                service_instances_[_service].erase(its_endpoint.get());
            }

            remote_services_[_service][_instance].erase(_reliable);
            auto found_endpoint = remote_services_[_service][_instance].find(
                    !_reliable);
            if (found_endpoint == remote_services_[_service][_instance].end()) {
                remote_services_[_service].erase(_instance);
            }
            if (1 >= remote_services_[_service].size()) {
                remote_services_.erase(_service);
            }
        }

        if (!its_info->get_endpoint(!_reliable)) {
            if (its_info->get_group()) {
                its_info->get_group()->remove_service(_service, _instance);
                if (1 >= services_[_service].size()) {
                    services_.erase(_service);
                } else {
                    services_[_service].erase(_instance);
                }
            }
        } else {
            its_info->set_endpoint(its_empty_endpoint, _reliable);
        }
    }
}

void routing_manager_impl::init_routing_info() {
    VSOMEIP_INFO<< "Service Discovery disabled. Using static routing information.";
    for (auto i : configuration_->get_remote_services()) {
        std::string its_address = configuration_->get_unicast(i.first, i.second);
        uint16_t its_reliable = configuration_->get_reliable_port(i.first,
                i.second);
        uint16_t its_unreliable = configuration_->get_unreliable_port(i.first,
                i.second);

        if (VSOMEIP_INVALID_PORT != its_reliable) {
            VSOMEIP_DEBUG << "Initializing route to service ["
                    << std::hex << i.first << "." << i.second << "]";
            add_routing_info(i.first, i.second, DEFAULT_MAJOR, DEFAULT_MINOR,
                    DEFAULT_TTL,
                    boost::asio::ip::address::from_string(its_address),
                    its_reliable, true);
        }

        if (VSOMEIP_INVALID_PORT != its_unreliable) {
            add_routing_info(i.first, i.second, DEFAULT_MAJOR, DEFAULT_MINOR,
                    DEFAULT_TTL,
                    boost::asio::ip::address::from_string(its_address),
                    its_unreliable, false);
        }
    }
}

void routing_manager_impl::init_event_routing_info() {
    // TODO: the following should be done before(!) initializing
    // the configuration object to allow the configuration to
    // directly write to the target structure!
    std::map<service_t, std::map<instance_t, std::set<event_t> > > its_events =
            configuration_->get_events();
    for (auto i : its_events) {
        for (auto j : i.second) {
            for (auto k : j.second) {
                std::shared_ptr<event> its_event(std::make_shared<event>(this));
                its_event->set_service(i.first);
                its_event->set_instance(j.first);
                its_event->set_event(k);
                configuration_->set_event(its_event); // sets is_field/is_reliable
                events_[i.first][j.first][k] = its_event;
            }
        }
    }

    std::map<service_t,
            std::map<instance_t, std::map<eventgroup_t, std::set<event_t> > > > its_eventgroups =
            configuration_->get_eventgroups();
    for (auto i : its_eventgroups) {
        for (auto j : i.second) {
            for (auto k : j.second) {
                eventgroups_[i.first][j.first][k.first] = std::make_shared<
                        eventgroupinfo>();
                for (auto l : k.second) {
                    std::shared_ptr<event> its_event = find_event(i.first,
                            j.first, l);
                    if (its_event) {
                        eventgroups_[i.first][j.first][k.first]->add_event(
                                its_event);
                        its_event->add_eventgroup(k.first);
                    }
                }
            }
        }
    }

#if 0
    for (auto i : eventgroups_) {
        for (auto j : i.second) {
            for (auto k : j.second) {
                VSOMEIP_DEBUG<< "Eventgroup [" << std::hex << std::setw(4)
                << std::setfill('0') << i.first << "." << j.first << "." << k.first
                << "]";
                for (auto l : k.second->get_events()) {
                    VSOMEIP_DEBUG << "  Event " << std::hex << std::setw(4)
                    << std::setfill('0') << l->get_event();
                }
            }
        }
    }
#endif
}

void routing_manager_impl::on_subscribe(
        service_t _service,    instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<endpoint_definition> _subscriber,
        std::shared_ptr<endpoint_definition> _target) {
    std::shared_ptr<eventgroupinfo> its_eventgroup
        = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        std::shared_ptr < eventgroupinfo > its_info = find_eventgroup(
                _service, _instance, _eventgroup);

        client_t client = 0;
        if (!its_info->is_multicast())  {
            if (!_target->is_reliable()) {
                uint16_t unreliable_port = configuration_->get_unreliable_port(_service, _instance);
                auto endpoint = find_or_create_server_endpoint(unreliable_port, false);
                client = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)->get_client(_target);
                _target->set_remote_port(unreliable_port);
            }
            else {
                uint16_t reliable_port = configuration_->get_reliable_port(_service, _instance);
                auto endpoint = find_or_create_server_endpoint(reliable_port, true);
                client = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(endpoint)->get_client(_target);
                _target->set_remote_port(reliable_port);
            }
        }

        if(!host_->on_subscription(_service, _instance, _eventgroup, client, true)) {
            VSOMEIP_INFO << "Subscription request for eventgroup " << _eventgroup
                    << " rejected from application handler";
            return;
        }

        VSOMEIP_DEBUG << "on_subscribe: target=" << _target->get_address().to_string()
                << ":" <<_target->get_port() << ":Client=" << std::hex << client;

        send_subscribe(client, _service, _instance, _eventgroup, 0, 0);

        remote_subscriber_map_[client] = _target;

        its_eventgroup->add_target(_target); // unicast or multicast
        for (auto its_event : its_eventgroup->get_events()) {
            if (its_event->is_field()) {
                its_event->notify_one(_subscriber); // unicast
            }
        }
    } else {
        VSOMEIP_ERROR<< "subscribe: attempt to subscribe to unknown eventgroup!";
    }
}

void routing_manager_impl::on_unsubscribe(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<endpoint_definition> _target) {
    std::shared_ptr<eventgroupinfo> its_eventgroup = find_eventgroup(_service,
            _instance, _eventgroup);
    if (its_eventgroup) {
        std::shared_ptr < eventgroupinfo > its_info = find_eventgroup(
                _service, _instance, _eventgroup);

        client_t client = 0;
        if (!its_info->is_multicast())  {
            if (!_target->is_reliable()) {
                uint16_t unreliable_port = _target->get_remote_port();
                auto endpoint = find_or_create_server_endpoint(unreliable_port, false);
                client = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)->
                        get_client(_target);
            } else {
                uint16_t reliable_port = _target->get_remote_port();
                auto endpoint = find_or_create_server_endpoint(reliable_port, true);
                client = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(endpoint)->
                                get_client(_target);
            }
        }

        VSOMEIP_DEBUG << "on_unsubscribe: target=" << _target->get_address().to_string()
                << ":" <<_target->get_port() << ":Client=" << std::hex << client;

        its_eventgroup->del_target(_target);

        if (remote_subscriber_map_.find(client) != remote_subscriber_map_.end()) {
            remote_subscriber_map_.erase(client);
        }
        host_->on_subscription(_service, _instance, _eventgroup, client, false);

    } else {
        VSOMEIP_ERROR<<"unsubscribe: attempt to subscribe to unknown eventgroup!";
    }
}

void routing_manager_impl::on_subscribe_ack(service_t _service,
        instance_t _instance, const boost::asio::ip::address &_address,
        uint16_t _port) {
    // TODO: find a way of getting rid of the following endpoint in
    // case it is used for multicast exclusively and the subscription
    // is withdrawn or the service got lost
    std::shared_ptr<endpoint> its_endpoint = find_or_create_server_endpoint(
            _port, false);
    if (its_endpoint) {
        service_instances_[_service][its_endpoint.get()] = _instance;
        its_endpoint->join(_address.to_string());
    } else {
        VSOMEIP_ERROR<<"Could not find/create multicast endpoint!";
    }
}

void routing_manager_impl::send_subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        major_version_t _major, ttl_t _ttl) {

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
    std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 7], &_ttl,
            sizeof(_ttl));

    std::shared_ptr<vsomeip::endpoint> target = find_local(_service, _instance);
    if (target) {
        target->send(its_command, sizeof(its_command));
    }
}

void routing_manager_impl::send_unsubscribe(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) {

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

    std::shared_ptr<vsomeip::endpoint> target = find_local(_service, _instance);
    if (target) {
        target->send(its_command, sizeof(its_command));
    }
}

}
 // namespace vsomeip

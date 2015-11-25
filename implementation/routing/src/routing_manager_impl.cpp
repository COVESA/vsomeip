// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <memory>
#include <sstream>

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

namespace vsomeip {

routing_manager_impl::routing_manager_impl(routing_manager_host *_host) :
        host_(_host),
        io_(_host->get_io()),
        deserializer_(std::make_shared<deserializer>()),
        serializer_(std::make_shared<serializer>()),
        configuration_(host_->get_configuration()) {
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
    serializer_->create_data(configuration_->get_max_message_size_local());

    // TODO: Only instantiate the stub if needed
    stub_ = std::make_shared<routing_manager_stub>(this, configuration_);
    stub_->init();

    // We need to be able to send messages to ourself (for delivering events)
    (void)create_local(VSOMEIP_ROUTING_CLIENT);

    if (configuration_->is_sd_enabled()) {
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
    } else {
        init_routing_info(); // Static routing
    }
}

void routing_manager_impl::start() {
    stub_->start();
    if (discovery_)
        discovery_->start();

    host_->on_state(state_type_e::ST_REGISTERED);
}

void routing_manager_impl::stop() {
    host_->on_state(state_type_e::ST_DEREGISTERED);

    if (discovery_)
        discovery_->stop();
    stub_->stop();
}

void routing_manager_impl::offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    std::shared_ptr<serviceinfo> its_info;
    {
        std::lock_guard<std::mutex> its_lock(local_mutex_);
        local_services_[_service][_instance] = _client;

        // Remote route (incoming only)
        its_info = find_service(_service, _instance);
        if (its_info) {
            if (its_info->get_major() == _major
                    && its_info->get_minor() == _minor) {
                its_info->set_ttl(DEFAULT_TTL);
            } else {
                host_->on_error(error_code_e::SERVICE_PROPERTY_MISMATCH);
            }
        } else {
            its_info = create_service_info(_service, _instance, _major, _minor,
                    DEFAULT_TTL, true);
        }
    }

    if (discovery_ && its_info) {
        discovery_->on_offer_change();
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
        bool _use_exclusive_proxy) {

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (its_info) {
        if ((_major == its_info->get_major()
                || DEFAULT_MAJOR == its_info->get_major())
                && (_minor <= its_info->get_minor()
                        || DEFAULT_MINOR == its_info->get_minor())) {
            its_info->add_client(_client);
        } else {
            host_->on_error(error_code_e::SERVICE_PROPERTY_MISMATCH);
        }
    } else {
        if (discovery_)
            discovery_->request_service(_service, _instance, _major, _minor,
                    DEFAULT_TTL);
    }

    // TODO: Mutex?!
    if (_use_exclusive_proxy) {
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
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major) {
    if (discovery_) {
        if (!host_->on_subscription(_service, _instance, _eventgroup, _client, true)) {
            VSOMEIP_INFO << "Subscription request for eventgroup " << _eventgroup
                         << " rejected from application handler";
            return;
        }

        if (insert_subscription(_service, _instance, _eventgroup, _client)) {
            if (0 == find_local_client(_service, _instance)) {
                client_t subscriber = VSOMEIP_ROUTING_CLIENT;
                // subscriber != VSOMEIP_ROUTING_CLIENT implies to use its own endpoint
                auto its_selective = specific_endpoint_clients.find(_client);
                if (its_selective != specific_endpoint_clients.end()) {
                    subscriber = _client;
                }
                discovery_->subscribe(_service, _instance, _eventgroup,
                                      _major, DEFAULT_TTL, subscriber);
            } else {
                send_subscribe(_client, _service, _instance, _eventgroup, _major);

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
            client_t subscriber = VSOMEIP_ROUTING_CLIENT;
            // subscriber != VSOMEIP_ROUTING_CLIENT implies to use its own endpoint
            auto its_selective = specific_endpoint_clients.find(_client);
            if (its_selective != specific_endpoint_clients.end()) {
                subscriber = _client;
            }
            discovery_->unsubscribe(_service, _instance, _eventgroup, subscriber);
        } else {
            send_unsubscribe(_client, _service, _instance, _eventgroup);
        }
        clear_multicast_endpoints(_service, _instance);
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
    } else {
        VSOMEIP_ERROR << "Failed to serialize message. Check message size!";
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

    return _target->send(&its_command[0], uint32_t(its_command.size()), _flush);
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
    std::shared_ptr<endpoint> its_endpoint = find_server_endpoint(
            _target->get_remote_port(), _target->is_reliable());

    return (its_endpoint && its_endpoint->send_to(_target, _data, _size));
}

void routing_manager_impl::register_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, std::set<eventgroup_t> _eventgroups,
        bool _is_field, bool _is_provided) {
    (void)_client;

    std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
    if (its_event) {
        if (its_event->is_field() == _is_field) {
            if (_is_provided) {
                its_event->set_provided(true);
            }
            for (auto eg : _eventgroups) {
                its_event->add_eventgroup(eg);
            }
        } else {
            VSOMEIP_ERROR << "Event registration update failed. "
                    "Specified arguments do not match existing registration.";
        }
    } else {
        its_event = std::make_shared<event>(this);
        its_event->set_service(_service);
        its_event->set_instance(_instance);
        its_event->set_event(_event);
        its_event->set_field(_is_field);
        its_event->set_provided(_is_provided);

        if (_eventgroups.size() == 0) { // No eventgroup specified
            _eventgroups.insert(_event);
        }

        its_event->set_eventgroups(_eventgroups);
    }

    its_event->add_ref();

    for (auto eg : _eventgroups) {
        std::shared_ptr<eventgroupinfo> its_eventgroup_info
            = find_eventgroup(_service, _instance, eg);
        if (!its_eventgroup_info) {
            its_eventgroup_info = std::make_shared<eventgroupinfo>();
            eventgroups_[_service][_instance][eg] = its_eventgroup_info;
        }
        its_eventgroup_info->add_event(its_event);
    }

    events_[_service][_instance][_event] = its_event;
}

void routing_manager_impl::unregister_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, bool _is_provided) {
    (void)_client;

    auto found_service = events_.find(_service);
    if (found_service != events_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_event = found_instance->second.find(_event);
            if (found_event != found_instance->second.end()) {
                auto its_event = found_event->second;
                if (!its_event->remove_ref()) {
                    auto its_eventgroups = its_event->get_eventgroups();
                    for (auto eg : its_eventgroups) {
                        std::shared_ptr<eventgroupinfo> its_eventgroup_info
                            = find_eventgroup(_service, _instance, eg);
                        if (its_eventgroup_info) {
                            its_eventgroup_info->remove_event(its_event);
                            if (0 == its_eventgroup_info->get_events().size()) {
                                remove_eventgroup_info(_service, _instance, eg);
                            }
                        }
                    }
                    found_instance->second.erase(_event);
                } else if (_is_provided) {
                    its_event->set_provided(false);
                }
            }
        }
    }
}

void routing_manager_impl::notify(
        service_t _service, instance_t _instance, event_t _event,
        std::shared_ptr<payload> _payload) {
    std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
    if (its_event) {
        its_event->set_payload(_payload);
    } else {
        VSOMEIP_WARNING << "Attempt to update the undefined event/field ["
            << std::hex << _service << "." << _instance << "." << _event
            << "]";
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
        VSOMEIP_WARNING << "Attempt to update the undefined event/field ["
            << std::hex << _service << "." << _instance << "." << _event
            << "]";
    }
}

void routing_manager_impl::on_error(const byte_t *_data, length_t _length, endpoint *_receiver) {
    instance_t its_instance = 0;
    if (_length >= VSOMEIP_SERVICE_POS_MAX) {
        service_t its_service = VSOMEIP_BYTES_TO_WORD(
                _data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        its_instance = find_instance(its_service, _receiver);
    }
    send_error(return_code_e::E_MALFORMED_MESSAGE, _data, _length,
            its_instance, _receiver->is_reliable(), _receiver);
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
    if (_size >= VSOMEIP_SOMEIP_HEADER_SIZE) {
        its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
                _data[VSOMEIP_SERVICE_POS_MAX]);
        if (its_service == VSOMEIP_SD_SERVICE) {
            if (discovery_)
                discovery_->on_message(_data, _size);
        } else {
            instance_t its_instance = find_instance(its_service, _receiver);
            return_code_e return_code = check_error(_data, _size, its_instance);
            if (return_code != return_code_e::E_OK) {
                if (return_code != return_code_e::E_NOT_OK) {
                    send_error(return_code, _data, _size, its_instance,
                            _receiver->is_reliable(), _receiver);
                }
                return;
            }

            if (!deliver_specific_endpoint_message(
                    its_service, its_instance, _data, _size, _receiver)) {
                // Common way of message handling
                on_message(its_service, its_instance, _data, _size, _receiver->is_reliable());
            }
        }
    }
}

void routing_manager_impl::on_message(
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _size,
        bool _reliable) {
#if 0
    std::stringstream msg;
    msg << "rmi::on_message("
            << std::hex << std::setw(4) << std::setfill('0')
            << _service << ", " << _instance << "): ";
    for (uint32_t i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
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
            std::shared_ptr<payload> its_payload =
                    runtime::get()->create_payload(
                            &_data[VSOMEIP_PAYLOAD_POS],
                            its_length);
            its_event->set_payload(its_payload);

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

    if (its_client == VSOMEIP_ROUTING_CLIENT
            && utility::is_notification(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        deliver_notification(_service, _instance, _data, _size, _reliable);
    } else if (its_client == host_->get_client()) {
        deliver_message(_data, _size, _instance, _reliable);
    } else {
        send(its_client, _data, _size, _instance, true, _reliable);
    }
}

void routing_manager_impl::on_notification(client_t _client,
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _size) {
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

        if (_client == VSOMEIP_ROUTING_CLIENT)
            its_event->set_payload(its_payload);
        else
            its_event->set_payload(its_payload, _client);
    }
}

void routing_manager_impl::on_connect(std::shared_ptr<endpoint> _endpoint) {
    // Is called when endpoint->connect succeded!
    for (auto &its_service : remote_services_) {
        for (auto &its_instance : its_service.second) {
            for (auto &its_client : its_instance.second) {
                if (its_client.first == VSOMEIP_ROUTING_CLIENT ||
                        its_client.first == get_client()) {
                    auto found_endpoint = its_client.second.find(false);
                    if (found_endpoint != its_client.second.end()) {
                        if (found_endpoint->second.get() == _endpoint.get()) {
                            host_->on_availability(its_service.first, its_instance.first,
                                    true);
                        }
                    }
                    found_endpoint = its_client.second.find(true);
                    if (found_endpoint != its_client.second.end()) {
                        if (found_endpoint->second.get() == _endpoint.get()) {
                            host_->on_availability(its_service.first,
                                    its_instance.first, true);
                        }
                    }
                }
            }
        }
    }
}

void routing_manager_impl::on_disconnect(std::shared_ptr<endpoint> _endpoint) {
    // Is called when endpoint->connect fails!
    for (auto &its_service : remote_services_) {
        for (auto &its_instance : its_service.second) {
            for (auto &its_client : its_instance.second) {
                if (its_client.first == VSOMEIP_ROUTING_CLIENT ||
                        its_client.first == get_client()) {
                    auto found_endpoint = its_client.second.find(false);
                    if (found_endpoint != its_client.second.end()) {
                        if (found_endpoint->second.get() == _endpoint.get()) {
                            host_->on_availability(its_service.first, its_instance.first,
                                    false);
                        }
                    }
                    found_endpoint = its_client.second.find(true);
                    if (found_endpoint != its_client.second.end()) {
                        if (found_endpoint->second.get() == _endpoint.get()) {
                            host_->on_availability(its_service.first,
                                    its_instance.first, false);
                        }
                    }
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
        auto found_service = services_.find(_service);
        if (found_service != services_.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                found_instance->second->set_ttl(0);
                discovery_->on_offer_change();
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
        instance_t _instance, bool _reliable) {
    bool is_delivered(false);
    deserializer_->set_data(_data, _size);
    std::shared_ptr<message> its_message(deserializer_->deserialize_message());
    if (its_message) {
        its_message->set_instance(_instance);
        its_message->set_reliable(_reliable);
        host_->on_message(its_message);
        is_delivered = true;
    } else {
        VSOMEIP_ERROR << "Deserialization of vSomeIP message failed";
        if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
            send_error(return_code_e::E_MALFORMED_MESSAGE, _data,
                    _size, _instance, _reliable, nullptr);
        }
    }
    return is_delivered;
}

bool routing_manager_impl::deliver_notification(
        service_t _service, instance_t _instance,
        const byte_t *_data, length_t _length,
        bool _reliable) {
    bool is_delivered(false);

    method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
            _data[VSOMEIP_METHOD_POS_MAX]);

    std::shared_ptr<event> its_event = find_event(_service, _instance, its_method);
    if (its_event) {
        std::vector< byte_t > its_data;

        for (auto its_group : its_event->get_eventgroups()) {
            auto its_local_clients = find_local_clients(_service, _instance, its_group);
            for (auto its_local_client : its_local_clients) {
                if (its_local_client == host_->get_client()) {
                    deliver_message(_data, _length, _instance, _reliable);
                } else {
                    std::shared_ptr<endpoint> its_local_target = find_local(its_local_client);
                    if (its_local_target) {
                        send_local(its_local_target, VSOMEIP_ROUTING_CLIENT,
                                _data, _length, _instance, true, _reliable);
                    }
                }
            }
        }
    }

    return is_delivered;
}

std::shared_ptr<eventgroupinfo> routing_manager_impl::find_eventgroup(
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) const {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);

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

void routing_manager_impl::remove_eventgroup_info(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    auto found_service = eventgroups_.find(_service);
    if (found_service != eventgroups_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            found_instance->second.erase(_eventgroup);
        }
    }
}

std::shared_ptr<configuration> routing_manager_impl::get_configuration() const {
    return (host_->get_configuration());
}

std::shared_ptr<endpoint> routing_manager_impl::create_service_discovery_endpoint(
        const std::string &_address, uint16_t _port, bool _reliable) {
    std::shared_ptr<endpoint> its_service_endpoint = find_server_endpoint(_port,
            _reliable);
    if (!its_service_endpoint) {
        its_service_endpoint = create_server_endpoint(_port, _reliable, true);

        if (its_service_endpoint) {
            std::shared_ptr<serviceinfo> its_info(
                    std::make_shared<serviceinfo>(ANY_MAJOR, ANY_MINOR, DEFAULT_TTL,
                            false)); // false, because we do _not_ want to announce it...
            its_info->set_endpoint(its_service_endpoint, _reliable);

            // routing info
            services_[VSOMEIP_SD_SERVICE][VSOMEIP_SD_INSTANCE] = its_info;

            its_service_endpoint->add_multicast(VSOMEIP_SD_SERVICE,
                    VSOMEIP_SD_METHOD, _address, _port);
            its_service_endpoint->join(_address);
        } else {
            VSOMEIP_ERROR << "Service Discovery endpoint could not be created. "
                    "Please check your network configuration.";
        }
    }
    return its_service_endpoint;
}

services_t routing_manager_impl::get_offered_services() const {
    services_t its_offers;
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    for (auto s : services_) {
        for (auto i : s.second) {
            if (i.second->is_local()) {
                its_offers[s.first][i.first] = i.second;
            }
        }
    }
    return (its_offers);
}

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_remote_client(
        service_t _service, instance_t _instance, bool _reliable, client_t _client) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    std::shared_ptr<endpoint> its_endpoint = find_remote_client(_service, _instance,
            _reliable, _client);
    if (!its_endpoint) {
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
    std::lock_guard<std::mutex> its_lock(services_mutex_);
    auto found_service = services_.find(_service);
    if (found_service != services_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            its_info = found_instance->second;
        }
    }
    return (its_info);
}

std::shared_ptr<serviceinfo> routing_manager_impl::create_service_info(
        service_t _service, instance_t _instance, major_version_t _major,
        minor_version_t _minor, ttl_t _ttl, bool _is_local_service) {
    std::shared_ptr<serviceinfo> its_info;
    if (configuration_) {
        its_info = std::make_shared<serviceinfo>(_major, _minor, _ttl, _is_local_service);

        uint16_t its_reliable_port = configuration_->get_reliable_port(_service,
                _instance);
        uint16_t its_unreliable_port = configuration_->get_unreliable_port(
                _service, _instance);

        bool is_someip = configuration_->is_someip(_service, _instance);

        its_info->set_multicast_address(
                configuration_->get_multicast_address(_service, _instance));
        its_info->set_multicast_port(
                configuration_->get_multicast_port(_service, _instance));
        its_info->set_multicast_group(
                configuration_->get_multicast_group(_service, _instance));

        std::shared_ptr<endpoint> its_reliable_endpoint;
        std::shared_ptr<endpoint> its_unreliable_endpoint;

        // Create server endpoints for local services only
        if (_is_local_service) {
            if (ILLEGAL_PORT != its_reliable_port) {
                its_reliable_endpoint = find_or_create_server_endpoint(
                        its_reliable_port, true, is_someip);
                if (its_reliable_endpoint) {
                    its_info->set_endpoint(its_reliable_endpoint, true);
                    its_reliable_endpoint->increment_use_count();
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
                    service_instances_[_service][its_unreliable_endpoint.get()] =
                            _instance;
                }
            }

            if (ILLEGAL_PORT == its_reliable_port
                   && ILLEGAL_PORT == its_unreliable_port) {
                   VSOMEIP_DEBUG << "Port configuration missing for ["
                           << std::hex << _service << "." << _instance
                           << "]. Service is internal.";
            }
        }

        {
            std::lock_guard<std::mutex> its_lock(services_mutex_);
            services_[_service][_instance] = its_info;
        }
    } else {
        host_->on_error(error_code_e::CONFIGURATION_MISSING);
    }

    return (its_info);
}

std::shared_ptr<endpoint> routing_manager_impl::create_client_endpoint(
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _reliable, client_t _client, bool _start) {
    (void)_client;

    std::shared_ptr<endpoint> its_endpoint;
    try {
        if (_reliable) {
            its_endpoint = std::make_shared<tcp_client_endpoint_impl>(
                    shared_from_this(),
                    boost::asio::ip::tcp::endpoint(_address, _port), io_,
                    configuration_->get_message_size_reliable(
                            _address.to_string(), _port));

            if (configuration_->has_enabled_magic_cookies(_address.to_string(),
                    _port)) {
                its_endpoint->enable_magic_cookies();
            }
        } else {
            its_endpoint = std::make_shared<udp_client_endpoint_impl>(
                    shared_from_this(),
                    boost::asio::ip::udp::endpoint(_address, _port), io_);
        }
        if (_start)
            its_endpoint->start();
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
                        configuration_->get_message_size_reliable(
                                its_unicast.to_string(), _port));
                if (configuration_->has_enabled_magic_cookies(
                        its_unicast.to_string(), _port)) {
                    its_endpoint->enable_magic_cookies();
                }
            } else {
                if (its_unicast.is_v4()) {
                    its_unicast = boost::asio::ip::address_v4::any();
                } else if (its_unicast.is_v6()) {
                    its_unicast = boost::asio::ip::address_v6::any();
                }
                boost::asio::ip::udp::endpoint ep(its_unicast, _port);
                its_endpoint = std::make_shared<udp_server_endpoint_impl>(
                        shared_from_this(),
                        ep, io_);
            }

        } else {
            its_endpoint = std::make_shared<virtual_server_endpoint_impl>(
                                its_unicast.to_string(), _port, _reliable);
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
    if (0 == its_endpoint) {
        its_endpoint = create_server_endpoint(_port, _reliable, _start);
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
    int port = VSOMEIP_INTERNAL_BASE_PORT + _client;
#endif

    std::shared_ptr<endpoint> its_endpoint = std::make_shared<
        local_client_endpoint_impl>(shared_from_this(),
#ifdef WIN32
        boost::asio::ip::tcp::endpoint(address, port)
#else
        boost::asio::local::stream_protocol::endpoint(its_path.str())
#endif
    , io_, configuration_->get_max_message_size_local());
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
    std::shared_ptr<endpoint_definition> its_endpoint_def;
    auto found_service = remote_service_info_.find(_service);
    if (found_service != remote_service_info_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_reliability = found_instance->second.find(_reliable);
            if (found_reliability != found_instance->second.end()) {
                its_endpoint_def = found_reliability->second;
                its_endpoint = create_client_endpoint(
                        its_endpoint_def->get_address(),
                        its_endpoint_def->get_port(), _reliable, _client,
                        configuration_->is_someip(_service, _instance)
                );
            }
        }
    }
    if (its_endpoint) {
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
    if(its_endpoint || _client != VSOMEIP_ROUTING_CLIENT) {
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


bool routing_manager_impl::is_field(service_t _service, instance_t _instance,
        event_t _event) const {
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
        {
            std::lock_guard<std::mutex> its_lock(services_mutex_);
            services_[_service][_instance] = its_info;
        }
    } else {
        its_info->set_ttl(_ttl);
    }

    // Check whether remote services are unchanged
    bool is_reliable_known(false);
    bool is_unreliable_known(false);

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
                    }
                } else {
                    VSOMEIP_WARNING << "Reliable service endpoint has changed!";
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
                        VSOMEIP_WARNING << "Unreliable service endpoint has changed!";
                    }
                }
            }
        }
    }

    // Add endpoint(s) if necessary
    bool is_added(false);
    if (_reliable_port != ILLEGAL_PORT && !is_reliable_known) {
        std::shared_ptr<endpoint_definition> endpoint_def
            = endpoint_definition::get(_reliable_address, _reliable_port, true);
        remote_service_info_[_service][_instance][true] = endpoint_def;
        is_added = !is_unreliable_known;
    }

    if (_unreliable_port != ILLEGAL_PORT && !is_unreliable_known) {
        std::shared_ptr<endpoint_definition> endpoint_def
            = endpoint_definition::get(_unreliable_address, _unreliable_port, false);
        remote_service_info_[_service][_instance][false] = endpoint_def;
        is_added = !is_reliable_known;
    }

    if (is_added) {
        host_->on_availability(_service, _instance, true);
        stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance);
    }
}

void routing_manager_impl::del_routing_info(service_t _service, instance_t _instance,
        bool _has_reliable, bool _has_unreliable) {

    host_->on_availability(_service, _instance, false);
    stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance);

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

    if (_has_reliable)
        clear_client_endpoints(_service, _instance, true);
    if (_has_unreliable)
        clear_client_endpoints(_service, _instance, false);

    clear_multicast_endpoints(_service, _instance);

    if (_has_reliable)
        clear_service_info(_service, _instance, true);
    if (_has_unreliable)
        clear_service_info(_service, _instance, false);
}

ttl_t routing_manager_impl::update_routing_info(ttl_t _elapsed) {
    ttl_t its_smallest_ttl(DEFAULT_TTL);
    std::map<service_t,
        std::map<instance_t,
            std::pair<bool, bool> > > its_expired_offers;

    for (auto &s : services_) {
        for (auto &i : s.second) {
            ttl_t its_ttl = i.second->get_ttl();
            if (its_ttl < DEFAULT_TTL) { // do not touch "forever"
                if (its_ttl < _elapsed || its_ttl == 0) {
                    i.second->set_ttl(0);
                    if (discovery_)
                        discovery_->unsubscribe_all(s.first, i.first);
                    its_expired_offers[s.first][i.first] = {
                            i.second->get_endpoint(true) != nullptr,
                            i.second->get_endpoint(false) != nullptr
                    };
                } else {
                    ttl_t its_new_ttl(its_ttl - _elapsed);
                    i.second->set_ttl(its_new_ttl);
                    if (its_smallest_ttl > its_new_ttl)
                        its_smallest_ttl = its_new_ttl;
                }
            }
        }
    }

    for (auto &s : its_expired_offers) {
        for (auto &i : s.second) {
            del_routing_info(s.first, i.first, i.second.first, i.second.second);
        }
    }

    return its_smallest_ttl;
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

void routing_manager_impl::on_subscribe(
        service_t _service,    instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<endpoint_definition> _subscriber,
        std::shared_ptr<endpoint_definition> _target) {
    std::shared_ptr<eventgroupinfo> its_eventgroup
        = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        client_t client = 0;
        if (!its_eventgroup->is_multicast())  {
            if (!_target->is_reliable()) {
                uint16_t unreliable_port = configuration_->get_unreliable_port(_service, _instance);
                _target->set_remote_port(unreliable_port);
                auto endpoint = find_server_endpoint(unreliable_port, false);
                if (endpoint) {
                    client = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)->
                            get_client(_target);
                }
            }
            else {
                uint16_t reliable_port = configuration_->get_reliable_port(_service, _instance);
                auto endpoint = find_server_endpoint(reliable_port, true);
                _target->set_remote_port(reliable_port);
                if (endpoint) {
                    client = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(endpoint)->
                            get_client(_target);
                }
            }
        }

        if(!host_->on_subscription(_service, _instance, _eventgroup, client, true)) {
            VSOMEIP_INFO << "Subscription request for eventgroup " << _eventgroup
                         << " rejected from application handler";
            return;
        }

        VSOMEIP_DEBUG << "on_subscribe: target=" << _target->get_address().to_string()
                << ":" << std::dec <<_target->get_port() << ":Client=" << std::hex
                << client << (_target->is_reliable() ? " reliable" : " unreliable");

        send_subscribe(client, _service, _instance, _eventgroup, its_eventgroup->get_major());

        remote_subscriber_map_[client] = _target;

        if (its_eventgroup->add_target(_target)) { // unicast or multicast
            for (auto its_event : its_eventgroup->get_events()) {
                if (its_event->is_field()) {
                    its_event->notify_one(_subscriber); // unicast
                }
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
        client_t client = 0;
        if (!its_eventgroup->is_multicast())  {
            if (!_target->is_reliable()) {
                uint16_t unreliable_port = configuration_->get_unreliable_port(_service, _instance);
                auto endpoint = find_server_endpoint(unreliable_port, false);
                if (endpoint) {
                    client = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)->
                            get_client(_target);
                }
            } else {
                uint16_t reliable_port = configuration_->get_reliable_port(_service, _instance);
                auto endpoint = find_server_endpoint(reliable_port, true);
                if (endpoint) {
                    client = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(endpoint)->
                                    get_client(_target);
                }
            }
        }

        VSOMEIP_DEBUG << "on_unsubscribe: target=" << _target->get_address().to_string()
                      << ":" << std::dec <<_target->get_port() << ":Client=" << std::hex
                      << client << (_target->is_reliable() ? " reliable" : " unreliable");;

        its_eventgroup->remove_target(_target);

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

    if (multicast_info.find(_service) != multicast_info.end()) {
        if (multicast_info[_service].find(_instance) != multicast_info[_service].end()) {
            auto endpoint_def = multicast_info[_service][_instance];
            if (endpoint_def->get_address() == _address &&
                    endpoint_def->get_port() == _port) {

                // Multicast info and endpoint already created before
                // This can happen when more than one client subscribe on the same instance!
                return;
            }
        }
    }

    // Save multicast info to be able to delete the endpoint
    // as soon as the instance stops offering its service
    std::shared_ptr<endpoint_definition> endpoint_def =
            endpoint_definition::get(_address, _port, false);
    multicast_info[_service][_instance] = endpoint_def;

    bool is_someip = configuration_->is_someip(_service, _instance);

    // Create multicast endpoint & join multicase group
    std::shared_ptr<endpoint> its_endpoint
        = find_or_create_server_endpoint(_port, false, is_someip);
    if (its_endpoint) {
        service_instances_[_service][its_endpoint.get()] = _instance;
        its_endpoint->join(_address.to_string());
    } else {
        VSOMEIP_ERROR<<"Could not find/create multicast endpoint!";
    }
}

void routing_manager_impl::send_subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        major_version_t _major) {

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

bool routing_manager_impl::insert_subscription(
        service_t _service, instance_t _instance, eventgroup_t _eventgroup,
        client_t _client) {

    auto found_service = eventgroup_clients_.find(_service);
    if (found_service != eventgroup_clients_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            auto found_eventgroup = found_instance->second.find(_eventgroup);
            if (found_eventgroup != found_instance->second.end()) {
                if (found_eventgroup->second.find(_client)
                        != found_eventgroup->second.end())
                    return false;
            }
        }
    }

    eventgroup_clients_[_service][_instance][_eventgroup].insert(_client);
    return true;
}


bool routing_manager_impl::deliver_specific_endpoint_message(service_t _service,
        instance_t _instance, const byte_t *_data, length_t _size, endpoint *_receiver) {
    // Try to deliver specific endpoint message (for selective subscribers)
    auto found_servic = remote_services_.find(_service);
    if (found_servic != remote_services_.end()) {
        auto found_instance = found_servic->second.find(_instance);
        if (found_instance != found_servic->second.end()) {
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
                            send_local(local_endpoint, client, _data, _size, _instance, true, _receiver->is_reliable());
                        } else {
                            deliver_message(_data, _size, _instance, _receiver->is_reliable());
                        }
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void routing_manager_impl::clear_client_endpoints(service_t _service, instance_t _instance,
        bool _reliable) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    std::shared_ptr<endpoint> deleted_endpoint;
    // Clear client endpoints for remote services (generic and specific ones)
    if (remote_services_.find(_service) != remote_services_.end()) {
        if (remote_services_[_service].find(_instance) != remote_services_[_service].end()) {
            auto endpoint = remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT][_reliable];
            if (endpoint) {
                service_instances_[_service].erase(endpoint.get());
                deleted_endpoint = endpoint;
            }
            remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT].erase(_reliable);
            auto found_endpoint = remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT].find(
                    !_reliable);
            if (found_endpoint == remote_services_[_service][_instance][VSOMEIP_ROUTING_CLIENT].end()) {
                remote_services_[_service][_instance].erase(VSOMEIP_ROUTING_CLIENT);
            }
        }
    }
    for (client_t client : specific_endpoint_clients) {
        if (remote_services_.find(_service) != remote_services_.end()) {
            if (remote_services_[_service].find(_instance) != remote_services_[_service].end()) {
                auto endpoint = remote_services_[_service][_instance][client][_reliable];
                if (endpoint) {
                    service_instances_[_service].erase(endpoint.get());
                    endpoint->stop();
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
    // Clear remote_service_info_
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

    if (1 >= service_instances_[_service].size()) {
        service_instances_.erase(_service);
    }
    if(deleted_endpoint) {
        stop_and_delete_client_endpoint(deleted_endpoint);
    }
}

void routing_manager_impl::stop_and_delete_client_endpoint(
        std::shared_ptr<endpoint> _endpoint) {
    // Only stop and delete the endpoint if none of the services
    // reachable through it is online anymore.
    bool delete_endpoint(true);

    for (const auto& service : remote_services_) {
        for (const auto& instance : service.second) {
            const auto& client = instance.second.find(VSOMEIP_ROUTING_CLIENT);
            if(client != instance.second.end()) {
                for (const auto& reliable : client->second) {
                    if(reliable.second == _endpoint) {
                        delete_endpoint = false;
                        break;
                    }
                }
            }
            if(!delete_endpoint) { break; }
        }
        if(!delete_endpoint) { break; }
    }

    if(delete_endpoint) {
        _endpoint->stop();
        for (auto address = client_endpoints_by_ip_.begin();
                address != client_endpoints_by_ip_.end();) {
            for (auto port = address->second.begin();
                    port != address->second.end();) {
                for (auto reliable = port->second.begin();
                        reliable != port->second.end();) {
                    std::shared_ptr<endpoint> its_endpoint = reliable->second;
                    if (_endpoint == its_endpoint) {
                        reliable = port->second.erase(reliable);
                    } else {
                        ++reliable;
                    }
                }
                if (!port->second.size()) {
                    port = address->second.erase(port);
                } else {
                    ++port;
                }
            }
            if(!address->second.size()) {
                address = client_endpoints_by_ip_.erase(address);
            } else {
                ++address;
            }
        }
    }
}

void routing_manager_impl::clear_multicast_endpoints(service_t _service, instance_t _instance) {
    std::lock_guard<std::recursive_mutex> its_lock(endpoint_mutex_);
    // Clear multicast info and endpoint and multicast instance (remote service)
    if (multicast_info.find(_service) != multicast_info.end()) {
        if (multicast_info[_service].find(_instance) != multicast_info[_service].end()) {
            std::string address = multicast_info[_service][_instance]->get_address().to_string();
            uint16_t port = multicast_info[_service][_instance]->get_port();
            auto multicast_endpoint = server_endpoints_[port][false];
            multicast_endpoint->leave(address);
            multicast_endpoint->stop();
            server_endpoints_[port].erase(false);
            if (server_endpoints_[port].find(true) == server_endpoints_[port].end()) {
                server_endpoints_.erase(port);
            }
            multicast_info[_service].erase(_instance);
            if (0 >= multicast_info[_service].size()) {
                multicast_info.erase(_service);
            }
            // Clear service_instances_ for multicase endpoint
            if (1 >= service_instances_[_service].size()) {
                service_instances_.erase(_service);
            } else {
                service_instances_[_service].erase(multicast_endpoint.get());
            }
        }
    }
}

void routing_manager_impl::clear_service_info(service_t _service, instance_t _instance,
        bool _reliable) {
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if (!its_info) {
        return;
    }
    // Clear service_info and service_group
    std::shared_ptr<endpoint> its_empty_endpoint;
    if (!its_info->get_endpoint(!_reliable)) {
        if (1 >= services_[_service].size()) {
            services_.erase(_service);
        } else {
            services_[_service].erase(_instance);
        }
    } else {
        its_info->set_endpoint(its_empty_endpoint, _reliable);
    }
}

return_code_e routing_manager_impl::check_error(const byte_t *_data, length_t _size,
        instance_t _instance) {

    service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
            _data[VSOMEIP_SERVICE_POS_MAX]);

    if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        if (_size >= VSOMEIP_PAYLOAD_POS) {
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
            // TODO: Check interface version handling?!
            if (_data[VSOMEIP_INTERFACE_VERSION_POS] != 0x0) {
                // Interface is currently set to zero always!
                return return_code_e::E_WRONG_INTERFACE_VERSION;
            }
            if (_data[VSOMEIP_RETURN_CODE_POS] != static_cast<byte_t> (return_code_e::E_OK)) {
                // Request calls must to have return code E_OK set!
                return return_code_e::E_NOT_OK;
            }
        } else {
            // Message shorter than vSomeIP message header
            return return_code_e::E_MALFORMED_MESSAGE;
        }
    }
    return return_code_e::E_OK;
}

void routing_manager_impl::send_error(return_code_e _return_code,
        const byte_t *_data, length_t _size,
        instance_t _instance, bool _reliable,
        endpoint *_receiver) {

    client_t its_client = 0;
    service_t its_service = 0;
    method_t its_method = 0;
    session_t its_session = 0;

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

    auto error_message = runtime::get()->create_message(_reliable);
    error_message->set_client(its_client);
    error_message->set_instance(_instance);
    error_message->set_interface_version(0);
    error_message->set_message_type(message_type_e::MT_ERROR);
    error_message->set_method(its_method);
    error_message->set_return_code(_return_code);
    error_message->set_service(its_service);
    error_message->set_session(its_session);

    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    if (serializer_->serialize(error_message.get())) {
        if (_receiver) {
            boost::asio::ip::address adr;
            uint16_t port;
            if (_receiver->is_reliable()) {
                auto remote = static_cast<tcp_server_endpoint_impl*>(_receiver)->get_remote();
                adr = remote.address();
                port = remote.port();
            } else {
                auto remote = static_cast<udp_server_endpoint_impl*>(_receiver)->get_remote();
                adr = remote.address();
                port = remote.port();
            }
            auto its_endpoint_def =
                    std::make_shared<endpoint_definition>(adr, port, _receiver->is_reliable());
            its_endpoint_def->set_remote_port(_receiver->get_local_port());
            send_to(its_endpoint_def, serializer_->get_data(), serializer_->get_size());
        } else {
            send(get_client(), serializer_->get_data(), serializer_->get_size(),
                    _instance, true, _reliable);
        }
        serializer_->reset();
    } else {
        VSOMEIP_ERROR << "Failed to serialize error message.";
    }
}

}
 // namespace vsomeip

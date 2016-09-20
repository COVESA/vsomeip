// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <climits>
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
        routing_manager_base(_host) {
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

    for (auto client: get_connected_clients()) {
        if (client != VSOMEIP_ROUTING_CLIENT) {
            remove_local(client);
        }
    }
}

void routing_manager_impl::offer_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);

    {
        std::lock_guard<std::mutex> its_lock(local_services_mutex_);
        local_services_[_service][_instance] = std::make_tuple(_major, _minor, _client);
    }

    routing_manager_base::offer_service(_client, _service, _instance, _major, _minor);
    init_service_info(_service, _instance, true);

    {
        std::lock_guard<std::mutex> its_lock(events_mutex_);
        // Set major version for all registered events of this service and instance
        auto find_service = events_.find(_service);
        if (find_service != events_.end()) {
            auto find_instance = find_service->second.find(_instance);
            if (find_instance != find_service->second.end()) {
                for (auto j : find_instance->second) {
                    j.second->set_version(_major);
                }
            }
        }
    }

    if (discovery_) {
        discovery_->on_offer_change();
    }

    stub_->on_offer_service(_client, _service, _instance, _major, _minor);
    host_->on_availability(_service, _instance, true, _major, _minor);
}

void routing_manager_impl::stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) {
    routing_manager_base::stop_offer_service(_client, _service, _instance, _major, _minor);
    on_stop_offer_service(_service, _instance, _major, _minor);
    stub_->on_stop_offer_service(_client, _service, _instance, _major, _minor);
}

void routing_manager_impl::request_service(client_t _client, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        bool _use_exclusive_proxy) {

    routing_manager_base::request_service(_client, _service, _instance, _major,
            _minor, _use_exclusive_proxy);

    auto its_info = find_service(_service, _instance);
    if (!its_info) {
        {
            std::lock_guard<std::mutex> ist_lock(requested_services_mutex_);
            requested_services_[_client][_service][_instance].insert({ _major, _minor });
        }
        // Unknown service instance ~> tell SD to find it!
        if (discovery_) {
            discovery_->request_service(_service, _instance, _major, _minor,
                    DEFAULT_TTL);
        }
    } else {
        if ((_major == its_info->get_major()
                || DEFAULT_MAJOR == its_info->get_major())
                && (_minor <= its_info->get_minor()
                        || DEFAULT_MINOR == its_info->get_minor()
                        || _minor == ANY_MINOR)) {
            if(!its_info->is_local()) {
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
}

void routing_manager_impl::release_service(client_t _client, service_t _service,
        instance_t _instance) {
    routing_manager_base::release_service(_client, _service, _instance);
    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if(its_info) {
        if(!its_info->get_requesters_size()) {
            if(discovery_) {
                discovery_->release_service(_service, _instance);
            }
        }
    } else {
        if(discovery_) {
            discovery_->release_service(_service, _instance);
        }
    }
}

void routing_manager_impl::subscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
        subscription_type_e _subscription_type) {
    if (get_client() == find_local_client(_service, _instance)) {
        if (!host_->on_subscription(_service, _instance, _eventgroup, _client, true)) {
            on_subscribe_nack(_client, _service, _instance, _eventgroup);
            VSOMEIP_INFO << "Subscription request from client: 0x" << std::hex
                         << _client << std::dec << " for eventgroup: 0x" << _eventgroup
                         << " rejected from application handler.";
            return;
        } else {
            on_subscribe_ack(_client, _service, _instance, _eventgroup);
        }
        routing_manager_base::subscribe(_client, _service, _instance, _eventgroup, _major, _subscription_type);
    } else {
        if (discovery_) {
            bool inserted = insert_subscription(_service, _instance, _eventgroup, _client);
            if (inserted) {
                if (0 == find_local_client(_service, _instance)) {
                    client_t subscriber = VSOMEIP_ROUTING_CLIENT;
                    // subscriber != VSOMEIP_ROUTING_CLIENT implies to use its own endpoint
                    {
                        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
                        auto found_service = specific_endpoint_clients_.find(_service);
                        if (found_service != specific_endpoint_clients_.end()) {
                            auto found_instance = found_service->second.find(_instance);
                            if (found_instance != found_service->second.end()) {
                                auto found_client = found_instance->second.find(_client);
                                if(found_client != found_instance->second.end()) {
                                    subscriber = _client;
                                    if (supports_selective(_service, _instance)) {
                                        identify_for_subscribe(_client, _service, _instance, _major);
                                    } else {
                                        VSOMEIP_INFO << "Subcribe to legacy selective service: " << std::hex
                                                     << _service << ":" << _instance << ".";
                                    }
                                }
                            }
                        }
                    }
                    discovery_->subscribe(_service, _instance, _eventgroup,
                                          _major, DEFAULT_TTL, subscriber, _subscription_type);
                } else {
                   stub_->send_subscribe(routing_manager_base::find_local(_service, _instance),
                           _client, _service, _instance, _eventgroup, _major, false);
                }
            }
        } else {
            VSOMEIP_ERROR<< "SOME/IP eventgroups require SD to be enabled!";
        }
    }
}

void routing_manager_impl::unsubscribe(client_t _client, service_t _service,
        instance_t _instance, eventgroup_t _eventgroup) {
    if (discovery_) {
        {
            std::lock_guard<std::mutex> its_lock(eventgroup_clients_mutex_);
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
        }
        host_->on_subscription(_service, _instance, _eventgroup, _client, false);
        if (0 == find_local_client(_service, _instance)) {
            client_t subscriber = VSOMEIP_ROUTING_CLIENT;
            // subscriber != VSOMEIP_ROUTING_CLIENT implies to use its own endpoint
            {
                std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
                auto found_service = specific_endpoint_clients_.find(_service);
                if (found_service != specific_endpoint_clients_.end()) {
                    auto found_instance = found_service->second.find(_instance);
                    if (found_instance != found_service->second.end()) {
                        auto found_client = found_instance->second.find(_client);
                        if(found_client != found_instance->second.end()) {
                            subscriber = _client;
                        }
                    }
                }
            }
            discovery_->unsubscribe(_service, _instance, _eventgroup, subscriber);
        } else {
            stub_->send_unsubscribe(routing_manager_base::find_local(_service, _instance),
                    _client, _service, _instance, _eventgroup);
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
        bool _flush, bool _reliable, bool _initial) {
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
            its_target = routing_manager_base::find_local(its_service, _instance);
        } else if (!is_notification) {
            its_target = find_local(its_client);
        } else if (is_notification && _client) {
            // Selective notifications!
            if (_client == get_client()) {
                deliver_message(_data, _size, _instance, _reliable);
                return true;
            }
            its_target = find_local(_client);
        }

        if (its_target) {
            is_sent = send_local(its_target, _client, _data, _size, _instance, _flush, _reliable, VSOMEIP_SEND, false, _initial);
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
                    {
                        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
                        auto found_service = specific_endpoint_clients_.find(its_service);
                        if (found_service != specific_endpoint_clients_.end()) {
                            auto found_instance = found_service->second.find(_instance);
                            if (found_instance != found_service->second.end()) {
                                auto found_client = found_instance->second.find(its_client);
                                if (found_client != found_instance->second.end()) {
                                    client = its_client;
                                }
                            }
                        }
                    }
                    its_target = find_or_create_remote_client(its_service, _instance, _reliable, client);
                    if (its_target) {
#ifdef USE_DLT
                        uint16_t its_data_size
                            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                        tc::trace_header its_header;
                        if (its_header.prepare(its_target, true))
                            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                    _data, its_data_size);
#endif
                        is_sent = its_target->send(_data, _size, _flush);
                    } else {
                        VSOMEIP_ERROR<< "Routing error. Client from remote service could not be found!";
                    }
                } else {
                    std::shared_ptr<serviceinfo> its_info(find_service(its_service, _instance));
                    if (its_info || is_service_discovery) {
                        if (is_notification && !is_service_discovery) {
                            send_local_notification(get_client(), _data, _size, _instance, _flush, _reliable);
                            method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
                                    _data[VSOMEIP_METHOD_POS_MAX]);
                            std::shared_ptr<event> its_event = find_event(its_service, _instance, its_method);
                            if (its_event) {
                                std::vector< byte_t > its_data;

                                for (auto its_group : its_event->get_eventgroups()) {
                                    // we need both endpoints as clients can subscribe to events via TCP and UDP
                                    std::shared_ptr<endpoint> its_unreliable_target = its_info->get_endpoint(false);
                                    std::shared_ptr<endpoint> its_reliable_target = its_info->get_endpoint(true);
                                    if (its_unreliable_target || its_reliable_target) {
                                        // remote
                                        auto its_eventgroup = find_eventgroup(its_service, _instance, its_group);
                                        if (its_eventgroup) {
                                            //Unicast targets
                                            for (auto its_remote : its_eventgroup->get_targets()) {
                                                if(its_remote.endpoint_->is_reliable() && its_reliable_target) {
#ifdef USE_DLT
                                                    uint16_t its_data_size
                                                        = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                                                    tc::trace_header its_header;
                                                    if (its_header.prepare(its_reliable_target, true))
                                                        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                                                _data, its_data_size);
#endif
                                                    its_reliable_target->send_to(its_remote.endpoint_, _data, _size);
                                                } else if(its_unreliable_target && !its_eventgroup->is_multicast()) {
#ifdef USE_DLT
                                                    uint16_t its_data_size
                                                        = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                                                    tc::trace_header its_header;
                                                    if (its_header.prepare(its_reliable_target, true))
                                                        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                                                _data, its_data_size);
#endif
                                                    its_unreliable_target->send_to(its_remote.endpoint_, _data, _size);
                                                }
                                            }
                                            //send to multicast targets if subscribers are still interested
                                            if(its_eventgroup->is_multicast()
                                                    && its_eventgroup->get_unreliable_target_count() > 0 ) {
                                                for (auto its_multicast_target : its_eventgroup->get_multicast_targets()) {
                                                    its_unreliable_target->send_to(its_multicast_target.endpoint_, _data, _size);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            its_target = is_service_discovery ? sd_info_->get_endpoint(false) : its_info->get_endpoint(_reliable);
                            if (its_target) {
#ifdef USE_DLT
                                uint16_t its_data_size
                                    = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

                                tc::trace_header its_header;
                                if (its_header.prepare(its_target, true))
                                    tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                                            _data, its_data_size);
#endif
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


    if (its_endpoint) {
#ifdef USE_DLT
        uint16_t its_data_size
            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

        tc::trace_header its_header;
        if (its_header.prepare(its_endpoint, true))
            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                    _data, its_data_size);
#endif
        return its_endpoint->send_to(_target, _data, _size);
    }
    return false;
}

bool routing_manager_impl::send_to(const std::shared_ptr<endpoint_definition> &_target,
        const byte_t *_data, uint32_t _size, uint16_t _sd_port) {
    std::shared_ptr<endpoint> its_endpoint = find_server_endpoint(
            _sd_port, _target->is_reliable());

    if (its_endpoint) {
#ifdef USE_DLT
        uint16_t its_data_size
            = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

        tc::trace_header its_header;
        if (its_header.prepare(its_endpoint, true))
            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                    _data, its_data_size);
#endif
        return its_endpoint->send_to(_target, _data, _size);
    }

    return false;
}

void routing_manager_impl::register_shadow_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, const std::set<eventgroup_t> &_eventgroups,
        bool _is_field, bool _is_provided) {
    routing_manager_base::register_event(_client, _service, _instance,
            _event, _eventgroups, _is_field, _is_provided, true);
}

void routing_manager_impl::unregister_shadow_event(client_t _client,
        service_t _service, instance_t _instance,
        event_t _event, bool _is_provided) {
    routing_manager_base::unregister_event(_client, _service, _instance,
            _event, _is_provided);
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
	if (find_local(_client)) {
		routing_manager_base::notify_one(_service, _instance, _event, _payload,
				_client);
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
				// Now set event's payload!
				// Either with endpoint_definition (remote) or with client (local).
				auto its_service = remote_subscribers_.find(_service);
				if (its_service != remote_subscribers_.end()) {
					auto its_instance = its_service->second.find(_instance);
					if (its_instance != its_service->second.end()) {
						auto its_subscriber = its_instance->second.find(_client);
						if (its_subscriber != its_instance->second.end()) {
							for (auto its_target : its_subscriber->second) {
								its_event->set_payload(_payload, its_target);
							}
						}
					}
				}
			}
		} else {
			VSOMEIP_WARNING << "Attempt to update the undefined event/field ["
				<< std::hex << _service << "." << _instance << "." << _event
				<< "]";
		}
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

void routing_manager_impl::release_port(uint16_t _port, bool _reliable) {
	used_client_ports_[_reliable].erase(_port);
}

void routing_manager_impl::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination) {
#if 0
    std::stringstream msg;
    msg << "rmi::on_message: ";
    for (uint32_t i = 0; i < _size; ++i)
    msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    service_t its_service;
    method_t its_method;
    if (_size >= VSOMEIP_SOMEIP_HEADER_SIZE) {
        its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN],
                _data[VSOMEIP_SERVICE_POS_MAX]);
        if (its_service == VSOMEIP_SD_SERVICE) {
            its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN],
                            _data[VSOMEIP_METHOD_POS_MAX]);
            if (discovery_ && its_method == sd::method) {
                if (configuration_->get_sd_port() == _receiver->get_remote_port()) {
                    boost::asio::ip::address its_address;
                    if (_receiver->get_remote_address(its_address)) {
                        discovery_->on_message(_data, _size, its_address, _destination);
                    } else {
                        VSOMEIP_ERROR << "Ignored SD message from unknown address.";
                    }
                } else {
                    VSOMEIP_ERROR << "Ignored SD message from unknown port ("
                            << _receiver->get_remote_port() << ")";
                }
            }
        } else {
            instance_t its_instance = find_instance(its_service, _receiver);
            return_code_e return_code = check_error(_data, _size, its_instance);
            if(!(_size >= VSOMEIP_MESSAGE_TYPE_POS && utility::is_request_no_return(_data[VSOMEIP_MESSAGE_TYPE_POS]))) {
                if (return_code != return_code_e::E_OK && return_code != return_code_e::E_NOT_OK) {
                    send_error(return_code, _data, _size, its_instance,
                            _receiver->is_reliable(), _receiver);
                    return;
                }
            } else if(return_code != return_code_e::E_OK && return_code != return_code_e::E_NOT_OK) {
                //Ignore request no response message if an error occured
                return;
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
                on_message(its_service, its_instance, _data, _size, _receiver->is_reliable());
            }
        }
    }
#ifdef USE_DLT
    uint16_t its_data_size
        = uint16_t(_size > USHRT_MAX ? USHRT_MAX : _size);

    tc::trace_header its_header;
    if (its_header.prepare(_receiver, false))
        tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE,
                _data, its_data_size);
#endif
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
    client_t its_client;

    if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
        its_client = find_local_client(_service, _instance);
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

        if (!_notify_one) {
            its_event->set_payload(its_payload);
        } else {
            notify_one(_service, _instance, its_event->get_event(), its_payload, _client);
        }

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
                            std::shared_ptr<serviceinfo> its_info(find_service(its_service.first, its_instance.first));
                            if(!its_info) {
                                return;
                            }
                            host_->on_availability(its_service.first, its_instance.first,
                                    true, its_info->get_major(), its_info->get_minor());
                        }
                    }
                    found_endpoint = its_client.second.find(true);
                    if (found_endpoint != its_client.second.end()) {
                        if (found_endpoint->second.get() == _endpoint.get()) {
                            std::shared_ptr<serviceinfo> its_info(find_service(its_service.first, its_instance.first));
                            if(!its_info){
                                return;
                            }
                            host_->on_availability(its_service.first, its_instance.first,
                                    true, its_info->get_major(), its_info->get_minor());
                            stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT,
                                    its_service.first, its_instance.first, its_info->get_major(), its_info->get_minor());
                            if(discovery_) {
                                discovery_->on_reliable_endpoint_connected(its_service.first, its_instance.first, _endpoint);
                            }
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

                            std::shared_ptr<serviceinfo> its_info(find_service(its_service.first, its_instance.first));
                            if(!its_info){
                                return;
                            }
                            host_->on_availability(its_service.first, its_instance.first,
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
                            host_->on_availability(its_service.first, its_instance.first,
                                    false, its_info->get_major(), its_info->get_minor());
                        }
                    }
                }
            }
        }
    }
}

void routing_manager_impl::on_stop_offer_service(service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {

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
    {
        std::lock_guard<std::mutex> its_lock(eventgroup_clients_mutex_);
        auto its_service = eventgroup_clients_.find(_service);
        if (its_service != eventgroup_clients_.end()) {
            auto its_instance = its_service->second.find(_instance);
            if (its_instance != its_service->second.end()) {
                its_instance->second.clear();
            }
        }
    }

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
        auto its_info = find_service(_service, _instance);
        if (its_info) {
            if (its_info->get_major() == _major && its_info->get_minor() == _minor) {
                its_info->set_ttl(0);
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
        VSOMEIP_ERROR << "Routing manager: deliver_message: "
                      << "SomeIP-Header deserialization failed!";
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
        std::set<client_t> its_local_client_set;
        if(its_event->is_field() && !its_event->is_shadow()) {
            // store the current value of the remote field
            const uint32_t its_length(utility::get_payload_size(_data, _length));
            std::shared_ptr<payload> its_payload =
                    runtime::get()->create_payload(
                            &_data[VSOMEIP_PAYLOAD_POS],
                            its_length);
            its_event->set_payload_dont_notify(its_payload);
        }
        for (auto its_group : its_event->get_eventgroups()) {
            auto its_local_clients
                = find_local_clients(_service, _instance, its_group);
            its_local_client_set.insert(
                    its_local_clients.begin(), its_local_clients.end());
        }

        for (auto its_local_client : its_local_client_set) {
            if (its_local_client == host_->get_client()) {
                deliver_message(_data, _length, _instance, _reliable);
            } else {
                std::shared_ptr<endpoint> its_local_target = find_local(its_local_client);
                if (its_local_target) {
                    send_local(its_local_target, VSOMEIP_ROUTING_CLIENT,
                            _data, _length, _instance, true, _reliable, VSOMEIP_SEND);
                }
            }
        }
    }

    return is_delivered;
}

std::shared_ptr<eventgroupinfo> routing_manager_impl::find_eventgroup(
        service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) const {
    return routing_manager_base::find_eventgroup(_service, _instance, _eventgroup);
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
            sd_info_ = std::make_shared<serviceinfo>(ANY_MAJOR, ANY_MINOR, DEFAULT_TTL,
                    false); // false, because we do _not_ want to announce it...
            sd_info_->set_endpoint(its_service_endpoint, _reliable);
            its_service_endpoint->add_default_target(VSOMEIP_SD_SERVICE,
                    _address, _port);
            its_service_endpoint->join(_address);
        } else {
            VSOMEIP_ERROR << "Service Discovery endpoint could not be created. "
                    "Please check your network configuration.";
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
void routing_manager_impl::init_service_info(
        service_t _service, instance_t _instance, bool _is_local_service) {
    std::shared_ptr<serviceinfo> its_info = find_service(_service, _instance);
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
    } else {
        host_->on_error(error_code_e::CONFIGURATION_MISSING);
    }
}

std::shared_ptr<endpoint> routing_manager_impl::create_client_endpoint(
        const boost::asio::ip::address &_address,
        uint16_t _local_port, uint16_t _remote_port,
        bool _reliable, client_t _client, bool _start) {
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
                    configuration_->get_message_size_reliable(
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
                    io_);
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
                        its_unicast.to_string(), _port) ||
                        configuration_->has_enabled_magic_cookies(
                                "local", _port)) {
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

std::shared_ptr<endpoint> routing_manager_impl::find_local(client_t _client) {
    return routing_manager_base::find_local(_client);
}

std::shared_ptr<endpoint> routing_manager_impl::find_or_create_local(
        client_t _client) {
    return routing_manager_base::find_or_create_local(_client);
}

void routing_manager_impl::remove_local(client_t _client) {
    routing_manager_base::remove_local(_client);
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

    uint16_t its_local_port;
    if (configuration_->get_client_port(_service, _instance, _reliable,
    		used_client_ports_, its_local_port)) {
		auto found_service = remote_service_info_.find(_service);
		if (found_service != remote_service_info_.end()) {
			auto found_instance = found_service->second.find(_instance);
			if (found_instance != found_service->second.end()) {
				auto found_reliability = found_instance->second.find(_reliable);
				if (found_reliability != found_instance->second.end()) {
					its_endpoint_def = found_reliability->second;
					its_endpoint = create_client_endpoint(
							its_endpoint_def->get_address(),
							its_local_port,
							its_endpoint_def->get_port(),
							_reliable, _client,
							configuration_->is_someip(_service, _instance)
					);
				}
			}
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
                    } else {
                        VSOMEIP_WARNING << "Reliable service endpoint has changed!";
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
                                    || _major == DEFAULT_MAJOR)
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

        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
        auto found_service2 = specific_endpoint_clients_.find(_service);
        if (found_service2 != specific_endpoint_clients_.end()) {
            auto found_instance = found_service2->second.find(_instance);
            if (found_instance != found_service2->second.end()) {
                for (const client_t& c : found_instance->second) {
                    find_or_create_remote_client(_service, _instance, true, c);
                }
            }
        }
    }

    if (_unreliable_port != ILLEGAL_PORT && !is_unreliable_known) {
        std::shared_ptr<endpoint_definition> endpoint_def
            = endpoint_definition::get(_unreliable_address, _unreliable_port, false);
        remote_service_info_[_service][_instance][false] = endpoint_def;
        is_added = !is_reliable_known;
        if (is_added) {
            host_->on_availability(_service, _instance, true, _major, _minor);
            stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance, _major, _minor);
        }
    }

}

void routing_manager_impl::del_routing_info(service_t _service, instance_t _instance,
        bool _has_reliable, bool _has_unreliable) {

    std::shared_ptr<serviceinfo> its_info(find_service(_service, _instance));
    if(!its_info)
        return;

    host_->on_availability(_service, _instance, false, its_info->get_major(), its_info->get_minor());
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
                }
            }
        }
    }
    {
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

std::chrono::milliseconds routing_manager_impl::update_routing_info(std::chrono::milliseconds _elapsed) {
    std::chrono::seconds default_ttl(DEFAULT_TTL);
    std::chrono::milliseconds its_smallest_ttl =
            std::chrono::duration_cast<std::chrono::milliseconds>(default_ttl);
    std::map<service_t,
        std::map<instance_t,
            std::pair<bool, bool> > > its_expired_offers;

    for (auto &s : get_services()) {
        for (auto &i : s.second) {
            if (routing_manager_base::find_local(s.first, i.first)) {
                continue; //don't expire local services
            }
            ttl_t its_ttl = i.second->get_ttl();
            std::chrono::milliseconds precise_ttl = i.second->get_precise_ttl();
            if (its_ttl < DEFAULT_TTL) { // do not touch "forever"
                if (precise_ttl.count() < _elapsed.count() || precise_ttl.count() == 0) {
                    i.second->set_ttl(0);
                    if (discovery_)
                        discovery_->unsubscribe_all(s.first, i.first);
                    its_expired_offers[s.first][i.first] = {
                            i.second->get_endpoint(true) != nullptr,
                            i.second->get_endpoint(false) != nullptr
                    };
                } else {
                    std::chrono::milliseconds its_new_ttl(precise_ttl - _elapsed);
                    i.second->set_precise_ttl(its_new_ttl);
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

void routing_manager_impl::expire_services(const boost::asio::ip::address &_address) {
    std::map<service_t,
        std::map<instance_t,
            std::pair<bool, bool> > > its_expired_offers;

    for (auto &s : get_services()) {
        for (auto &i : s.second) {
            if (routing_manager_base::find_local(s.first, i.first)) {
                continue; //don't expire local services
            }
            bool is_gone(false);
            boost::asio::ip::address its_address;
            std::shared_ptr<endpoint> its_endpoint = i.second->get_endpoint(true);
            if (its_endpoint) {
                if (its_endpoint->get_remote_address(its_address)) {
                    is_gone = (its_address == _address);
                }
            } else {
                its_endpoint = i.second->get_endpoint(false);
                if (its_endpoint) {
                    if (its_endpoint->get_remote_address(its_address)) {
                        is_gone = (its_address == _address);
                    }
                }
            }

            if (is_gone) {
                if (discovery_)
                    discovery_->unsubscribe_all(s.first, i.first);
                its_expired_offers[s.first][i.first] = {
                        i.second->get_endpoint(true) != nullptr,
                        i.second->get_endpoint(false) != nullptr
                };
            }
        }
    }

    for (auto &s : its_expired_offers) {
        for (auto &i : s.second) {
            del_routing_info(s.first, i.first, i.second.first, i.second.second);
        }
    }
}

void routing_manager_impl::expire_subscriptions(const boost::asio::ip::address &_address) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (auto &its_service : eventgroups_) {
        for (auto &its_instance : its_service.second) {
            for (auto &its_eventgroup : its_instance.second) {
                std::set<std::shared_ptr<endpoint_definition>> its_invalid_endpoints;
                for (auto &its_target : its_eventgroup.second->get_targets()) {
                    if (its_target.endpoint_->get_address() == _address)
                        its_invalid_endpoints.insert(its_target.endpoint_);
                }

                for (auto &its_endpoint : its_invalid_endpoints) {
                    its_eventgroup.second->remove_target(its_endpoint);
                }
                if(its_eventgroup.second->is_multicast() &&
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

bool routing_manager_impl::on_subscribe_accepted(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, const std::shared_ptr<endpoint_definition> _target,
            const std::chrono::high_resolution_clock::time_point &_expiration) {
    std::shared_ptr<eventgroupinfo> its_eventgroup
        = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        client_t client = VSOMEIP_ROUTING_CLIENT;
        if (!_target->is_reliable()) {
            if (!its_eventgroup->is_multicast())  {
                uint16_t unreliable_port = configuration_->get_unreliable_port(_service, _instance);
                _target->set_remote_port(unreliable_port);
                auto endpoint = find_server_endpoint(unreliable_port, false);
                if (endpoint) {
                    client = std::dynamic_pointer_cast<udp_server_endpoint_impl>(endpoint)->
                            get_client(_target);
                }
            }
        }
        else {
            uint16_t reliable_port = configuration_->get_reliable_port(_service, _instance);
            _target->set_remote_port(reliable_port);
            auto endpoint = find_server_endpoint(reliable_port, true);
            if (endpoint) {
                client = std::dynamic_pointer_cast<tcp_server_endpoint_impl>(endpoint)->
                        get_client(_target);
            }
        }

        if (its_eventgroup->update_target(_target, _expiration)) {
            return true;
        }

        if (!host_->on_subscription(_service, _instance, _eventgroup, client, true)) {
            VSOMEIP_INFO << "Subscription request from client: 0x" << std::hex
                         << client << " for eventgroup: 0x" << _eventgroup << std::dec
                         << " rejected from application handler.";
            return false;
        }

        if (client != VSOMEIP_ROUTING_CLIENT) {
            VSOMEIP_DEBUG << "Subscription accepted: eventgroup=" << _eventgroup
                          << " : target: " << _target->get_address().to_string()
                          << ":" << std::dec <<_target->get_port()
                          << (_target->is_reliable() ? " reliable" : " unreliable")
                          << " from client: 0x" << std::hex << client << ".";
        } else {
            VSOMEIP_DEBUG << "Subscription accepted: eventgroup: " << _eventgroup
                          << " : target: " << _target->get_address().to_string()
                          << ":" << std::dec <<_target->get_port()
                          << (_target->is_reliable() ? " reliable" : " unreliable")
                          << " from unknown client.";
        }

        stub_->send_subscribe(routing_manager_base::find_local(_service, _instance),
                client, _service, _instance, _eventgroup, its_eventgroup->get_major(), true);

        remote_subscribers_[_service][_instance][client].insert(_target);
    } else {
        VSOMEIP_ERROR<< "subscribe: attempt to subscribe to unknown eventgroup!";
        return false;
    }
    return true;
}

void routing_manager_impl::on_subscribe(
        service_t _service,    instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<endpoint_definition> _subscriber,
        std::shared_ptr<endpoint_definition> _target,
        const std::chrono::high_resolution_clock::time_point &_expiration) {

    std::shared_ptr<eventgroupinfo> its_eventgroup
        = find_eventgroup(_service, _instance, _eventgroup);
    if (its_eventgroup) {
        // IP address of target is a multicast address if the event is in a multicast eventgroup
        bool target_added(false);
        if( its_eventgroup->is_multicast() && !_subscriber->is_reliable()) {
            // Event is in multicast eventgroup and subscribe for UDP
            target_added = its_eventgroup->add_target({ _target, _expiration }, {_subscriber, _expiration});

            // If the target is multicast, we need to set the remote port
            // of the unicast(!) here, as its only done in on_subscribe_accepted
            // for unicast subscribes and it needs to be done before calling
            // notify_one on the events.
            uint16_t unreliable_port =
                    configuration_->get_unreliable_port(_service, _instance);
            _subscriber->set_remote_port(unreliable_port);
        }
        else {
            // subscribe for TCP or UDP
            target_added = its_eventgroup->add_target({ _target, _expiration });
        }

        if (target_added) { // unicast or multicast
            // send initial events
            for (auto its_event : its_eventgroup->get_events()) {
                if (its_event->is_field()) {
                    its_event->notify_one(_subscriber); // unicast
                }
            }
        }
    }
}

void routing_manager_impl::on_unsubscribe(service_t _service,
        instance_t _instance, eventgroup_t _eventgroup,
        std::shared_ptr<endpoint_definition> _target) {
    std::shared_ptr<eventgroupinfo> its_eventgroup = find_eventgroup(_service,
            _instance, _eventgroup);
    if (its_eventgroup) {
        client_t its_client = find_client(_service, _instance, its_eventgroup, _target);

        if (its_client != VSOMEIP_ROUTING_CLIENT) {
            VSOMEIP_DEBUG << "on_unsubscribe: target: " << _target->get_address().to_string()
                            << ":" << std::dec <<_target->get_port()
                            << (_target->is_reliable() ? " reliable" : " unreliable")
                            << " from client: 0x" << std::hex << its_client;
        } else {
            VSOMEIP_DEBUG << "on_unsubscribe: target: " << _target->get_address().to_string()
                                    << ":" << std::dec <<_target->get_port()
                                    << (_target->is_reliable() ? " reliable" : " unreliable");
        }

        its_eventgroup->remove_target(_target);
        clear_remote_subscriber(_service, _instance, its_client, _target);

        stub_->send_unsubscribe(routing_manager_base::find_local(_service, _instance),
                its_client, _service, _instance, _eventgroup);

        host_->on_subscription(_service, _instance, _eventgroup, its_client, false);

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

void routing_manager_impl::on_subscribe_ack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    {
        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
        auto found_service = specific_endpoint_clients_.find(_service);
        if(found_service != specific_endpoint_clients_.end()){
            auto found_instance = found_service->second.find(_instance);
            if(found_instance != found_service->second.end()) {
                auto found_client = found_instance->second.find(_client);
                if(found_client == found_instance->second.end()) {
                    // Ack is only interesting for proxies using its own endpoint!
                    return;
                }
            }
        }
    }
    if (_client == get_client()) {
        host_->on_subscription_error(_service, _instance, _eventgroup, 0x0 /*OK*/);
    } else {
        stub_->send_subscribe_ack(_client, _service, _instance, _eventgroup);
    }
}

void routing_manager_impl::on_subscribe_nack(client_t _client,
        service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    {
        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
        auto found_service = specific_endpoint_clients_.find(_service);
        if(found_service != specific_endpoint_clients_.end()){
            auto found_instance = found_service->second.find(_instance);
            if(found_instance != found_service->second.end()) {
                auto found_client = found_instance->second.find(_client);
                if(found_client == found_instance->second.end()) {
                    // Nack is only interesting for proxies using its own endpoint!
                    return;
                }
            }
        }
    }
    if (_client == get_client()) {
        host_->on_subscription_error(_service, _instance, _eventgroup, 0x7 /*Rejected*/);
    } else {
        stub_->send_subscribe_nack(_client, _service, _instance, _eventgroup);
    }
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
                        if (client != get_client()) {
                            auto local_endpoint = find_local(client);
                            if (local_endpoint) {
                                send_local(local_endpoint, client, _data, _size, _instance, true,
                                        _receiver->is_reliable(), VSOMEIP_SEND);
                            }
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
    {
        std::lock_guard<std::mutex> its_lock(specific_endpoint_clients_mutex_);
        auto found_service = specific_endpoint_clients_.find(_service);
        if(found_service != specific_endpoint_clients_.end()){
            auto found_instance = found_service->second.find(_instance);
            if(found_instance != found_service->second.end()) {
                for (const client_t& client : found_instance->second) {
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
        endpoint *_receiver) {

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

    std::lock_guard<std::mutex> its_lock(serialize_mutex_);
    if (serializer_->serialize(error_message.get())) {
        if (_receiver) {
            boost::asio::ip::address adr;
            uint16_t port;
            if (_receiver->is_reliable()) {
                auto endpoint = dynamic_cast<tcp_server_endpoint_impl*>(_receiver);
                if(!endpoint) {
                    return;
                }
                auto remote = endpoint->get_remote();
                adr = remote.address();
                port = remote.port();
            } else {
                auto endpoint = dynamic_cast<udp_server_endpoint_impl*>(_receiver);
                if (!endpoint) {
                    return;
                }
                auto remote = endpoint->get_remote();
                adr = remote.address();
                port = remote.port();
            }
            auto its_endpoint_def =
                    std::make_shared<endpoint_definition>(adr, port, _receiver->is_reliable());
            its_endpoint_def->set_remote_port(_receiver->get_local_port());
            send_to(its_endpoint_def, serializer_->get_data(), serializer_->get_size());
        }
        serializer_->reset();
    } else {
        VSOMEIP_ERROR << "Failed to serialize error message.";
    }
}

void routing_manager_impl::on_identify_response(client_t _client, service_t _service,
        instance_t _instance, bool _reliable) {
    std::lock_guard<std::mutex> its_lock(identified_clients_mutex_);
    identified_clients_[_service][_instance][_reliable].insert(_client);
    discovery_->send_subscriptions(_service, _instance, _client, _reliable);
}

void routing_manager_impl::identify_for_subscribe(client_t _client,
        service_t _service, instance_t _instance, major_version_t _major) {
    if (!has_identified(_client, _service, _instance, false)) {
        auto unreliable_endpoint = find_or_create_remote_client(_service, _instance, false, _client);
        if (unreliable_endpoint) {
            auto message = runtime::get()->create_message(false);
            message->set_service(_service);
            message->set_instance(_instance);
            message->set_client(_client);
            message->set_method(ANY_METHOD - 1);
            message->set_interface_version(_major);
            message->set_message_type(message_type_e::MT_REQUEST);
            std::lock_guard<std::mutex> its_lock(serialize_mutex_);
            if (serializer_->serialize(message.get())) {
                unreliable_endpoint->send(serializer_->get_data(),
                        serializer_->get_size());
                serializer_->reset();
            }
        }
    }
    if (!has_identified(_client, _service, _instance, true)) {
        auto reliable_endpoint = find_or_create_remote_client(_service, _instance, true, _client);
        if (reliable_endpoint) {
            auto message = runtime::get()->create_message(true);
            message->set_service(_service);
            message->set_instance(_instance);
            message->set_client(_client);
            message->set_method(ANY_METHOD - 1);
            message->set_interface_version(_major);
            message->set_message_type(message_type_e::MT_REQUEST);
            std::lock_guard<std::mutex> its_lock(serialize_mutex_);
            if (serializer_->serialize(message.get())) {
                reliable_endpoint->send(serializer_->get_data(),
                        serializer_->get_size());
                serializer_->reset();
            }
        }
    }
}

bool routing_manager_impl::supports_selective(service_t _service, instance_t _instance) {
    bool supports_selective(false);
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
    auto its_service = remote_subscribers_.find(_service);
    if (its_service != remote_subscribers_.end()) {
        auto its_instance = its_service->second.find(_instance);
        if (its_instance != its_service->second.end()) {
            auto its_client = its_instance->second.find(_client);
            if (its_client != its_instance->second.end()) {
                if (its_client->second.size() <= 1) {
                    its_instance->second.erase(_client);
                } else {
                    its_client->second.erase(_target);
                }
            }
        }
    }
}

std::chrono::high_resolution_clock::time_point
routing_manager_impl::expire_subscriptions() {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    std::chrono::high_resolution_clock::time_point now
        = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point next_expiration
        = std::chrono::high_resolution_clock::now() + std::chrono::hours(24);

    for (auto &its_service : eventgroups_) {
        for (auto &its_instance : its_service.second) {
            for (auto &its_eventgroup : its_instance.second) {
                std::set<std::shared_ptr<endpoint_definition>> its_expired_endpoints;
                for (auto &its_target : its_eventgroup.second->get_targets()) {
                    if (its_target.expiration_ < now) {
                        its_expired_endpoints.insert(its_target.endpoint_);
                    } else if (its_target.expiration_ < next_expiration) {
                        next_expiration = its_target.expiration_;
                    }
                }

                for (auto its_endpoint : its_expired_endpoints) {
                    its_eventgroup.second->remove_target(its_endpoint);

                    client_t its_client
                        = find_client(its_service.first, its_instance.first,
                                      its_eventgroup.second, its_endpoint);
                    clear_remote_subscriber(its_service.first, its_instance.first,
                            its_client, its_endpoint);

                    VSOMEIP_DEBUG << "Expired subscription ("
                            << std::hex << its_service.first << "."
                            << its_instance .first << "."
                            << its_eventgroup.first << " from "
                            << its_endpoint->get_address() << ":"
                            << std::dec << its_endpoint->get_port()
                            << "(" << std::hex << its_client << ")";
                }
                if(its_eventgroup.second->is_multicast() &&
                        0 == its_eventgroup.second->get_unreliable_target_count() ) {
                    //clear multicast targets if no unreliable subscriber is left for multicast eventgroup
                    its_eventgroup.second->clear_multicast_targets();
                }
            }
        }
    }

    return next_expiration;
}

bool routing_manager_impl::queue_message(const byte_t *_data, uint32_t _size) const {
    return stub_->queue_message(_data, _size);
}

} // namespace vsomeip

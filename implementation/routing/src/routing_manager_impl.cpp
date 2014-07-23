// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
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

#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_impl.hpp"
#include "../include/routing_manager_stub.hpp"
#include "../include/servicegroup.hpp"
#include "../include/serviceinfo.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/local_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_client_endpoint_impl.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_client_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../message/include/byteorder.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/event_impl.hpp"
#include "../../message/include/serializer.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../service_discovery/include/service_discovery_impl.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

routing_manager_impl::routing_manager_impl(routing_manager_host *_host)
	: host_(_host),
	  io_(_host->get_io()),
	  deserializer_(std::make_shared< deserializer>()),
	  serializer_(std::make_shared< serializer >()),
	  configuration_(host_->get_configuration()) {
}

routing_manager_impl::~routing_manager_impl() {
}

boost::asio::io_service & routing_manager_impl::get_io() {
	return io_;
}

void routing_manager_impl::init() {
	uint32_t its_max_message_size = VSOMEIP_MAX_LOCAL_MESSAGE_SIZE;
	if (VSOMEIP_MAX_TCP_MESSAGE_SIZE > its_max_message_size)
		its_max_message_size = VSOMEIP_MAX_TCP_MESSAGE_SIZE;
	if (VSOMEIP_MAX_UDP_MESSAGE_SIZE > its_max_message_size)
		its_max_message_size = VSOMEIP_MAX_UDP_MESSAGE_SIZE;

	serializer_->create_data(its_max_message_size);

	// TODO: Only instantiate the stub if needed
	stub_ = std::make_shared< routing_manager_stub >(this);
	stub_->init();

	if (configuration_->is_service_discovery_enabled()) {
		VSOMEIP_INFO << "Service Discovery enabled.";
		sd::runtime **its_runtime = static_cast< sd::runtime ** >(
										utility::load_library(
										VSOMEIP_SD_LIBRARY,
										VSOMEIP_SD_RUNTIME_SYMBOL_STRING)
									);

		if (0 != its_runtime && 0 != (*its_runtime)) {
			VSOMEIP_INFO << "Service Discovery module loaded.";
			discovery_ = (*its_runtime)->create_service_discovery(this);
			discovery_->init();
		}
	} else {
		init_routing_info();
	}
}

void routing_manager_impl::start() {
	stub_->start();
	if (discovery_) discovery_->start();

	host_->on_event(event_type_e::REGISTERED);
}

void routing_manager_impl::stop() {
	host_->on_event(event_type_e::DEREGISTERED);

	if (discovery_) discovery_->stop();
	stub_->stop();
}

void routing_manager_impl::offer_service(client_t _client,
		service_t _service, instance_t _instance,
		major_version_t _major, minor_version_t _minor, ttl_t _ttl) {
	local_services_[_service][_instance] = _client;

	// Remote route (incoming only)
	std::shared_ptr< serviceinfo > its_info(find_service(_service, _instance));
	if (its_info) {
		if (its_info->get_major() == _major && its_info->get_minor() == _minor) {
			VSOMEIP_DEBUG << "Setting TTL=" << std::dec << _ttl;
			its_info->set_ttl(_ttl);
		} else {
			host_->on_error(error_code_e::SERVICE_PROPERTY_MISMATCH);
		}
	} else {
		(void)create_service(_service, _instance, _major, _minor, _ttl);
	}

	host_->on_availability(_service, _instance, true);
}

void routing_manager_impl::stop_offer_service(client_t its_client,
		service_t _service, instance_t _instance) {
	if (discovery_) {
		auto found_service = services_.find(_service);
		if (found_service != services_.end()) {
			auto found_instance = found_service->second.find(_instance);
			if (found_instance != found_service->second.end()) {
				found_instance->second->set_ttl(0);
			}
		}
	} else {
		del_routing_info(_service, _instance, false);
		del_routing_info(_service, _instance, true);
	}
}

void routing_manager_impl::publish_eventgroup(client_t its_client,
		service_t _service, instance_t _instance, eventgroup_t _eventgroup,
		major_version_t _major, ttl_t _ttl) {
}

void routing_manager_impl::stop_publish_eventgroup(client_t its_client,
		service_t _service, instance_t _instance, eventgroup_t _eventgroup) {

}

std::shared_ptr< event > routing_manager_impl::add_event(client_t _client,
		service_t _service, instance_t _instance,
		eventgroup_t _eventgroup, event_t _event) {

	return std::make_shared< event_impl >(io_);
}

std::shared_ptr< event > routing_manager_impl::add_field(client_t _client,
		service_t _service, instance_t _instance,
		eventgroup_t _eventgroup, event_t _event,
		std::shared_ptr< payload > _payload) {
	std::shared_ptr< event > its_event = add_event(_client, _service, _instance,
												   _eventgroup, _event);
	its_event->set_payload(_payload);
	return its_event;
}

void routing_manager_impl::remove_event_or_field(std::shared_ptr< event > _event) {

}

void routing_manager_impl::request_service(client_t _client,
		service_t _service, instance_t _instance,
		major_version_t _major, minor_version_t _minor, ttl_t _ttl) {

	std::shared_ptr< serviceinfo > its_info(find_service(_service, _instance));
	if (its_info) {
		if ((_major > its_info->get_major()) ||
			(_major == its_info->get_major() && _minor > its_info->get_minor()) ||
			(_ttl > its_info->get_ttl())) {
			host_->on_error(error_code_e::SERVICE_PROPERTY_MISMATCH);
		} else {
			its_info->add_client(_client);
		}
	} else {
		if (discovery_)
			discovery_->request_service(_service, _instance, _major, _minor, _ttl);
	}
}

void routing_manager_impl::release_service(client_t _client,
		service_t _service, instance_t _instance) {
	std::shared_ptr< serviceinfo > its_info(find_service(_service, _instance));
	if (its_info) {
		its_info->remove_client(_client);
	} else {
		if (discovery_)
			discovery_->release_service(_service, _instance);
	}
}

void routing_manager_impl::subscribe(client_t its_client,
		service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
}

void routing_manager_impl::unsubscribe(client_t its_client,
		service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
}

void routing_manager_impl::send(client_t its_client,
		std::shared_ptr< message > _message, bool _reliable, bool _flush) {
	std::unique_lock< std::mutex > its_lock(serialize_mutex_);
	if (serializer_->serialize(_message.get())) {
		send(its_client, serializer_->get_data(), serializer_->get_size(), _message->get_instance(), _reliable, _flush);
		serializer_->reset();
	}
}

void routing_manager_impl::send(client_t _client,
		const byte_t *_data, length_t _size, instance_t _instance, bool _flush, bool _reliable) {
	std::shared_ptr< endpoint > its_target;

	client_t its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN], _data[VSOMEIP_CLIENT_POS_MAX]);
	service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
	bool is_request = utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS]);

	if (_size > VSOMEIP_MESSAGE_TYPE_POS) {
		if (is_request) {
			its_target = find_local(its_service, _instance);
		} else {
			its_target = find_local(its_client);
		}

		if (its_target) {
			std::vector< byte_t > its_command(VSOMEIP_COMMAND_HEADER_SIZE + _size + sizeof(instance_t) + sizeof(bool) + sizeof(bool));
			its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SEND;
			std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client, sizeof(client_t));
			std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &_size, sizeof(_size));
			std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], _data, _size);
			std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size], &_instance, sizeof(instance_t));
			std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size + sizeof(instance_t)], &_reliable, sizeof(bool));
			std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + _size + sizeof(instance_t) + sizeof(bool)] , &_flush, sizeof(bool));
			its_target->send(&its_command[0], its_command.size(), _flush);
		} else {
			// Check whether hosting application should get the message
			// If not, check routes to external
			if ((its_client == host_->get_client() && !is_request) ||
				(find_local_client(its_service, _instance) == host_->get_client() && is_request)) {
				deliver_message(_data, _size, _instance);
			} else {
				if (is_request) {
					its_target = find_remote_client(its_service, _instance, _reliable);
					if (its_target) {
						its_target->send(_data, _size, _flush);
					} else {
						VSOMEIP_ERROR << "Routing error. Client from remote service could not be found!";
					}
				} else {
					std::shared_ptr< serviceinfo > its_info(find_service(its_service, _instance));
					if (its_info) {
						its_target = its_info->get_endpoint(_reliable);
						if (its_target) {
							its_target->send(_data, _size, _flush);
						} else {
							VSOMEIP_ERROR << "Routing error. Service endpoint could not be found!";
						}
					} else {
						VSOMEIP_ERROR << "Routing error. Service could not be found!";
					}
				}
			}
		}
	}
}

void routing_manager_impl::set(client_t its_client,
		service_t _service, instance_t _instance,
		event_t _event, const std::vector< byte_t > &_value) {
}

void routing_manager_impl::on_message(const byte_t *_data, length_t _size, endpoint *_receiver) {
#if 0
	std::stringstream msg;
	msg << "rmi::on_message: ";
	for (uint32_t i = 0; i < _size; ++i)
		msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
	VSOMEIP_DEBUG << msg.str();
#endif
	if (_size >= VSOMEIP_SOMEIP_HEADER_SIZE) {
		service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
		if (its_service == VSOMEIP_SD_SERVICE) {
			if (discovery_)
				discovery_->on_message(_data, _size);
		} else {
			instance_t its_instance = find_instance(its_service, _receiver);
			if (its_instance != any_instance) {
				client_t its_client(VSOMEIP_ROUTING_CLIENT);
				if (utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS])) {
					its_client = find_local_client(its_service, its_instance);
				} else {
					its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN], _data[VSOMEIP_CLIENT_POS_MAX]);
				}

				if (its_client == host_->get_client()) {
					deliver_message(_data, _size, its_instance);
				} else if (its_client != VSOMEIP_ROUTING_CLIENT) {
					send(its_client, _data, _size, its_instance, true, false);
				} else {
					VSOMEIP_ERROR << "Could not determine target application!";
				}
			} else {
				VSOMEIP_ERROR << "Could not determine service instance for [" << its_service << "]";
			}
		}
	} else {
		//send_error(); // TODO: send error "malformed message"
	}
}

void routing_manager_impl::on_connect(std::shared_ptr< endpoint > _endpoint) {
	for (auto &its_service : remote_services_) {
		for (auto &its_instance : its_service.second) {
			auto found_endpoint = its_instance.second.find(false);
			if (found_endpoint != its_instance.second.end()) {
				host_->on_availability(its_service.first, its_instance.first, true);
			} else {
				found_endpoint = its_instance.second.find(true);
				if (found_endpoint != its_instance.second.end()) {
					host_->on_availability(its_service.first, its_instance.first, true);
				}
			}
		}
	}
}

void routing_manager_impl::on_disconnect(std::shared_ptr< endpoint > _endpoint) {
	for (auto &its_service : remote_services_) {
		for (auto &its_instance : its_service.second) {
			auto found_endpoint = its_instance.second.find(false);
			if (found_endpoint != its_instance.second.end()) {
				host_->on_availability(its_service.first, its_instance.first, false);
			} else {
				found_endpoint = its_instance.second.find(true);
				if (found_endpoint != its_instance.second.end()) {
					host_->on_availability(its_service.first, its_instance.first, false);
				}
			}
		}
	}
}

void routing_manager_impl::deliver_message(const byte_t *_data, length_t _size, instance_t _instance) {
	deserializer_->set_data(_data, _size);
	std::shared_ptr< message > its_message(deserializer_->deserialize_message());
	if (its_message) {
		its_message->set_instance(_instance);
		host_->on_message(its_message);
	} else {
		// TODO: send error "Malformed Message"
		//send_error();
	}
}

bool routing_manager_impl::is_available(service_t _service, instance_t _instance) const {
	auto find_local_service = local_services_.find(_service);
	if (find_local_service != local_services_.end()) {
		auto find_local_instance = find_local_service->second.find(_instance);
		if (find_local_instance != find_local_service->second.end()) {
			return true;
		}
	}

	auto find_remote_service = remote_services_.find(_service);
	if (find_remote_service != remote_services_.end()) {
		auto find_remote_instance = find_remote_service->second.find(_instance);
		if (find_remote_instance != find_remote_service->second.end()) {
			return true;
		}
	}

	return false;
}

const std::map< std::string, std::shared_ptr< servicegroup > > &
routing_manager_impl::get_servicegroups() const {
	return servicegroups_;
}

std::shared_ptr< configuration > routing_manager_impl::get_configuration() const {
	return host_->get_configuration();
}

void routing_manager_impl::create_service_discovery_endpoint(
		const std::string &_address, uint16_t _port, const std::string &_protocol) {
	bool is_reliable = (_protocol != "udp");

	std::shared_ptr< endpoint > its_service_endpoint = find_server_endpoint(_port, is_reliable);
	if (!its_service_endpoint) {
		its_service_endpoint = create_server_endpoint(_port, is_reliable);

		std::shared_ptr< serviceinfo > its_info(
				std::make_shared< serviceinfo >(any_major, any_minor, any_ttl));
		its_info->set_endpoint(its_service_endpoint, is_reliable);

		// routing info
		services_[VSOMEIP_SD_SERVICE][VSOMEIP_SD_INSTANCE] = its_info;

		its_service_endpoint->join(_address);
		its_service_endpoint->start();
	}
}

service_map_t routing_manager_impl::get_offered_services(const std::string &_name) const {
	service_map_t its_offers;

	auto find_servicegroup = servicegroups_.find(_name);
	if (find_servicegroup != servicegroups_.end()) {
		return find_servicegroup->second->get_services();
	}

	return its_offers;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr< serviceinfo > routing_manager_impl::find_service(
									service_t _service, instance_t _instance) {
	std::shared_ptr< serviceinfo > its_info;
	auto found_service = services_.find(_service);
	if (found_service != services_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			its_info = found_instance->second;
		}
	}
	return its_info;
}

std::shared_ptr< serviceinfo > routing_manager_impl::create_service(
	service_t _service, instance_t _instance,
	major_version_t _major, minor_version_t _minor, ttl_t _ttl) {
	std::shared_ptr< serviceinfo > its_info;
	if (configuration_) {
		its_info = std::make_shared< serviceinfo >(_major, _minor, _ttl);

		uint16_t its_reliable_port
			= configuration_->get_reliable_port(_service, _instance);
		uint16_t its_unreliable_port
			= configuration_->get_unreliable_port(_service, _instance);

		if (its_reliable_port != illegal_port) {
			std::shared_ptr< endpoint > its_reliable_endpoint(
				find_or_create_server_endpoint(its_reliable_port, true));
			its_info->set_endpoint(its_reliable_endpoint, true);

			// TODO: put this in a method and check whether an assignment already exists!
			service_instances_[_service][its_reliable_endpoint.get()] = _instance;
		}

		if (its_unreliable_port != illegal_port) {
			std::shared_ptr< endpoint > its_unreliable_endpoint(
				find_or_create_server_endpoint(its_unreliable_port, false));
			its_info->set_endpoint(its_unreliable_endpoint, false);

			service_instances_[_service][its_unreliable_endpoint.get()] = _instance;
		}

		if (illegal_port != its_reliable_port || illegal_port != its_unreliable_port) {
			std::string its_servicegroup = configuration_->get_group(_service, _instance);
			auto found_servicegroup = servicegroups_.find(its_servicegroup);
			if (found_servicegroup == servicegroups_.end()) {
				servicegroups_[its_servicegroup] = std::make_shared< servicegroup >(its_servicegroup);
			}
			servicegroups_[its_servicegroup]->add_service(_service, _instance, its_info);
			services_[_service][_instance] = its_info;
		} else {
			host_->on_error(error_code_e::PORT_CONFIGURATION_MISSING);
		}
	} else {
		host_->on_error(error_code_e::CONFIGURATION_MISSING);
	}

	return its_info;
}

std::shared_ptr< endpoint > routing_manager_impl::create_client_endpoint(
		const boost::asio::ip::address &_address, uint16_t _port, bool _reliable) {
	std::shared_ptr< endpoint > its_endpoint;
	try {
		if (_reliable) {
			its_endpoint = std::make_shared< tcp_client_endpoint_impl >(
				shared_from_this(),
				boost::asio::ip::tcp::endpoint(_address, _port),
				io_
			);

			if (configuration_->has_enabled_magic_cookies(_address.to_string(), _port)) {
				its_endpoint->enable_magic_cookies();
			}
		} else {
			its_endpoint = std::make_shared< udp_client_endpoint_impl >(
				shared_from_this(),
				boost::asio::ip::udp::endpoint(_address, _port),
				io_
			);
		}

		client_endpoints_[_address][_port][_reliable] = its_endpoint;
		its_endpoint->start();
	}
	catch (std::exception &e) {
		host_->on_error(error_code_e::CLIENT_ENDPOINT_CREATION_FAILED);
	}

	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::find_client_endpoint(
		const boost::asio::ip::address &_address, uint16_t _port, bool _reliable) {
	std::shared_ptr< endpoint > its_endpoint;
	auto found_address = client_endpoints_.find(_address);
	if (found_address != client_endpoints_.end()) {
		auto found_port = found_address->second.find(_port);
		if (found_port != found_address->second.end()) {
			auto found_endpoint = found_port->second.find(_reliable);
			if (found_endpoint != found_port->second.end()) {
				its_endpoint = found_endpoint->second;
			}
		}
	}
	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::find_or_create_client_endpoint(
		const boost::asio::ip::address &_address, uint16_t _port, bool _reliable) {
	std::shared_ptr< endpoint > its_endpoint = find_client_endpoint(_address, _port, _reliable);
	if (0 == its_endpoint) {
		its_endpoint = create_client_endpoint(_address, _port, _reliable);
	}
	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::create_server_endpoint(uint16_t _port, bool _reliable) {
	std::shared_ptr< endpoint > its_endpoint;

	try {
		boost::asio::ip::address its_address = configuration_->get_address();
		if (_reliable) {
			its_endpoint = std::make_shared< tcp_server_endpoint_impl >(
				shared_from_this(),
				boost::asio::ip::tcp::endpoint(its_address, _port),
				io_
			);
			if (configuration_->has_enabled_magic_cookies(its_address.to_string(), _port)) {
				its_endpoint->enable_magic_cookies();
			}
		} else {
			if (its_address.is_v4()) {
				its_address = boost::asio::ip::address_v4::any();
			} else {
				// TODO: how is "ANY" specified in IPv6?
			}
			its_endpoint = std::make_shared< udp_server_endpoint_impl >(
				shared_from_this(),
				boost::asio::ip::udp::endpoint(its_address, _port),
				io_
			);
		}

		server_endpoints_[_port][_reliable] = its_endpoint;
		its_endpoint->start();
	}
	catch (std::exception &e) {
		host_->on_error(error_code_e::SERVER_ENDPOINT_CREATION_FAILED);
	}

	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::find_server_endpoint(uint16_t _port, bool _reliable) {
	std::shared_ptr< endpoint > its_endpoint;
	auto found_port = server_endpoints_.find(_port);
	if (found_port != server_endpoints_.end()) {
		auto found_endpoint = found_port->second.find(_reliable);
		if (found_endpoint != found_port->second.end()) {
			its_endpoint = found_endpoint->second;
		}
	}
	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::find_or_create_server_endpoint(uint16_t _port, bool _reliable) {
	std::shared_ptr< endpoint > its_endpoint = find_server_endpoint(_port, _reliable);
	if (0 == its_endpoint) {
		its_endpoint = create_server_endpoint(_port, _reliable);
	}
	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::find_local(client_t _client) {
	std::unique_lock< std::recursive_mutex > its_lock(endpoint_mutex_);
	std::shared_ptr< endpoint > its_endpoint;
	auto found_endpoint = local_clients_.find(_client);
	if (found_endpoint != local_clients_.end()) {
		its_endpoint = found_endpoint->second;
	}
	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::create_local(client_t _client) {
	std::unique_lock< std::recursive_mutex > its_lock(endpoint_mutex_);

	std::stringstream its_path;
	its_path << base_path << std::hex << _client;

	std::shared_ptr< endpoint > its_endpoint
		= std::make_shared< local_client_endpoint_impl >(
				shared_from_this(),
				boost::asio::local::stream_protocol::endpoint(its_path.str()),
				io_);
	local_clients_[_client] = its_endpoint;
	its_endpoint->start();
	return its_endpoint;
}

std::shared_ptr< endpoint > routing_manager_impl::find_or_create_local(client_t _client) {
	std::shared_ptr< endpoint > its_endpoint(find_local(_client));
	if (!its_endpoint) {
		its_endpoint = create_local(_client);
	}
	return its_endpoint;
}

void routing_manager_impl::remove_local(client_t _client) {
	std::unique_lock< std::recursive_mutex > its_lock(endpoint_mutex_);
	std::shared_ptr< endpoint > its_endpoint = find_local(_client);
	its_endpoint->stop();
	local_clients_.erase(_client);
}

std::shared_ptr< endpoint > routing_manager_impl::find_local(service_t _service, instance_t _instance) {
	return find_local(find_local_client(_service, _instance));
}

client_t routing_manager_impl::find_local_client(service_t _service, instance_t _instance) {
	client_t its_client(0);
	auto found_service = local_services_.find(_service);
	if (found_service != local_services_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			its_client = found_instance->second;
		}
	}
	return its_client;
}

instance_t routing_manager_impl::find_instance(service_t _service, endpoint * _endpoint) {
	instance_t its_instance(0xFFFF);
	auto found_service = service_instances_.find(_service);
	if (found_service != service_instances_.end()) {
		auto found_endpoint = found_service->second.find(_endpoint);
		if (found_endpoint != found_service->second.end()) {
			its_instance = found_endpoint->second;
		}
	}
	return its_instance;
}

std::shared_ptr< endpoint > routing_manager_impl::find_remote_client(
		service_t _service, instance_t _instance, bool _reliable) {
	std::shared_ptr< endpoint > its_endpoint;
	auto found_service = remote_services_.find(_service);
	if (found_service != remote_services_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			auto found_reliability = found_instance->second.find(_reliable);
			if (found_reliability != found_instance->second.end()) {
				its_endpoint = found_reliability->second;
			}
		}
	}
	return its_endpoint;
}

void routing_manager_impl::add_routing_info(
		service_t _service, instance_t _instance,
		major_version_t _major, minor_version_t _minor,	ttl_t _ttl,
		const boost::asio::ip::address &_address, uint16_t _port, bool _reliable) {
	std::shared_ptr< serviceinfo > its_info(find_service(_service, _instance));
	if (!its_info)
		its_info = create_service(_service, _instance, _major, _minor, _ttl);

	std::shared_ptr< endpoint > its_endpoint(create_client_endpoint(_address, _port, _reliable));
	its_info->set_endpoint(its_endpoint, _reliable);
	remote_services_[_service][_instance][_reliable] = its_endpoint;
	service_instances_[_service][its_endpoint.get()] = _instance;
	services_[_service][_instance] = its_info;
	stub_->on_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance);
	host_->on_availability(_service, _instance, true);
}

void routing_manager_impl::del_routing_info(
		service_t _service, instance_t _instance, bool _reliable) {
	std::shared_ptr< serviceinfo > its_info(find_service(_service, _instance));
	if (its_info) {
		std::shared_ptr< endpoint > its_empty_endpoint;
		host_->on_availability(_service, _instance, false);
		stub_->on_stop_offer_service(VSOMEIP_ROUTING_CLIENT, _service, _instance);

		std::shared_ptr< endpoint > its_endpoint = its_info->get_endpoint(_reliable);
		if (its_endpoint) {
			if (1 >= service_instances_[_service].size()) {
				service_instances_.erase(_service);
			} else {
				service_instances_[_service].erase(its_endpoint.get());
			}

			remote_services_[_service][_instance].erase(_reliable);
			auto found_endpoint = remote_services_[_service][_instance].find(!_reliable);
			if (found_endpoint == remote_services_[_service][_instance].end()) {
				remote_services_[_service].erase(_instance);
			}
			if (1 >= remote_services_[_service].size()) {
				remote_services_.erase(_service);
			}
		}

		if (!its_info->get_endpoint(!_reliable)) {
			its_info->get_group()->remove_service(_service, _instance);
			if (1 >= services_[_service].size()) {
				services_.erase(_service);
			} else {
				services_[_service].erase(_instance);
			}
		} else {
			its_info->set_endpoint(its_empty_endpoint, _reliable);
		}
	}
}

void routing_manager_impl::init_routing_info() {
	VSOMEIP_INFO << "Service Discovery disabled. Using static routing information.";
	for (auto i : configuration_->get_remote_services()) {
		std::string its_address = configuration_->get_address(i.first, i.second);
		uint16_t its_reliable = configuration_->get_reliable_port(i.first, i.second);
		uint16_t its_unreliable = configuration_->get_unreliable_port(i.first, i.second);

		if (VSOMEIP_INVALID_PORT != its_reliable) {
			add_routing_info(i.first, i.second,
					default_major, default_minor, default_ttl,
					boost::asio::ip::address::from_string(its_address), its_reliable, true);
		}

		if (VSOMEIP_INVALID_PORT != its_unreliable) {
			add_routing_info(i.first, i.second,
					default_major, default_minor, default_ttl,
					boost::asio::ip::address::from_string(its_address), its_reliable, true);
		}
	}
}

} // namespace vsomeip

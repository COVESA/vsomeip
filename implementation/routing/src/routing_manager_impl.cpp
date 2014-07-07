// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <memory>

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
	if (!configuration_->is_service_discovery_enabled()) {
		for (auto s : configuration_->get_remote_services()) {
			VSOMEIP_DEBUG << "Remote service loaded ["
					<< std::hex << s.first << "." << s.second << "]";
		}
	}

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
#if 1
	std::cout << "RM: Client [ " << std::hex << _client << "] offers service ["
			  << std::hex << _service << "." << _instance << "]" << std::endl;
#endif
	// Local route
	local_services_[_service][_instance] = _client;

	// Remote route (incoming only)
	serviceinfo *its_info = find_service(_service, _instance);
	if (its_info) {
		if (its_info->get_major() == _major && its_info->get_minor() == _minor) {
			its_info->set_ttl(_ttl);
		} else {
			host_->on_error();
		}
	} else {
		create_service(_service, _instance, _major, _minor, _ttl);
	}

	host_->on_availability(_service, _instance, true);

	if (discovery_)
		discovery_->offer_service(_service, _instance);
}

void routing_manager_impl::stop_offer_service(client_t its_client,
		service_t _service, instance_t _instance) {
	auto found_service = services_.find(_service);
	if (found_service != services_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			found_service->second.erase(found_instance);
			if (0 == found_service->second.size())
				services_.erase(found_service);
		}
	}

	host_->on_availability(_service, _instance, false);

	if (discovery_)
		discovery_->stop_offer_service(_service, _instance);
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

	serviceinfo *its_info = find_service(_service, _instance);
	if (nullptr != its_info) {
		if ((_major > its_info->get_major()) ||
			(_major == its_info->get_major() && _minor > its_info->get_minor()) ||
			(_ttl > its_info->get_ttl())) {
			host_->on_error(); // TODO: Service locally available, but not with the specified properties
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
	serviceinfo *its_info = find_service(_service, _instance);
	if (nullptr != its_info) {
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
	endpoint *its_target(nullptr);

	client_t its_client = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_CLIENT_POS_MIN], _data[VSOMEIP_CLIENT_POS_MAX]);
	service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
	bool is_request = utility::is_request(_data[VSOMEIP_MESSAGE_TYPE_POS]);
#if 1
	std::cout << "Message ["
			  << std::hex << its_service << "." << _instance
			  << "] "
			  << (is_request ? "from " : "to ")
			  << "client ["
			  << std::hex << its_client
			  << "]" << std::endl;
#endif
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
				(find_client(its_service, _instance) == host_->get_client() && is_request)) {
				on_message(_data, _size, _instance);
			} else {
				VSOMEIP_ERROR << "Must route to other device. Not implemented yet.";
			}
		}
	}
}

void routing_manager_impl::set(client_t its_client,
		service_t _service, instance_t _instance,
		event_t _event, const std::vector< byte_t > &_value) {
}

void routing_manager_impl::on_message(const byte_t *_data, length_t _size, endpoint *_receiver) {
	if (_size >= VSOMEIP_SOMEIP_HEADER_SIZE) {
		service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
		instance_t its_instance = find_instance(its_service, _receiver);
		if (its_instance != VSOMEIP_ANY_INSTANCE) {
			on_message(_data, _size, its_instance);
		}
	} else {
		//send_error(); // TODO: send error "malformed message"
	}
}

void routing_manager_impl::on_message(const byte_t *_data, length_t _size, instance_t _instance) {
#if 0
	std::cout << "rmi::on_message: ";
	for (int i = 0; i < _size; ++i)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
	std::cout << std::endl;
#endif

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

const std::map< std::string, std::shared_ptr< servicegroup > > & routing_manager_impl::get_servicegroups() const {
	return servicegroups_;
}

std::shared_ptr< configuration > routing_manager_impl::get_configuration() const {
	return host_->get_configuration();
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////
serviceinfo * routing_manager_impl::find_service(
					service_t _service, instance_t _instance) {
	serviceinfo *its_info = 0;
	auto found_service = services_.find(_service);
	if (found_service != services_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			its_info = found_instance->second.get();
		}
	}
	return its_info;
}

void routing_manager_impl::create_service(
					service_t _service, instance_t _instance,
					major_version_t _major, minor_version_t _minor, ttl_t _ttl) {

	if (configuration_) {
		std::shared_ptr< serviceinfo > its_info(std::make_shared< serviceinfo >(_major, _minor, _ttl));

		uint16_t its_reliable_port = configuration_->get_reliable_port(_service, _instance);
		uint16_t its_unreliable_port = configuration_->get_unreliable_port(_service, _instance);

		if (its_reliable_port != VSOMEIP_ILLEGAL_PORT) {
			std::shared_ptr< endpoint > its_reliable_endpoint(
				find_or_create_service_endpoint(its_reliable_port, true));
			its_info->set_reliable_endpoint(its_reliable_endpoint);

			// TODO: put this in a method and check whether an assignment already exists!
			service_instances_[_service][its_reliable_endpoint.get()] = _instance;
		}

		if (its_unreliable_port != VSOMEIP_ILLEGAL_PORT) {
			std::shared_ptr< endpoint > its_unreliable_endpoint(
				find_or_create_service_endpoint(its_unreliable_port, false));
			its_info->set_unreliable_endpoint(its_unreliable_endpoint);

			service_instances_[_service][its_unreliable_endpoint.get()] = _instance;
		}

		if (VSOMEIP_ILLEGAL_PORT != its_reliable_port || VSOMEIP_ILLEGAL_PORT != its_unreliable_port) {
			std::string its_servicegroup = configuration_->get_group(_service, _instance);
			auto found_servicegroup = servicegroups_.find(its_servicegroup);
			if (found_servicegroup == servicegroups_.end()) {
				servicegroups_[its_servicegroup] = std::make_shared< servicegroup >(its_servicegroup);
			}
			servicegroups_[its_servicegroup]->add_service(its_info);
			services_[_service][_instance] = its_info;
		} else {
			host_->on_error(); // TODO: Define configuration error "No valid port for service!"
		}
	} else {
		host_->on_error(); // TODO: Define configuration error!
	}
}

endpoint * routing_manager_impl::find_service_endpoint(uint16_t _port, bool _reliable) {
	endpoint *its_endpoint(0);

	auto found_port = service_endpoints_.find(_port);
	if (found_port != service_endpoints_.end()) {
		auto found_endpoint = found_port->second.find(_reliable);
		if (found_endpoint != found_port->second.end()) {
			its_endpoint = found_endpoint->second.get();
		}
	}

	return its_endpoint;
}

endpoint * routing_manager_impl::create_service_endpoint(uint16_t _port, bool _reliable) {
	endpoint *its_endpoint(0);

	try {
		boost::asio::ip::address its_address = configuration_->get_address();
		if (_reliable) {
			its_endpoint = new tcp_server_endpoint_impl(
				shared_from_this(),
				boost::asio::ip::tcp::endpoint(its_address, _port),
				io_
			);
		} else {
			its_endpoint = new udp_server_endpoint_impl(
				shared_from_this(),
				boost::asio::ip::udp::endpoint(its_address, _port),
				io_
			);
		}
	}
	catch (std::exception &e) {
		host_->on_error(); // Define error for "Server endpoint could not be created. Reason: ...
	}

	return its_endpoint;
}

endpoint * routing_manager_impl::find_or_create_service_endpoint(uint16_t _port, bool _reliable) {
	endpoint *its_endpoint = find_service_endpoint(_port, _reliable);
	if (0 == its_endpoint) {
		its_endpoint = create_service_endpoint(_port, _reliable);
	}
	return its_endpoint;
}

endpoint * routing_manager_impl::find_local(client_t _client) {
	std::unique_lock< std::recursive_mutex > its_lock(endpoint_mutex_);
	endpoint *its_endpoint(0);
	auto found_endpoint = clients_.find(_client);
	if (found_endpoint != clients_.end()) {
		its_endpoint = found_endpoint->second.get();
	}
	return its_endpoint;
}

endpoint * routing_manager_impl::create_local(client_t _client) {
	std::unique_lock< std::recursive_mutex > its_lock(endpoint_mutex_);

	std::stringstream its_path;
	its_path << VSOMEIP_BASE_PATH << std::hex << _client;

	std::shared_ptr< endpoint > its_endpoint
		= std::make_shared< local_client_endpoint_impl >(
				shared_from_this(),
				boost::asio::local::stream_protocol::endpoint(its_path.str()),
				io_);
	clients_[_client] = its_endpoint;
	its_endpoint->start();
	return its_endpoint.get();
}

endpoint * routing_manager_impl::find_or_create_local(client_t _client) {
	endpoint *its_endpoint(find_local(_client));
	if (0 == its_endpoint) {
		its_endpoint = create_local(_client);
	}
	return its_endpoint;
}

void routing_manager_impl::remove_local(client_t _client) {
	std::unique_lock< std::recursive_mutex > its_lock(endpoint_mutex_);
	endpoint *its_endpoint = find_local(_client);
	its_endpoint->stop();
	clients_.erase(_client);
}

endpoint * routing_manager_impl::find_local(service_t _service, instance_t _instance) {
	return find_local(find_client(_service, _instance));
}

client_t routing_manager_impl::find_client(service_t _service, instance_t _instance) {
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

} // namespace vsomeip

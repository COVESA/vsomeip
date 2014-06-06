//
// managing_proxy_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/endpoint.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/client.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/managing_proxy_impl.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/serializer.hpp>
#include <vsomeip_internal/service.hpp>
#include <vsomeip_internal/tcp_client_impl.hpp>
#include <vsomeip_internal/tcp_service_impl.hpp>
#include <vsomeip_internal/udp_client_impl.hpp>
#include <vsomeip_internal/udp_service_impl.hpp>

namespace vsomeip {

managing_proxy_impl::managing_proxy_impl(application_base_impl &_owner)
	: proxy_base_impl(_owner),
	  log_user(_owner) {
}

managing_proxy_impl::~managing_proxy_impl() {
}

void managing_proxy_impl::init() {
}

void managing_proxy_impl::start() {
}

void managing_proxy_impl::stop() {
}

bool managing_proxy_impl::request_service(
			service_id _service, instance_id _instance,
			const endpoint *_location) {

	client * its_client = 0;

	if (0 != _location) {
		bool is_reliable = (ip_protocol::TCP == _location->get_protocol());

		its_client = find_or_create_client(_location);
		if (its_client) {
			client_locations_[_service][_instance][is_reliable] = _location;
			client_instances_[_location][_service] = _instance;

			its_client->open_filter(_service);
		}
	} else {
		VSOMEIP_DEBUG
			<< "Specification of communication endpoint is missing.";
	}

	return (0 != its_client);
}

bool managing_proxy_impl::release_service(
			service_id _service, instance_id _instance) {

	client_locations_[_service].erase(_instance);
	if (0 == client_locations_[_service].size())
		client_locations_.erase(_service);

	const endpoint *its_reliable = find_service_location(_service, _instance, true);
	client_instances_[its_reliable].erase(_service);
	if (0 == client_instances_[its_reliable].size())
		client_instances_.erase(its_reliable);

	if (its_reliable) {
		client *its_client = find_client(its_reliable);
		if (its_client) {
			its_client->close_filter(_service);
		}
	}

	const endpoint *its_unreliable = find_service_location(_service, _instance, false);
	client_instances_[its_unreliable].erase(_service);
	if (0 == client_instances_[its_unreliable].size())
		client_instances_.erase(its_unreliable);

	if (its_unreliable) {
		client *its_client = find_client(its_unreliable);
		if (its_client) {
			its_client->close_filter(_service);
		}
	}

	return true;
}

bool managing_proxy_impl::provide_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	service *its_service = 0;

	if (_location) {
		bool is_reliable = (ip_protocol::TCP == _location->get_protocol());

		its_service = find_or_create_service(_location);
		service_locations_[_service][_instance][is_reliable] = _location;
		service_instances_[_location][_service] = _instance;

		if (its_service) {
			its_service->open_filter(_service);
		}
	}

	return (0 != its_service);
}

bool managing_proxy_impl::withdraw_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	if (0 == _location) {
		service_locations_[_service].erase(_instance);
		if (0 == service_locations_[_service].size())
			service_locations_.erase(_service);

		const endpoint *its_reliable = find_service_location(_service, _instance, true);
		if (0 != its_reliable) {
			service_instances_[its_reliable].erase(_service);
			if (0 == service_instances_.size())
				service_instances_.erase(its_reliable);

			service *its_service = find_service(its_reliable);
			if (its_service) {
				its_service->close_filter(_service);
			}
		}

		const endpoint *its_unreliable = find_service_location(_service, _instance, false);
		if (0 != its_unreliable) {
			service_instances_[its_unreliable].erase(_service);
			if (0 == service_instances_.size())
				service_instances_.erase(its_unreliable);

			service *its_service = find_service(its_unreliable);
			if (its_service) {
				its_service->close_filter(_service);
			}

		}
	} else {
		service_instances_[_location].erase(_service);
		if (0 == service_instances_[_location].size())
			service_instances_.erase(_location);

		service *its_service = find_service(_location);
		if (its_service) {
			its_service->close_filter(_service);
		}
	}
	return true;
}

bool managing_proxy_impl::start_service(
			service_id _service, instance_id _instance) {
	bool is_started(false);
	const endpoint *its_location = find_service_location(_service, _instance, true);
	if (0 == its_location) {
		its_location = find_service_location(_service, _instance, false);
	}

	if (0 != its_location) {
		service *its_service = find_service(its_location);
		if (its_service) {
			its_service->start();
			is_started = true;
		}
	}
	return is_started;
}

bool managing_proxy_impl::stop_service(
			service_id _service, instance_id _instance) {
	bool is_stopped(false);
	const endpoint *its_location = find_service_location(_service, _instance, true);
	if (0 == its_location) {
		its_location = find_service_location(_service, _instance, false);
	}

	if (0 != its_location) {
		service *its_service = find_service(its_location);
		if (its_service) {
			its_service->stop();
			is_stopped = true;
		}
	}
	return is_stopped;
}

bool managing_proxy_impl::send(message_base *_message, bool _reliable, bool _flush) {
	bool is_sent = false;

	if (0 != _message) {
		message_type_enum message_type = _message->get_message_type();
		if (message_type < message_type_enum::NOTIFICATION) {
			_message->set_client_id(owner_.get_id());
		}

		boost::shared_ptr< serializer > its_serializer(owner_.get_serializer());
		if (its_serializer->serialize(_message)) {
			if (message_type < message_type_enum::NOTIFICATION) {
				const endpoint *target = find_client_location(
											_message->get_service_id(),
											_message->get_instance_id(),
											_reliable
										 );

				if (0 != target) {
					client * the_client = find_client(target);
					if (the_client) {
						is_sent = the_client->send(
							its_serializer->get_data(),
							its_serializer->get_size(),
							_flush
						);
					}
				} else {
					VSOMEIP_ERROR << "Cannot determine endpoint for service!";
				}
			} else {
				const endpoint *source = find_service_location(
											_message->get_service_id(),
											_message->get_instance_id(),
											_reliable
								   	     );

				service * the_service = find_service(source);
				if (the_service) {
					is_sent = the_service->send(
						its_serializer->get_data(),
						its_serializer->get_size(),
						_message->get_target(),
						_flush
					);
				} else {
					VSOMEIP_DEBUG << "Attempt to send using unknown service object.";
				}
			}
			its_serializer->reset();
		} else {
			VSOMEIP_ERROR << "SOME/IP message serialization failed.";
		}
	}

	return is_sent;
}

void managing_proxy_impl::register_method(service_id _service, instance_id _instance, method_id _method) {
}

void managing_proxy_impl::deregister_method(service_id _service, instance_id _instance, method_id _method) {
}

bool managing_proxy_impl::provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) {
	return false;
}

bool managing_proxy_impl::withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) {
	return false;
}

bool managing_proxy_impl::add_field(service_id _service, instance_id _instance, eventgroup_id _eventgroup, field *_field) {
	return false;
}

bool managing_proxy_impl::remove_field(service_id _service, instance_id _instance, eventgroup_id _eventgroup, field *_field) {
	return false;
}

bool managing_proxy_impl::request_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) {
	return false;
}

bool managing_proxy_impl::release_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) {
	return false;
}

bool managing_proxy_impl::enable_magic_cookies(	service_id _service, instance_id _instance) {
	return false;
}

bool managing_proxy_impl::disable_magic_cookies(service_id _service, instance_id _instance) {
	return false;
}

boost::asio::io_service & managing_proxy_impl::get_service() {
	return owner_.get_sender_service();
}

const endpoint * managing_proxy_impl::find_client_location(
					service_id _service, instance_id _instance, bool _is_reliable) const {

	const endpoint *the_location = 0;
	auto found_client = client_locations_.find(_service);
	if (found_client != client_locations_.end()) {
		auto found_instance = found_client->second.find(_instance);
		if (found_instance != found_client->second.end()) {
			auto found_reliable = found_instance->second.find(_is_reliable);
			if (found_reliable != found_instance->second.end()) {
				the_location = found_reliable->second;
			}
		}
	}
	return the_location;
}

const endpoint * managing_proxy_impl::find_service_location(
					service_id _service, instance_id _instance, bool _is_reliable) const {

	const endpoint *the_location = 0;
	auto found_client = service_locations_.find(_service);
	if (found_client != service_locations_.end()) {
		auto found_instance = found_client->second.find(_instance);
		if (found_instance != found_client->second.end()) {
			auto found_reliable = found_instance->second.find(_is_reliable);
			if (found_reliable != found_instance->second.end()) {
				the_location = found_reliable->second;
			}
		}
	}
	return the_location;
}

client * managing_proxy_impl::find_client(const endpoint *_location) const {
	auto found = managed_clients_.find(_location);
	if (found == managed_clients_.end())
		return 0;

	return found->second;
}

client * managing_proxy_impl::create_client(const endpoint *_location) {
	client *the_client = 0;
	if (0 != _location) {
		if (_location->get_protocol() == ip_protocol::TCP) {
			the_client = new tcp_client_impl(this, _location);
		} else if (_location->get_protocol() == ip_protocol::UDP) {
			the_client = new udp_client_impl(this, _location);
		} else {
			VSOMEIP_ERROR << "Unsupported/unknown transport protocol";
		}

		if (the_client) {
			managed_clients_[_location] = the_client;
			the_client->start();
		}
	}
	return the_client;
}

client * managing_proxy_impl::find_or_create_client(const endpoint *_location) {
	client *the_client = find_client(_location);
	if (0 == the_client)
		the_client = create_client(_location);
	return the_client;
}

service * managing_proxy_impl::find_service(const endpoint *_location) const {
	auto found = managed_services_.find(_location);
	if (found == managed_services_.end())
		return 0;

	return found->second;
}

service * managing_proxy_impl::create_service(const endpoint *_location) {
	service *the_service = 0;
	if (0 != _location) {
		if (_location->get_protocol() == ip_protocol::UDP) {
			the_service = new udp_service_impl(this, _location);
		} else if (_location->get_protocol() == ip_protocol::TCP) {
			the_service = new tcp_service_impl(this, _location);
		} else {
			VSOMEIP_ERROR << "Unsupported/unknown transport protocol";
		}

		if (the_service) {
			managed_services_[_location] = the_service;
			the_service->start();
		}
	}

	return the_service;
}

service * managing_proxy_impl::find_or_create_service(const endpoint *_location) {
	service *the_service = find_service(_location);
	if (0 == the_service)
		the_service = create_service(_location);
	return the_service;
}

instance_id managing_proxy_impl::find_instance(
				const endpoint *_location, service_id _service, message_type_enum _message_type) const {

	instance_id its_instance = 0;

	if (_message_type < message_type_enum::RESPONSE) {
		auto find_location = service_instances_.find(_location);
		if (find_location != service_instances_.end()) {
			auto find_service = find_location->second.find(_service);
			if (find_service != find_location->second.end()) {
				its_instance = find_service->second;
			}
		}
	} else {
		auto find_location = client_instances_.find(_location);
		if (find_location != client_instances_.end()) {
			auto find_service = find_location->second.find(_service);
			if (find_service != find_location->second.end()) {
				its_instance = find_service->second;
			}
		}
	}

	return its_instance;
}

void managing_proxy_impl::receive(
		const uint8_t *_data, uint32_t _size,
		const endpoint *_source, const endpoint *_target) {

	if (_data) {
		boost::shared_ptr< deserializer > its_deserializer(owner_.get_deserializer());
		its_deserializer->set_data(_data, _size);
		boost::shared_ptr< message > its_message (its_deserializer->deserialize_message());
		its_deserializer->reset();

		if (its_message) {
			its_message->set_source(_source);

			service_id its_service = its_message->get_service_id();
			message_type_enum its_message_type = its_message->get_message_type();

			instance_id its_instance = find_instance(_target, its_service, its_message_type);
			its_message->set_instance_id(its_instance);

			owner_.handle_message(its_message.get());
		} else {
			VSOMEIP_ERROR << "Message deserialization error!";
		}
	}
}

} // namespace vsomeip


//
// managing_application_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <algorithm>
#include <iomanip>
#include <vector>

#include <boost/asio/placeholders.hpp>
#include <boost/atomic.hpp>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/client.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/deserializer_impl.hpp>
#include <vsomeip_internal/serializer_impl.hpp>
#include <vsomeip_internal/service.hpp>
#include <vsomeip_internal/managing_application_impl.hpp>
#include <vsomeip_internal/tcp_client_impl.hpp>
#include <vsomeip_internal/tcp_service_impl.hpp>
#include <vsomeip_internal/udp_client_impl.hpp>
#include <vsomeip_internal/udp_service_impl.hpp>

namespace vsomeip {

///////////////////////////////////////////////////////////////////////////////
// Object members
///////////////////////////////////////////////////////////////////////////////
managing_application_impl::managing_application_impl(const std::string &_name)
	: log_owner(_name),
	  serializer_(new serializer_impl),
	  deserializer_(new deserializer_impl) {

	serializer_->create_data(VSOMEIP_MAX_TCP_MESSAGE_SIZE + VSOMEIP_PROTOCOL_OVERHEAD);
}

managing_application_impl::~managing_application_impl() {
}

void managing_application_impl::init(int _options_count, char **_options) {
}

void managing_application_impl::start() {
	configuration *vsomeip_configuration = configuration::request(name_);
}

void managing_application_impl::stop() {
}

std::size_t managing_application_impl::poll_one() {
	return service_.poll_one();
}

std::size_t managing_application_impl::poll() {
	return service_.poll();
}

std::size_t managing_application_impl::run() {
	return service_.run();
}

bool managing_application_impl::request_service(
			service_id _service, instance_id _instance,
			const endpoint *_location) {

	client * the_client = find_or_create_client(_location);
	client_locations_[_service][_instance] = _location;
	if (the_client) {
		the_client->open_filter(_service);
	}
	return (0 != the_client);
}

bool managing_application_impl::release_service(
			service_id _service, instance_id _instance) {

	bool is_released(false);
	const endpoint *the_location = find_service_location(_service, _instance);
	client_locations_[_service].erase(_instance);
	if (0 == client_locations_[_service].size())
		client_locations_.erase(_service);
	if (the_location) {
		client *the_client = find_client(the_location);
		if (the_client) {
			the_client->close_filter(_service);
		}
	}
	return is_released;
}

bool managing_application_impl::provide_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	service *the_service = find_or_create_service(_location);
	service_locations_[_service][_instance] = _location;
	if (the_service) {
		the_service->open_filter(_service);
	}
	return (0 != the_service);
}

bool managing_application_impl::withdraw_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	service *the_service = find_service(_location);
	if (the_service) {
		the_service->close_filter(_service);
	}
	return (0 != the_service);
}

bool managing_application_impl::start_service(
			service_id _service, instance_id _instance) {
	bool is_started(false);
	const endpoint *the_location = find_service_location(_service, _instance);
	if (the_location) {
		service *the_service = find_service(the_location);
		if (the_service) {
			the_service->start();
			is_started = true;
		}
	}
	return is_started;
}

bool managing_application_impl::stop_service(
			service_id _service, instance_id _instance) {
	bool is_stopped(false);
	const endpoint *the_location = find_service_location(_service, _instance);
	if (the_location) {
		service *the_service = find_service(the_location);
		if (the_service) {
			the_service->stop();
			is_stopped = true;
		}
	}
	return is_stopped;
}

bool managing_application_impl::send(message_base *_message, bool _flush) {
	bool is_sent = false;

	if (0 != _message) {
		if (serializer_->serialize(_message)) {
			message_type_enum message_type = _message->get_message_type();
			if (message_type < message_type_enum::RESPONSE) {
				client * the_client = find_client(_message->get_target());
				if (the_client) {
					is_sent = the_client->send(
						serializer_->get_data(),
						serializer_->get_size(),
						_flush
					);
				}
			} else {
				service * the_service = find_service(_message->get_source());
				if (the_service) {
					is_sent = the_service->send(
						serializer_->get_data(),
						serializer_->get_size(),
						_message->get_target(),
						_flush
					);
				} else {
					VSOMEIP_DEBUG << "Attempt to send using unknown service object.";
				}
			}
			serializer_->reset();
		} else {
			VSOMEIP_ERROR << "SOME/IP message serialization failed.";
		}
	}

	return is_sent;
}

void managing_application_impl::register_cbk(
		service_id _service, method_id _method, receive_cbk_t _cbk) {

	receive_cbks_[_service][_method].insert(_cbk);
}

void managing_application_impl::deregister_cbk(
		service_id _service, method_id _method, receive_cbk_t _cbk) {

	service_filter_map::iterator found_service = receive_cbks_.find(_service);
	if (found_service != receive_cbks_.end()) {
		method_filter_map::iterator found_method = found_service->second.find(_method);
		if (found_method != found_service->second.end()) {
			found_method->second.erase(_cbk);
			if (0 == found_method->second.size())
				found_service->second.erase(found_method);
			if (0 == found_service->second.size())
				receive_cbks_.erase(found_service);
		}
	}

	receive_cbks_[_service][_method].erase(_cbk);
}

void managing_application_impl::enable_magic_cookies(
			service_id _service, instance_id _instance) {
}

void managing_application_impl::disable_magic_cookies(
			service_id _service, instance_id _instance) {
}

boost::asio::io_service & managing_application_impl::get_io_service() {
	return service_;
}

void managing_application_impl::receive(
		const uint8_t *_data, uint32_t _size,
		const endpoint *_source, const endpoint *_target) {
	if (_data) {
		deserializer_->set_data(_data, _size);
		boost::shared_ptr< message_base > the_message (deserializer_->deserialize_message());
		deserializer_->reset();

		if (the_message) {
			the_message->set_source(_source);
			the_message->set_target(_target);
			service_id service = the_message->get_service_id();
			service_filter_map::iterator found_service = receive_cbks_.find(service);
			if (found_service != receive_cbks_.end()) {
				method_id method = the_message->get_method_id();
				method_filter_map::iterator found_method = found_service->second.find(method);
				if (found_method != found_service->second.end()) {
					std::for_each(
						found_method->second.begin(),
						found_method->second.end(),
						[the_message](receive_cbk_t _func) { _func(the_message.get()); }
					);
				} else {
					message_type_enum message_type = the_message->get_message_type();
					if (message_type < message_type_enum::RESPONSE) {
						send_error_message(the_message.get(), return_code_enum::UNKNOWN_METHOD);
					}
				}
			} else {
				message_type_enum message_type = the_message->get_message_type();
				if (message_type < message_type_enum::RESPONSE) {
					send_error_message(the_message.get(), return_code_enum::UNKNOWN_SERVICE);
				}
			}
		} else {
			send_error_message(the_message.get(), return_code_enum::MALFORMED_MESSAGE);
		}
	}
}

const endpoint * managing_application_impl::find_client_location(
					service_id _service, instance_id _instance) const {

	const endpoint *the_location = 0;
	auto found_client = client_locations_.find(_service);
	if (found_client != client_locations_.end()) {
		auto found_instance = found_client->second.find(_instance);
		if (found_instance != found_client->second.end()) {
			the_location = found_instance->second;
		}
	}
	return the_location;
}

const endpoint * managing_application_impl::find_service_location(
					service_id _service, instance_id _instance) const {

	const endpoint *the_location = 0;
	auto found_client = service_locations_.find(_service);
	if (found_client != service_locations_.end()) {
		auto found_instance = found_client->second.find(_instance);
		if (found_instance != found_client->second.end()) {
			the_location = found_instance->second;
		}
	}
	return the_location;
}

client * managing_application_impl::find_client(const endpoint *_location) {
	auto found = managed_clients_.find(_location);
	if (found == managed_clients_.end())
		return 0;

	return found->second;
}

client * managing_application_impl::create_client(const endpoint *_location) {
	client *the_client = 0;
	if (0 != _location) {
		if (_location->get_protocol() == ip_protocol::UDP) {
			the_client = new udp_client_impl(this, _location);
		} else if (_location->get_protocol() == ip_protocol::TCP) {
			the_client = new tcp_client_impl(this, _location);
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

client * managing_application_impl::find_or_create_client(const endpoint *_location) {
	client *the_client = find_client(_location);
	if (0 == the_client)
		the_client = create_client(_location);
	return the_client;
}

service * managing_application_impl::find_service(const endpoint *_location) {
	auto found = managed_services_.find(_location);
	if (found == managed_services_.end())
		return 0;

	return found->second;
}

service * managing_application_impl::create_service(const endpoint *_location) {
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

service * managing_application_impl::find_or_create_service(const endpoint *_location) {
	service *the_service = find_service(_location);
	if (0 == the_service)
		the_service = create_service(_location);
	return the_service;
}

void managing_application_impl::send_error_message(message_base *_request, return_code_enum _error) {
	message *response = factory::get_instance()->create_response(_request);
	response->set_return_code(_error);
	send(response, true);
}

} // namespace vsomeip

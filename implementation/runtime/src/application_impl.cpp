// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <thread>

#include <vsomeip/configuration.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/application_impl.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../message/include/serializer.hpp"
#include "../../routing/include/routing_manager_impl.hpp"
#include "../../routing/include/routing_manager_proxy.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

application_impl::application_impl(const std::string &_name)
	: name_(_name), routing_(0), signals_(host_io_, SIGINT, SIGTERM) {
}

application_impl::~application_impl() {
}

bool application_impl::init() {
	bool is_initialized(false);

	// Set default path
	std::string its_path(VSOMEIP_DEFAULT_CONFIGURATION_FILE_PATH);

	// Override with path from environment
	const char *its_env_path = getenv(VSOMEIP_ENV_CONFIGURATION_FILE_PATH);
	if (nullptr != its_env_path && utility::exists(its_env_path))
		its_path = its_env_path;

	// Override with local path
	std::string its_local_path(VSOMEIP_LOCAL_CONFIGURATION_FILE_PATH);
	if (utility::exists(its_local_path))
		its_path = its_local_path;

	configuration_.reset(configuration::get(its_path));
	VSOMEIP_INFO << "Using configuration file: " << its_path;

	if (configuration_) {
		client_ = configuration_->get_id(name_);

		// Routing
		if (name_ == configuration_->get_routing_host()) {
			routing_ = std::make_shared< routing_manager_impl >(this);
		} else {
			routing_ = std::make_shared< routing_manager_proxy >(this);
		}

		routing_->init();

		// Smallest allowed session identifier
		session_ = 0x0001;

		VSOMEIP_DEBUG << "Application(" << name_ << ", " << std::hex << client_ << ") is initialized.";

		is_initialized = true;
	}

	// Register signal handler
	std::function< void (boost::system::error_code const &, int) > its_signal_handler
			= [this] (boost::system::error_code const &_error, int _signal) {
					if (!_error) {
						switch (_signal) {
						case SIGTERM:
						case SIGINT:
							stop();
							exit(0);
							break;
						default:
							break;
						}
					}
			  };
	signals_.async_wait(its_signal_handler);

	return is_initialized;
}

void application_impl::start() {
	VSOMEIP_DEBUG << "Client [" << std::hex << client_ << "]: starting up...";
	if (routing_)
		routing_->start();

	// start the threads that process the io service queues
	std::thread its_host_thread(std::bind(&application_impl::service, this, std::ref(host_io_)));
	its_host_thread.join();
}

void application_impl::stop() {
	VSOMEIP_DEBUG << "Client [" << std::hex << client_ << "]: shutting down...";
	if (routing_)
		routing_->stop();

	host_io_.stop();
}

void application_impl::offer_service(
		service_t _service, instance_t _instance,
		major_version_t _major, minor_version_t _minor, ttl_t _ttl) {
	if (routing_)
		routing_->offer_service(client_, _service, _instance, _major, _minor, _ttl);
}

void application_impl::stop_offer_service(
		service_t _service, instance_t _instance) {
	if (routing_)
		routing_->stop_offer_service(client_, _service, _instance);
}

void application_impl::publish_eventgroup(
		service_t _service, instance_t _instance, eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl) {
	if (routing_)
		routing_->publish_eventgroup(client_, _service, _instance, _eventgroup, _major, _ttl);
}

void application_impl::stop_publish_eventgroup(
		service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
	if (routing_)
		routing_->stop_publish_eventgroup(client_, _service, _instance, _eventgroup);
}

void application_impl::request_service(
		service_t _service, instance_t _instance,
		major_version_t _major, minor_version_t _minor, ttl_t _ttl) {
	if (routing_)
		routing_->request_service(client_, _service, _instance, _major, _minor, _ttl);
}

void application_impl::release_service(
		service_t _service, instance_t _instance) {
	if (routing_)
		routing_->release_service(client_, _service, _instance);
}

void application_impl::subscribe(
		service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
	if (routing_)
		routing_->subscribe(client_, _service, _instance, _eventgroup);
}

void application_impl::unsubscribe(
		service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
	if (routing_)
		routing_->unsubscribe(client_, _service, _instance, _eventgroup);
}

bool application_impl::is_available(service_t _service, instance_t _instance) {
	return true; // TODO: ask routing manager (proxy)
}

std::shared_ptr< event > application_impl::add_event(
		service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event) {
	return routing_->add_event(client_, _service, _instance, _eventgroup, _event);
}

std::shared_ptr< event > application_impl::add_field(
		service_t _service, instance_t _instance,
		eventgroup_t _eventgroup, event_t _event, std::shared_ptr< payload > _payload) {
	return routing_->add_field(client_, _service, _instance, _eventgroup, _event, _payload);
}

void application_impl::remove_event_or_field(std::shared_ptr< event > _event) {
	routing_->remove_event_or_field(_event);
}

void application_impl::send(std::shared_ptr< message > _message, bool _flush, bool _reliable) {
	if (routing_) {
		if (utility::is_request(_message)) {
			_message->set_client(client_);
			_message->set_session(session_++);
		}
		routing_->send(client_, _message, _flush, _reliable);
	}
}

bool application_impl::register_message_handler(
		service_t _service, instance_t _instance, method_t _method, message_handler_t _handler) {
	std::cout << "Registering for message ["
			  << std::hex
			  << _service << "." << _instance << "." << _method
			  << "]" << std::endl;

	members_[_service][_instance][_method] = _handler;
	return true;
}

bool application_impl::unregister_message_handler(
		service_t _service, instance_t _instance, method_t _method) {
	bool its_result = false;

	auto found_service = members_.find(_service);
	if (found_service != members_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			auto found_method = found_instance->second.find(_method);
			if (found_method != found_instance->second.end()) {
				found_instance->second.erase(_method);
				its_result = true;
			}
		}
	}

	return its_result;
}

bool application_impl::register_availability_handler(
		service_t _service, instance_t _instance, availability_handler_t _handler) {
	availability_[_service][_instance] = _handler;
	return true;
}

bool application_impl::unregister_availability_handler(
		service_t _service, instance_t _instance) {
	bool its_result = false;

	auto found_service = availability_.find(_service);
	if (found_service != availability_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			found_service->second.erase(_instance);
			its_result = true;
		}
	}
	return its_result;
}

// Interface "routing_manager_host"
const std::string & application_impl::get_name() const {
	return name_;
}

client_t application_impl::get_client() const {
	return client_;
}

std::shared_ptr< configuration > application_impl::get_configuration() const {
	return configuration_;
}

boost::asio::io_service & application_impl::get_io() {
	return host_io_;
}

void application_impl::on_availability(service_t _service, instance_t _instance, bool _is_available) const {
	auto found_service = availability_.find(_service);
	if (found_service != availability_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			found_instance->second(_service, _instance, _is_available);
		}
	}
}

void application_impl::on_message(std::shared_ptr< message > _message) {
	service_t its_service = _message->get_service();
	instance_t its_instance = _message->get_instance();
	method_t its_method = _message->get_method();

	// find list of handlers
	auto found_service = members_.find(its_service);
	if (found_service == members_.end()) {
		found_service = members_.find(VSOMEIP_ANY_SERVICE);
	}
	if (found_service != members_.end()) {
		auto found_instance = found_service->second.find(its_instance);
		if (found_instance == found_service->second.end()) {
			found_instance = found_service->second.find(VSOMEIP_ANY_SERVICE);
		}
		if (found_instance != found_service->second.end()) {
			auto found_method = found_instance->second.find(its_method);
			if (found_method == found_instance->second.end()) {
				found_method = found_instance->second.find(VSOMEIP_ANY_METHOD);
			}

			if (found_method != found_instance->second.end()) {
				found_method->second(_message);
			}
		}
	}
}

void application_impl::on_error() {
	std::cerr << "ERROR" << std::endl;
}

// Interface "service_discovery_host"
routing_manager * application_impl::get_routing_manager() const {
	return routing_.get();
}

// Internal
void application_impl::service(boost::asio::io_service &_io) {
	_io.run();
	VSOMEIP_ERROR << "Service stopped running..." << std::endl;
}

} // namespace vsomeip

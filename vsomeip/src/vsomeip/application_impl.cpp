//
// application_base_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/managed_proxy_impl.hpp>
#include <vsomeip_internal/managing_proxy_impl.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/serializer.hpp>
#include <vsomeip_internal/supervised_managing_proxy_impl.hpp>
#include <vsomeip_internal/utility.hpp>
#include <vsomeip/sd/factory.hpp>

namespace vsomeip {

application_impl::application_impl(const std::string &_name)
	: application_base_impl(_name),
	  log_owner(_name),
	  proxy_owner(_name),
	  owner_base(_name),
	  sender_work_(sender_service_),
	  receiver_work_(receiver_service_),
	  id_(0),
	  serializer_(new serializer),
	  deserializer_(new deserializer),
	  is_service_discovery_enabled_(false),
	  is_managing_(false) {

	serializer_->create_data(VSOMEIP_MAX_TCP_MESSAGE_SIZE);
}

application_impl::~application_impl() {
}

client_id application_impl::get_id() const {
	return id_;
}

void application_impl::set_id(client_id _id) {
	id_ = _id;
}

std::string application_impl::get_name() const {
	return name_;
}

void application_impl::set_name(const std::string &_name) {
	name_ = _name;
}

bool application_impl::is_managing() const {
	return is_managing_;
}

boost::asio::io_service & application_impl::get_sender_service() {
	return sender_service_;
}

boost::asio::io_service & application_impl::get_receiver_service() {
	return receiver_service_;
}

void application_impl::init(int _options_count, char **_options) {
	configuration::init(_options_count, _options);
	configuration * its_configuration = configuration::request(name_);

	configure_logging(
		its_configuration->use_console_logger(),
		its_configuration->use_file_logger(),
		its_configuration->use_dlt_logger()
	);

	set_channel(name_);
	set_loglevel(its_configuration->get_loglevel());

	id_ = its_configuration->get_client_id();

	bool is_watchdog_enabled = its_configuration->is_watchdog_enabled();
	is_service_discovery_enabled_ = its_configuration->is_service_discovery_enabled();
	bool is_endpoint_manager_enabled = its_configuration->is_endpoint_manager_enabled();

	if (is_endpoint_manager_enabled) {
		is_managing_ = true;
		if (is_watchdog_enabled || is_service_discovery_enabled_) {
			VSOMEIP_DEBUG << "Application \"" << name_ << "\" uses supervised managing proxy.";
			proxy_.reset(new supervised_managing_proxy_impl(*this));
		} else {
			VSOMEIP_DEBUG << "Application \"" << name_ << "\" uses managing proxy.";
			proxy_.reset(new managing_proxy_impl(*this));
		}
	} else {
		is_managing_ = false;
		VSOMEIP_DEBUG << "Application \"" << name_ << "\" uses managed proxy.";
		proxy_.reset(new managed_proxy_impl(*this));
	}

	configuration::release(name_);

	proxy_->init();
}

void application_impl::start() {
	proxy_->start();

	// start processing
	receiver_thread_.reset(
		new boost::thread(
			boost::bind(
				&application_impl::service,
				this,
				boost::ref(receiver_service_)
			)
		)
	);

	sender_thread_.reset(
		new boost::thread(
			boost::bind(
				&application_impl::service,
				this,
				boost::ref(sender_service_)
			)
		)
	);

	sender_thread_->join();
	receiver_thread_->join();
}

void application_impl::stop() {
	proxy_->stop();

	receiver_service_.stop();
	sender_service_.stop();

	receiver_thread_.reset();
	sender_thread_.reset();
}

void application_impl::service(boost::asio::io_service &_service) {
	_service.run();
}

bool application_impl::provide_service(service_id _service, instance_id _instance, const endpoint *_location) {
	return proxy_->provide_service(_service, _instance, _location);
}

bool application_impl::withdraw_service(service_id _service, instance_id _instance, const endpoint *_location) {
	return proxy_->withdraw_service(_service, _instance, _location);
}

bool application_impl::start_service(service_id _service, instance_id _instance) {
	return proxy_->start_service(_service, _instance);
}

bool application_impl::stop_service(service_id _service, instance_id _instance) {
	return proxy_->stop_service(_service, _instance);
}

bool application_impl::request_service(service_id _service, instance_id _instance, const endpoint *_location) {
	return proxy_->request_service(_service, _instance, _location);
}

bool application_impl::release_service(service_id _service, instance_id _instance) {
	return proxy_->release_service(_service, _instance);
}

bool application_impl::is_service_available(service_id _service, instance_id _instance) const {
	if (is_service_discovery_enabled_) {
		bool is_available = false;
		auto found_service = availability_.find(_service);
		if (found_service != availability_.end()) {
			is_available = (found_service->second.find(_instance) != found_service->second.end());
		}
		return is_available;
	}

	return true;
}

bool application_impl::register_availability_handler(service_id _service, instance_id _instance,
		availability_handler_t _handler) {
	if (is_service_discovery_enabled_) {
		auto found_service = availability_handlers_.find(_service);
		if (found_service != availability_handlers_.end()) {
			auto found_instance = found_service->second.find(_instance);
			if (found_instance != found_service->second.end()) {
				return false;
			}
		}

		availability_handlers_[_service][_instance] = _handler;
		return true;
	}

	return false;
}

void application_impl::deregister_availability_handler(service_id _service, instance_id _instance) {
	auto found_service = availability_handlers_.find(_service);
	if (found_service != availability_handlers_.end()) {
		found_service->second.erase(_instance);
	}
}

void application_impl::handle_service_availability(service_id _service, instance_id _instance, const endpoint *_location, bool _is_available)  {
	if (_is_available) {
		VSOMEIP_DEBUG << "Service ["
					  << std::hex << std::setw(4) << std::setfill('0')
					  << _service << "." << _instance
					  << "] is available on "
					  << _location->get_address() << ":" << _location->get_port();
	} else {
		VSOMEIP_DEBUG << "Service ["
					  << std::hex << std::setw(4) << std::setfill('0')
					  << _service << "." << _instance
					  << "] is NOT available";

	}

	bool must_call_handler = false;
	auto found_service = availability_.find(_service);
	if (found_service == availability_.end()) {
		if (_is_available) {
			availability_[_service].insert(_instance);
			must_call_handler = true;
		}
	} else {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance == found_service->second.end()) {
			if (_is_available) {
				found_service->second.insert(_instance);
				must_call_handler = true;
			}
		} else {
			if (!_is_available) {
				found_service->second.erase(_instance);
				must_call_handler = true;
			}
		}
	}

	if (must_call_handler) {
		auto found_service_availability_handlers = availability_handlers_.find(_service);
		if (found_service_availability_handlers != availability_handlers_.end()) {
			auto found_service_instance_availability_handler = found_service_availability_handlers->second.find(_instance);
			if (found_service_instance_availability_handler != found_service_availability_handlers->second.end()) {
				found_service_instance_availability_handler->second(_service, _instance, _is_available);
			}
		}
	}
}

bool application_impl::provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) {
	return proxy_->provide_eventgroup(_service, _instance, _eventgroup, _location);
}

bool application_impl::withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) {
	return proxy_->withdraw_eventgroup(_service, _instance, _eventgroup, _location);
}

bool application_impl::add_to_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, event_id _event) {
	return proxy_->add_to_eventgroup(_service, _instance, _eventgroup, _event);
}

bool application_impl::add_to_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, message *_field) {
	return proxy_->add_to_eventgroup(_service, _instance, _eventgroup, _field);
}

bool application_impl::remove_from_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, event_id _event) {
	return proxy_->remove_from_eventgroup(_service, _instance, _eventgroup, _event);
}

bool application_impl::request_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) {
	return proxy_->request_eventgroup(_service, _instance, _eventgroup);
}

bool application_impl::release_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) {
	return proxy_->release_eventgroup(_service, _instance, _eventgroup);
}

bool application_impl::send(message *_message, bool _flush) {
	return proxy_->send(_message, _flush);
}

bool application_impl::enable_magic_cookies(service_id _service, instance_id _instance) {
	return proxy_->enable_magic_cookies(_service, _instance);
}

bool application_impl::disable_magic_cookies(service_id _service, instance_id _instance) {
	return proxy_->disable_magic_cookies(_service, _instance);
}

bool application_impl::register_message_handler(
		service_id _service, instance_id _instance, method_id _method,
		message_handler_t _handler) {

	auto found_service = message_handlers_.find(_service);
	if (found_service != message_handlers_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			auto found_method = found_instance->second.find(_method);
			if (found_method != found_instance->second.end()) {
				return false;
			}
		}
	}

	message_handlers_[_service][_instance][_method] = _handler;
	proxy_->register_method(_service, _instance, _method);
	return true;
}

void application_impl::deregister_message_handler(
		service_id _service, instance_id _instance, method_id _method) {
	auto found_service = message_handlers_.find(_service);
	if (found_service != message_handlers_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			auto found_method = found_service->second.find(_method);
			if (found_method != found_service->second.end()) {
				found_method->second.erase(_method);
				proxy_->deregister_method(_service, _instance, _method);
			}
		}
	}
}

void application_impl::handle_message(const message *_message) {
	auto found_service = message_handlers_.find(_message->get_service_id());
	if (found_service != message_handlers_.end()) {
		auto found_instance = found_service->second.find(_message->get_instance_id());
		if (found_instance != found_service->second.end()) {
			auto found_method = found_instance->second.find(_message->get_method_id());
			if (found_method == found_instance->second.end()) {
				found_method = found_instance->second.find(VSOMEIP_ANY_METHOD);
			}

			if (found_method != found_instance->second.end()) {
				found_method->second(_message);
			} else {
				if (_message->get_message_type() < message_type_enum::RESPONSE) {
					VSOMEIP_ERROR << "Unknown method " << std::hex << (int)_message->get_method_id();
					send_error_message(_message, return_code_enum::UNKNOWN_METHOD);
				}
			}
		} else {
			// It would be nice to be able to send "UNKNOWN SERVICE INSTANCE" here, but
			// SOME/IP does not define this error...
			if (_message->get_message_type() < message_type_enum::RESPONSE) {
				VSOMEIP_ERROR << "Unknown instance " << std::hex << (int)_message->get_instance_id();
				send_error_message(_message, return_code_enum::UNKNOWN_SERVICE);
			}
		}
	} else {
		if (_message->get_message_type() < message_type_enum::RESPONSE) {
			VSOMEIP_ERROR << "Unknown service " << std::hex << (int)_message->get_service_id();
			send_error_message(_message, return_code_enum::UNKNOWN_SERVICE);
		}
	}
}

void application_impl::send_error_message(const message *_request, return_code_enum _error) {
	boost::shared_ptr< message > response (vsomeip::factory::get_instance()->create_response(_request));
	response->set_return_code(_error);
	send(response.get());
}

void application_impl::catch_up_registrations() {
}

boost::shared_ptr< serializer > & application_impl::get_serializer() {
	return serializer_;
}

boost::shared_ptr< deserializer > & application_impl::get_deserializer() {
	return deserializer_;
}

} // namespace vsomeip

//
// application_base_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <dlfcn.h>

#include <vsomeip_internal/application_base_impl.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/log_macros.hpp>

#include <vsomeip/sd/factory.hpp>
#include <vsomeip_internal/sd/client_manager.hpp>

namespace vsomeip {

application_base_impl::application_base_impl(const std::string &_name, boost::asio::io_service &_service)
	: log_owner(_name),
	  client_manager_(0),
	  service_(_service) {

}

application_base_impl::~application_base_impl() {

}

std::string application_base_impl::get_name() const {
	return name_;
}

void application_base_impl::init(int _options_count, char **_options) {
	configuration::init(_options_count, _options);
	configuration *its_configuration = configuration::request(name_);

	// check for SD
	if (its_configuration->use_service_discovery()) {
		void *handle = dlopen(VSOMEIP_SD_LIBRARY, RTLD_LAZY | RTLD_GLOBAL);
		if (0 != handle) {
			sd::factory **its_factory = static_cast< sd::factory **>(dlsym(handle, VSOMEIP_SD_FACTORY_SYMBOL_STRING));
			if (0 != *its_factory) {
				client_manager_ = (*its_factory)->create_client_manager(service_);
				if (0 != client_manager_) {
					client_manager_->set_owner(this);
					std::cout << "SD Client Manager loaded for application \"" << name_ << "\"" << std::endl;
				} else {
					VSOMEIP_ERROR << "Could not create manager!";
				}
			} else {
				VSOMEIP_ERROR << "Could not find factory symbol [" << VSOMEIP_SD_FACTORY_SYMBOL_STRING << "]";
			}
		} else {
			VSOMEIP_ERROR << "Could not load SD library! Reason: " << dlerror();
		}
	} else {
		VSOMEIP_INFO << "Application \"" << name_ << "\" does not use service discovery.";
	}

	configuration::release(name_);
}

void application_base_impl::start() {

}

void application_base_impl::stop() {

}

void application_base_impl::on_provide_service(service_id _service, instance_id _instance) {

}

void application_base_impl::on_withdraw_service(service_id _service, instance_id _instance) {

}

void application_base_impl::on_start_service(service_id _service, instance_id _instance) {

}

void application_base_impl::on_stop_service(service_id _service, instance_id _instance) {

}

void application_base_impl::on_request_service(service_id _service, instance_id _instance) {

}

void application_base_impl::on_release_service(service_id _service, instance_id _instance) {

}

} // namespace vsomeip



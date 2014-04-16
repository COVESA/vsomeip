//
// client_manager_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/application.hpp>
#include <vsomeip_internal/sd/client_manager_impl.hpp>
#include <vsomeip_internal/sd/client_state_machine.hpp>

namespace vsomeip {
namespace sd {

client_manager_impl::client_manager_impl(boost::asio::io_service &_service)
	: service_(_service) {
}

client_manager_impl::~client_manager_impl() {
}

application * client_manager_impl::get_owner() const {
	return owner_;
}

void client_manager_impl::set_owner(application *_owner) {
	owner_ = _owner;
}

boost::asio::io_service & client_manager_impl::get_service() {
	return service_;
}

void client_manager_impl::init() {

}

void client_manager_impl::start() {

}

void client_manager_impl::stop() {

}

void client_manager_impl::on_provide_service(
		service_id _service, instance_id _instance) {

}

void client_manager_impl::on_withdraw_service(
		service_id _service, instance_id _instance) {

}

void client_manager_impl::on_start_service(
		service_id _service, instance_id _instance) {

}

void client_manager_impl::on_stop_service(
		service_id _service, instance_id _instance) {

}

void client_manager_impl::on_request_service(
		service_id _service, instance_id _instance) {
}

void client_manager_impl::on_release_service(
		service_id _service, instance_id _instance) {
}

bool client_manager_impl::send(message_base *_message, bool _flush) {
	if (owner_)
		return owner_->send(_message, _flush);

	return false;
}

} // namespace sd
} // namespace vsomeip

//
// service_manager_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/application.hpp>
#include <vsomeip/sd/factory.hpp>
#include <vsomeip/sd/message.hpp>
#include <vsomeip/sd/service_entry.hpp>
#include <vsomeip_internal/sd/service_manager_impl.hpp>
#include <vsomeip_internal/sd/service_state_machine.hpp>

namespace vsomeip {
namespace sd {

service_manager_impl::service_manager_impl(boost::asio::io_service &_service)
	: service_(_service), factory_(factory::get_instance()) {

}

service_manager_impl::~service_manager_impl() {
}

application * service_manager_impl::get_owner() const {
	return owner_;
}

void service_manager_impl::set_owner(application *_owner) {
	owner_ = _owner;
}

boost::asio::io_service & service_manager_impl::get_service() {
	return service_;
}

void service_manager_impl::on_provide_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance == find_service->second.end()) {
			service_state_machine::machine *its_machine = new service_state_machine::machine(this);
			find_service->second[_instance].reset(its_machine);
			its_machine->initiate();
			its_machine->process_event(ev_none());
			its_machine->process_event(ev_network_status_change(true, true));
		}
	} else {
		service_state_machine::machine *its_machine = new service_state_machine::machine(this);
		its_machine->initiate();
		its_machine->process_event(ev_none());
		its_machine->process_event(ev_network_status_change(true, true));
		administrated_[_service][_instance].reset(its_machine);
	}
}

void service_manager_impl::on_withdraw_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_service->second.erase(find_instance);
		}
	}
}

void service_manager_impl::on_start_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_instance->second->process_event(ev_service_status_change(true));
		}
	}
}

void service_manager_impl::on_stop_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_instance->second->process_event(ev_service_status_change(false));
		}
	}
}

void service_manager_impl::on_request_service(
		service_id _service, instance_id _instance) {
}

void service_manager_impl::on_release_service(
		service_id _service, instance_id _instance) {
}

bool service_manager_impl::send_offer_service(service_state_machine_t *_data, const endpoint *_target) {
	message * offer = factory_->create_message();
	service_entry & entry = offer->create_service_entry();
	entry.set_type(entry_type::OFFER_SERVICE);
	entry.set_service_id(_data->service_);
	entry.set_instance_id(_data->instance_);
	entry.set_major_version(_data->major_);
	entry.set_minor_version(_data->minor_);
	return true;
}

bool service_manager_impl::send_stop_offer_service(service_state_machine_t *_data) {
	message * offer = factory_->create_message();
	service_entry & entry = offer->create_service_entry();
	entry.set_type(entry_type::STOP_OFFER_SERVICE);
	entry.set_service_id(_data->service_);
	entry.set_instance_id(_data->instance_);
	entry.set_time_to_live(0);
	entry.set_major_version(_data->major_);
	entry.set_minor_version(_data->minor_);
	return true;
}

bool service_manager_impl::send(message_base *_message, bool _flush) {
	if (owner_)
		return owner_->send(_message, _flush);

	return false;
}

} // namespace sd
} // namespace vsomeip

//
// client_manager_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/application.hpp>
#include <vsomeip/sd/entry.hpp>
#include <vsomeip/sd/factory.hpp>
#include <vsomeip/sd/message.hpp>
#include <vsomeip/sd/service_entry.hpp>

#include <vsomeip_internal/sd/client_manager_impl.hpp>

namespace vsomeip {
namespace sd {

client_manager_impl::client_manager_impl(boost::asio::io_service &_service)
	: factory_(factory::get_instance()),
	  service_(_service),
	  deserializer_(new deserializer) {
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

	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance == find_service->second.end()) {
			client_state_machine::machine *its_machine = new client_state_machine::machine(this);
			find_service->second[_instance].reset(its_machine);
			its_machine->initiate();
			its_machine->process_event(ev_none());
			its_machine->process_event(ev_network_status_change(true, true));
		}
	} else {
		client_state_machine::machine *its_machine = new client_state_machine::machine(this);
		its_machine->initiate();
		its_machine->process_event(ev_none());
		its_machine->process_event(ev_network_status_change(true, true));
		administrated_[_service][_instance].reset(its_machine);
	}
}

void client_manager_impl::on_release_service(
		service_id _service, instance_id _instance) {
}

void client_manager_impl::on_message(const uint8_t *_data, uint32_t _size) {
	deserializer_->set_data(_data, _size);
	message *its_message = deserializer_->deserialize_sd_message();
	if (0 != its_message) {
		for (auto e : its_message->get_entries()) {
			if (e->is_service_entry()) {
				if (e->get_type() == entry_type::OFFER_SERVICE) {
					if (0 != e->get_time_to_live()) {
						std::cout << "Received a Service Offer!" << std::endl;
					} else {
						std::cout << "Received a Stop Service Offer!" << std::endl;
					}
				}
			}
		}
	}
}

bool client_manager_impl::send_find_service(client_state_machine_t *_data) {
	message * find = factory_->create_message();
	service_entry & entry = find->create_service_entry();
	entry.set_type(entry_type::FIND_SERVICE);
	entry.set_service_id(_data->service_);
	entry.set_instance_id(_data->instance_);
	entry.set_major_version(_data->major_);
	entry.set_minor_version(_data->minor_);
	return send(find, true);
}

bool client_manager_impl::send(message_base *_message, bool _flush) {
	if (owner_)
		return owner_->send(_message, _flush);

	return false;
}

} // namespace sd
} // namespace vsomeip

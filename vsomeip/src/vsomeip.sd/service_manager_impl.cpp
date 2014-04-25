//
// service_manager_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <sstream>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/sd/factory.hpp>
#include <vsomeip/sd/message.hpp>
#include <vsomeip/sd/service_entry.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/sd/service_manager_impl.hpp>
#include <vsomeip_internal/sd/service_state_machine.hpp>

namespace vsomeip {
namespace sd {

service_manager_impl::service_manager_impl(boost::asio::io_service &_service)
	: service_(_service), factory_(factory::get_instance()), owner_(0) {

}

service_manager_impl::~service_manager_impl() {
}

application * service_manager_impl::get_owner() const {
	return owner_;
}

bool service_manager_impl::init() {
	bool is_initialized = false;

	if (0 != owner_) {
		configuration *its_configuration
			= configuration::request(owner_->get_name());

		// Calculate the network name based on unicast address
		// and netmask
		std::string address = its_configuration->get_unicast_address();
		std::string netmask = its_configuration->get_netmask();
		uint16_t port = its_configuration->get_port();
		std::string protocol = its_configuration->get_protocol();

		std::string broadcast_address = get_broadcast_address(address, netmask);
		broadcast_ = vsomeip::factory::get_instance()->get_endpoint(
						broadcast_address,
						port,
						(protocol == "tcp" ? ip_protocol::TCP : ip_protocol::UDP)
					 );

		is_initialized = true;
	}

	return is_initialized;
}

void service_manager_impl::set_owner(application *_owner) {
	owner_ = _owner;
}

boost::asio::io_service & service_manager_impl::get_service() {
	return service_;
}

std::string service_manager_impl::get_broadcast_address(
				const std::string &_address, const std::string &_mask) const {
	std::string result(_address); // Default to host address

	int masked_bits = 0;

	if (_mask[0] == '/') {
		std::stringstream converter;
		converter << _mask.substr(1);
		converter >> masked_bits;
	}

	boost::asio::ip::address its_address
		= boost::asio::ip::address::from_string(_address);
	if (its_address.is_v4() && masked_bits > 7 && masked_bits < 31) {
		boost::asio::ip::address_v4 its_v4_address = its_address.to_v4();
		if (masked_bits < 31) {
			int subnet_size = (2 << (32 - masked_bits - 1));
			uint32_t subnet_address = its_v4_address.to_ulong();
			boost::asio::ip::address_v4 its_test_address(subnet_address);
			while ((subnet_address % subnet_size)) subnet_address--;
			boost::asio::ip::address_v4 broadcast_address(subnet_address + subnet_size -1);
			result = broadcast_address.to_string();
		}
	} else {
		// TODO: implement CIDR based broadcast address determination for IPv6
	}

	return result;
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
	if (_target)
		offer->set_target(_target);
	else
		offer->set_target(broadcast_);

	service_entry & entry = offer->create_service_entry();
	entry.set_type(entry_type::OFFER_SERVICE);
	entry.set_service_id(_data->service_);
	entry.set_instance_id(_data->instance_);
	entry.set_major_version(_data->major_);
	entry.set_minor_version(_data->minor_);
	return send(offer, true);
}

bool service_manager_impl::send_stop_offer_service(service_state_machine_t *_data) {
	message * stop_offer = factory_->create_message();
	stop_offer->set_target(broadcast_);

	service_entry & entry = stop_offer->create_service_entry();
	entry.set_type(entry_type::STOP_OFFER_SERVICE);
	entry.set_service_id(_data->service_);
	entry.set_instance_id(_data->instance_);
	entry.set_time_to_live(0);
	entry.set_major_version(_data->major_);
	entry.set_minor_version(_data->minor_);
	return send(stop_offer, true);
}

bool service_manager_impl::send(message_base *_message, bool _flush) {
	if (owner_)
		return owner_->send(_message, _flush);

	return false;
}

} // namespace sd
} // namespace vsomeip

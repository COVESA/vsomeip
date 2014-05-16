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

#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/sd/factory.hpp>
#include <vsomeip/sd/message.hpp>
#include <vsomeip/sd/service_entry.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/daemon.hpp>
#include <vsomeip_internal/sd/constants.hpp>
#include <vsomeip_internal/sd/deserializer.hpp>
#include <vsomeip_internal/sd/service_discovery_impl.hpp>
#include <vsomeip_internal/sd/service_state_machine.hpp>

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(boost::asio::io_service &_service)
	: service_(_service),
	  factory_(factory::get_instance()),
	  owner_(0),
	  session_(1),
	  deserializer_(new deserializer) {
}

service_discovery_impl::~service_discovery_impl() {
}

daemon * service_discovery_impl::get_owner() const {
	return owner_;
}

void service_discovery_impl::init() {
	if (0 != owner_) {
		configuration *its_configuration
			= configuration::request(owner_->get_name());

		// Calculate the network name based on unicast address and netmask
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
	}
}

void service_discovery_impl::start() {
}

void service_discovery_impl::stop() {
}

void service_discovery_impl::set_owner(daemon *_owner) {
	owner_ = _owner;
}

boost::asio::io_service & service_discovery_impl::get_service() {
	return service_;
}

std::string service_discovery_impl::get_broadcast_address(
				const std::string &_address, const std::string &_mask) const {
	std::string result(_address); // Default to host address

	boost::asio::ip::address its_address
		= boost::asio::ip::address::from_string(_address);
	if (its_address.is_v4()) {
		std::string its_mask;

		if (_mask[0] == '/') { // CIDR notation
			std::stringstream converter;
			converter << _mask.substr(1);

			uint32_t set_bits;
			converter >> set_bits;

			if (set_bits < 32) {
				uint32_t unset_bits(31 - set_bits);

				uint32_t its_binary_mask = 0x0;
				while (set_bits) {
					its_binary_mask |= (0x1 << (set_bits + unset_bits));
					set_bits--;
				}

				converter.clear();
				converter.str("");
				converter << std::dec
					<< VSOMEIP_LONG_BYTE3(its_binary_mask)
					<< "."
					<< VSOMEIP_LONG_BYTE2(its_binary_mask)
					<< "."
					<< VSOMEIP_LONG_BYTE1(its_binary_mask)
					<< "."
					<< VSOMEIP_LONG_BYTE0(its_binary_mask);

				its_mask = converter.str();
			} else {
				// TODO: error message "Illegal netmask definition"
			}
		} else {
			its_mask = _mask;
		}

		boost::asio::ip::address_v4 its_v4_address = its_address.to_v4();
		boost::asio::ip::address_v4 its_v4_netmask;
		try {
			its_v4_netmask = boost::asio::ip::address_v4::from_string(its_mask);
		}
		catch (...) {
			its_v4_netmask = boost::asio::ip::address_v4::from_string(
								VSOMEIP_SERVICE_DISCOVERY_DEFAULT_NETMASK);
		}
		result = boost::asio::ip::address_v4::broadcast(its_v4_address, its_v4_netmask).to_string();
	} else {
		// TODO: handle IPv6 subnets
	}

	return result;
}

void service_discovery_impl::on_provide_service(
		service_id _service, instance_id _instance) {
	auto find_service = its_services_.find(_service);
	if (find_service != its_services_.end()) {
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
		its_services_[_service][_instance].reset(its_machine);
	}
}

void service_discovery_impl::on_withdraw_service(
		service_id _service, instance_id _instance) {
	auto find_service = its_services_.find(_service);
	if (find_service != its_services_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_service->second.erase(find_instance);
		}
	}
}

void service_discovery_impl::on_start_service(
		service_id _service, instance_id _instance) {
	auto find_service = its_services_.find(_service);
	if (find_service != its_services_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_instance->second->process_event(ev_service_status_change(true));
		}
	}
}

void service_discovery_impl::on_stop_service(
		service_id _service, instance_id _instance) {
	auto find_service = its_services_.find(_service);
	if (find_service != its_services_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_instance->second->process_event(ev_service_status_change(false));
		}
	}
}

void service_discovery_impl::on_request_service(
		service_id _service, instance_id _instance) {
}

void service_discovery_impl::on_release_service(
		service_id _service, instance_id _instance) {
}

bool service_discovery_impl::send_find_service(client_state_machine_t *_data) {
	return false;
}

bool service_discovery_impl::send_offer_service(service_state_machine_t *_data, const endpoint *_target) {
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

bool service_discovery_impl::send_stop_offer_service(service_state_machine_t *_data) {
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

bool service_discovery_impl::send(message_base *_message, bool _flush) {
	if (owner_) {
		_message->set_session_id(session_++);
		return owner_->send(_message, _flush);
	}

	return false;
}

void service_discovery_impl::on_message(
		const uint8_t *_data, uint32_t _size,
		const endpoint *_source, const endpoint *_target) {

	std::cout << "service_discovery_impl::on_message: got a message ("
			  << _size
			  << ")"
			  << std::endl;

	deserializer_->set_data(_data, _size);
	message *its_message = deserializer_->deserialize_sd_message();
	if (0 != its_message) {

		const std::vector< entry * > its_entries = its_message->get_entries();
		const std::vector< option * > its_options = its_message->get_options();

		std::cout << "service_discovery_impl::on_message: Message has "
				  << std::dec
				  << its_entries.size() << " entries and "
				  << its_options.size() << " options."
				  << std::endl;
	} else {

		std::cout << "service_discovery_impl::on_message: Message deserialization failed!" << std::endl;
	}

	delete its_message;
}

} // namespace sd
} // namespace vsomeip

//
// service_manager_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������������������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <sstream>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/sd/factory.hpp>
#include <vsomeip/sd/ipv4_endpoint_option.hpp>
#include <vsomeip/sd/ipv6_endpoint_option.hpp>
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
		service_id _service, instance_id _instance, const endpoint *_location) {
	auto find_service = its_services_.find(_service);
	if (find_service != its_services_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance == find_service->second.end()) {
			service_state_machine::machine *its_machine = new service_state_machine::machine(this);
			its_machine->service_ = _service;
			its_machine->instance_ = _instance;
			if (_location->get_protocol() == ip_protocol::TCP) {
				its_machine->tcp_endpoint_ = _location;
			} else {
				its_machine->udp_endpoint_ = _location;
			}

			find_service->second[_instance].reset(its_machine);

			its_machine->initiate();
			its_machine->process_event(ev_none());
			its_machine->process_event(ev_network_status_change(true, true));
		} else {
			service_state_machine::machine *its_machine = find_instance->second.get();
			if (_location->get_protocol() == ip_protocol::TCP) { // TODO: handle endpoint changes
				if (0 == its_machine->tcp_endpoint_) {
					its_machine->tcp_endpoint_ = _location;
				}
			} else {
				if (0 == its_machine->udp_endpoint_) {
					its_machine->udp_endpoint_ = _location;
				}
			}
		}
	} else {
		service_state_machine::machine *its_machine = new service_state_machine::machine(this);
		its_machine->service_ = _service;
		its_machine->instance_ = _instance;
		if (_location->get_protocol() == ip_protocol::TCP) {
			its_machine->tcp_endpoint_ = _location;
		} else {
			its_machine->udp_endpoint_ = _location;
		}

		its_services_[_service][_instance].reset(its_machine);

		its_machine->initiate();
		its_machine->process_event(ev_none());
		its_machine->process_event(ev_network_status_change(true, true));
	}
}

void service_discovery_impl::on_withdraw_service(
		service_id _service, instance_id _instance, const endpoint *_location) {
	auto find_service = its_services_.find(_service);
	if (find_service != its_services_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			bool must_erase = false;
			service_state_machine_t *its_machine = find_instance->second.get();
			if (0 == _location) {
				must_erase = true;
			} else if (_location->get_protocol() == ip_protocol::TCP) {
				if (0 == its_machine->udp_endpoint_) {
					must_erase = true;
				} else {
					// TODO: stop offer on TCP endpoint
				}
			} else {
				if (0 == its_machine->tcp_endpoint_) {
					must_erase = true;
				} else {
					// TODO: stop offer on UDP endpoint
				}
			}

			if (must_erase)
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

// TODO: connect applications to client state machines
void service_discovery_impl::on_request_service(
		client_id _client, service_id _service, instance_id _instance) {

	auto find_service = its_clients_.find(_service);
	if (find_service != its_clients_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance == find_service->second.end()) {
			client_state_machine::machine *its_machine = new client_state_machine::machine(this);
			its_machine->service_ = _service;
			its_machine->instance_ = _instance;

			find_service->second[_instance] = std::make_pair(
				boost::shared_ptr< client_state_machine_t >(its_machine),
				std::set< client_id >()
			);
			find_service->second[_instance].second.insert(_client);

			its_machine->initiate();
			its_machine->process_event(ev_network_status_change(true, true));
			its_machine->process_event(ev_request_status_change(true));
		} else {
			find_instance->second.second.insert(_client);
		}
	} else {
		client_state_machine::machine *its_machine = new client_state_machine::machine(this);
		its_machine->service_ = _service;
		its_machine->instance_ = _instance;

		its_clients_[_service][_instance] = std::make_pair(
			boost::shared_ptr< client_state_machine_t >(its_machine),
			std::set< client_id >()
		);
		its_clients_[_service][_instance].second.insert(_client);

		its_machine->initiate();
		its_machine->process_event(ev_network_status_change(true, true));
		its_machine->process_event(ev_request_status_change(true));
	}
}

void service_discovery_impl::on_release_service(
		client_id _client, service_id _service, instance_id _instance) {
	auto find_service = its_clients_.find(_service);
	if (find_service != its_clients_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_instance->second.second.erase(_client);
			if (0 == find_instance->second.second.size()) {
				find_service->second.erase(_instance);
				if (0 == find_service->second.size()) {
					its_clients_.erase(_service);
				}
			}
		}
	}
}

bool service_discovery_impl::send_find_service(client_state_machine_t *_data) {
	boost::shared_ptr< message > find(factory_->create_message());
	find->set_target(broadcast_);

	service_entry & entry = find->create_service_entry();
	entry.set_type(entry_type::FIND_SERVICE);
	entry.set_service_id(_data->service_);
	entry.set_instance_id(_data->instance_);
	entry.set_time_to_live(0xFFFFFF); // TODO: find out what to set here
	entry.set_major_version(0xFF); // TODO: add major version to interface and default to "Any"
	entry.set_minor_version(0xFFFFFFFF); // TODO: add minor version to interface and default to "Any"
	return send(find.get(), true);
}

bool service_discovery_impl::send_offer_service(service_state_machine_t *_data, const endpoint *_target) {
	message * its_offer = factory_->create_message();
	if (_target)
		its_offer->set_target(_target);
	else
		its_offer->set_target(broadcast_);

	its_offer->set_interface_version(0x1);
	//its_offer->set_message_type(message_type_enum::NOTIFICATION); // TODO: check why this causes failure...
	its_offer->set_return_code(return_code_enum::OK);

	service_entry & its_entry = its_offer->create_service_entry();
	its_entry.set_type(entry_type::OFFER_SERVICE);
	its_entry.set_service_id(_data->service_);
	its_entry.set_instance_id(_data->instance_);
	its_entry.set_time_to_live(0xFFFFFF); // TODO: set "real" time to live
	its_entry.set_major_version(_data->major_);
	its_entry.set_minor_version(_data->minor_);

	// endpoint options
	if ((_data->tcp_endpoint_ && ip_protocol_version::V4 == _data->tcp_endpoint_->get_version()) ||
		(_data->udp_endpoint_ && ip_protocol_version::V4 == _data->udp_endpoint_->get_version())) {

		const endpoint *location = _data->tcp_endpoint_;
		if (0 != location) {
			ipv4_address its_address = boost::asio::ip::address_v4::from_string(
					location->get_address()).to_ulong();

			ipv4_endpoint_option & its_option = its_offer->create_ipv4_endpoint_option();
			its_option.set_address(its_address);
			its_option.set_port(location->get_port());
			its_option.set_protocol(location->get_protocol());
			its_entry.assign_option(its_option, 1);
		}

		location = _data->udp_endpoint_;
		if (0 != location) {
			ipv4_address its_address = boost::asio::ip::address_v4::from_string(
					location->get_address()).to_ulong();

			ipv4_endpoint_option & its_option = its_offer->create_ipv4_endpoint_option();
			its_option.set_address(its_address);
			its_option.set_port(location->get_port());
			its_option.set_protocol(location->get_protocol());
			its_entry.assign_option(its_option, 1);
		}
	} else {

	}

	return send(its_offer, true);
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

	deserializer_->set_data(_data, _size);
	boost::shared_ptr< message > its_message(deserializer_->deserialize_sd_message());
	if (its_message) {
		const std::vector< entry * > its_entries = its_message->get_entries();
		const std::vector< option * > its_options = its_message->get_options();

		for (auto its_entry : its_entries) {
			switch (its_entry->get_type()) {
			case entry_type::FIND_SERVICE:
				on_find_service(dynamic_cast< service_entry * >(its_entry), its_options, _source);
				break;

			case entry_type::OFFER_SERVICE: // included STOP_OFFER_SERVICE
				on_offer_service(dynamic_cast< service_entry * >(its_entry), its_options);
				break;

			case entry_type::REQUEST_SERVICE:
				// TODO: find out what REQUEST_SERVICE is meant to do!
				break;

			default:
				break;
			};
		}
	}
}

void service_discovery_impl::send_service_state(service_id _service, instance_id _instance, bool _is_available) {
	const client_info_t *its_info = find_client_info(_service, _instance);
	if (0 != its_info) {
		const endpoint *its_location = its_info->first->tcp_endpoint_;
		if (0 != its_location) {
			for (client_id its_client : its_info->second) {
				owner_->on_service_availability(its_client, _service, _instance, its_location, _is_available);
			}
		}

		its_location = its_info->first->udp_endpoint_;
		if (0 != its_location) {
			for (client_id its_client : its_info->second) {
				owner_->on_service_availability(its_client, _service, _instance, its_location, _is_available);
			}
		}
	} else {
		// TODO: add error message as this must not happen!
	}
}

const service_discovery_impl::client_info_t *
service_discovery_impl::find_client_info(service_id _service, instance_id _instance) const {
	const client_info_t *its_info = 0;

	auto find_service = its_clients_.find(_service);
	if (find_service != its_clients_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			its_info = &(find_instance->second);
		}
	}

	return its_info;
}

client_state_machine_t * service_discovery_impl::find_client_machine(service_id _service, instance_id _instance) const {
	client_state_machine_t *its_machine = 0;
	auto client_info = find_client_info(_service, _instance);
	if (0 != client_info)
		its_machine = client_info->first.get();
	return its_machine;
}

service_state_machine_t * service_discovery_impl::find_service_machine(service_id _service, instance_id _instance) const {
	service_state_machine_t *its_machine = 0;

	auto find_service = its_services_.find(_service);
	if (find_service != its_services_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			its_machine = find_instance->second.get();
		}
	}

	return its_machine;
}

void service_discovery_impl::on_find_service(
		const service_entry *_entry, const std::vector< option * > &_options,
		const endpoint *_source) {

	service_id its_service = _entry->get_service_id();
	instance_id its_instance = _entry->get_instance_id();

	service_state_machine_t *its_machine = find_service_machine(its_service, its_instance);
	if (0 != its_machine) {
		its_machine->process_event(ev_find_service(_source));
	}
}

void service_discovery_impl::on_offer_service(const service_entry *_entry, const std::vector< option * > &_options) {
	service_id its_service = _entry->get_service_id();
	instance_id its_instance = _entry->get_instance_id();

	client_state_machine_t *its_machine = find_client_machine(its_service, its_instance);
	if (0 != its_machine) {
		if (0 < _entry->get_time_to_live()) {
			const std::vector< uint8_t > &indices = _entry->get_options(1);

			for (auto i : indices) {
				option *its_option = _options[i];
				if (its_option->get_type() == option_type::IP4_ENDPOINT) {
					ipv4_endpoint_option *its_endpoint_option
						= dynamic_cast< ipv4_endpoint_option * >(its_option);

					const endpoint *its_endpoint
						= vsomeip::factory::get_instance()->get_endpoint(
								boost::asio::ip::address_v4(its_endpoint_option->get_address()).to_string(),
								its_endpoint_option->get_port(),
								its_endpoint_option->get_protocol()
						  );

					if (ip_protocol::TCP == its_endpoint->get_protocol()) {
						its_machine->tcp_endpoint_ = its_endpoint;
					} else {
						its_machine->udp_endpoint_ = its_endpoint;
					}
				}
			}

			its_machine->process_event(ev_offer_service());

		} else {
			its_machine->process_event(ev_stop_offer_service());
		}
	}
}

} // namespace sd
} // namespace vsomeip

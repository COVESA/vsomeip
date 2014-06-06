//
// group.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>

#include <boost/asio/ip/address.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/sd/enumeration_types.hpp>
#include <vsomeip/sd/eventgroup_entry.hpp>
#include <vsomeip/sd/factory.hpp>
#include <vsomeip/sd/ipv4_endpoint_option.hpp>
#include <vsomeip/sd/ipv4_multicast_option.hpp>
#include <vsomeip/sd/ipv6_endpoint_option.hpp>
#include <vsomeip/sd/ipv6_multicast_option.hpp>
#include <vsomeip/sd/message.hpp>
#include <vsomeip/sd/option.hpp>
#include <vsomeip/sd/service_entry.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/sd/client_proxy.hpp>
#include <vsomeip_internal/sd/eventgroup_client_proxy.hpp>
#include <vsomeip_internal/sd/group.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>
#include <vsomeip_internal/sd/service_proxy.hpp>

namespace vsomeip {
namespace sd {

group::group(const std::string &_name, service_discovery *_owner)
	: name_(_name), owner_(_owner), machine_(this), is_client_started_(false), is_service_started_(false) {
}

void group::init() {
	configuration *the_configuration = configuration::request();

	boost::random::mt19937 random_generator;
	boost::random::uniform_int_distribution<> distribution(
		the_configuration->get_min_initial_delay(),
		the_configuration->get_max_initial_delay()
	);

	machine_.initial_delay_ = distribution(random_generator);
	machine_.repetition_base_delay_ = the_configuration->get_repetition_base_delay();
	machine_.cyclic_offer_delay_ = the_configuration->get_cyclic_offer_delay();
	machine_.repetition_max_ = the_configuration->get_repetition_max();

	machine_.initiate();
}

void group::start() {
	machine_.process_event(ev_none());
}

boost::asio::io_service & group::get_service() {
	return owner_->get_service();
}

const std::string & group::get_name() const {
	return name_;
}

bool group::is_started() const {
	return (is_service_started_ || is_client_started_);
}

bool group::is_network_configured() const {
	return true; // TODO: use "real" network status
}

void group::send_services_status(
				service_id _service, instance_id _instance,
				const endpoint *_target,
				bool _is_announcing) {
	factory *its_factory = factory::get_instance();
	std::map< const endpoint *, option * > endpoint_options;

	boost::shared_ptr< message > its_message(its_factory->create_message());
	its_message->set_target(_target);

	bool has_entry = false;

#if 0
	if (!_is_announcing) {
		for (auto a_client : clients_) {
			if (!a_client->is_available()) {
				service_entry &its_entry = its_message->create_service_entry();
				its_entry.set_type(entry_type::FIND_SERVICE);
				its_entry.set_service_id(a_client->get_service());
				its_entry.set_instance_id(a_client->get_instance());
				its_entry.set_major_version(a_client->get_major_version());
				its_entry.set_minor_version(a_client->get_minor_version());
				its_entry.set_time_to_live(a_client->get_time_to_live());
				has_entry = true;
			}
		}
	}
#endif
	for (auto a_service : services_) {
		if ((_service == VSOMEIP_ANY_SERVICE || _service == a_service->get_service()) &&
			(_instance == VSOMEIP_ANY_INSTANCE || _instance == a_service->get_instance())) {

			if (a_service->is_local()) {
				service_entry & its_entry = its_message->create_service_entry();
				its_entry.set_type(entry_type::OFFER_SERVICE);
				its_entry.set_service_id(a_service->get_service());
				its_entry.set_instance_id(a_service->get_instance());
				its_entry.set_major_version(a_service->get_major_version());
				its_entry.set_minor_version(a_service->get_minor_version());
				its_entry.set_time_to_live(a_service->get_time_to_live());
				has_entry = true;

				const endpoint *reliable = a_service->get_reliable();
				option *reliable_option = 0;
				if (0 != reliable) {
					auto found_option = endpoint_options.find(reliable);
					if (found_option == endpoint_options.end()) {
						reliable_option = add_endpoint_option(its_message.get(), reliable);
						if (0 != reliable_option) {
							endpoint_options[reliable] = reliable_option;
						}
					} else {
						reliable_option = found_option->second;
					}

					if (0 != reliable_option) {
						its_entry.assign_option(*reliable_option, 1);
					}
				}

				const endpoint *unreliable = a_service->get_unreliable();
				option *unreliable_option = 0;
				if (0 != unreliable) {
					auto found_option = endpoint_options.find(unreliable);
					if (found_option == endpoint_options.end()) {
						unreliable_option = add_endpoint_option(its_message.get(), unreliable);
						if (0 != unreliable_option) {
							endpoint_options[unreliable] = unreliable_option;
						}
					} else {
						unreliable_option = found_option->second;
					}

					if (0 != unreliable_option) {
						its_entry.assign_option(*unreliable_option, 1);
					}
				}
			}
		}
	}

	if (has_entry)
		owner_->send(its_message.get());
}

option * group::add_endpoint_option(message *_message, const endpoint *_endpoint) const {
	option *its_new_option = 0;

	if (0 != _endpoint) {
		if (ip_protocol_version::V4 == _endpoint->get_version()) {
			ipv4_address its_address = boost::asio::ip::address_v4::from_string(
					_endpoint->get_address()).to_ulong();

			if (its_address < VSOMEIP_MIN_MULTICAST) {
				ipv4_endpoint_option & its_option = _message->create_ipv4_endpoint_option();
				its_option.set_address(its_address);
				its_option.set_port(_endpoint->get_port());
				its_option.set_protocol(_endpoint->get_protocol());

				its_new_option = &its_option;
			} else {
				ipv4_multicast_option & its_option = _message->create_ipv4_multicast_option();
				its_option.set_address(its_address);
				its_option.set_port(_endpoint->get_port());

				its_new_option = &its_option;
			}
		} else { // TODO: support IPv6
			ipv6_address its_address; // = boost::asio::ip::address_v6::from_string(_endpoint->get_address())

			ipv6_endpoint_option & its_option = _message->create_ipv6_endpoint_option();
			its_option.set_address(its_address);
			its_option.set_port(_endpoint->get_port());
			its_option.set_protocol(_endpoint->get_protocol());

			its_new_option = &its_option;
		}
	}

	return its_new_option;
}

///////////////////////////////////////////////////////////////////////////////
// Client management
///////////////////////////////////////////////////////////////////////////////
client_proxy * group::find_client(service_id _service, instance_id _instance) {
	client_proxy *its_proxy = 0;
	for (auto i : clients_) {
		if (_service == i->get_service() && _instance == i->get_instance()) {
			its_proxy = i.get();
			break;
		}
	}
	return its_proxy;
}

client_proxy * group::add_client(service_id _service, instance_id _instance) {
	client_proxy *its_proxy = find_client(_service, _instance);
	if (0 == its_proxy) {
		its_proxy = new client_proxy(this, _service, _instance);
		clients_.insert(boost::shared_ptr< client_proxy >(its_proxy));
	}
	return its_proxy;
}

void group::remove_client(service_id _service, instance_id _instance) {
	for (auto i : clients_) {
		if (i->get_service() == _service &&
			i->get_instance() == _instance) {
			clients_.erase(i);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Service management
///////////////////////////////////////////////////////////////////////////////
service_proxy * group::find_service(service_id _service, instance_id _instance) {
	service_proxy *its_proxy = 0;
	for (auto i : services_) {
		if (_service == i->get_service() && _instance == i->get_instance()) {
			its_proxy = i.get();
			break;
		}
	}
	return its_proxy;
}

service_proxy * group::add_service(service_id _service, instance_id _instance) {
	service_proxy *its_proxy = find_service(_service, _instance);
	if (0 == its_proxy) {
		its_proxy = new service_proxy(this, _service, _instance);
		services_.insert(boost::shared_ptr< service_proxy >(its_proxy));
	}
	return its_proxy;
}

void group::remove_service(service_id _service, instance_id _instance) {
	for (auto i : services_) {
		if (i->get_service() == _service &&
			i->get_instance() == _instance) {
			services_.erase(i);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void group::on_service_withdraw() {
}

void group::on_service_started() {
	is_service_started_ = true;
	machine_.process_event(ev_status_change());
}

void group::on_service_stopped() {
	is_service_started_ = false;
	for (auto a_service : services_)
		is_service_started_ = is_service_started_ || a_service->is_started();

	if (!is_started())
		machine_.process_event(ev_status_change());
}

void group::on_service_requested() {
	is_client_started_ = true;
	machine_.process_event(ev_status_change());
}

void group::on_service_released() {
	is_client_started_ = false;
	for (auto a_service : services_)
		is_client_started_ = is_client_started_ || a_service->is_started();

	if (!is_started())
		machine_.process_event(ev_status_change());
}

void group::on_eventgroup_provided() {
}

void group::on_eventgroup_withdrawn() {
}

void group::on_eventgroup_requested() {
}

void group::on_eventgroup_released() {
}

///////////////////////////////////////////////////////////////////////////////
// Message handlers
///////////////////////////////////////////////////////////////////////////////

void group::on_find_service(service_id _service, instance_id _instance, const endpoint *_source, bool _is_unicast_enabled) {
	machine_.process_event(ev_find_service(_service, _instance, _source, _is_unicast_enabled));
}

void group::on_offer_service(service_id _service, instance_id _instance, const endpoint *_source,
							 const endpoint *_reliable, const endpoint *_unreliable) {
	client_proxy *its_proxy = find_client(_service, _instance);
	if (0 != its_proxy) {
		factory *its_factory = factory::get_instance();

		boost::shared_ptr< message > its_message(its_factory->create_message());
		auto its_eventgroups = its_proxy->get_eventgroups();
		for (auto i : its_eventgroups) {
			if (!i.second->is_acknowledged()) {
				eventgroup_entry & its_entry = its_message->create_eventgroup_entry();
				its_entry.set_type(entry_type::SUBSCRIBE_EVENTGROUP);
				its_entry.set_service_id(_service);
				its_entry.set_instance_id(_instance);
				its_entry.set_eventgroup_id(i.first);
				its_entry.set_time_to_live(0xFFFFFF); // TODO: find out where to get this value!

				if (0 != _reliable) {
					option *its_option = add_endpoint_option(its_message.get(), _reliable);
					its_entry.assign_option(*its_option, 1);
				}

				if (0 != _unreliable) {
					option *its_option = add_endpoint_option(its_message.get(), _unreliable);
					its_entry.assign_option(*its_option, 1);
				}
			}
		}

		if (0 < its_message->get_entries().size())
			owner_->send(its_message.get());
	}
}

void group::on_subscribe_eventgroup(
		 service_id _service, instance_id _instance, eventgroup_id _eventgroup,
		 const endpoint *_source, const endpoint *_multicast,
		 bool _stop) {

	factory *its_factory = factory::get_instance();
	boost::shared_ptr< message > its_message(its_factory->create_message());
	its_message->set_target(_source);

	eventgroup_entry &its_entry = its_message->create_eventgroup_entry();
	its_entry.set_type(entry_type::SUBSCRIBE_EVENTGROUP_ACK);
	its_entry.set_service_id(_service);
	its_entry.set_instance_id(_instance);
	its_entry.set_eventgroup_id(_eventgroup);
	its_entry.set_time_to_live((_stop ? 0 : 0xFFFFFF)); // TODO: get ttl from eventgroup

	if (_multicast) {
		if (ip_protocol_version::V4 == _multicast->get_version()) {
			option *its_option = add_endpoint_option(its_message.get(), _multicast);
			its_entry.assign_option(*its_option, 1);
		} else {
			// TODO: support IPv6
		}
	}

	owner_->send(its_message.get());
}

} // namespace sd
} // namespace vsomeip




//
// service_manager_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������������������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/sd/eventgroup_entry.hpp>
#include <vsomeip/sd/factory.hpp>
#include <vsomeip/sd/ipv4_endpoint_option.hpp>
#include <vsomeip/sd/ipv4_multicast_option.hpp>
#include <vsomeip/sd/ipv6_endpoint_option.hpp>
#include <vsomeip/sd/ipv6_multicast_option.hpp>
#include <vsomeip/sd/message.hpp>
#include <vsomeip/sd/service_entry.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/daemon.hpp>
#include <vsomeip_internal/service.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/sd/client_proxy.hpp>
#include <vsomeip_internal/sd/constants.hpp>
#include <vsomeip_internal/sd/deserializer.hpp>
#include <vsomeip_internal/sd/group.hpp>
#include <vsomeip_internal/sd/service_discovery_impl.hpp>
#include <vsomeip_internal/sd/service_proxy.hpp>

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(daemon &_owner)
	: owner_(_owner),
	  service_(_owner.get_service()),
	  logger_(_owner.get_logger()),
	  factory_(factory::get_instance()),
	  session_(1),
	  broadcast_(0),
	  multicast_(0),
	  deserializer_(new deserializer),
	  default_group_(new group("default", this)) {

	groups_.insert(default_group_);
}

service_discovery_impl::~service_discovery_impl() {
}

void service_discovery_impl::init() {
	configuration *its_configuration
		= configuration::request(owner_.get_name());

	// Calculate the network name based on unicast address and netmask
	std::string address = its_configuration->get_unicast_address();
	std::string netmask = its_configuration->get_netmask();
	uint16_t port = its_configuration->get_port();
	std::string protocol = its_configuration->get_protocol();

	std::string broadcast_address = get_broadcast_address(address, netmask);

	vsomeip::factory *base_factory = vsomeip::factory::get_instance();

	broadcast_ = base_factory->get_endpoint(
					broadcast_address,
					port,
					(protocol == "tcp" ? ip_protocol::TCP : ip_protocol::UDP)
				 );

	std::string multicast_address = its_configuration->get_multicast_address();
	if (multicast_address != "") {
		multicast_ = base_factory->get_endpoint(
						multicast_address,
						port,
						(protocol == "tcp" ? ip_protocol::TCP : ip_protocol::UDP)
			         );
	}

	for (auto a_group : groups_)
		a_group->init();
}

void service_discovery_impl::start() {
	for (auto a_group : groups_)
		a_group->start();
}

void service_discovery_impl::stop() {
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

///////////////////////////////////////////////////////////////////////////////
// Helper
///////////////////////////////////////////////////////////////////////////////
client_proxy * service_discovery_impl::find_client(service_id _service, instance_id _instance) const {
	client_proxy *its_proxy = 0;

	for (auto i : groups_) {
		its_proxy = i->find_client(_service, _instance);
		if (0 != its_proxy)
			break;
	}

	return its_proxy;
}

client_proxy * service_discovery_impl::find_or_create_client(service_id _service, instance_id _instance) {
	client_proxy *its_proxy = find_client(_service, _instance);
	if (0 == its_proxy) {
		its_proxy = default_group_->add_client(_service, _instance);
	}
	return its_proxy;
}

service_proxy * service_discovery_impl::find_service(service_id _service, instance_id _instance) const {
	service_proxy *its_proxy = 0;

	for (auto i : groups_) {
		its_proxy = i->find_service(_service, _instance);
		if (0 != its_proxy)
			break;
	}

	return its_proxy;
}

service_proxy * service_discovery_impl::find_or_create_service(service_id _service, instance_id _instance) {
	service_proxy *its_proxy = find_service(_service, _instance);
	if (0 == its_proxy) {
		its_proxy = default_group_->add_service(_service, _instance);
	}
	return its_proxy;
}

group * service_discovery_impl::find_client_group(service_id _service, instance_id _instance) const {
	group *its_group = 0;

	for (auto i : groups_) {
		auto its_proxy = i->find_client(_service, _instance);
		if (0 != its_proxy) {
			its_group = i.get();
			break;
		}
	}
	return its_group;
}

group * service_discovery_impl::find_service_group(service_id _service, instance_id _instance) const {
	group *its_group = 0;

	for (auto i : groups_) {
		auto its_proxy = i->find_service(_service, _instance);
		if (0 != its_proxy) {
			its_group = i.get();
			break;
		}
	}
	return its_group;
}

///////////////////////////////////////////////////////////////////////////////
// Application interface
///////////////////////////////////////////////////////////////////////////////
void service_discovery_impl::on_provide_service(
		service_id _service, instance_id _instance, const endpoint *_location) {
	service_proxy *its_proxy = find_or_create_service(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->provide(_location);
	} else {
		VSOMEIP_ERROR << "Could not provide service ["
				      << std::hex << std::setw(4) << std::setfill('0')
					  << _service << "." << _instance
					  << "]. Creation of service proxy failed!";
	}
}

void service_discovery_impl::on_withdraw_service(
		service_id _service, instance_id _instance, const endpoint *_location) {
	service_proxy *its_proxy = find_service(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->withdraw(_location);
	} else {
		VSOMEIP_WARNING << "Could not withdraw service ["
				      << std::hex << std::setw(4) << std::setfill('0')
					  << _service << "." << _instance
					  << "]. Service proxy does not exist!";
	}
}

void service_discovery_impl::on_start_service(
		service_id _service, instance_id _instance) {
	service_proxy *its_proxy = find_service(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->start();
	}
}

void service_discovery_impl::on_stop_service(
		service_id _service, instance_id _instance) {
	service_proxy *its_proxy = find_service(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->stop();
	}
}

void service_discovery_impl::on_request_service(
		client_id _client, service_id _service, instance_id _instance) {
	client_proxy *its_proxy = find_or_create_client(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->request(_client);
	}
}

void service_discovery_impl::on_release_service(
		client_id _client, service_id _service, instance_id _instance) {
	client_proxy *its_proxy = find_client(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->release(_client);
	}
}

void service_discovery_impl::on_provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) {
	service_proxy *its_proxy = find_service(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->provide_eventgroup(_eventgroup, _location);
	}
}

void service_discovery_impl::on_withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) {
	service_proxy *its_proxy = find_service(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->withdraw_eventgroup(_eventgroup);
	}
}

void service_discovery_impl::on_request_eventgroup(client_id _client, service_id _service, instance_id _instance, eventgroup_id _eventgroup) {
	client_proxy *its_proxy = find_or_create_client(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->subscribe(_client, _eventgroup);
	}
}

void service_discovery_impl::on_release_eventgroup(client_id _client, service_id _service, instance_id _instance, eventgroup_id _eventgroup) {
	client_proxy *its_proxy = find_or_create_client(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->unsubscribe(_client, _eventgroup);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Interface functions
///////////////////////////////////////////////////////////////////////////////
void service_discovery_impl::on_service_available(
		service_id _service, instance_id _instance, const endpoint *_reliable, const endpoint *_unreliable) {
	client_proxy *its_proxy = find_client(_service, _instance);
	if (0 != its_proxy) {
		its_proxy->set_local(_reliable, _unreliable);
	}
}

bool service_discovery_impl::send(message *_message) {
	// Enable Unicast
	_message->set_unicast_flag(true);

	// Session
	session_id old_session = session_;
	session_++;

	if (old_session > session_) {
		session_ = 1;
		_message->set_reboot_flag(false);
	}

	_message->set_session_id(session_);

	// Target address
	const endpoint *target = _message->get_target();
	if (0 == target) {
		if (0 != multicast_) {
			_message->set_target(multicast_);
		} else {
			_message->set_target(broadcast_);
		}
	}

	bool is_reliable = (ip_protocol::TCP == _message->get_target()->get_protocol());
	return owner_.send(_message, is_reliable, true);
}

void service_discovery_impl::on_message(
		const uint8_t *_data, uint32_t _size,
		const endpoint *_source, const endpoint *_target) {

#if 0
	std::cout << "SD: ";
	for (uint32_t i = 0; i < _size; ++i)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
	std::cout << std::endl;
#endif

	deserializer_->set_data(_data, _size);
	boost::shared_ptr< message > its_message(deserializer_->deserialize_sd_message());

	if (its_message) {
		const std::vector< entry * > its_entries = its_message->get_entries();
		const std::vector< option * > its_options = its_message->get_options();
		bool is_unicast_enabled = its_message->get_unicast_flag();

		for (auto its_entry : its_entries) {
			switch (its_entry->get_type()) {
			case entry_type::FIND_SERVICE:
				on_find_service(dynamic_cast< service_entry * >(its_entry), its_options, _source, is_unicast_enabled);
				break;

			case entry_type::OFFER_SERVICE: // includes STOP_OFFER_SERVICE
				on_offer_service(dynamic_cast< service_entry * >(its_entry), its_options, _source);
				break;

			case entry_type::REQUEST_SERVICE:
				//on_request_service(_source, dynamic_cast< service_entry * >(its_entry), its_option);
				break;

			case entry_type::SUBSCRIBE_EVENTGROUP: // includes STOP_SUBSCRIBE_EVENTGROUP
				on_subscribe_eventgroup(dynamic_cast< eventgroup_entry * >(its_entry), its_options, _source);
				break;

			case entry_type::SUBSCRIBE_EVENTGROUP_ACK: // includes STOP_SUBSCRIBE_EVENTGROUP_ACK
				on_subscribe_eventgroup_ack(dynamic_cast< eventgroup_entry * >(its_entry), its_options);
				break;

			default:
				break;
			};
		}
	} else {
		VSOMEIP_ERROR << "[SD] Deserialization failed. Message discarded!";
	}
}

void service_discovery_impl::on_find_service(
		const service_entry *_entry, const std::vector< option * > &_options,
		const endpoint *_source, bool _is_unicast_enabled) {

	service_id its_service = _entry->get_service_id();
	instance_id its_instance = _entry->get_instance_id();

	group *its_group = find_service_group(its_service, its_instance);
	if (0 != its_group) {
		its_group->on_find_service(its_service, its_instance, _source, _is_unicast_enabled);
	}
}

void service_discovery_impl::on_offer_service(
		const service_entry *_entry, const std::vector< option * > &_options,
		const endpoint *_source) {

	service_id its_service = _entry->get_service_id();
	instance_id its_instance = _entry->get_instance_id();

	const endpoint *its_tcp = 0;
	const endpoint *its_udp = 0;

	client_proxy *its_proxy = find_or_create_client(its_service, its_instance);
	if (0 != its_proxy) {
		const std::vector< uint8_t > &its_options = _entry->get_options(1);
		for (auto i : its_options) {
			if (option_type::IP4_ENDPOINT == _options[i]->get_type()) {
				ipv4_endpoint_option *its_endpoint_option = dynamic_cast< ipv4_endpoint_option * >(_options[i]);
				if (ip_protocol::TCP == its_endpoint_option->get_protocol()) {
					its_tcp = vsomeip::factory::get_instance()->get_endpoint(
								boost::asio::ip::address_v4(its_endpoint_option->get_address()).to_string(),
								its_endpoint_option->get_port(),
								ip_protocol::TCP
						  	  );
				} else {
					its_udp = vsomeip::factory::get_instance()->get_endpoint(
								boost::asio::ip::address_v4(its_endpoint_option->get_address()).to_string(),
								its_endpoint_option->get_port(),
								ip_protocol::UDP
						  	  );
				}
			} else {
				// TODO: support IPv6
			}
		}

		its_proxy->set_available(its_tcp, its_udp, (_entry->get_time_to_live() > 0));
	}
}

void service_discovery_impl::on_subscribe_eventgroup(
		const eventgroup_entry *_entry, const std::vector< option * > &_options,
		const endpoint *_source) {
	service_id its_service = _entry->get_service_id();
	instance_id its_instance = _entry->get_instance_id();
	eventgroup_id its_eventgroup = _entry->get_eventgroup_id();

	const endpoint *its_reliable = 0;
	const endpoint *its_unreliable = 0;

	service_proxy *its_proxy = find_or_create_service(its_service, its_instance);
	if (0 != its_proxy) {
		const std::vector< uint8_t > &its_options = _entry->get_options(1);
		for (auto i : its_options) {
			if (option_type::IP4_ENDPOINT == _options[i]->get_type()) {
				ipv4_endpoint_option *its_endpoint_option = dynamic_cast< ipv4_endpoint_option * >(_options[i]);
				if (ip_protocol::TCP == its_endpoint_option->get_protocol()) {
					its_reliable = vsomeip::factory::get_instance()->get_endpoint(
								boost::asio::ip::address_v4(its_endpoint_option->get_address()).to_string(),
								its_endpoint_option->get_port(),
								ip_protocol::TCP
							  );
				} else {
					its_unreliable = vsomeip::factory::get_instance()->get_endpoint(
								boost::asio::ip::address_v4(its_endpoint_option->get_address()).to_string(),
								its_endpoint_option->get_port(),
								ip_protocol::UDP
							  );
				}
			} else {
				// TODO: support IPv6
			}
		}

		its_proxy->subscribe(its_eventgroup, _source, its_reliable, its_unreliable, (_entry->get_time_to_live() == 0));
	}
}

void service_discovery_impl::on_subscribe_eventgroup_ack(
		const eventgroup_entry *_entry, const std::vector< option * > &_options) {
	service_id its_service = _entry->get_service_id();
	instance_id its_instance = _entry->get_instance_id();
	eventgroup_id its_eventgroup = _entry->get_eventgroup_id();

	client_proxy *its_proxy = find_client(its_service, its_instance);
	if (0 != its_proxy) {
		option *its_multicast_option = 0;

		const std::vector< uint8_t > &its_options = _entry->get_options(1);
		for (auto i : its_options) {
			if (option_type::IP4_MULTICAST == _options[its_options[i]]->get_type() ||
				option_type::IP6_MULTICAST == _options[its_options[i]]->get_type()) {
				if (0 == its_multicast_option) {
					its_multicast_option = _options[0];
				} else {
					// TODO: error: "SubscribeEventgroupAck must not contain more than one option!";
				}
			} else {
				// TODO: warning: "Unsupported option!"
			}
		}

		ipv4_multicast_option *its_option = dynamic_cast< ipv4_multicast_option * >(its_multicast_option);
		if (0 != its_option) {
			service *the_service = owner_.find_multicast_service(its_option->get_port());
			if (0 != the_service) {
				if (_entry->get_time_to_live()) {
					the_service->join(boost::asio::ip::address_v4(its_option->get_address()).to_string());
				} else {
					the_service->leave(boost::asio::ip::address_v4(its_option->get_address()).to_string());
				}
			}
		}

		// TODO: support IPv6
	}
}

bool service_discovery_impl::update_client(
		client_id _client,
		service_id _service, instance_id _instance, const endpoint *_location,
		bool _is_available) {
	owner_.on_service_availability(_client, _service, _instance, _location, _is_available);
	return true;
}

} // namespace sd
} // namespace vsomeip

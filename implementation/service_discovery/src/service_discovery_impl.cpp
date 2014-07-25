// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/configuration.hpp>
#include <vsomeip/logger.hpp>

#include "../include/constants.hpp"
#include "../include/defines.hpp"
#include "../include/deserializer.hpp"
#include "../include/enumeration_types.hpp"
#include "../include/eventgroupentry_impl.hpp"
#include "../include/ipv4_option_impl.hpp"
#include "../include/ipv6_option_impl.hpp"
#include "../include/message_impl.hpp"
#include "../include/runtime.hpp"
#include "../include/service_discovery_fsm.hpp"
#include "../include/service_discovery_host.hpp"
#include "../include/service_discovery_impl.hpp"
#include "../include/serviceentry_impl.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../routing/include/servicegroup.hpp"
#include "../../routing/include/serviceinfo.hpp"

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(service_discovery_host *_host) :
		host_(_host), io_(_host->get_io()), default_(
				std::make_shared < service_discovery_fsm > ("default", this)), deserializer_(
				std::make_shared<deserializer>()) {
}

service_discovery_impl::~service_discovery_impl() {
}

std::shared_ptr<configuration> service_discovery_impl::get_configuration() const {
	return host_->get_configuration();
}

boost::asio::io_service & service_discovery_impl::get_io() {
	return io_;
}

void service_discovery_impl::init() {
	std::shared_ptr<configuration> its_configuration =
			host_->get_configuration();
	if (its_configuration) {
		std::set < std::string > its_servicegroups =
				its_configuration->get_servicegroups();
		for (auto its_group : its_servicegroups) {
			if (its_group != "default"
					&& its_configuration->is_local_servicegroup(its_group)) {
				additional_[its_group] = std::make_shared
						< service_discovery_fsm > (its_group, this);
			}
		}
	} else {
		VSOMEIP_ERROR << "SD: no configuration found!";
	}

	host_->create_service_discovery_endpoint(
			its_configuration->get_service_discovery_multicast(),
			its_configuration->get_service_discovery_port(),
			its_configuration->get_service_discovery_protocol());
}

void service_discovery_impl::start() {
	default_->start();
	for (auto &its_group : additional_) {
		its_group.second->start();
	}

	default_->process(ev_none());
	for (auto &its_group : additional_) {
		its_group.second->process(ev_none());
	}
}

void service_discovery_impl::stop() {
}

void service_discovery_impl::request_service(service_t _service,
		instance_t _instance, major_version_t _major, minor_version_t _minor,
		ttl_t _ttl) {
	auto find_service = requested_.find(_service);
	if (find_service != requested_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			// TODO: check version and report errors
		} else {
			find_service->second[_instance]
			    = std::make_shared< serviceinfo>(_major, _minor, _ttl);
		}
	} else {
		requested_[_service][_instance]
		    = std::make_shared<serviceinfo>(_major, _minor, _ttl);
	}
}

void service_discovery_impl::release_service(service_t _service,
		instance_t _instance) {
	auto find_service = requested_.find(_service);
	if (find_service != requested_.end()) {
		find_service->second.erase(_instance);
	}
}

void service_discovery_impl::subscribe(service_t _service, instance_t _instance,
		eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl) {
	auto found_service = subscribed_.find(_service);
	if (found_service != subscribed_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			auto found_eventgroup = found_instance->second.find(_eventgroup);
			if (found_eventgroup != found_instance->second.end()) {

			} else {

			}
		} else {
			// insert new subscription
		}
	} else {
		// insert new subscription
	}
}

void service_discovery_impl::unsubscribe(service_t _service,
		instance_t _instance, eventgroup_t _eventgroup) {

}

void service_discovery_impl::insert_service_option(
		std::shared_ptr<message_impl> &_message,
		std::shared_ptr<serviceentry_impl> &_entry,
		std::shared_ptr<endpoint> _endpoint) {

	ipv4_address_t its_address;
	if (_endpoint->get_address(its_address)) {
		std::shared_ptr<ipv4_option_impl> its_option =
				_message->create_ipv4_option(false);
		if (its_option) {
			its_option->set_address(its_address);
			its_option->set_port(_endpoint->get_port());
			its_option->set_udp(_endpoint->is_udp());
			_entry->assign_option(its_option, 1);
		}
	} else {
		ipv6_address_t its_address;
		if (_endpoint->get_address(its_address)) {
			std::shared_ptr<ipv6_option_impl> its_option =
					_message->create_ipv6_option(false);
			if (its_option) {
				its_option->set_address(its_address);
				its_option->set_port(_endpoint->get_port());
				its_option->set_udp(_endpoint->is_udp());
				_entry->assign_option(its_option, 1);
			}
		}
	}
}

void service_discovery_impl::insert_service_entries(
		std::shared_ptr<message_impl> &_message, service_map_t &_services,
		bool _is_offer) {
	for (auto its_service : _services) {
		for (auto its_instance : its_service.second) {
			auto its_info = its_instance.second;
			std::shared_ptr<serviceentry_impl> its_entry =
					_message->create_service_entry();
			if (its_entry) {
				its_entry->set_type(
						(_is_offer ?
								entry_type_e::OFFER_SERVICE :
								entry_type_e::FIND_SERVICE));
				its_entry->set_service(its_service.first);
				its_entry->set_instance(its_instance.first);
				its_entry->set_major_version(its_info->get_major());
				its_entry->set_minor_version(its_info->get_minor());
				its_entry->set_ttl(its_info->get_ttl());

				std::shared_ptr<endpoint> its_endpoint = its_info->get_endpoint(
						true);
				if (its_endpoint) {
					insert_service_option(_message, its_entry, its_endpoint);
					if (0 == its_info->get_ttl()) {
						host_->del_routing_info(its_service.first,
								its_instance.first, true);
					}
				}

				its_endpoint = its_info->get_endpoint(false);
				if (its_endpoint) {
					insert_service_option(_message, its_entry, its_endpoint);
					if (0 == its_info->get_ttl()) {
						host_->del_routing_info(its_service.first,
								its_instance.first, false);
					}
				}
			} else {
				VSOMEIP_ERROR << "Failed to create service entry.";
			}
		}
	}
}

void service_discovery_impl::send(const std::string &_name,
		bool _is_announcing) {
	std::shared_ptr<message_impl> its_message =
			runtime::get()->create_message();

	// TODO: optimize building of SD message (common options, utilize the two runs)

	// If we are the default group and not in main phase, include "FindOffer"-entries
	if (_name == "default" && !_is_announcing) {
		insert_service_entries(its_message, requested_, false);
	}

	// Always include the "OfferService"-entries for the service group
	service_map_t its_offers = host_->get_offered_services(_name);
	insert_service_entries(its_message, its_offers, true);

	// Serialize and send
	host_->send(VSOMEIP_SD_CLIENT, its_message, true, false);
}

// Interface endpoint_host
void service_discovery_impl::on_message(const byte_t *_data, length_t _length) {
#if 0
	std::stringstream msg;
	msg << "sdi::on_message: ";
	for (length_t i = 0; i < _length; ++i)
	msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
	VSOMEIP_DEBUG << msg.str();
#endif
	deserializer_->set_data(_data, _length);
	std::shared_ptr<message_impl> its_message(
			deserializer_->deserialize_sd_message());
	if (its_message) {
		std::vector < std::shared_ptr<option_impl> > its_options =
				its_message->get_options();
		for (auto its_entry : its_message->get_entries()) {
			if (its_entry->is_service_entry()) {
				std::shared_ptr<serviceentry_impl> its_service_entry =
						std::dynamic_pointer_cast < serviceentry_impl
								> (its_entry);
				process_serviceentry(its_service_entry, its_options);
			} else {
				std::shared_ptr<eventgroupentry_impl> its_eventgroup_entry =
						std::dynamic_pointer_cast < eventgroupentry_impl
								> (its_entry);
				process_eventgroupentry(its_eventgroup_entry, its_options);
			}
		}
	}
}

// Entry processing
void service_discovery_impl::process_serviceentry(
		std::shared_ptr<serviceentry_impl> &_entry,
		const std::vector<std::shared_ptr<option_impl> > &_options) {
	service_t its_service = _entry->get_service();
	instance_t its_instance = _entry->get_instance();
	major_version_t its_major = _entry->get_major_version();
	minor_version_t its_minor = _entry->get_minor_version();
	ttl_t its_ttl = _entry->get_ttl();

	for (auto i : { 1, 2 }) {
		for (auto its_index : _entry->get_options(i)) {
			std::vector<byte_t> its_option_address;
			uint16_t its_option_port = VSOMEIP_INVALID_PORT;
			std::shared_ptr<option_impl> its_option = _options[its_index];
			switch (its_option->get_type()) {
			case option_type_e::IP4_ENDPOINT: {
				std::shared_ptr<ipv4_option_impl> its_ipv4_option =
						std::dynamic_pointer_cast < ipv4_option_impl
								> (its_option);

				boost::asio::ip::address_v4 its_ipv4_address(
						its_ipv4_option->get_address());
				boost::asio::ip::address its_address(its_ipv4_address);

				its_option_port = its_ipv4_option->get_port();

				if (0 < its_ttl) {
					host_->add_routing_info(its_service, its_instance,
							its_major, its_minor, its_ttl, its_address,
							its_option_port, !its_ipv4_option->is_udp());
				} else {
					host_->del_routing_info(its_service, its_instance,
							!its_ipv4_option->is_udp());
				}
			}
				break;
			case option_type_e::IP6_ENDPOINT: {
				std::shared_ptr<ipv6_option_impl> its_ipv6_option =
						std::dynamic_pointer_cast < ipv6_option_impl
								> (its_option);

				boost::asio::ip::address_v6 its_ipv6_address(
						its_ipv6_option->get_address());
				boost::asio::ip::address its_address(its_ipv6_address);

				its_option_port = its_ipv6_option->get_port();

				if (0 < its_ttl) {
					host_->add_routing_info(its_service, its_instance,
							its_major, its_minor, its_ttl, its_address,
							its_option_port, !its_ipv6_option->is_udp());
				} else {
					host_->del_routing_info(its_service, its_instance,
							!its_ipv6_option->is_udp());
				}
			}
				break;
			case option_type_e::IP4_MULTICAST:
			case option_type_e::IP6_MULTICAST:
				VSOMEIP_ERROR << "Invalid service option (Multicast)";
				break;
			default:
				VSOMEIP_WARNING << "Unsupported service option";
				break;
			}
		}
	}
}

void service_discovery_impl::process_eventgroupentry(
		std::shared_ptr<eventgroupentry_impl> &_entry,
		const std::vector<std::shared_ptr<option_impl> > &_options) {
	service_t its_service = _entry->get_service();
	instance_t its_instance = _entry->get_instance();
	eventgroup_t its_eventgroup = _entry->get_eventgroup();
	major_version_t its_major = _entry->get_major_version();
	ttl_t its_ttl = _entry->get_ttl();

	VSOMEIP_DEBUG << "Eventgroup [" << std::hex << std::setw(4)
			<< std::setfill('0') << its_service << "." << its_instance << "."
			<< its_eventgroup << "], version " << std::dec << (int) its_major
			<< " is offered for " << its_ttl << " seconds.";
}

} // namespace sd
} // namespace vsomeip

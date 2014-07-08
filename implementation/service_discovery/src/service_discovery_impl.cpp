// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/configuration.hpp>
#include <vsomeip/logger.hpp>

#include "../include/constants.hpp"
#include "../include/ipv4_option_impl.hpp"
#include "../include/ipv6_option_impl.hpp"
#include "../include/message_impl.hpp"
#include "../include/runtime.hpp"
#include "../include/service_discovery_fsm.hpp"
#include "../include/service_discovery_host.hpp"
#include "../include/service_discovery_impl.hpp"
#include "../include/serviceentry_impl.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../routing/include/servicegroup.hpp"
#include "../../routing/include/serviceinfo.hpp"

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(service_discovery_host *_host) :
		host_(_host), io_(_host->get_io()), default_(
				std::make_shared < service_discovery_fsm > ("default", this)) {
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

	// SD endpoint
	boost::asio::ip::address its_address = its_configuration->get_address();
	uint16_t its_port = its_configuration->get_service_discovery_port();
	if (its_configuration->get_service_discovery_protocol() == "tcp") {
		endpoint_ = std::make_shared< tcp_server_endpoint_impl >(
						shared_from_this(),
						boost::asio::ip::tcp::endpoint(its_address, its_port),
						io_);
	} else {
		endpoint_ = std::make_shared< udp_server_endpoint_impl >(
						shared_from_this(),
						boost::asio::ip::udp::endpoint(its_address, its_port),
						io_);
	}
}

void service_discovery_impl::start() {
	endpoint_->start();

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

void service_discovery_impl::offer_service(service_t _service,
		instance_t _instance) {
	VSOMEIP_DEBUG << "sdi::offer_service [" << std::hex << _service << "."
			<< _instance << "]";
}

void service_discovery_impl::stop_offer_service(service_t _service,
		instance_t _instance) {
	VSOMEIP_DEBUG << "sdi::stop_offer_service [" << std::hex << _service << "."
			<< _instance << "]";
}

void service_discovery_impl::request_service(service_t _service,
		instance_t _instance, major_version_t _major, minor_version_t _minor,
		ttl_t _ttl) {
	VSOMEIP_DEBUG << "sdi::request_service [" << std::hex << _service << "."
			<< _instance << "]";
	auto find_service = requested_.find(_service);
	if (find_service != requested_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			// TODO: check version and report errors
		} else {
			find_service->second[_instance]
			    = std::make_shared< serviceinfo >(_major, _minor, _ttl);
		}
	} else {
		requested_[_service][_instance]
		    = std::make_shared< serviceinfo >(_major, _minor, _ttl);
	}
}

void service_discovery_impl::release_service(service_t _service,
		instance_t _instance) {
	auto find_service = requested_.find(_service);
	if (find_service != requested_.end()) {
		find_service->second.erase(_instance);
	}
}

void service_discovery_impl::insert_service_entries(
		std::shared_ptr< message_impl > &_message, service_map_t &_services,
		bool _is_offer) {
	for (auto its_service : _services) {
		for (auto its_instance : its_service.second) {
			auto its_info = its_instance.second;
			std::shared_ptr< serviceentry_impl > its_entry
				= _message->create_service_entry();
			if (its_entry) {
				its_entry->set_type((_is_offer ?
					entry_type_e::OFFER_SERVICE :
					entry_type_e::FIND_SERVICE));
				its_entry->set_service(its_service.first);
				its_entry->set_instance(its_instance.first);
				its_entry->set_major_version(its_info->get_major());
				its_entry->set_minor_version(its_info->get_minor());
				its_entry->set_ttl(its_info->get_ttl());

				std::shared_ptr< endpoint > its_endpoint
					= its_info->get_reliable_endpoint();
				if (its_endpoint) {
					if (its_endpoint->is_v4()) {
						std::shared_ptr< ipv4_option_impl > its_option
							= _message->create_ipv4_option(false);
						if (its_option) {
							std::vector< byte_t > its_address;
							if (its_endpoint->get_address(its_address)) {
								its_option->set_address(its_address);
								its_option->set_port(its_endpoint->get_port());
								its_option->set_udp(its_endpoint->is_udp());
								its_entry->assign_option(its_option, 1);
							}
						}
					} else {
						std::shared_ptr< ipv6_option_impl > its_option
							= _message->create_ipv6_option(false);
						if (its_option) {
							std::shared_ptr< ipv4_option_impl > its_option
								= _message->create_ipv4_option(false);
							if (its_option) {
								std::vector< byte_t > its_address;
								if (its_endpoint->get_address(its_address)) {
									its_option->set_address(its_address);
									its_option->set_port(its_endpoint->get_port());
									its_option->set_udp(its_endpoint->is_udp());
									its_entry->assign_option(its_option, 1);
								}
							}
						}
					}
				}
			} else {
				VSOMEIP_ERROR << "Failed to create service entry.";
			}
		}
	}
}


void service_discovery_impl::send(const std::string &_name, bool _is_announcing) {
	//std::unique_lock its_lock(serializer_mutex_);

	std::shared_ptr< message_impl > its_message = runtime::get()->create_message();

	// TODO: optimize building of SD message (common options)

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
void service_discovery_impl::on_message(const byte_t *_data, length_t _length,
		endpoint *_receiver) {
	VSOMEIP_DEBUG << "sdi::on_message";
}

} // namespace sd
} // namespace vsomeip

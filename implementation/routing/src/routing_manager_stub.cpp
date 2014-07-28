// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <functional>
#include <iomanip>

#include <boost/system/error_code.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/logger.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/routing_manager.hpp"
#include "../include/routing_manager_stub.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/local_server_endpoint_impl.hpp"

namespace vsomeip {

routing_manager_stub::routing_manager_stub(routing_manager *_routing) :
		routing_(_routing), io_(_routing->get_io()), watchdog_timer_(
				_routing->get_io()) {
}

routing_manager_stub::~routing_manager_stub() {
}

void routing_manager_stub::init() {
	std::stringstream its_endpoint_path;
	its_endpoint_path << base_path << VSOMEIP_ROUTING_CLIENT;
	::unlink(its_endpoint_path.str().c_str());

	VSOMEIP_DEBUG << "Routing endpoint at " << its_endpoint_path.str();
	endpoint_ =
			std::make_shared < local_server_endpoint_impl
					> (shared_from_this(), boost::asio::local::stream_protocol::endpoint(
							its_endpoint_path.str()), io_);
}

void routing_manager_stub::start() {
	endpoint_->start();

	// Start watchdog (TODO: only if configured)
	start_watchdog();
}

void routing_manager_stub::stop() {
	watchdog_timer_.cancel();
	endpoint_->stop();

	std::stringstream its_endpoint_path;
	its_endpoint_path << base_path << VSOMEIP_ROUTING_CLIENT;
	::unlink(its_endpoint_path.str().c_str());
}

routing_manager * routing_manager_stub::get_manager() {
	return routing_;
}

void routing_manager_stub::on_connect(std::shared_ptr<endpoint> _endpoint) {

}

void routing_manager_stub::on_disconnect(std::shared_ptr<endpoint> _endpoint) {

}

void routing_manager_stub::on_message(const byte_t *_data, length_t _size,
		endpoint *_receiver) {
#if 0
	std::cout << "rms::on_message: ";
	for (int i = 0; i < _size; ++i)
	std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
	std::cout << std::endl;
#endif

	if (VSOMEIP_COMMAND_SIZE_POS_MAX < _size) {
		byte_t its_command;
		client_t its_client;
		std::string its_client_endpoint;
		service_t its_service;
		instance_t its_instance;
		eventgroup_t its_eventgroup;
		event_t its_event;
		major_version_t its_major;
		minor_version_t its_minor;
		ttl_t its_ttl;
		std::shared_ptr<payload> its_payload;
		const byte_t *its_data;
		uint32_t its_size;
		bool its_reliable;
		bool its_flush;

		its_command = _data[VSOMEIP_COMMAND_TYPE_POS];
		std::memcpy(&its_client, &_data[VSOMEIP_COMMAND_CLIENT_POS],
				sizeof(its_client));
		std::memcpy(&its_size, &_data[VSOMEIP_COMMAND_SIZE_POS_MIN],
				sizeof(its_size));

		if (its_size <= _size - VSOMEIP_COMMAND_HEADER_SIZE) {
			switch (its_command) {
			case VSOMEIP_REGISTER_APPLICATION:
				(void) routing_->find_or_create_local(its_client);
				routing_info_[its_client].first = 0;
				broadcast_routing_info();
				VSOMEIP_DEBUG << "Application/Client " << its_client
						<< " got registered!";
				break;

			case VSOMEIP_DEREGISTER_APPLICATION:
				on_deregister_application(its_client);
				VSOMEIP_DEBUG << "Application/Client " << its_client
						<< " got deregistered!";
				break;

			case VSOMEIP_PONG:
				on_pong(its_client);
				break;

			case VSOMEIP_OFFER_SERVICE:
				std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
						sizeof(its_service));
				std::memcpy(&its_instance,
						&_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
						sizeof(its_instance));
				std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
						sizeof(its_major));
				std::memcpy(&its_minor, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 5],
						sizeof(its_minor));
				std::memcpy(&its_ttl, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 9],
						sizeof(its_ttl));
				routing_->offer_service(its_client, its_service, its_instance,
						its_major, its_minor, its_ttl);
				on_offer_service(its_client, its_service, its_instance);
				break;

			case VSOMEIP_STOP_OFFER_SERVICE:
				std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
						sizeof(its_service));
				std::memcpy(&its_instance,
						&_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
						sizeof(its_instance));
				routing_->stop_offer_service(its_client, its_service,
						its_instance);
				on_stop_offer_service(its_client, its_service, its_instance);
				break;

			case VSOMEIP_PUBLISH_EVENTGROUP:
				routing_->publish_eventgroup(its_client, its_service,
						its_instance, its_eventgroup, its_major, its_ttl);
				break;

			case VSOMEIP_STOP_PUBLISH_EVENTGROUP:
				routing_->stop_publish_eventgroup(its_client, its_service,
						its_instance, its_eventgroup);
				break;

			case VSOMEIP_SEND:
				its_data = &_data[VSOMEIP_COMMAND_PAYLOAD_POS];
				std::memcpy(&its_instance, &_data[_size - 4],
						sizeof(its_instance));
				its_flush = static_cast<bool>(_data[_size - 2]);
				its_reliable = static_cast<bool>(_data[_size - 1]);
				routing_->send(its_client, its_data, its_size, its_instance,
						its_flush, its_reliable);
				break;

			case VSOMEIP_SET:
				its_data = &_data[VSOMEIP_COMMAND_PAYLOAD_POS];
				its_payload = runtime::get()->create_payload(its_data, its_size);
				routing_->set(its_client, its_service, its_instance, its_event,
						its_payload);
				break;
			}
		}
	}
}

void routing_manager_stub::on_register_application(client_t _client) {

}

void routing_manager_stub::on_deregister_application(client_t _client) {
	auto its_info = routing_info_.find(_client);
	if (its_info != routing_info_.end()) {
		for (auto &its_service : its_info->second.second) {
			for (auto &its_instance : its_service.second) {
				routing_->stop_offer_service(_client, its_service.first,
						its_instance);
			}
		}
	}

	routing_->remove_local(_client);
	routing_info_.erase(_client);
	broadcast_routing_info();
}

void routing_manager_stub::on_offer_service(client_t _client,
		service_t _service, instance_t _instance) {
	routing_info_[_client].second[_service].insert(_instance);
	broadcast_routing_info();
}

void routing_manager_stub::on_stop_offer_service(client_t _client,
		service_t _service, instance_t _instance) {
	auto found_client = routing_info_.find(_client);
	if (found_client != routing_info_.end()) {
		auto found_service = found_client->second.second.find(_service);
		if (found_service != found_client->second.second.end()) {
			auto found_instance = found_service->second.find(_instance);
			if (found_instance != found_service->second.end()) {
				found_service->second.erase(_instance);
				broadcast_routing_info();
			}
		}
	}
}

void routing_manager_stub::send_routing_info(client_t _client) {
	std::shared_ptr<endpoint> its_endpoint = routing_->find_local(_client);
	if (its_endpoint) {
		uint32_t its_capacity = 4096; // TODO: dynamic resizing
		std::vector<byte_t> its_command(its_capacity);
		its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_ROUTING_INFO;
		std::memset(&its_command[VSOMEIP_COMMAND_CLIENT_POS], 0,
				sizeof(client_t));
		uint32_t its_size = VSOMEIP_COMMAND_PAYLOAD_POS;
		for (auto &info : routing_info_) {
			uint32_t its_size_pos = its_size;
			uint32_t its_entry_size = its_size;

			its_size += sizeof(uint32_t); // placeholder

			std::memcpy(&its_command[its_size], &info.first, sizeof(client_t));
			its_size += sizeof(client_t);

			for (auto &service : info.second.second) {
				uint32_t its_service_entry_size = sizeof(service_t)
						+ service.second.size() * sizeof(instance_t);

				std::memcpy(&its_command[its_size], &its_service_entry_size,
						sizeof(uint32_t));
				its_size += sizeof(uint32_t);

				std::memcpy(&its_command[its_size], &service.first,
						sizeof(service_t));
				its_size += sizeof(service_t);

				for (auto &instance : service.second) {
					std::memcpy(&its_command[its_size], &instance,
							sizeof(instance_t));
					its_size += sizeof(instance_t);
				}
			}

			its_entry_size = its_size - its_entry_size - sizeof(uint32_t);
			std::memcpy(&its_command[its_size_pos], &its_entry_size,
					sizeof(uint32_t));
		}

		its_size -= VSOMEIP_COMMAND_PAYLOAD_POS;
		std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
				sizeof(its_size));
		its_size += VSOMEIP_COMMAND_PAYLOAD_POS;
#if 0
		std::cout << "rms::send_routing_info ";
		for (int i = 0; i < its_size; ++i)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)its_command[i] << " ";
		std::cout << std::endl;
#endif
		its_endpoint->send(&its_command[0], its_size, true);
	}
}

void routing_manager_stub::broadcast_routing_info() {
	for (auto& info : routing_info_) {
		send_routing_info(info.first);
	}
}

void routing_manager_stub::broadcast(std::vector<byte_t> &_command) const {
	for (auto a : routing_info_) {
		if (a.first > 0) {
			std::shared_ptr<endpoint> its_endpoint = routing_->find_local(
					a.first);
			if (its_endpoint) {
				its_endpoint->send(&_command[0], _command.size(), true);
			}
		}
	}
}

// Watchdog
void routing_manager_stub::broadcast_ping() const {
	const byte_t its_ping[] = {
	VSOMEIP_PING, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

	std::vector<byte_t> its_command(sizeof(its_ping));
	its_command.assign(its_ping, its_ping + sizeof(its_ping));
	broadcast(its_command);
}

void routing_manager_stub::on_pong(client_t _client) {
	auto found_info = routing_info_.find(_client);
	if (found_info != routing_info_.end()) {
		found_info->second.first = 0;
	} else {
		std::cerr << "Received PONG from unregistered application!"
				<< std::endl;
	}
}

void routing_manager_stub::start_watchdog() {
	watchdog_timer_.expires_from_now(
			std::chrono::milliseconds(VSOMEIP_DEFAULT_WATCHDOG_CYCLE)); // TODO: use config variable

	std::function<void(boost::system::error_code const &)> its_callback =
			[this](boost::system::error_code const &_error) {
				if (!_error)
				check_watchdog();
			};

	watchdog_timer_.async_wait(its_callback);
}

void routing_manager_stub::check_watchdog() {
	for (auto i = routing_info_.begin(); i != routing_info_.end(); ++i) {
		i->second.first++;
	}
	broadcast_ping();

	watchdog_timer_.expires_from_now(
			std::chrono::milliseconds(VSOMEIP_DEFAULT_WATCHDOG_TIMEOUT)); // TODO: use config variable

	std::function<void(boost::system::error_code const &)> its_callback =
			[this](boost::system::error_code const &_error) {
				std::list< client_t > lost;

				for (auto i : routing_info_) {
					if (i.first > 0) {
						if (i.second.first > VSOMEIP_DEFAULT_MAX_MISSING_PONGS) { // TODO: use config variable
							VSOMEIP_WARNING << "Lost contact to application " << std::hex << (int)i.first;
							lost.push_back(i.first);
						}
					}
				}

				for (auto i : lost) {
					routing_info_.erase(i);
				}

				if (0 < lost.size())
				send_application_lost(lost);

				start_watchdog();
			};

	watchdog_timer_.async_wait(its_callback);
}

void routing_manager_stub::send_application_lost(std::list<client_t> &_lost) {
	uint32_t its_size = _lost.size() * sizeof(client_t);
	std::vector<byte_t> its_command(VSOMEIP_COMMAND_HEADER_SIZE + its_size);
	its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_APPLICATION_LOST;
	std::memset(&its_command[VSOMEIP_COMMAND_CLIENT_POS], 0, sizeof(client_t));
	std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
			sizeof(uint32_t));

	uint32_t its_offset = 0;
	for (auto i : _lost) {
		std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + its_offset], &i,
				sizeof(client_t));
		its_offset += sizeof(client_t);
	}

	broadcast(its_command);
}

} // namespace vsomeip

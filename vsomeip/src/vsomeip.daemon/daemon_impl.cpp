//
// daemon_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <chrono>
#include <iomanip>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/log/trivial.hpp>

#include <boost_ext/asio/mq.hpp>
#include <boost_ext/asio/placeholders.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/message_base.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/version.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/enumeration_types.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/protocol.hpp>
#include <vsomeip_internal/tcp_client_impl.hpp>
#include <vsomeip_internal/tcp_service_impl.hpp>
#include <vsomeip_internal/udp_client_impl.hpp>
#include <vsomeip_internal/udp_service_impl.hpp>

#include "daemon_impl.hpp"

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

using namespace boost::log::trivial;

namespace vsomeip {

client_id daemon_impl::id__ = 1; // ID 0 is reserved for the daemon

daemon * daemon_impl::get_instance() {
	static daemon_impl the_daemon;
	return &the_daemon;
}

daemon_impl::daemon_impl()
	: managing_application_impl("vsomeip_daemon"),
	  daemon_queue_(receiver_service_),
	  queue_name_prefix_("/vsomeip-"),
	  daemon_queue_name_("0"),
	  watchdog_timer_(sender_service_),
	  sender_work_(sender_service_),
	  network_work_(service_)
#ifdef VSOMEIP_DAEMON_DEBUG
	  ,dump_timer_(sender_service_)
#endif
{
}

void daemon_impl::init(int _count, char **_options) {
	configuration::init(_count, _options);
	configuration *vsomeip_configuration = configuration::request();

	configure_logging(vsomeip_configuration->use_console_logger(),
			vsomeip_configuration->use_file_logger(),
			vsomeip_configuration->use_dlt_logger());

	set_channel("0");
	set_loglevel(vsomeip_configuration->get_loglevel());

	configuration::release();

	daemon_queue_.async_create(
			queue_name_prefix_ + daemon_queue_name_,
			10,	// TODO: replace
			VSOMEIP_QUEUE_SIZE,
			boost::bind(&daemon_impl::create_cbk, this,
					boost::asio::placeholders::error));

#ifdef VSOMEIP_DAEMON_DEBUG
	start_dump_cycle();
#endif

	VSOMEIP_INFO << "vsomeipd v" << VSOMEIP_VERSION << ": initialized.";
}

void daemon_impl::run_receiver() {
	receiver_service_.run();
	VSOMEIP_DEBUG << "Receiver services stopped.";
}

void daemon_impl::run_sender() {
	sender_service_.run();
	VSOMEIP_DEBUG << "Sender services stopped.";
}

void daemon_impl::run_network() {
	managing_application_impl::run();
	VSOMEIP_DEBUG << "Network services stopped.";
}

void daemon_impl::start() {
	boost::thread sender_thread(boost::bind(&daemon_impl::run_sender, this));
	boost::thread receiver_thread(boost::bind(&daemon_impl::run_receiver, this));
	boost::thread network_thread(boost::bind(&daemon_impl::run_network, this));

	VSOMEIP_INFO << "vsomeipd v" << VSOMEIP_VERSION << ": started.";
	sender_thread.join();
	receiver_thread.join();
	network_thread.join();
}

void daemon_impl::start_watchdog_cycle() {
	watchdog_timer_.expires_from_now(
			std::chrono::milliseconds(VSOMEIP_WATCHDOG_CYCLE));

	watchdog_timer_.async_wait(
			boost::bind(&daemon_impl::watchdog_cycle_cbk, this,
					boost::asio::placeholders::error));
}

void daemon_impl::start_watchdog_check() {
	for (auto i = applications_.begin(); i != applications_.end(); ++i) {
		i->second.watchdog_++;
		send_ping(i->first);
	}

	watchdog_timer_.expires_from_now(
			std::chrono::milliseconds(VSOMEIP_WATCHDOG_TIMEOUT));

	watchdog_timer_.async_wait(
			boost::bind(&daemon_impl::watchdog_check_cbk, this,
					boost::asio::placeholders::error));
}

void daemon_impl::do_send(client_id _id, std::vector< uint8_t > &_data) {
	auto found_application = applications_.find(_id);
	if (found_application != applications_.end()) {
		std::memcpy(&_data[VSOMEIP_PROTOCOL_ID], &_id, sizeof(_id));
		send_buffers_.push_back(_data);
		std::vector<uint8_t> &send_data = send_buffers_.back();

		found_application->second.queue_->async_send(
			send_data.data(),
			send_data.size(),
			0,
			boost::bind(
				&daemon_impl::send_cbk,
				this,
				boost::asio::placeholders::error,
				_id
			)
		);
	} else {
		VSOMEIP_WARNING << "No queue for application " << _id;
	}
}

void daemon_impl::do_broadcast(std::vector< uint8_t > &_data) {
	for (auto a : applications_) {
		do_send(a.first, _data);
	}
}

void daemon_impl::do_receive() {
	daemon_queue_.async_receive(receive_buffer_, sizeof(receive_buffer_),
			boost::bind(&daemon_impl::receive_cbk, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					boost_ext::asio::placeholders::priority));
}

void daemon_impl::receive(
		const uint8_t *_data, uint32_t _size,
		const endpoint *_source, const endpoint *_target) {

	client_id target_application = 0;
	service_id service = VSOMEIP_BYTES_TO_WORD(_data[0], _data[1]);
	method_id method = VSOMEIP_BYTES_TO_WORD(_data[2], _data[3]);
	message_type_enum message_type = static_cast< message_type_enum >(_data[14]);

	// Do we search for a client or a service?
	if (is_request(_data, _size)) {
		auto find_endpoint = service_channels_.find(_target);
		if (find_endpoint != service_channels_.end()) {
			auto find_service = find_endpoint->second.find(service);
			if (find_service != find_endpoint->second.end()) {
				auto find_method = find_service->second.second.find(method);
				if (find_method != find_service->second.second.end()) {
					target_application = find_service->second.first;
				} else {
					// TODO: add wildcard handling here?!
				}
			}
		}
	} else {
		client_id client = VSOMEIP_BYTES_TO_WORD(_data[8], _data[9]);
		auto find_client = client_channels_.find(client);
		if (find_client != client_channels_.end()) {
			auto find_service = find_client->second.find(service);
			if (find_service != find_client->second.end()) {
				auto find_method = find_service->second.second.find(method);
				if (find_method != find_service->second.second.end()) {
					target_application = find_service->second.first;
				} else {
					// TODO: add wildcard handling here?!
				}
			}
		}
	}

	// If the target application could be found, forward the message
	if (target_application > 0) {
		std::vector< uint8_t > message_buffer(
			_size +
			(2 * VSOMEIP_MAX_ENDPOINT_SIZE) +
			VSOMEIP_PROTOCOL_OVERHEAD
		);

		// Fill in start tag and command
		std::memcpy(&message_buffer[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
		message_buffer[VSOMEIP_PROTOCOL_COMMAND] = static_cast< uint8_t >(command_enum::SOMEIP_MESSAGE);

		// Set source application id
		std::memset(&message_buffer[VSOMEIP_PROTOCOL_ID], 0, sizeof(client_id));

		// Fill in message data
		std::memcpy(&message_buffer[VSOMEIP_PROTOCOL_PAYLOAD], _data, _size);

		// Fill in source / target endpoints
		if (!serializer_->serialize(_source)) {
			VSOMEIP_ERROR << "Serializing of source endpoint failed!";
			return;
		}
		uint32_t source_size = serializer_->get_size();
		std::memcpy(&message_buffer[VSOMEIP_PROTOCOL_PAYLOAD+_size], &source_size, sizeof(source_size));
		std::memcpy(&message_buffer[VSOMEIP_PROTOCOL_PAYLOAD+4+_size], serializer_->get_data(), serializer_->get_size());
		serializer_->reset();

		if (!serializer_->serialize(_target)) {
			VSOMEIP_ERROR << "Serializing of source endpoint failed!";
			return;
		}
		uint32_t target_size = serializer_->get_size();
		std::memcpy(&message_buffer[VSOMEIP_PROTOCOL_PAYLOAD+4+_size+source_size], &target_size, sizeof(target_size));
		std::memcpy(&message_buffer[VSOMEIP_PROTOCOL_PAYLOAD+8+_size+source_size], serializer_->get_data(), serializer_->get_size());
		serializer_->reset();

		uint32_t payload_size = 8 + _size + source_size + target_size;
		std::memcpy(&message_buffer[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &payload_size, sizeof(payload_size));
		std::memcpy(&message_buffer[VSOMEIP_PROTOCOL_PAYLOAD+8+_size+source_size+target_size], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));
		message_buffer.resize(VSOMEIP_PROTOCOL_OVERHEAD + payload_size);

		do_send(target_application, message_buffer);
	} else {
		VSOMEIP_WARNING << "Could not find target application!";
	}
}

void daemon_impl::on_register_application(client_id _id, const std::string &_name) {
	boost_ext::asio::message_queue *application_queue =
			new boost_ext::asio::message_queue(sender_service_);

	VSOMEIP_INFO << "Registering application " << _id << " for queue " << _name;

	application_info info;
	info.queue_ = application_queue;
	info.queue_name_ = _name;
	info.watchdog_ = 0;

	auto found_application = applications_.find(_id);
	if (found_application != applications_.end()) {
		while ((found_application = applications_.find(id__)) != applications_.end()) {
			id__++;
		}

		VSOMEIP_WARNING << "As an application with id=\"" << _id << "\" is already registered, "
				           "the application id will be modified to \"" << id__ << "\"";
		_id = id__;
	}

	applications_[_id] = info;

	application_queue->async_open(
			queue_name_prefix_ + _name,
			boost::bind(&daemon_impl::open_cbk, this,
					boost::asio::placeholders::error, _id));
}

void daemon_impl::on_deregister_application(client_id _id) {
	applications_.erase(_id);

	/*
	 application_queue->async_close(
	 _name.c_str(),
	 boost::bind(
	 &daemon_impl::close_cbk,
	 this,
	 boost::asio::placeholders::error,
	 id__
	 )
	 );
	 */
}

void daemon_impl::on_provide_service(client_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") PROVIDES ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";

	auto find_application = applications_.find(_id);
	if (find_application != applications_.end()) {
		auto find_service = find_application->second.services_.find(_service);
		if (find_service != find_application->second.services_.end()) {
			// the given location must not be used for another instance of the service
			for (auto i : find_service->second) {
				if (i.first != _instance) {
					for (auto j : i.second) {
						if (j == _location) {
							// TODO: send message to application that registration has failed (reason: another service is already registered)
							BOOST_LOG_SEV(logger_, error)
								<< "Registration failed as another instance of the service is already registered to the same endpoint";
							return;
						}
					}
				}
			}

			auto find_instance = find_service->second.find(_instance);
			if (find_instance != find_service->second.end()) {
				find_instance->second.insert(_location);
			} else {
				find_service->second[_instance].insert(_location);
			}
		} else {
			find_application->second.services_[_service][_instance].insert(_location);
		}

		// create a channel for the new service
		service_channels_[_location][_service] = std::make_pair(_id, std::set< method_id >());

		// maybe the new provided service was already requested
		for (auto r : requests_) {
			if (r.service_ == _service && r.instance_ == _instance) {
				VSOMEIP_INFO << "  reporting service location to application " << r.id_;
				on_request_service(r.id_, r.service_, r.instance_, r.location_);
			} else {
				VSOMEIP_INFO << std::hex << r.service_ << " == " << _service << ", " << r.instance_ << " == " << _instance;
			}
		}

		// open the corresponding IP access point
		(void) find_or_create_service(_location);

	} else {
		VSOMEIP_ERROR << "Attempting to register service for unknown application!";
	}
}

void daemon_impl::on_withdraw_service(client_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") WITHDRAWS ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";

	auto find_application = applications_.find(_id);
	if (find_application != applications_.end()) {
		auto find_service = find_application->second.services_.find(_service);
		if (find_service != find_application->second.services_.end()) {
			if (0 == _location) {
				find_service->second.erase(_instance);
			} else {
				auto find_instance = find_service->second.find(_instance);
				if (find_instance != find_service->second.end()) {
					find_instance->second.erase(_location);
				}
			}
			if (find_service->second.size() == 0)
				find_application->second.services_.erase(_service);
		}
	}

	// remove the channel for the service
	auto found_location = service_channels_.find(_location);
	if (found_location != service_channels_.end()) {
		found_location->second.erase(_service);
		if (0 == found_location->second.size()) {
			service_channels_.erase(_location);
		}
	}
}

void daemon_impl::on_start_service(client_id _id, service_id _service, instance_id _instance) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") STARTS ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";
}

void daemon_impl::on_stop_service(client_id _id, service_id _service, instance_id _instance) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") STOPS ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";
}

void daemon_impl::on_request_service(client_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") REQUESTS ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";

	request_info info(_id, _service, _instance, _location);
	requests_.insert(info);

	// create a channel for the new service
	client_channels_[_id][_service] = std::make_pair(_id, std::set< method_id >());

	for (auto a : applications_) {
		auto s = a.second.services_.find(_service);
		if (s != a.second.services_.end()) {
			if (_instance == VSOMEIP_ANY_INSTANCE && 0 < s->second.size()) {
				send_request_service_ack(_id, _service, s->second.begin()->first, a.second.queue_name_);
				return;
			} else {
				auto i = s->second.find(_instance);
				if (i != s->second.end()) {
					send_request_service_ack(_id, _service, _instance, a.second.queue_name_);
					return;
				}
			}
		}
	}
}

void daemon_impl::on_release_service(client_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") RELEASES ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";

	// remove the channel for the client
	auto found_client = client_channels_.find(_id);
	if (found_client != client_channels_.end()) {
		found_client->second.erase(_service);
		if (0 == found_client->second.size()) {
			client_channels_.erase(_id);
		}
	}

	request_info info(_id, _service, _instance, _location);
	requests_.erase(info);
}

void daemon_impl::on_register_method(client_id _id, service_id _service, method_id _method) {
	for (auto& e : service_channels_) {
		auto found_service = e.second.find(_service);
		if (found_service != e.second.end()) {
			if (found_service->second.first == _id) {
				found_service->second.second.insert(_method);
			}
		}
	}

	for (auto& c : client_channels_) {
		auto found_service = c.second.find(_service);
		if (found_service != c.second.end()) {
			if (found_service->second.first == _id) {
				found_service->second.second.insert(_method);
			}
		}
	}
}

void daemon_impl::on_deregister_method(client_id _id, service_id _service, method_id _method) {
	for (auto& e : service_channels_) {
		auto found_service = e.second.find(_service);
		if (found_service != e.second.end()) {
			if (found_service->second.first == _id) {
				found_service->second.second.erase(_method);
			}
		}
	}

	for (auto& c : client_channels_) {
		auto found_service = c.second.find(_service);
		if (found_service != c.second.end()) {
			if (found_service->second.first == _id) {
				found_service->second.second.erase(_method);
			}
		}
	}
}

void daemon_impl::on_pong(client_id _id) {
	auto info = applications_.find(_id);
	if (info != applications_.end()) {
		info->second.watchdog_ = 0;
	} else {
		VSOMEIP_WARNING
			<< "Received PONG from unregistered application " << _id;
	}
}

void daemon_impl::on_message(client_id _id, const uint8_t *_data, uint32_t _size) {
	const uint8_t *payload = &_data[VSOMEIP_PROTOCOL_PAYLOAD];
	uint32_t payload_size;
	std::memcpy(&payload_size, &_data[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], sizeof(payload_size));

	length message_size = VSOMEIP_BYTES_TO_LONG(
		payload[4], payload[5], payload[6], payload[7]
	)  + VSOMEIP_STATIC_HEADER_SIZE;

	// Get source and target endpoint
	uint32_t source_size;
	std::memcpy(&source_size, &payload[message_size], sizeof(source_size));

	endpoint *source
		= factory::get_instance()->get_endpoint(
				&payload[message_size + 4],
				source_size
		  );

	uint32_t target_size;
	std::memcpy(&target_size, &payload[message_size + source_size + 4], sizeof(target_size));

	endpoint *target
		= factory::get_instance()->get_endpoint(
				&payload[message_size + source_size + 8],
				target_size
		  );

	service_id the_service = VSOMEIP_BYTES_TO_WORD(payload[0], payload[1]);
	client_id the_client = VSOMEIP_BYTES_TO_WORD(payload[8], payload[9]);

	if (is_request(payload, payload_size)) {
		auto found_endpoint = service_channels_.find(target);
		if (found_endpoint  != service_channels_.end()) {
			auto found_service = found_endpoint->second.find(the_service);
			if (found_service != found_endpoint->second.end()) {
				std::vector< uint8_t > message_data(_data, _data + _size);
				do_send(found_service->second.first, message_data);
			} else {
				VSOMEIP_ERROR << "Found the endpoint, but the requested service is not registered!";
			}
		} else {
			client *target_client = find_or_create_client(target);
			if (target_client) {
				target_client->send(payload, message_size);
			} else {
				VSOMEIP_ERROR << "Client for target endpoint ("
					<< (target ? target->get_address() : "UNKNOWN")
					<< ":" << std::dec << (target ? target->get_port() : 0)
					<< ") could not be found!";

			}
		}
	} else {
		// find the application that holds the target client and forward the message
		auto found_client = client_channels_.find(the_client);
		if (found_client != client_channels_.end()) {
			auto found_service = found_client->second.find(the_service);
			if (found_service != found_client->second.end()) {
				std::vector< uint8_t > message_data(_data, _data + _size);
				do_send(found_service->second.first, message_data);
			} else {
				VSOMEIP_ERROR << "Found the client, but the service is not requested!";
			}
		} else {
			service *target_service = find_service(source);
			if (target_service) {
				target_service->send(payload, message_size, target);
			} else {
				VSOMEIP_ERROR << "Service for source endpoint ("
					<< (source ? source->get_address() : "UNKNOWN")
					<< ":" << std::dec << (source ? source->get_port() : 0)
					<< ") could not be found!";			}
		}
	}
}

bool daemon_impl::request_service(service_id _service, instance_id _instance,
		const endpoint *_location) {

	if (0 == _location)
		return false;

	client *the_client = find_or_create_client(_location);
	if (0 == the_client)
		return false;

	the_client->open_filter(_service);

	return true;
}

bool daemon_impl::provide_service(service_id _service, instance_id _instance,
		const endpoint *_location) {

	if (0 == _location)
		return false;

	service *the_service = find_or_create_service(_location);
	if (0 == the_service)
		return false;

	the_service->open_filter(_service);

	return true;
}

bool daemon_impl::is_request(const message_base *_message) const {
	return (_message->get_message_type() < message_type_enum::RESPONSE);
}

bool daemon_impl::is_request(const uint8_t *_data, uint32_t _size) const {
	return (_size >= VSOMEIP_POS_MESSAGE_TYPE &&
			static_cast< message_type_enum >(_data[VSOMEIP_POS_MESSAGE_TYPE]) < message_type_enum::RESPONSE);
}

void daemon_impl::send_request_service_ack(client_id _id, service_id _service, instance_id _instance, const std::string &_queue_name) {
	std::vector< uint8_t > request_ack_buffer;

	// Resize to the needed size
	uint32_t payload_size = 4 + _queue_name.size();
	request_ack_buffer.resize(payload_size + VSOMEIP_PROTOCOL_OVERHEAD);

	std::memcpy(&request_ack_buffer[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memset(&request_ack_buffer[VSOMEIP_PROTOCOL_ID], 0, sizeof(client_id));
	request_ack_buffer[VSOMEIP_PROTOCOL_COMMAND] = static_cast< uint8_t >(command_enum::REQUEST_SERVICE_ACK);
	std::memcpy(&request_ack_buffer[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &payload_size, sizeof(payload_size));
	std::memcpy(&request_ack_buffer[VSOMEIP_PROTOCOL_PAYLOAD], &_service, sizeof(_service));
	std::memcpy(&request_ack_buffer[VSOMEIP_PROTOCOL_PAYLOAD+2], &_instance, sizeof(_instance));
	std::copy(_queue_name.begin(), _queue_name.end(), request_ack_buffer.begin() + VSOMEIP_PROTOCOL_PAYLOAD+4);
	std::memcpy(&request_ack_buffer[payload_size + VSOMEIP_PROTOCOL_PAYLOAD], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));

	do_send(_id, request_ack_buffer);
}

void daemon_impl::send_ping(client_id _id) {
	uint8_t ping_message[] = {
		0x67, 0x37, 0x6D, 0x07,
		0x00, 0x00,
		static_cast<uint8_t>(command_enum::PING), 0x00, 0x00, 0x00, 0x00,
		0x07, 0x6D, 0x37, 0x67
	};

	std::vector<uint8_t> ping_buffer(ping_message,
			ping_message + sizeof(ping_message));

	do_send(_id, ping_buffer);
}

void daemon_impl::send_application_info() {
	std::vector< uint8_t > message_data;

	// determine the message size
	std::size_t message_size = VSOMEIP_PROTOCOL_OVERHEAD;
	for (auto a: applications_)
		message_size += (8 + a.second.queue_name_.size());

	// resize vector and fill in the static part
	message_data.resize(message_size);
	std::memcpy(&message_data[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&message_data[message_size-4], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));
	message_data[VSOMEIP_PROTOCOL_COMMAND] = static_cast<uint8_t>(command_enum::APPLICATION_INFO);

	message_size -= VSOMEIP_PROTOCOL_OVERHEAD;
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &message_size, sizeof(message_size));

	std::size_t position = VSOMEIP_PROTOCOL_PAYLOAD;
	for (auto a: applications_) {
		std::size_t queue_name_size = a.second.queue_name_.size();

		// Application Id
		std::memcpy(&message_data[position], &a.first, sizeof(a.first));
		position += 4;

		// Queue name size
		std::memcpy(&message_data[position], &queue_name_size, sizeof(message_size));
		position += 4;

		// Queue name
		std::copy(a.second.queue_name_.begin(), a.second.queue_name_.end(),
				  message_data.begin() + position);
		position += queue_name_size;
	}

	do_broadcast(message_data);
}

void daemon_impl::send_application_lost(const std::list< client_id > &_lost_applications) {
	uint32_t message_size = VSOMEIP_PROTOCOL_OVERHEAD
		+ _lost_applications.size() * sizeof(client_id);

	std::vector< uint8_t > message_data(message_size);

	std::memcpy(&message_data[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&message_data[message_size-4], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));
	std::memset(&message_data[VSOMEIP_PROTOCOL_ID], 0, sizeof(client_id));
	message_data[VSOMEIP_PROTOCOL_COMMAND] = static_cast<uint8_t>(command_enum::APPLICATION_LOST);
	message_size -= VSOMEIP_PROTOCOL_OVERHEAD;
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &message_size, sizeof(message_size));

	for (int i = 0; i < message_data.size(); ++i) {
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)message_data[i] << " ";
	}
	std::cout << std::endl;

	uint32_t position = VSOMEIP_PROTOCOL_PAYLOAD;
	for (auto a : _lost_applications) {
		std::memcpy(&message_data[position], &a, sizeof(a));
		position += sizeof(a);
	}

	do_broadcast(message_data);
}

void daemon_impl::process_command(std::size_t _bytes) {
	uint32_t start_tag, end_tag, payload_size;
	command_enum command;
	client_id an_id;
	service_id a_service;
	instance_id an_instance;
	method_id a_method;
	endpoint *location = 0;

	std::memcpy(&start_tag, &receive_buffer_[0], sizeof(start_tag));
	std::memcpy(&end_tag, &receive_buffer_[_bytes-4], sizeof(end_tag));

	std::memcpy(&an_id, &receive_buffer_[VSOMEIP_PROTOCOL_ID], sizeof(an_id));
	command = static_cast<command_enum>(receive_buffer_[VSOMEIP_PROTOCOL_COMMAND]);
	std::memcpy(&payload_size, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], sizeof(payload_size));

	switch (command) {
	case command_enum::REGISTER_APPLICATION: {
		std::string queue_name((char*) &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], payload_size);
		on_register_application(an_id, queue_name);
		}
		break;

	case command_enum::DEREGISTER_APPLICATION:
		on_deregister_application(an_id);
		break;

	case command_enum::REGISTER_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(an_instance));
		location = factory::get_instance()->get_endpoint(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+4], payload_size - 4);
		on_provide_service(an_id, a_service, an_instance, location);
		break;

	case command_enum::DEREGISTER_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(an_instance));
		if (payload_size - 4 > 0)
			location = factory::get_instance()->get_endpoint(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+4], payload_size - 4);
		on_withdraw_service(an_id, a_service, an_instance, location);
		break;

	case command_enum::REQUEST_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(an_instance));
		location = factory::get_instance()->get_endpoint(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+4], payload_size - 4);
		on_request_service(an_id, a_service, an_instance, location);
		break;

	case command_enum::RELEASE_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(an_instance));
		if (payload_size - 4 > 0)
			location = factory::get_instance()->get_endpoint(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+4], payload_size - 4);
		on_release_service(an_id, a_service, an_instance, location);
		break;

	case command_enum::REGISTER_METHOD:
		std::memcpy(&a_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(a_service));
		std::memcpy(&a_method, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(a_method));
		on_register_method(an_id, a_service, a_method);
		break;

	case command_enum::DEREGISTER_METHOD:
		std::memcpy(&a_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(a_service));
		std::memcpy(&a_method, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(a_method));
		on_deregister_method(an_id, a_service, a_method);
		break;

	case command_enum::PONG:
		on_pong(an_id);
		break;

	case command_enum::SOMEIP_MESSAGE:
		on_message(an_id, &receive_buffer_[0], _bytes);
		break;

	default:
		VSOMEIP_WARNING << "Unknown command " << (int)command << " received.";
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////
void daemon_impl::open_cbk(boost::system::error_code const &_error, client_id _id) {
	if (!_error) {
		VSOMEIP_INFO << "Distributing application info";
		send_application_info();
	} else {
		VSOMEIP_ERROR	<< "Opening queue for " << _id << " failed!";
	}
}

void daemon_impl::create_cbk(boost::system::error_code const &_error) {
	if (!_error) {
		start_watchdog_cycle();
		do_receive();
	} else {
		// Try destroying before creating
		daemon_queue_.async_close(
			boost::bind(
				&daemon_impl::destroy_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// TODO: define maximum number of retries
		daemon_queue_.async_create(
			queue_name_prefix_ + daemon_queue_name_,
			10,
			VSOMEIP_QUEUE_SIZE,
			boost::bind(
				&daemon_impl::create_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}
}

void daemon_impl::destroy_cbk(boost::system::error_code const &_error) {

}

void daemon_impl::send_cbk(boost::system::error_code const &_error,
		client_id _id) {
	if (_error) {
		VSOMEIP_ERROR << "Message sending failed (Application " << _id << ")";
	}
}

void daemon_impl::receive_cbk(boost::system::error_code const &_error,
		std::size_t _bytes, unsigned int _priority) {

	if (!_error) {
		process_command(_bytes);
	} else {
		VSOMEIP_ERROR << "Receive error (" << _error.message() << ")";
	}

	do_receive();
}

void daemon_impl::watchdog_cycle_cbk(boost::system::error_code const &_error) {
	if (!_error) {
		start_watchdog_check();
	}
}

void daemon_impl::watchdog_check_cbk(boost::system::error_code const &_error) {
	if (!_error) {
		std::list< client_id > gone;

		for (auto i : applications_) {
			if (i.second.watchdog_ > VSOMEIP_MAX_MISSING_PONGS) {
				VSOMEIP_WARNING	<< "Lost contact to application " << (int)i.first;
				gone.push_back(i.first);
				try {
					i.second.queue_->unlink();
				}
				catch (...) {
					// we tried to clean up and do not care about errors...
				}
			}
		}

		for (auto i : gone) {
			applications_.erase(i);
		}

		if (0 < gone.size())
			send_application_lost(gone);

		start_watchdog_cycle();
	}
}

#ifdef VSOMEIP_DAEMON_DEBUG
#define VSOMEIP_DUMP_CYCLE	10000

void daemon_impl::start_dump_cycle() {
	dump_timer_.expires_from_now(
		std::chrono::milliseconds(VSOMEIP_DUMP_CYCLE));

	dump_timer_.async_wait(
		boost::bind(&daemon_impl::dump_cycle_cbk, this,
					boost::asio::placeholders::error));
}

void daemon_impl::dump_cycle_cbk(boost::system::error_code const &_error) {
	if (!_error) {
		for (auto a : applications_) {
			VSOMEIP_TRACE << "App (" << a.first << ") ";

			for (auto s : a.second.services_) {
				VSOMEIP_TRACE << "  " << std::hex << s.first;
				for (auto i : s.second) {
					VSOMEIP_TRACE << "    " << std::hex << i.first;

					for (auto e : i.second) {
						VSOMEIP_TRACE << "      ["
							<< e->get_address() << ":" << std::dec << e->get_port()
							<< " " << (e->get_protocol() == ip_protocol::UDP ? "udp" : "tcp")
							<< "]";
					}
				}
			}
		}
	}

	start_dump_cycle();
}
#endif

} // namespace vsomeip

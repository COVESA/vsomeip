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
#include <vsomeip_internal/tcp_client_impl.hpp>
#include <vsomeip_internal/tcp_service_impl.hpp>
#include <vsomeip_internal/udp_client_impl.hpp>
#include <vsomeip_internal/udp_service_impl.hpp>

#include "daemon_impl.hpp"

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

using namespace boost::log::trivial;

namespace vsomeip {

application_id daemon_impl::id__ = 1; // ID 0 is reserved for the daemon

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

void daemon_impl::do_send(application_id _id, std::vector< uint8_t > &_data) {
	auto found_application = applications_.find(_id);
	if (found_application != applications_.end()) {
		std::memcpy(&_data[4], &_id, sizeof(_id));
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
		VSOMEIP_ERROR << "No queue for application " << _id;
	}
}

void daemon_impl::do_broadcast(std::vector< uint8_t > &_data) {
	for (auto a : applications_)
		do_send(a.first, _data);
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

	service_id service = VSOMEIP_BYTES_TO_WORD(_data[0], _data[1]);
	auto found_service = channels_.find(service);

	if (found_service != channels_.end()) {

		method_id method = VSOMEIP_BYTES_TO_WORD(_data[2], _data[3]);
		auto found_method = found_service->second.find(method);

		if (found_method != found_service->second.end()) {

			std::vector< uint8_t > message_buffer(
				_size +
				(2 * VSOMEIP_MAX_ENDPOINT_SIZE) +
				VSOMEIP_PROTOCOL_OVERHEAD
			);

			// Fill in start tag and command
			std::memcpy(&message_buffer[0], &VSOMEIP_START_TAG, sizeof(VSOMEIP_START_TAG));
			message_buffer[8] = static_cast< uint8_t >(command_enum::SOMEIP_MESSAGE);

			// Fill in message data
			std::memcpy(&message_buffer[13], _data, _size);

			// Fill in source / target endpoints
			if (!serializer_->serialize(_source)) {
				VSOMEIP_ERROR << "Serializing of source endpoint failed!";
				return;
			}
			uint32_t source_size = serializer_->get_size();
			std::memcpy(&message_buffer[13+_size], &source_size, sizeof(source_size));
			std::memcpy(&message_buffer[17+_size], serializer_->get_data(), serializer_->get_size());
			serializer_->reset();

			if (!serializer_->serialize(_target)) {
				VSOMEIP_ERROR << "Serializing of source endpoint failed!";
				return;
			}
			uint32_t target_size = serializer_->get_size();
			std::memcpy(&message_buffer[17+_size+source_size], &target_size, sizeof(target_size));
			std::memcpy(&message_buffer[21+_size+source_size], serializer_->get_data(), serializer_->get_size());
			serializer_->reset();

			uint32_t payload_size = 8 + _size + source_size + target_size;
			std::memcpy(&message_buffer[9], &payload_size, sizeof(payload_size));
			std::memcpy(&message_buffer[21+_size+source_size+target_size], &VSOMEIP_END_TAG, sizeof(VSOMEIP_END_TAG));
			message_buffer.resize(VSOMEIP_PROTOCOL_OVERHEAD + payload_size);

			for (auto a : found_method->second) {
				std::memcpy(&message_buffer[4], &a, sizeof(a));
				do_send(a, message_buffer);
			}
		} else {
			VSOMEIP_WARNING << "Service is registered, but not for method " << std::hex << method;
		}
	} else {
		VSOMEIP_WARNING << "Service " << std::hex << service << " is not registered.";
	}
}

void daemon_impl::on_register_application(const std::string &_name) {
	boost_ext::asio::message_queue *application_queue =
			new boost_ext::asio::message_queue(sender_service_);

	application_queue->async_open(
			queue_name_prefix_ + _name,
			boost::bind(&daemon_impl::open_cbk, this,
					boost::asio::placeholders::error, id__));

	application_info info;
	info.queue_ = application_queue;
	info.queue_name_ = _name;
	info.watchdog_ = 0;

	applications_[id__] = info;

	id__++;
}

void daemon_impl::on_deregister_application(application_id _id) {
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

void daemon_impl::on_provide_service(application_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
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

		// make the application accessible by (endpoint, service-id)
		service_to_application_[_location][_service] = _id;

		// maybe the new provided service was already requested
		for (auto r : requests_) {
			if (r.service_ == _service && r.instance_ == _instance) {
				on_request_service(r.id_, r.service_, r.instance_, r.location_);
			}
		}

		// open the corresponding IP access point
		(void) find_or_create_service(_location);

	} else {
		VSOMEIP_ERROR << "Attempting to register service for unknown application!";
	}
}

void daemon_impl::on_withdraw_service(application_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
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
}

void daemon_impl::on_start_service(application_id _id, service_id _service, instance_id _instance) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") STARTS ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";
}

void daemon_impl::on_stop_service(application_id _id, service_id _service, instance_id _instance) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") STOPS ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";
}

void daemon_impl::on_request_service(application_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") REQUESTS ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";

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

	request_info info(_id, _service, _instance, _location);
	requests_.insert(info);
}

void daemon_impl::on_release_service(application_id _id, service_id _service, instance_id _instance, const endpoint *_location) {
	VSOMEIP_DEBUG
		<< "APPLICATION " << "(" << _id
		<< ") RELEASES ("
		<< std::hex << std::setw(4) << std::setfill('0')
		<< _service << ", " << _instance
		<< ")";

	request_info info(_id, _service, _instance, _location);
	requests_.erase(info);
}

void daemon_impl::on_register_method(application_id _id, service_id _service, method_id _method) {
	channels_[_service][_method].insert(_id);
}

void daemon_impl::on_deregister_method(application_id _id, service_id _service, method_id _method) {
}

void daemon_impl::on_pong(uint32_t _id) {
	auto info = applications_.find(_id);
	if (info != applications_.end()) {
		info->second.watchdog_ = 0;
	} else {
		VSOMEIP_WARNING
			<< "Received PONG from unregistered Application " << _id;
	}
}

void daemon_impl::on_message(application_id _id, const uint8_t *_data, uint32_t _size) {
	const uint8_t *payload = &_data[13];
	uint32_t payload_size;
	std::memcpy(&payload_size, &_data[9], sizeof(payload_size));

	length message_size = VSOMEIP_BYTES_TO_LONG(payload[4], payload[5], payload[6], payload[7])  + VSOMEIP_STATIC_HEADER_SIZE;

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

	message_type_enum message_type = static_cast< message_type_enum >(payload[14]);
	if (message_type < message_type_enum::RESPONSE) {
		service_id service = VSOMEIP_BYTES_TO_WORD(payload[0], payload[1]);

		auto found_service = service_to_application_.find(target);
		if (found_service  != service_to_application_.end()) {
			auto found_application = found_service->second.find(service);
			if (found_application != found_service->second.end()) {
				std::vector< uint8_t > message_data(_data, _data + _size);
				VSOMEIP_DEBUG << "Forwarding to application " << found_application->second;
				do_send(found_application->second, message_data);
			} else {
				VSOMEIP_ERROR << "Found the endpoint, but the requested service is not registered!";
			}
		} else {
			client *client = find_or_create_client(target);
			if (client) {
				client->send(payload, message_size);
			} else {
				VSOMEIP_ERROR << "Client for target endpoint ("
					<< (target ? target->get_address() : "UNKNOWN")
					<< ":" << std::dec << (target ? target->get_port() : 0)
					<< ") could not be found!";

			}
		}
	} else {
		client_id client = VSOMEIP_BYTES_TO_WORD(payload[8], payload[9]);

		// find the application that holds the target client and forward the message
		auto found_application = client_to_application_.find(client);
		if (found_application != client_to_application_.end()) {
			std::vector< uint8_t > message_data(_data, _data + _size);
			do_send(found_application->second, message_data);
		} else {

			service *service = find_service(source);
			if (service) {
				service->send(payload, message_size, target);
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

bool daemon_impl::is_client_message(const message_base *_message) {
	return (_message->get_message_type() < message_type_enum::REQUEST_ACK);
}

void daemon_impl::send_request_service_ack(application_id _id, service_id _service, instance_id _instance, const std::string &_queue_name) {
	std::vector< uint8_t > request_ack_buffer;

	// Resize to the needed size
	uint32_t payload_size = 4 + _queue_name.size();
	request_ack_buffer.resize(payload_size + VSOMEIP_PROTOCOL_OVERHEAD);

	std::memcpy(&request_ack_buffer[0], &VSOMEIP_START_TAG, sizeof(VSOMEIP_START_TAG));
	std::memset(&request_ack_buffer[4], _id, 4);
	request_ack_buffer[8] = static_cast<uint8_t>(command_enum::REQUEST_SERVICE_ACK);
	std::memcpy(&request_ack_buffer[9], &payload_size, sizeof(payload_size));
	std::memcpy(&request_ack_buffer[13], &_service, sizeof(_service));
	std::memcpy(&request_ack_buffer[15], &_instance, sizeof(_instance));
	std::copy(_queue_name.begin(), _queue_name.end(), request_ack_buffer.begin() + 17);
	std::memcpy(&request_ack_buffer[payload_size + 13], &VSOMEIP_END_TAG, sizeof(VSOMEIP_END_TAG));

	do_send(_id, request_ack_buffer);
}

void daemon_impl::send_ping(application_id _id) {
	uint8_t ping_message[] = { 0xAB, 0xAB, 0xAB, 0xAB, 0x00, 0x00, 0x00, 0x00,
			static_cast<uint8_t>(command_enum::PING), 0x00, 0x00, 0x00, 0x00,
			0xBA, 0xBA, 0xBA, 0xBA };

	std::memcpy(&ping_message[4], &_id, sizeof(_id));

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
	std::memcpy(&message_data[0], &VSOMEIP_START_TAG, sizeof(VSOMEIP_START_TAG));
	std::memcpy(&message_data[message_size-4], &VSOMEIP_END_TAG, sizeof(VSOMEIP_END_TAG));
	message_data[8] = static_cast<uint8_t>(command_enum::APPLICATION_INFO);

	message_size -= VSOMEIP_PROTOCOL_OVERHEAD;
	std::memcpy(&message_data[9], &message_size, sizeof(message_size));

	std::size_t position = 13;
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

void daemon_impl::send_application_lost(const std::list< application_id > &_lost_applications) {
	std::size_t message_size = VSOMEIP_PROTOCOL_OVERHEAD
		+ _lost_applications.size() * sizeof(application_id);

	std::vector< uint8_t > message_data(message_size);

	std::memcpy(&message_data[0], &VSOMEIP_START_TAG, sizeof(VSOMEIP_START_TAG));
	std::memcpy(&message_data[message_size-4], &VSOMEIP_END_TAG, sizeof(VSOMEIP_END_TAG));
	message_data[8] = static_cast<uint8_t>(command_enum::APPLICATION_LOST);

	message_size -= VSOMEIP_PROTOCOL_OVERHEAD;
	std::memcpy(&message_data[9], &message_size, sizeof(message_size));

	std::size_t position = 13;
	for (auto a : _lost_applications) {
		std::memcpy(&message_data[position], &a, sizeof(a));
		position += sizeof(a);
	}

	do_broadcast(message_data);
}

void daemon_impl::process_command(std::size_t _bytes) {
	uint32_t start_tag, end_tag, payload_size;
	command_enum command;
	uint32_t application_id;
	service_id a_service;
	instance_id an_instance;
	method_id a_method;
	endpoint *location = 0;

	std::memcpy(&start_tag, &receive_buffer_[0], sizeof(start_tag));
	std::memcpy(&application_id, &receive_buffer_[4], sizeof(application_id));
	command = static_cast<command_enum>(receive_buffer_[8]);
	std::memcpy(&payload_size, &receive_buffer_[9], sizeof(payload_size));
	std::memcpy(&end_tag, &receive_buffer_[_bytes - sizeof(start_tag)],
			sizeof(start_tag));

	switch (command) {
	case command_enum::REGISTER_APPLICATION: {
		std::string queue_name((char*) &receive_buffer_[13], payload_size);
		on_register_application(queue_name);
		}
		break;

	case command_enum::DEREGISTER_APPLICATION:
		on_deregister_application(application_id);
		break;

	case command_enum::REGISTER_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[13], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[15], sizeof(an_instance));
		location = factory::get_instance()->get_endpoint(&receive_buffer_[17], payload_size - 4);
		on_provide_service(application_id, a_service, an_instance, location);
		break;

	case command_enum::DEREGISTER_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[13], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[15], sizeof(an_instance));
		if (payload_size - 4 > 0)
			location = factory::get_instance()->get_endpoint(&receive_buffer_[17], payload_size - 4);
		on_withdraw_service(application_id, a_service, an_instance, location);
		break;

	case command_enum::REQUEST_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[13], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[15], sizeof(an_instance));
		location = factory::get_instance()->get_endpoint(&receive_buffer_[17], payload_size - 4);
		on_request_service(application_id, a_service, an_instance, location);
		break;

	case command_enum::RELEASE_SERVICE:
		std::memcpy(&a_service, &receive_buffer_[13], sizeof(a_service));
		std::memcpy(&an_instance, &receive_buffer_[15], sizeof(an_instance));
		if (payload_size - 4 > 0)
			location = factory::get_instance()->get_endpoint(&receive_buffer_[17], payload_size - 4);
		on_release_service(application_id, a_service, an_instance, location);
		break;

	case command_enum::REGISTER_METHOD:
		std::memcpy(&a_service, &receive_buffer_[13], sizeof(a_service));
		std::memcpy(&a_method, &receive_buffer_[15], sizeof(a_method));
		on_register_method(application_id, a_service, a_method);
		break;

	case command_enum::DEREGISTER_METHOD:
		std::memcpy(&a_service, &receive_buffer_[13], sizeof(a_service));
		std::memcpy(&a_method, &receive_buffer_[15], sizeof(a_method));
		on_deregister_method(application_id, a_service, a_method);
		break;

	case command_enum::PONG:
		on_pong(application_id);
		break;

	case command_enum::SOMEIP_MESSAGE:
		on_message(application_id, &receive_buffer_[0], _bytes);
		break;

	default:
		VSOMEIP_WARNING << "Unknown command received";
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////
void daemon_impl::open_cbk(boost::system::error_code const &_error, application_id _id) {
	if (!_error) {
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
		uint32_t _id) {
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
		std::list< application_id > gone;

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

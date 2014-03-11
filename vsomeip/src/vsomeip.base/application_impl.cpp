//
// application_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <algorithm>
#include <vector>

#include <boost/asio/placeholders.hpp>
#include <boost/atomic.hpp>
#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>

#include <boost_ext/asio/mq.hpp>
#include <boost_ext/asio/placeholders.hpp>
#include <boost_ext/process.hpp>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message_base.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/config.hpp>
#include <vsomeip_internal/constants.hpp>

using namespace boost::log::trivial;

#define WATCHDOG_TEST

namespace vsomeip {

///////////////////////////////////////////////////////////////////////////////
// Object members
///////////////////////////////////////////////////////////////////////////////
application_impl::application_impl(const std::string &_name)
	: id_(0),
	  name_(_name),
	  daemon_queue_(service_),
	  application_queue_(service_),
	  watchdog_timer_(service_) {
}

application_impl::~application_impl() {
}

void application_impl::init() {
	static boost::atomic<uint32_t> queue_id(1);

	// configure application message queue name (use thread id)
	std::stringstream message_queue_id_stream;
	message_queue_id_stream
		<< "/vsomeip-"
		<< (int)boost_ext::process::process_id()
		<< "."
		<< queue_id;

	// Message queue name
	application_queue_name_ = message_queue_id_stream.str();

	queue_id++;

	// Read configuration
	enable_console();
	enable_file(name_);

	set_id(application_queue_name_.substr(9));
	set_loglevel(debug);

	BOOST_LOG_SEV(logger_, debug)
		<< "Application uses queue " << application_queue_name_
		<< " and id " << application_queue_name_.substr(9);
}

void application_impl::start() {
	// Number of slots the message queue provides
	int message_queue_slots = 100; // TODO: read number of slots from configuration

	application_queue_.async_create(
		application_queue_name_.c_str(),
		message_queue_slots,
		VSOMEIP_MAX_TCP_MESSAGE_SIZE, // TODO: add number for protocol overhead
		boost::bind(
			&application_impl::create_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void application_impl::stop() {
	send_deregister_application();
	application_queue_.async_close(
		application_queue_name_.c_str(),
		boost::bind(
			&application_impl::destroy_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

std::size_t application_impl::poll_one() {
	return service_.poll_one();
}

std::size_t application_impl::poll() {
	return service_.poll();
}

std::size_t application_impl::run() {
	return service_.run();
}

bool application_impl::request_service(
			service_id _service, instance_id _instance,
			const endpoint *_location) const {
	return false;
}

bool application_impl::release_service(
			service_id _service, instance_id _instance) const {
	return false;
}

bool application_impl::provide_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) const {
	return true;
}

bool application_impl::start_service(
			service_id _service, instance_id _instance) const {
	return false;
}

bool application_impl::stop_service(
			service_id _service, instance_id _instance) const {
	return false;
}

bool application_impl::send(message_base *_message, bool _flush) const {
	uint32_t message_size = VSOMEIP_STATIC_HEADER_SIZE + _message->get_length();

	// TODO: check message length based on target endpoint

	serializer_->reset();
	bool is_successful(serializer_->serialize(_message));
	if (!is_successful) {
		return false;
	}

	// TODO: transmit data to daemon
	return true;
}

void application_impl::enable_magic_cookies(
			service_id _service, instance_id _instance) const {
}

void application_impl::disable_magic_cookies(
			service_id _service, instance_id _instance) const {
}

void application_impl::enable_watchdog() {
}

void application_impl::disable_watchdog() {
}

///////////////////////////////////////////////////////////////////////////////
// Internal
///////////////////////////////////////////////////////////////////////////////
void application_impl::do_send(const std::vector<uint8_t> &_data) {
	send_buffers_.push_back(_data);
	std::vector< uint8_t >& current_buffer = send_buffers_.back();

	daemon_queue_.async_send(
		current_buffer.data(), current_buffer.size(), 0,
		boost::bind(
			&application_impl::send_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void application_impl::do_receive() {
	application_queue_.async_receive(
		receive_buffer_,
		sizeof(receive_buffer_),
		boost::bind(
			&application_impl::receive_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			boost_ext::asio::placeholders::priority
		)
	);
}

void application_impl::send_register_application() {
	std::vector<uint8_t> registration;

	// Resize to the needed size
	uint32_t payload_size = application_queue_name_.size();
	registration.resize(payload_size + 17);

	std::memcpy(&registration[0], &VSOMEIP_START_TAG, sizeof(VSOMEIP_START_TAG));
	std::memset(&registration[4], 0, 4); // application id not yet known...
	registration[8] = static_cast<uint8_t>(command_enum::REGISTER_APPLICATION);
	std::memcpy(&registration[9], &payload_size, sizeof(payload_size));
	std::copy(application_queue_name_.begin(), application_queue_name_.end(), registration.begin() + 13);
	std::memcpy(&registration[payload_size + 13], &VSOMEIP_END_TAG, sizeof(VSOMEIP_END_TAG));

	// send
	do_send(registration);
}

void application_impl::send_deregister_application() {
	uint8_t deregistration_message[] = {
		0xAB, 0xAB, 0xAB, 0xAB,
		0x00, 0x00, 0x00, 0x00,
		static_cast<uint8_t>(command_enum::DEREGISTER_APPLICATION),
		0x00, 0x00, 0x00, 0x00,
		0xBA, 0xBA, 0xBA, 0xBA
	};

	std::memcpy(&deregistration_message[4], &id_, sizeof(id_));

	std::vector< uint8_t > deregistration_buffer(
		deregistration_message,
		deregistration_message + sizeof(deregistration_message)
	);

	// send
	do_send(deregistration_buffer);
}

void application_impl::send_pong() {
#ifdef WATCHDOG_TEST
	static int pong_counter = 0;
	if (pong_counter > 20 && 0 == id_ % 3) {
		return;
	}

	pong_counter ++;
#endif
	uint8_t pong_message[] = {
		0xAB, 0xAB, 0xAB, 0xAB,
		0x00, 0x00, 0x00, 0x00,
		static_cast<uint8_t>(command_enum::PONG),
		0x00, 0x00, 0x00, 0x00,
		0xBA, 0xBA, 0xBA, 0xBA
	};

	std::memcpy(&pong_message[4], &id_, sizeof(id_));

	std::vector< uint8_t > pong_buffer(
		pong_message,
		pong_message + sizeof(pong_message)
	);

	do_send(pong_buffer);
}

void application_impl::process_message(std::size_t _bytes) {
	if (_bytes < VSOMEIP_PROTOCOL_OVERHEAD) {
		BOOST_LOG_SEV(logger_, error)
			<< "Message too short (< " << VSOMEIP_PROTOCOL_OVERHEAD << " bytes)";
		return;
	}

	uint32_t start_tag, end_tag, payload_size;
	command_enum command;

	std::memcpy(&start_tag, &receive_buffer_[0], sizeof(start_tag));
	std::memcpy(&end_tag, &receive_buffer_[_bytes-sizeof(start_tag)], sizeof(start_tag));
	std::memcpy(&payload_size, &receive_buffer_[9], sizeof(payload_size));
	command = static_cast<command_enum>(receive_buffer_[8]);

	if (start_tag == VSOMEIP_START_TAG && end_tag == VSOMEIP_END_TAG) {
		if (_bytes == payload_size + VSOMEIP_PROTOCOL_OVERHEAD) {

			switch (command) {
			case command_enum::REGISTER_APPLICATION_ACK:
				std::memcpy(&id_, &receive_buffer_[4], 4);
				break;

			case command_enum::PING:
				BOOST_LOG_SEV(logger_, debug)
					<< "Application " << id_ << " received PING from Daemon";
				send_pong();
				break;

			default:
				BOOST_LOG_SEV(logger_, error)
					<< "Message contains illegal command " << (int)command;
				break;
			}

		} else {
			BOOST_LOG_SEV(logger_, error)
				<< "Message has incorrect size ("
				<< _bytes << "/" << payload_size + VSOMEIP_PROTOCOL_OVERHEAD << ")";
		}
	} else {
		BOOST_LOG_SEV(logger_, error)
			<< "Message is not correctly tagged";
	}
}

///////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////
void application_impl::open_cbk(boost::system::error_code const &_error) {
	if (!_error) {
		send_register_application();
		do_receive();
	} else {
		// TODO: define maximum number of retries
		daemon_queue_.async_open(
			"/vsomeip-0",
			boost::bind(
				&application_impl::open_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}
}

void application_impl::create_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		// configure daemon message queue
		daemon_queue_.async_open(
			"/vsomeip-0",
			boost::bind(
				&application_impl::open_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	} else {
		// Try destroying before creating
		application_queue_.async_close(
			application_queue_name_.c_str(),
			boost::bind(
				&application_impl::destroy_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// TODO: define maximum number of retries
		application_queue_.async_create(
			application_queue_name_.c_str(),
			10,
			100,
			boost::bind(
				&application_impl::create_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}
}

void application_impl::destroy_cbk(
		boost::system::error_code const &_error) {

}

void application_impl::send_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		send_buffers_.pop_front();
	} else {
		BOOST_LOG_SEV(logger_, error)
			<< "Sending to daemon failed";
	}
}

void application_impl::receive_cbk(
		boost::system::error_code const &_error,
		std::size_t _bytes, unsigned int _priority) {
	if (!_error && _bytes) {
		process_message(_bytes);
	} else {
		BOOST_LOG_SEV(logger_, error)
			<< "Received error message (" << _bytes << " bytes)";
	}

	do_receive();
}

} // namespace vsomeip

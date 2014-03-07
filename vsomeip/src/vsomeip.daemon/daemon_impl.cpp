//
// daemon_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <chrono>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#include <boost_ext/asio/mq.hpp>
#include <boost_ext/asio/placeholders.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/message_base.hpp>
#include <vsomeip_internal/daemon_impl.hpp>
#include <vsomeip_internal/tcp_client_impl.hpp>
#include <vsomeip_internal/tcp_service_impl.hpp>
#include <vsomeip_internal/udp_client_impl.hpp>
#include <vsomeip_internal/udp_service_impl.hpp>

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

namespace vsomeip {

uint32_t daemon_impl::id__ = 1; // ID 0 is reserved for the daemon

daemon * daemon_impl::get_instance() {
	static daemon_impl the_daemon;
	return &the_daemon;
}

daemon_impl::daemon_impl()
	: daemon_queue_(receiver_service_),
	  watchdog_timer_(sender_service_),
	  work_(sender_service_) {
}

void daemon_impl::init(int _count, char **_options) {
	std::cout << "vsomeip-daemon started" << std::endl;
	daemon_queue_.async_create(
		"/vsomeip-0",
		10,	// TODO: replace
		100,	// TODO: replace
		boost::bind(
			&daemon_impl::create_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void daemon_impl::run_receiver() {
	receiver_service_.run();
	std::cout << "Receiver run ended!" << std::endl;
}

void daemon_impl::run_sender() {
	sender_service_.run();
	std::cout << "Sender run ended!" << std::endl;
}


void daemon_impl::start() {
	boost::thread sender_thread(
		boost::bind(
			&daemon_impl::run_sender,
			this
		)
	);

	boost::thread receiver_thread(
		boost::bind(
			&daemon_impl::run_receiver,
			this
		)
	);

	sender_thread.join();
	receiver_thread.join();
}

void daemon_impl::start_watchdog_cycle() {
	watchdog_timer_.expires_from_now(
		std::chrono::milliseconds(VSOMEIP_WATCHDOG_CYCLE)
	);

	watchdog_timer_.async_wait(
		boost::bind(
			&daemon_impl::watchdog_cycle_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void daemon_impl::start_watchdog_check() {
	for (auto i : application_queues_) {
		watchdogs_[i.first] ++;
		send_ping(i.first);
	}

	watchdog_timer_.expires_from_now(
		std::chrono::milliseconds(VSOMEIP_WATCHDOG_TIMEOUT)
	);

	watchdog_timer_.async_wait(
		boost::bind(
			&daemon_impl::watchdog_check_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void daemon_impl::do_send(uint32_t _id, const std::vector<uint8_t> &_data) {
	auto found_queue = application_queues_.find(_id);
	if (found_queue != application_queues_.end()) {
		send_buffers_.push_back(_data);
		std::vector< uint8_t > &send_data = send_buffers_.back();
		found_queue->second->async_send(
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
		// TODO: log "no queue for application"
	}
}

void daemon_impl::do_receive() {
	daemon_queue_.async_receive(
		receive_buffer_,
		sizeof(receive_buffer_),
		boost::bind(
			&daemon_impl::receive_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			boost_ext::asio::placeholders::priority
		)
	);
}

void daemon_impl::on_register_application(const std::string &_name) {
	boost_ext::asio::message_queue *application_queue
		= new boost_ext::asio::message_queue(sender_service_);

	application_queue->async_open(
		_name.c_str(),
		boost::bind(
			&daemon_impl::open_cbk,
			this,
			boost::asio::placeholders::error,
			id__
		)
	);

	application_queues_[id__] = application_queue;
	id__++;
}

void daemon_impl::on_deregister_application(uint32_t _id) {
	application_queues_.erase(_id);

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

void daemon_impl::on_pong(uint32_t _id) {
	std::cout << "Received PONG from Application " << _id << std::endl;
	watchdogs_[_id] --;
}

bool daemon_impl::request_service(
			service_id _service,
			instance_id _instance,
			const endpoint *_location) {

	if (0 == _location)
		return false;

	client *the_client = find_or_create_client(_location);
	if (0 == the_client)
		return false;

	the_client->register_for(_service, _instance);

	return true;
}

bool daemon_impl::provide_service(
			service_id _service,
			instance_id _instance,
			const endpoint *_location) {

	if (0 == _location)
		return false;

	service *the_service = find_or_create_service(_location);
	if (0 == the_service)
		return false;

	the_service->register_for(_service, _instance);

	return true;
}

bool daemon_impl::send(const message_base *_message, bool _flush) {
	endpoint *location = _message->get_endpoint();

	if (0 == location)
		return false;

	if (is_client_message(_message)) {
		client * the_client = find_client(location);
		if (0 == the_client)
			return false;

		return false; //the_client->send(_message, _flush);
	}

	service * the_service = find_service(location);
	if (0 == the_service)
		return false;

	return false; //the_service->send(_message, _flush);
}

bool daemon_impl::is_client_message(const message_base *_message) {
	return (_message->get_message_type() < message_type::REQUEST_ACK);
}

client * daemon_impl::find_client(const endpoint *_location) {
	auto found = clients_.find(_location);
	if (found == clients_.end())
		return 0;

	return found->second;
}

client * daemon_impl::create_client(const endpoint *_location) {
	client *the_client = 0;
	if (0 != _location) {
		if (_location->get_protocol() == transport_protocol::UDP) {
			the_client = new udp_client_impl(sender_service_, _location);
		} else if (_location->get_protocol() == transport_protocol::TCP) {
			the_client = new tcp_client_impl(sender_service_, _location);
		} else {
			// TODO: log "unsupported/unknown transport protocol"
		}
	}
	return the_client;
}

client * daemon_impl::find_or_create_client(const endpoint *_location) {
	client *the_client = find_client(_location);
	if (0 == the_client)
		the_client = create_client(_location);
	return the_client;
}

service * daemon_impl::find_service(const endpoint *_location) {
	auto found = services_.find(_location);
	if (found == services_.end())
		return 0;

	return found->second;
}

service * daemon_impl::create_service(const endpoint *_location) {
	service *the_service = 0;
	if (0 != _location) {
		if (_location->get_protocol() == transport_protocol::UDP) {
			the_service = new udp_service_impl(sender_service_, _location);
		} else if (_location->get_protocol() == transport_protocol::TCP) {
			the_service = new tcp_service_impl(sender_service_, _location);
		} else {
			// TODO: log "unsupported/unknown transport protocol"
		}
	}
	return the_service;
}

service * daemon_impl::find_or_create_service(const endpoint *_location) {
	service *the_service = find_service(_location);
	if (0 == the_service)
		the_service = create_service(_location);
	return the_service;
}

void daemon_impl::send_register_ack(uint32_t _id) {
	uint8_t register_ack_message[] = {
		0xAB, 0xAB, 0xAB, 0xAB,
		0x00, 0x00, 0x00, 0x00,
		static_cast< uint8_t >(command_enum::REGISTER_APPLICATION_ACK),
		0x00, 0x00, 0x00, 0x00,
		0xBA, 0xBA, 0xBA, 0xBA
	};

	std::memcpy(&register_ack_message[4], &_id, sizeof(_id));

	std::vector< uint8_t > register_ack_buffer(
		register_ack_message,
		register_ack_message + sizeof(register_ack_message)
	);

	do_send(_id, register_ack_buffer);
}

void daemon_impl::send_deregister_ack(uint32_t _id) {
	uint8_t deregister_ack_message[] = {
		0xAB, 0xAB, 0xAB, 0xAB,
		0x00, 0x00, 0x00, 0x00,
		static_cast< uint8_t >(command_enum::DEREGISTER_APPLICATION_ACK),
		0x00, 0x00, 0x00, 0x00,
		0xBA, 0xBA, 0xBA, 0xBA
	};

	std::memcpy(&deregister_ack_message[4], &_id, sizeof(_id));

	std::vector< uint8_t > deregister_ack_buffer(
		deregister_ack_message,
		deregister_ack_message + sizeof(deregister_ack_message)
	);

	do_send(_id, deregister_ack_buffer);
}

void daemon_impl::send_ping(uint32_t _id) {
	uint8_t ping_message[] = {
		0xAB, 0xAB, 0xAB, 0xAB,
		0x00, 0x00, 0x00, 0x00,
		static_cast< uint8_t >(command_enum::PING),
		0x00, 0x00, 0x00, 0x00,
		0xBA, 0xBA, 0xBA, 0xBA
	};

	std::memcpy(&ping_message[4], &_id, sizeof(_id));

	std::vector<uint8_t> ping_buffer(
		ping_message,
		ping_message + sizeof(ping_message)
	);

	do_send(_id, ping_buffer);
}

void daemon_impl::process_command(std::size_t _bytes) {
	uint32_t start_tag, end_tag, payload_size;
	command_enum command;
	uint32_t application_id;

	std::memcpy(&start_tag, &receive_buffer_[0], sizeof(start_tag));
	std::memcpy(&application_id, &receive_buffer_[4], sizeof(application_id));
	command = static_cast<command_enum>(receive_buffer_[8]);
	std::memcpy(&payload_size, &receive_buffer_[9], sizeof(payload_size));
	std::memcpy(&end_tag, &receive_buffer_[_bytes-sizeof(start_tag)], sizeof(start_tag));

#ifdef VSOMEIP_DEBUG
	std::cout << "Received command " << std::hex << (int)command << "("
		<< std::hex << start_tag << ", " << end_tag << std::dec << ", " << payload_size
		<< ")" << std::endl;
#endif

	switch (command) {
	case command_enum::REGISTER_APPLICATION:
	{
		std::string queue_name((char*)&receive_buffer_[13], payload_size);
		on_register_application(queue_name);
		break;
	}
	case command_enum::DEREGISTER_APPLICATION:
		on_deregister_application(application_id);
		break;

	case command_enum::PONG:
		on_pong(application_id);
		break;

	default:
		// TODO: log "unknown command"
		break;
	}
}


///////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////
void daemon_impl::open_cbk(boost::system::error_code const &_error, uint32_t _id) {
	if (!_error) {
		send_register_ack(_id);
		watchdogs_[_id] = true;
	} else {
		std::cout << "Opening queue for " << _id << " failed!" << std::endl;
	}
}

void daemon_impl::create_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		start_watchdog_cycle();
		do_receive();
	} else {
		// Try destroying before creating
		daemon_queue_.async_close(
			"/vsomeip-0",
			boost::bind(
				&daemon_impl::destroy_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// TODO: define maximum number of retries
		daemon_queue_.async_create(
			"/vsomeip-0",
			100,
			100,
			boost::bind(
				&daemon_impl::create_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}
}

void daemon_impl::destroy_cbk(
		boost::system::error_code const &_error) {

}

void daemon_impl::send_cbk(
		boost::system::error_code const &_error, uint32_t _id) {
	if (_error) {
		std::cout << "Message sending failed (Application " << _id << ")" << std::endl;
	}
}

void daemon_impl::receive_cbk(
		boost::system::error_code const &_error,
		std::size_t _bytes, unsigned int _priority) {

	if (!_error) {
		process_command(_bytes);
	} else {
		std::cout << "Daemon received an erroneous message!" << std::endl;
	}

	do_receive();
}

void daemon_impl::watchdog_cycle_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		start_watchdog_check();
	}
}

void daemon_impl::watchdog_check_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		std::list< uint32_t > gone;

		for (auto i : watchdogs_) {
			if (i.second > VSOMEIP_MAX_MISSING_PONGS) {
				application_queues_.erase(i.first);
				gone.push_back(i.first);
				// TODO: log "Lost contact to application ID"
				std::cout << "Lost application " << (int)i.first << std::endl;
			}
		}

		for (auto i : gone)
			watchdogs_.erase(i);

		start_watchdog_cycle();
	}
}


} // namespace vsomeip

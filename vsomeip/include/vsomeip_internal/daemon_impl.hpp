//
// daemon_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_DAEMON_IMPL_HPP
#define VSOMEIP_INTERNAL_DAEMON_IMPL_HPP

#include <map>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/scoped_ptr.hpp>

#include <boost_ext/asio/mq.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/config.hpp>
#include <vsomeip_internal/daemon.hpp>

namespace vsomeip {

class endpoint;
class client;
class service;
class message_base;

class daemon_impl
	: public daemon {
public:
	static daemon * get_instance();

	daemon_impl();

	void init(int _count, char **_options);
	void start();

	bool request_service(service_id _service, instance_id _instance,
							const endpoint *_location);
	bool provide_service(service_id _service, instance_id _instance,
							const endpoint *_location);
	bool send(const message_base *_message, bool _flush);

private:
	void run_receiver();
	void run_sender();

	bool is_client_message(const message_base *_message);

	client * find_client(const endpoint *_location);
	client * create_client(const endpoint *_location);
	client * find_or_create_client(const endpoint *_location);
	service * find_service(const endpoint *_location);
	service * create_service(const endpoint *_location);
	service * find_or_create_service(const endpoint *_location);

	void do_send(uint32_t _id, const std::vector<uint8_t> &_data);
	void do_receive();

	void on_register_application(const std::string &_name);
	void on_deregister_application(uint32_t _id);
	void on_pong(uint32_t _id);

	void send_ping(uint32_t _id);
	void send_register_ack(uint32_t _id);
	void send_deregister_ack(uint32_t _id);


	void process_command(std::size_t _bytes);
	void start_watchdog_cycle();
	void start_watchdog_check();

private:
	void open_cbk(boost::system::error_code const &_error, uint32_t _id);
	void create_cbk(boost::system::error_code const &_error);
	void destroy_cbk(boost::system::error_code const &_error);
	void send_cbk(boost::system::error_code const &_error, uint32_t _id);
	void receive_cbk(
			boost::system::error_code const &_error,
			std::size_t _bytes, unsigned int _priority);

	void watchdog_cycle_cbk(boost::system::error_code const &_error);
	void watchdog_check_cbk(boost::system::error_code const &_error);

private:
	boost::asio::io_service receiver_service_;
	boost::asio::io_service sender_service_;
	boost::asio::io_service::work work_;
	boost::asio::system_timer watchdog_timer_;

	std::map< const endpoint *, client * > clients_;
	std::map< const endpoint *, service * > services_;

	boost_ext::asio::message_queue daemon_queue_;
	std::map< uint32_t, boost_ext::asio::message_queue * > application_queues_;

	uint8_t receive_buffer_[VSOMEIP_MAX_TCP_MESSAGE_SIZE + VSOMEIP_PROTOCOL_OVERHEAD];

	// buffers for sending messages
	std::deque< std::vector< uint8_t > > send_buffers_;

	// watchdog registry
	std::map< uint32_t, int8_t > watchdogs_;

private:
	static uint32_t id__;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_DAEMON_IMPL_HPP

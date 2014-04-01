//
// daemon_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_DAEMON_IMPL_HPP
#define VSOMEIP_DAEMON_DAEMON_IMPL_HPP

#include <map>
#include <set>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/scoped_ptr.hpp>

#include <boost_ext/asio/mq.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/managing_application_impl.hpp>

#include "application_info.hpp"
#include "client_info.hpp"
#include "request_info.hpp"
#include "service_info.hpp"
#include "daemon.hpp"

#define VSOMEIP_DAEMON_DEBUG

namespace vsomeip {

class endpoint;
class client;
class service;
class message_base;

class daemon_impl
	: public daemon,
	  public managing_application_impl {
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
	void run_network();

	bool is_client_message(const message_base *_message);

	void do_send(application_id, std::vector< uint8_t > &);
	void do_broadcast(std::vector< uint8_t > &);
	void do_receive();

	void on_register_application(const std::string &);
	void on_deregister_application(application_id);
	void on_provide_service(application_id, service_id, instance_id, const endpoint *);
	void on_withdraw_service(application_id, service_id, instance_id, const endpoint *);
	void on_start_service(application_id, service_id, instance_id);
	void on_stop_service(application_id, service_id, instance_id);
	void on_request_service(application_id, service_id, instance_id, const endpoint *);
	void on_release_service(application_id, service_id, instance_id, const endpoint *);

	void on_pong(application_id);

	void on_message(application_id, const uint8_t *, uint32_t);
	void on_register_method(application_id, service_id, method_id);
	void on_deregister_method(application_id, service_id, method_id);

	void send_ping(application_id);
	void send_application_info();
	void send_application_lost(const std::list< application_id > &);

	void send_request_service_ack(application_id, service_id, instance_id, const std::string &);
	void send_release_service_ack(application_id, service_id, instance_id);

	void process_command(std::size_t _bytes);
	void start_watchdog_cycle();
	void start_watchdog_check();

	void receive(const uint8_t *, uint32_t, const endpoint *, const endpoint *);

private:
	void open_cbk(boost::system::error_code const &_error, application_id _id);
	void create_cbk(boost::system::error_code const &_error);
	void destroy_cbk(boost::system::error_code const &_error);
	void send_cbk(boost::system::error_code const &_error, application_id _id);
	void receive_cbk(
			boost::system::error_code const &_error,
			std::size_t _bytes, unsigned int _priority);

	void watchdog_cycle_cbk(boost::system::error_code const &_error);
	void watchdog_check_cbk(boost::system::error_code const &_error);

private:
	boost::asio::io_service receiver_service_;
	boost::asio::io_service sender_service_;
	boost::asio::io_service network_service_;
	boost::asio::io_service::work sender_work_;
	boost::asio::io_service::work network_work_;
	boost::asio::system_timer watchdog_timer_;

	std::string queue_name_prefix_;
	std::string daemon_queue_name_;
	boost_ext::asio::message_queue daemon_queue_;

	uint8_t receive_buffer_[VSOMEIP_QUEUE_SIZE];

	// buffers for sending messages
	std::deque< std::vector< uint8_t > > send_buffers_;

	// applications
	std::map< application_id, application_info > applications_;

	// clients & services
	std::map< service_id, std::map< instance_id, service_info > > services_;

	// sessions
	std::map< client_id, std::map< session_id, application_id > > sessions_;

	// requests (which could not yet be answered)
	std::set< request_info > requests_;

	// endpoints
//	std::map< const endpoint *, client * > client_locations_;
//	std::map< const endpoint *, service * > service_locations_;

	// application mappings
	std::map< client_id, application_id > client_to_application_;
	std::map< const endpoint *,
	          std::map< service_id,
	                    application_id > > service_to_application_;

	// channels
	std::map< service_id,
	          std::map< method_id,
	           	   	    std::set< application_id > > > channels_;

private:
	static application_id id__;

#ifdef VSOMEIP_DAEMON_DEBUG
private:
	void start_dump_cycle();
	void dump_cycle_cbk(boost::system::error_code const &);
	boost::asio::system_timer dump_timer_;
#endif
};

} // namespace vsomeip

#endif // VSOMEIP_DAEMON_DAEMON_IMPL_HPP

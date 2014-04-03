//
// application_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_APPLICATION_IMPL_HPP
#define VSOMEIP_INTERNAL_APPLICATION_IMPL_HPP

#include <deque>
#include <map>
#include <set>
#include <tuple>
#include <vector>

#include <boost/asio/system_timer.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/utility.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip/config.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip_internal/enumeration_types.hpp>
#include <vsomeip_internal/log_owner.hpp>

namespace vsomeip {

class deserializer;
class endpoint;
class message_base;
class message_queue;
class serializer;

class application_impl
	: virtual public application,
	  public log_owner,
	  boost::noncopyable {

public:

	typedef std::map< service_id,
			           std::map< instance_id,
			                     std::set< const endpoint * > > > provided_t;

	typedef boost::intrusive_ptr< message_queue > message_queue_t;

	typedef std::map< service_id,
					   std::map< instance_id,
					   	   	     std::pair< const endpoint *,
					   	   	     	 	 	message_queue_t > > > requested_t;


	application_impl(const std::string &_name);
	virtual ~application_impl();

	void init(int _options_count, char **_options);
	void start();
	void stop();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

	boost::asio::io_service & get_io_service();

	bool request_service(service_id _service, instance_id _instance,
							const endpoint *_location);
	bool release_service(service_id _service, instance_id _instance);

	bool provide_service(service_id _service, instance_id _instance,
							const endpoint *_location);
	bool withdraw_service(service_id _service, instance_id _instance,
							 const endpoint *_location);
	bool start_service(service_id _service, instance_id _instance);
	bool stop_service(service_id _service, instance_id _instance);

	bool send(message_base *_message, bool _flush);

	void enable_magic_cookies(service_id _service, instance_id _instance);
	void disable_magic_cookies(service_id _service, instance_id _instance);

	void register_cbk(service_id _service, method_id _method, receive_cbk_t _cbk);
	void deregister_cbk(service_id _service, method_id _method, receive_cbk_t _cbk);

	void remove_queue(const std::string &_name);

private:
	message_queue * find_target_queue(service_id, const endpoint *) const;
	message_queue * find_target_queue(client_id);
	void remove_requested_services(message_queue *);

	void do_send(const std::vector<uint8_t> &);
	void do_send_buffer(const std::vector<uint8_t> &);
	void do_receive();

	void on_application_info(const uint8_t *, uint32_t);
	void on_application_lost(const uint8_t *, uint32_t);
	void on_request_service_ack(service_id, instance_id, const std::string &);
	void on_message(client_id, const uint8_t *, uint32_t);

	void process_message(std::size_t _bytes);
	void process_early_registrations();

	void send_register_application();
	void send_deregister_application();
	void send_callback_command(command_enum, service_id, method_id, receive_cbk_t);
	void send_service_command(command_enum, service_id, instance_id, const endpoint *);

	void send_pong();

private: // callbacks
	void create_cbk(boost::system::error_code const &);
	void open_cbk(boost::system::error_code const &);
	void retry_open_cbk(boost::system::error_code const &);
	void close_cbk(boost::system::error_code const &);
	void send_cbk(boost::system::error_code const &);
	void retry_send_cbk(boost::system::error_code const &);
	void receive_cbk(boost::system::error_code const &, std::size_t, unsigned int);
	void request_cbk(boost::system::error_code const &, service_id, instance_id, message_queue *, const std::string &);
	void response_cbk(boost::system::error_code const &, message_queue *);

private: // object members
	boost::asio::io_service service_;
	boost::asio::system_timer watchdog_timer_;
	boost::asio::system_timer retry_timer_;

	bool is_registered_;

	uint32_t retry_timeout_;
	bool is_open_;

	client_id id_;
	int receiver_slots_;

	// Prefix for all(!) queue names
	std::string queue_name_prefix_;

	// Message queue to communicate to the vsomeip-daemon
	std::string daemon_queue_name_;
	boost::shared_ptr< message_queue > daemon_queue_;

	std::string application_queue_name_;
	boost::shared_ptr< message_queue > application_queue_;

	std::deque< std::vector< uint8_t > > send_buffers_;
	uint8_t receive_buffer_[VSOMEIP_QUEUE_SIZE];

	serializer *serializer_;
	deserializer *deserializer_;

	provided_t provided_;
	requested_t requested_;

	std::map< std::string, message_queue * > queues_;
	std::map< client_id, std::string > other_queue_names_;

	// receiver
	typedef std::map< method_id,
  	   	     std::set< receive_cbk_t > > method_filter_map;
	typedef std::map< service_id,
					   method_filter_map > service_filter_map;

	service_filter_map receive_cbks_;

	boost::mutex mutex_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_APPLICATION_IMPL_HPP

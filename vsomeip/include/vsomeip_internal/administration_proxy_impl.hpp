//
// administration_proxy_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_ADMINISTRATION_PROXY_IMPL_HPP
#define VSOMEIP_ADMINISTRATION_PROXY_IMPL_HPP

#include <deque>
#include <map>
#include <set>
#include <vector>

#include <boost/asio/system_timer.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip_internal/enumeration_types.hpp>
#include <vsomeip_internal/message_queue.hpp>
#include <vsomeip_internal/method_info.hpp>
#include <vsomeip_internal/proxy_base_impl.hpp>

namespace vsomeip {

class administration_proxy_impl
			: virtual public proxy_base_impl {
public:
	administration_proxy_impl(application_base_impl &_owner);
	virtual ~administration_proxy_impl();

	void init();
	void start();
	void stop();

	bool provide_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool withdraw_service(service_id _service, instance_id _instance, const endpoint *_location);

	bool start_service(service_id _service, instance_id _instance);
	bool stop_service(service_id _service, instance_id _instance);

	bool request_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool release_service(service_id _servive, instance_id _instance);

	void register_method(service_id _service, instance_id _instance, method_id _method);
	void deregister_method(service_id _service, instance_id _instance, method_id _method);

	bool provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location);
	bool withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location);

	bool add_to_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, event_id _event);
	bool add_to_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, message_base *_field);
	bool remove_from_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, event_id _event);

	bool request_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup);
	bool release_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup);

	void remove_queue(const std::string &_name);

protected:
	void catch_up_registrations();

	void do_send(const std::vector< uint8_t > &_buffer);
	void do_send_buffer(const std::vector< uint8_t > &_buffer);

	void send_pong();
	void send_register_application();
	void send_deregister_application();
	void send_service_command(command_enum _command, service_id _service, instance_id _instance, const endpoint * _location = 0);
	void send_registration_command(command_enum _command, service_id _service, instance_id _instance, method_id _method);

	void do_receive();

	void process_message(std::size_t _bytes);
	virtual void process_command(command_enum _command, client_id _client, const uint8_t *_payload, uint32_t payload_size);

	void on_application_info(client_id _client, const uint8_t *_data, uint32_t _size);
	void on_application_lost(const uint8_t *_data, uint32_t _size);
	void on_request_service_ack(service_id _service, instance_id _instance, const std::string &_queue_name);
	virtual void on_service_availability(service_id _service, instance_id _instance, const endpoint *_location, bool _is_available);

	void remove_requested_services(message_queue *_queue);

private:
	void create_cbk(boost::system::error_code const &);
	void open_cbk(boost::system::error_code const &);
	void retry_open_cbk(boost::system::error_code const &);
	void close_cbk(boost::system::error_code const &);
	void send_cbk(boost::system::error_code const &);
	void receive_cbk(boost::system::error_code const &, std::size_t, unsigned int);
	void request_cbk(boost::system::error_code const &, service_id, instance_id, message_queue *, const std::string &);

protected:
	// Queues
	std::string queue_name_prefix_;

	std::string daemon_queue_name_;
	boost::shared_ptr< message_queue > daemon_queue_;

	std::string application_queue_name_;
	boost::shared_ptr< message_queue > application_queue_;
	int slots_;

	boost::asio::system_timer retry_timer_;
	uint32_t retry_timeout_;

	// Flags
	bool is_open_;
	bool is_created_;
	bool is_registered_;

	// Buffers
	std::deque< std::vector< uint8_t > > send_buffers_;
	uint8_t receive_buffer_[VSOMEIP_DEFAULT_QUEUE_SIZE];

	// Provided & requested services
	typedef std::map< service_id,
                      std::map< instance_id,
                                std::pair< bool,
                                           std::set< const endpoint * > > > > provided_t;

	typedef boost::intrusive_ptr< message_queue > message_queue_ptr_t;

	typedef std::map< service_id,
	                  std::map< instance_id,
	                  	  	    std::pair< const endpoint *,
	                  	  	               message_queue_ptr_t > > > requested_t;

	provided_t provided_;
	requested_t requested_;

	std::map< std::string, message_queue * > queues_;
	std::map< client_id, std::string > other_queue_names_;

	std::set< method_info > methods_;

	boost::mutex mutex_;
};

} // namespace vsomeip

#endif // VSOMEIP_PROXY_HPP

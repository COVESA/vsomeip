//
// application_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_APPLICATION_IMPL_HPP
#define VSOMEIP_APPLICATION_IMPL_HPP

#include <set>

#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip_internal/application_base_impl.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/serializer.hpp>

namespace vsomeip {

class application_impl
		: public application,
		  public application_base_impl {
public:
	application_impl(const std::string &_name);
	virtual ~application_impl();

	client_id get_id() const;
	void set_id(client_id _id);

	std::string get_name() const;
	void set_name(const std::string &_name);

	bool is_managing() const;

	boost::asio::io_service & get_sender_service();
	boost::asio::io_service & get_receiver_service();

	void init(int _options_count, char **_options);
	void start();
	void stop();

	bool provide_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool withdraw_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool start_service(service_id _service, instance_id _instance);
	bool stop_service(service_id _service, instance_id _instance);

	bool request_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool release_service(service_id _service, instance_id _instance);

	bool send(message_base *_message, bool _flush = true);

	bool enable_magic_cookies(service_id _service, instance_id _instance);
	bool disable_magic_cookies(service_id _service, instance_id _instance);

	bool register_message_handler(
			service_id _service, instance_id _instance, method_id _method,
			message_handler_t _handler);
	void deregister_message_handler(
			service_id _service, instance_id _instance, method_id _method);

	void catch_up_registrations();

	void handle_message(const message_base *_message);

	boost::shared_ptr< serializer > & get_serializer();
	boost::shared_ptr< deserializer > & get_deserializer();

protected:
	void service(boost::asio::io_service &_service);
	void send_error_message(const message_base *_request, return_code_enum _error);

protected:
	// Threads for independently run sending & receiving
	boost::shared_ptr< boost::thread > sender_thread_;
	boost::shared_ptr< boost::thread > receiver_thread_;

	// IO objects to independently control sending & receiving
	boost::asio::io_service sender_service_;
	boost::asio::io_service receiver_service_;

	// Work objects for each service to avoid processing being
	// stopped because of an empty queue
	boost::asio::io_service::work sender_work_;
	boost::asio::io_service::work receiver_work_;

protected:
	// Application (Client) id
	client_id id_;

	//
	bool is_managing_;

	// Handler callbacks for incoming messages
	typedef std::map< service_id,
			 	 	  std::map< instance_id,
			 	 	  	  	    std::map< method_id,
			 	 	  	  	     	 	  message_handler_t > > > message_handler_map_t;
	message_handler_map_t message_handlers_;

	boost::shared_ptr< serializer > serializer_;
	boost::shared_ptr< deserializer > deserializer_;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_IMPL_HPP

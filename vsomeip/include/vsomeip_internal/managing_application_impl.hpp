//
// managing_application_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_MANAGING_APPLICATION_IMPL_HPP
#define VSOMEIP_INTERNAL_MANAGING_APPLICATION_IMPL_HPP

#include <deque>
#include <set>
#include <utility>
#include <vector>

#include <boost/asio/io_service.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip_internal/log_owner.hpp>
#include <vsomeip_internal/managing_application.hpp>

namespace vsomeip {

class endpoint;
class message_base;

class client;
class service;

class serializer;
class deserializer;

class managing_application_impl
	: virtual public managing_application,
	  public log_owner,
	  boost::noncopyable {

public:
	managing_application_impl(const std::string &_name);
	virtual ~managing_application_impl();

	void init(int _options_count, char **_options);
	void start();
	void stop();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

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

	boost::asio::io_service & get_io_service();
	boost::log::sources::severity_logger<
					boost::log::trivial::severity_level > & get_logger();
	void receive(const uint8_t *_data, uint32_t _size, const endpoint *_source, const endpoint *_target);

protected:
	const endpoint * find_client_location(service_id, instance_id) const;
	const endpoint * find_service_location(service_id, instance_id) const;

	client * find_client(const endpoint *);
	client * create_client(const endpoint *);
	client * find_or_create_client(const endpoint *);

	service * find_service(const endpoint *);
	service * create_service(const endpoint *);
	service * find_or_create_service(const endpoint *);

	void send_error_message(message_base *, return_code_enum);

protected:
	boost::asio::io_service service_;
	boost::shared_ptr< serializer > serializer_;

private:
	boost::shared_ptr< deserializer > deserializer_;

	client_id id_;

	// locations of clients & services
	typedef std::map< service_id,
	                   std::map< instance_id,
	                           	 const endpoint * > > location_map;

	location_map client_locations_;
	location_map service_locations_;

	std::map< const endpoint *, client * > managed_clients_;
	std::map< const endpoint *, service * > managed_services_;

	// receiver
	typedef std::map< method_id,
  	   	     std::set< receive_cbk_t > > method_filter_map;
	typedef std::map< service_id,
					   method_filter_map > service_filter_map;

	service_filter_map receive_cbks_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MANAGING_APPLICATION_IMPL_HPP

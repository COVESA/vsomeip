//
// managing_proxy_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_MANAGING_PROXY_IMPL_HPP
#define VSOMEIP_MANAGING_PROXY_IMPL_HPP

#include <map>

#include <boost/asio/io_service.hpp>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/proxy_base_impl.hpp>
#include <vsomeip_internal/log_user.hpp>

namespace vsomeip {

class client;
class deserializer;
class endpoint;
class message_base;
class serializer;
class service;

class managing_proxy_impl
		: virtual public proxy_base_impl {
public:
	managing_proxy_impl(application_base_impl &_owner);
	virtual ~managing_proxy_impl();

	void init();
	void start();
	void stop();

	boost::asio::io_service & get_service();

	bool provide_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool withdraw_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool start_service(service_id _service, instance_id _instance);
	bool stop_service(service_id _service, instance_id _instance);

	bool request_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool release_service(service_id _service, instance_id _instance);

	bool send(message_base *_message, bool _flush);

	bool enable_magic_cookies(service_id _service, instance_id _instance);
	bool disable_magic_cookies(service_id _service, instance_id _instance);

	virtual void receive(const uint8_t *_data, uint32_t _size,
				 	     const endpoint *_source, const endpoint *_target);

	// The following methods are specific for this proxy type and
	// used by the daemon implementation only...
	const endpoint * find_client_location(service_id, instance_id) const;
	const endpoint * find_service_location(service_id, instance_id) const;

	client * find_client(const endpoint *) const;
	client * create_client(const endpoint *);
	client * find_or_create_client(const endpoint *);

	service * find_service(const endpoint *) const;
	service * create_service(const endpoint *);
	service * find_or_create_service(const endpoint *);

	instance_id find_instance(const endpoint *, service_id, message_type_enum) const;

private:
	// locations of clients & services
	typedef std::map< service_id,
					   std::map< instance_id,
								 const endpoint * > > location_map_t;

	location_map_t client_locations_;
	location_map_t service_locations_;

	// clients & services at endpoints
	std::map< const endpoint *, client * > managed_clients_;
	std::map< const endpoint *, service * > managed_services_;

	// instances on an endpoint
	typedef std::map< const endpoint *,
					  std::map< service_id, instance_id > > instance_map_t;

	instance_map_t client_instances_;
	instance_map_t service_instances_;
};

} // namespace vsomeip

#endif // VSOMEIP_MANAGING_PROXY_IMPL_HPP

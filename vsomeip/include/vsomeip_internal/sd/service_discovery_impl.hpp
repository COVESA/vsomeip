//
// service_discovery_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_IMPL_HPP

#include <map>
#include <set>

#include <boost/shared_ptr.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/sd/client_state_machine.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>
#include <vsomeip_internal/sd/service_state_machine.hpp>

namespace vsomeip {

class application;
class endpoint;
class message_base;

namespace sd {

class deserializer;

class service_discovery_impl
		: public service_discovery {
public:
	service_discovery_impl(boost::asio::io_service &_service);
	virtual ~service_discovery_impl();

	void init();
	void start();
	void stop();

	daemon * get_owner() const;
	void set_owner(daemon *_owner);

	boost::asio::io_service & get_service();

	void on_provide_service(service_id _service, instance_id _instance, const endpoint *_location);
	void on_withdraw_service(service_id _service, instance_id _instance, const endpoint *_location);

	void on_start_service(service_id _service, instance_id _instance);
	void on_stop_service(service_id _service, instance_id _instance);

	void on_request_service(client_id _client, service_id _service, instance_id _instance);
	void on_release_service(client_id _client, service_id _service, instance_id _instance);

	void on_message(const uint8_t *_data, uint32_t _size,
					const endpoint *_source, const endpoint *_target);

	bool send_find_service(client_state_machine_t *_service);
	bool send_offer_service(service_state_machine_t *_service, const endpoint *_target);
	bool send_stop_offer_service(service_state_machine_t *_service);

	bool send(message_base *_message, bool _flush);

	void send_service_state(service_id _service, instance_id _instance, bool _is_available);

private:
	typedef std::pair< boost::shared_ptr< client_state_machine_t >,
	  	           	   std::set< client_id > > client_info_t;

	typedef std::map< service_id,
					  std::map< instance_id,
					  	  	  	client_info_t > > client_machines_t;

	typedef std::map< service_id,
					  std::map< instance_id,
			  	  	  			boost::shared_ptr< service_state_machine_t > > > service_machines_t;

private:
	const client_info_t * find_client_info(service_id _service, instance_id _instance) const;
	client_state_machine_t * find_client_machine(service_id _service, instance_id _instance) const;
	service_state_machine_t * find_service_machine(service_id _service, instance_id _instance) const;

	std::string get_broadcast_address(const std::string &_address, const std::string &_mask) const;

	void on_find_service(const service_entry *_entry, const std::vector< option * > &_options, const endpoint *_source);
	void on_offer_service(const service_entry *_entry, const std::vector< option * > &_options);

private:
	client_machines_t its_clients_;
	service_machines_t its_services_;

	daemon *owner_;
	boost::asio::io_service &service_;

	const endpoint *broadcast_;
	session_id session_;

	boost::shared_ptr< deserializer > deserializer_;
	factory *factory_;
};

} // sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_IMPL_HPP

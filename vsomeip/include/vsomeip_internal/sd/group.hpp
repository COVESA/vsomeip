//
// group.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_GROUP_HPP
#define VSOMEIP_INTERNAL_SD_GROUP_HPP

#include <set>
#include <string>

#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/sd/group_machine.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

class client_proxy;
class message;
class option;
class service_discovery;
class service_proxy;

class group {
public:
	group(const std::string &_name, service_discovery *_owner);

	inline service_discovery * get_owner() const { return owner_; };

	void init();
	void start();

	const std::string & get_name() const;
	boost::asio::io_service & get_service();

	bool is_started() const;
	bool is_network_configured() const;

	void send_services_status(service_id _service, instance_id _instance, const endpoint *_target, bool _is_announcing);

	client_proxy * find_client(service_id _service,  instance_id _instance);
	client_proxy * add_client(service_id _service, instance_id _instance);
	void remove_client(service_id _service, instance_id _instance);

	service_proxy * find_service(service_id _service,  instance_id _instance);
	service_proxy * add_service(service_id _service, instance_id _instance);
	void remove_service(service_id _service, instance_id _instance);

	void on_service_withdraw();
	void on_service_started();
	void on_service_stopped();

	void on_service_requested();
	void on_service_released();

	void on_eventgroup_provided();
	void on_eventgroup_withdrawn();

	void on_eventgroup_requested();
	void on_eventgroup_released();

	void on_find_service(service_id _service, instance_id _instance, const endpoint *_source, bool _is_unicast_enabled);
	void on_offer_service(service_id _service, instance_id _instance, const endpoint *_source,
						  const endpoint *_reliable, const endpoint *_unreliable);
	void on_subscribe_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup,
			 	 	 	 	 	 const endpoint *_source, const endpoint *_multicast,
			 	 	 	 	 	 bool _stop);

private:
	option * add_endpoint_option(message *_message, const endpoint *_endpoint) const;

private:
	std::string name_;
	service_discovery *owner_;

	std::set< boost::shared_ptr< client_proxy > > clients_;
	std::set< boost::shared_ptr< service_proxy > > services_;

	bool is_client_started_;
	bool is_service_started_;

	group_machine machine_;
};

} // namespace sd
} // namespace vsomeip



#endif // VSOMEIP_INTERNAL_SD_SERVICE_GROUP_HPP

//
// service_manager_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_MANAGER_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_MANAGER_IMPL_HPP

#include <map>

#include <boost/shared_ptr.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/sd/service_manager.hpp>
#include <vsomeip_internal/sd/service_state_machine.hpp>

namespace vsomeip {

class application;
class message_base;

namespace sd {

class service_manager_impl
		: public service_manager {
public:
	service_manager_impl(boost::asio::io_service &_service);
	virtual ~service_manager_impl();

	application * get_owner() const;
	void set_owner(application *_owner);

	boost::asio::io_service & get_service();

	void on_provide_service(service_id _service, instance_id _instance);
	void on_withdraw_service(service_id _service, instance_id _instance);

	void on_start_service(service_id _service, instance_id _instance);
	void on_stop_service(service_id _service, instance_id _instance);

	void on_request_service(service_id _service, instance_id _instance);
	void on_release_service(service_id _service, instance_id _instance);

	bool send_offer_service(service_state_machine_t *_service, const endpoint *_target);
	bool send_stop_offer_service(service_state_machine_t *_service);
	bool send(message_base *_message, bool _flush);

private:
	typedef std::map< service_id,
					  std::map< instance_id,
			  	  	  			boost::shared_ptr< service_state_machine_t > > > service_machines_t;

	service_machines_t administrated_;

	application *owner_;
	boost::asio::io_service &service_;

	factory *factory_;
};

} // sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_SERVICE_MANAGER_IMPL_HPP

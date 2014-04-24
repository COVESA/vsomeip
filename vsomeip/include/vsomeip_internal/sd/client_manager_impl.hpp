//
// client_manager_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_CLIENT_MANAGER_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_CLIENT_MANAGER_IMPL_HPP

#include <map>

#include <boost/shared_ptr.hpp>

#include <vsomeip/primitive_types.hpp>

#include <vsomeip_internal/sd/client_manager.hpp>
#include <vsomeip_internal/sd/client_state_machine.hpp>
#include <vsomeip_internal/sd/deserializer.hpp>

namespace vsomeip {

class application;
class message_base;

namespace sd {

class client_manager_impl
		: public client_manager {
public:
	client_manager_impl(boost::asio::io_service &_service);
	virtual ~client_manager_impl();

	application * get_owner() const;
	void set_owner(application *_owner);

	boost::asio::io_service & get_service();

	void init();
	void start();
	void stop();

	void on_provide_service(service_id _service, instance_id _instance);
	void on_withdraw_service(service_id _service, instance_id _instance);

	void on_start_service(service_id _service, instance_id _instance);
	void on_stop_service(service_id _service, instance_id _instance);

	void on_request_service(service_id _service, instance_id _instance);
	void on_release_service(service_id _service, instance_id _instance);

	void on_message(const uint8_t *_data, uint32_t _size);

	bool send_find_service(client_state_machine_t *_machine);
	bool send(message_base *_message, bool _flush);

private:
	typedef std::map< service_id,
					  std::map< instance_id,
			  	  	  			boost::shared_ptr< client_state_machine_t > > > client_machines_t;

	client_machines_t administrated_;

	factory *factory_;
	application *owner_;
	boost::shared_ptr< deserializer > deserializer_;
	boost::asio::io_service &service_;
};

} // sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_CLIENT_MANAGER_IMPL_HPP

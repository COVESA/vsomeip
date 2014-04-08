//
// registry.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_REGISTRY_HPP
#define VSOMEIP_INTERNAL_SD_REGISTRY_HPP

#include <map>

#include <boost/shared_ptr.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/sd/service_behavior.hpp>

namespace vsomeip {

class daemon;
class message_base;

namespace sd {

class registry {
public:
	registry(daemon &_daemon);

	void on_provide_service(service_id _service, instance_id _instance);
	void on_withdraw_service(service_id _service, instance_id _instance);

	void on_start_service(service_id _service, instance_id _instance);
	void on_stop_service(service_id _service, instance_id _instance);

	void on_request_service(service_id _service, instance_id _instance);
	void on_release_service(service_id _service, instance_id _instance);

	bool send(const message_base *_message, bool _flush);

private:
	typedef std::map< service_id,
					  std::map< instance_id,
			  	  	  			boost::shared_ptr< service_fsm::behavior > > > service_behavior_t;

	service_behavior_t administrated_;

	daemon &daemon_;
};

} // sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_HPP

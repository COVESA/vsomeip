//
// managing_application_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_MANAGING_APPLICATION_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_MANAGING_APPLICATION_IMPL_HPP

#include <boost/shared_ptr.hpp>

#include <vsomeip_internal/sd/client_behavior.hpp>
#include <vsomeip_internal/managing_application_impl.hpp>

namespace vsomeip {

class client;

namespace sd {

class managing_application_impl : public vsomeip::managing_application_impl {
public:
	managing_application_impl(const std::string &_name);
	virtual ~managing_application_impl();

	void init(int _options_count, char **_options);

	bool request_service(service_id _service, instance_id _instance,
			 	 	     const endpoint * /* unused */);
	bool release_service(service_id _service, instance_id _instance);

	bool provide_service(service_id _service, instance_id _instance,
						 const endpoint *_location);
	bool withdraw_service(service_id _service, instance_id _instance,
						  const endpoint *_location);

	bool start_service(service_id _service, instance_id _instance);
	bool stop_service(service_id _service, instance_id _instance);

private:
	typedef std::map< service_id,
					  std::map< instance_id,
					  	  	    boost::shared_ptr< client_fsm::behavior > > > client_behavior_t;

	client_behavior_t used_clients_;
	boost::shared_ptr< client > sd_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_MANAGING_APPLICATION_IMPL_HPP

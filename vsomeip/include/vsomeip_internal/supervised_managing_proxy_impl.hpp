//
//supervised_managing_proxy_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SUPERVISED_MANAGING_PROXY_IMPL_HPP
#define VSOMEIP_INTERNAL_SUPERVISED_MANAGING_PROXY_IMPL_HPP

#include <vsomeip_internal/administration_proxy_impl.hpp>
#include <vsomeip_internal/managing_proxy_impl.hpp>

namespace vsomeip {

class supervised_managing_proxy_impl
		: virtual public administration_proxy_impl,
		  virtual public managing_proxy_impl {
public:
	supervised_managing_proxy_impl(application_base_impl &_owner);
	virtual ~supervised_managing_proxy_impl();

	void init();
	void start();
	void stop();

	bool provide_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool withdraw_service(service_id _service, instance_id _instance, const endpoint *_location);

	bool start_service(service_id _service, instance_id _instance);
	bool stop_service(service_id _service, instance_id _instance);

	bool request_service(service_id _service, instance_id _instance, const endpoint *_location);
	bool release_service(service_id _service, instance_id _instance);

	void register_method(service_id _service, instance_id _instance, method_id _method);
	void deregister_method(service_id _service, instance_id _instance, method_id _method);

	bool provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location);
	bool withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location);

	bool add_field(service_id _service, instance_id _instance, eventgroup_id _eventgroup, field *_field);
	bool remove_field(service_id _service, instance_id _instance, eventgroup_id _eventgroup, field *_field);

	bool request_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup);
	bool release_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup);

	bool enable_magic_cookies(service_id _service, instance_id _instance);
	bool disable_magic_cookies(service_id _service, instance_id _instance);

	void on_service_availability(service_id _service, instance_id _instance, const endpoint *_location, bool _is_available);
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SUPERVISED_MANAGING_PROXY_IMPL_HPP

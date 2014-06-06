//
// field_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_FIELD_IMPL_HPP
#define VSOMEIP_INTERNAL_FIELD_IMPL_HPP

#include <map>

#include <vsomeip/field.hpp>
#include <vsomeip_internal/payload_impl.hpp>
#include <vsomeip_internal/payload_owner.hpp>

namespace vsomeip {

class field_impl
		: public field,
		  public payload_owner {
public:
	field_impl(application *_application, service_id _service, instance_id _instance, event_id _event);
	virtual ~field_impl();

	service_id get_service() const;
	instance_id get_instance() const;
	event_id get_event() const;

	uint32_t get_update_cycle() const;
	void set_update_cycle(uint32_t _cycle);

	const payload & get_payload() const;
	payload & get_payload();

	void notify() const;

private:
	application *application_;

	service_id service_;
	instance_id instance_;
	event_id event_;

	uint32_t update_cycle_;
	payload_impl payload_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_FIELD_IMPL_HPP

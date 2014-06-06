//
// field_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iostream>

#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/field_impl.hpp>

namespace vsomeip {

field_impl::field_impl(application *_application, service_id _service, instance_id _instance, event_id _event)
	: application_(_application),
	  service_(_service),
	  instance_(_instance),
	  event_(_event),
	  update_cycle_(0),
	  payload_(this) {
}

field_impl::~field_impl() {
}

service_id field_impl::get_service() const {
	return service_;
}

instance_id field_impl::get_instance() const {
	return instance_;
}


event_id field_impl::get_event() const {
	return event_;
}

uint32_t field_impl::get_update_cycle() const {
	return update_cycle_;
}

void field_impl::set_update_cycle(uint32_t _update_cycle) {
	update_cycle_ = _update_cycle;
}

const payload & field_impl::get_payload() const {
	return payload_;
}

payload & field_impl::get_payload() {
	return payload_;
}

void field_impl::notify() const {
	application_->update_field(this);
}


} // namespace vsomeip




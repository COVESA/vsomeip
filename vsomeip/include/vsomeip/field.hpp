//
// field.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_FIELD_HPP
#define VSOMEIP_FIELD_HPP

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class payload;

class field {
public:
	virtual ~field() {};

	virtual service_id get_service() const = 0;
	virtual instance_id get_instance() const = 0;
	virtual event_id get_event() const = 0;

	virtual uint32_t get_update_cycle() const = 0;
	virtual void set_update_cycle(uint32_t _cycle) = 0;

	virtual const payload & get_payload() const = 0;
	virtual payload & get_payload() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_FIELD_HPP

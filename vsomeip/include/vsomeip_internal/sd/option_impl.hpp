//
// option_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_INTERNAL_SD_OPTION_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_OPTION_IMPL_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/sd/enumeration_types.hpp>
#include <vsomeip/sd/option.hpp>
#include <vsomeip_internal/sd/message_element_impl.hpp>

namespace vsomeip {

class serializer;
class deserializer;

namespace sd {

class message_impl;

class option_impl
		: virtual public option,
		  virtual public message_element_impl {
public:
	option_impl();
	virtual ~option_impl();

	uint16_t get_length() const;
	option_type get_type() const;

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

protected:
	uint16_t length_;
	option_type type_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_OPTION_IMPL_HPP

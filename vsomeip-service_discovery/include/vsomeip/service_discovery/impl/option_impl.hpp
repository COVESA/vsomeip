//
// option_impl.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL_OPTION_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_IMPL_OPTION_IMPL_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/service_discovery/enumeration_types.hpp>
#include <vsomeip/service_discovery/option.hpp>
#include <vsomeip/service_discovery/impl/message_element_impl.hpp>

namespace vsomeip {

class serializer;
class deserializer;

namespace service_discovery {

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

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_OPTION_IMPL_HPP

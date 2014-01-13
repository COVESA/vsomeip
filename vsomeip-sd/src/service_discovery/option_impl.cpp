//
// option_impl.cpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/impl/constants_impl.hpp>
#include <vsomeip/service_discovery/impl/option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

option_impl::option_impl() {
	length_ = 0;
	type_ = option_type::UNKNOWN;
}

option_impl::~option_impl() {
}

uint16_t option_impl::get_length() const {
	return length_;
}

option_type option_impl::get_type() const {
	return type_;
}

bool option_impl::serialize(vsomeip::serializer *_to) const {
	return (0 != _to
			&& _to->serialize(length_)
			&& _to->serialize(static_cast<uint8_t>(type_))
			&& _to->serialize(vsomeip_protocol_reserved_byte));
}

bool option_impl::deserialize(vsomeip::deserializer *_from) {
	uint8_t tmp_type, reserved;
	bool l_result = (0 != _from
			&& _from->deserialize(length_)
			&& _from->deserialize(tmp_type)
			&& _from->deserialize(reserved));

	if (l_result) {
		type_ = static_cast<option_type>(tmp_type);
	}

	return l_result;
}

} // namespace service_discovery
} // namespace vsomeip


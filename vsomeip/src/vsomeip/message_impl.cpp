//
// message_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/message_impl.hpp>

namespace vsomeip {

message_impl::~message_impl() {
};

length message_impl::get_length() const {
	return VSOMEIP_STATIC_HEADER_SIZE + payload_.get_length();
}

payload & message_impl::get_payload() {
	return payload_;
}

const payload & message_impl::get_payload() const {
	return payload_;
}

bool message_impl::serialize(serializer *_to) const {
	return (header_.serialize(_to) && payload_.serialize(_to));
}

bool message_impl::deserialize(deserializer *_from) {
	bool is_successful = header_.deserialize(_from);
	if (is_successful) {
		payload_.set_capacity(header_.length_);
		is_successful = payload_.deserialize(_from);
	}
	return is_successful;
}

} // namespace vsomeip

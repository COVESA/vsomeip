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
#include <vsomeip/constants.hpp>
#include <vsomeip/internal/byteorder.hpp>
#include <vsomeip/internal/message_impl.hpp>

namespace vsomeip {

message_impl::~message_impl() {
};

length message_impl::get_length() const {
	return VSOMEIP_STATIC_HEADER_LENGTH + payload_.get_length();
}

void message_impl::set_length(length _length) {
	payload_.set_capacity(_length - VSOMEIP_STATIC_HEADER_LENGTH);
}

payload & message_impl::get_payload() {
	return payload_;
}

bool message_impl::serialize(serializer *_to) const {
	return (header_.serialize(_to) && payload_.serialize(_to));
}

bool message_impl::deserialize(deserializer *_from) {
	return (header_.deserialize(_from) && payload_.deserialize(_from));
}

} // namespace vsomeip

//
// message_impl.cpp
//
// Date: 	Nov 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <vsomeip/impl/byteorder_impl.hpp>
#include <vsomeip/impl/constants_impl.hpp>
#include <vsomeip/impl/message_impl.hpp>

namespace vsomeip {

message_impl::~message_impl() {
};

length message_impl::get_length() const {
	return VSOMEIP_STATIC_HEADER_LENGTH + payload_.get_length();
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

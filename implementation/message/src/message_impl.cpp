// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/defines.hpp>

#include "../include/byteorder.hpp"
#include "../include/message_impl.hpp"

namespace vsomeip {

message_impl::~message_impl() {
};

length_t message_impl::get_length() const {
	return VSOMEIP_SOMEIP_HEADER_SIZE + payload_.get_length();
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

// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>

#include "../include/ipv6_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

ipv6_option_impl::ipv6_option_impl(bool _is_multicast) {
	length_ = (1 + 16 + 1 + 1 + 2);
	type_ = (_is_multicast ? option_type_e::IP6_MULTICAST : option_type_e::IP6_ENDPOINT) ;
}

ipv6_option_impl::~ipv6_option_impl() {
}

bool ipv6_option_impl::operator ==(const option_impl &_other) const {
	if (_other.get_type() != option_type_e::IP6_ENDPOINT)
		return false;

	const ipv6_option_impl& other
		= dynamic_cast< const ipv6_option_impl & >(_other);

	return true; // TODO:
}


bool ipv6_option_impl::serialize(vsomeip::serializer *_to) const {
	bool is_successful = option_impl::serialize(_to);
	// TODO: deserialize content
	return is_successful;
}

bool ipv6_option_impl::deserialize(vsomeip::deserializer *_from) {
	bool is_successful = option_impl::deserialize(_from);
	// TODO: deserialize content
	return is_successful;
}

} // namespace sd
} // namespace vsomeip




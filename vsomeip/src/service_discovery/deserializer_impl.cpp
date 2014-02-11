//
// deserializer_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <vsomeip/message_base.hpp>
#include <vsomeip/service_discovery/internal/deserializer_impl.hpp>
#include <vsomeip/service_discovery/internal/message_impl.hpp>

namespace vsomeip {
namespace service_discovery {

deserializer_impl::~deserializer_impl() {
}

message_base * deserializer_impl::deserialize_message() {

	if (position_[0] == 0xFF && position_[1] == 0xFF &&
		position_[2] == 0x81 && position_[3] == 0x00) {

		message_impl *tmp_message = new message_impl;
		if (0 == tmp_message)
			return 0;

		if (!tmp_message->deserialize(this))
			return 0;

		return tmp_message;
	}

	return vsomeip::deserializer_impl::deserialize_message();
}

} // namespace service_discovery
} // namespace vsomeip





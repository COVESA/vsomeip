//
// deserializer.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/sd/deserializer.hpp>
#include <vsomeip_internal/sd/message_impl.hpp>

namespace vsomeip {
namespace sd {

deserializer::deserializer()
	: vsomeip::deserializer() {
}

deserializer::deserializer(uint8_t *_data, std::size_t _length)
	: vsomeip::deserializer(_data, _length) {
}

deserializer::deserializer(const deserializer &_other)
	: vsomeip::deserializer(_other) {
}

deserializer::~deserializer() {
}

message * deserializer::deserialize_sd_message() {
	message_impl* deserialized_message = new message_impl;
	if (0 != deserialized_message) {
		if (false == deserialized_message->deserialize(this)) {
			delete deserialized_message;
			deserialized_message = 0;
		}
	}

	return deserialized_message;
}

} // namespace sd
} // namespace vsomeip

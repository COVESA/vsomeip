//
// factory_impl.cpp
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <vsomeip/impl/factory_impl.hpp>
#include <vsomeip/impl/message_impl.hpp>
#include <vsomeip/impl/serializer_impl.hpp>
#include <vsomeip/impl/deserializer_impl.hpp>

namespace vsomeip {

factory * factory_impl::get_default_factory() {
	static factory_impl factory__;
	return &factory__;
}

factory_impl::~factory_impl() {
}

message * factory_impl::create_message() const {
	return new message_impl;
}

serializer * factory_impl::create_serializer() const {
	return new serializer_impl;
}

deserializer * factory_impl::create_deserializer(uint8_t *_data, uint32_t _length) const {
	if (0 == _data)
		return new deserializer_impl;

	return new deserializer_impl(_data, _length);
}

}; // namespace vsomeip


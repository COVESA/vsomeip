//
// factory_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <vsomeip/service_discovery/impl/factory_impl.hpp>
#include <vsomeip/service_discovery/impl/message_impl.hpp>
#include <vsomeip/service_discovery/impl/deserializer_impl.hpp>

namespace vsomeip {
namespace service_discovery {

factory* factory_impl::get_default_factory() {
	static factory_impl factory__;
	return &factory__;
}

factory_impl::~factory_impl() {
}

message * factory_impl::create_service_discovery_message() const {
	return new message_impl;
}

deserializer * factory_impl::create_deserializer(uint8_t *_data, uint32_t _length) const {
	return new deserializer_impl(_data, _length);
}

} // namespace service_discovery
} // namespace vsomeip



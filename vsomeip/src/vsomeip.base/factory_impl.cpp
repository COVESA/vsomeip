//
// factory_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/factory_impl.hpp>

namespace vsomeip {

factory * factory_impl::get_instance() {
	static factory_impl the_factory;
	return &the_factory;
}

factory_impl::~factory_impl() {
}

application * factory_impl::create_application() const {
	return new application_impl;
}

} // namespace vsomeip



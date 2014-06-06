//
// factory_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/factory.hpp>
#include <vsomeip_internal/factory_impl.hpp>

namespace vsomeip {

factory * factory::get_instance() {
	return factory_impl::get_instance();
}

} // namespace vsomeip







//
// factory_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/sd/factory.hpp>
#include <vsomeip_internal/sd/factory_impl.hpp>

vsomeip::sd::factory * VSOMEIP_SD_FACTORY_SYMBOL;


static void init_vsomeip_sd() __attribute__((constructor));

static void init_vsomeip_sd() {
	VSOMEIP_SD_FACTORY_SYMBOL = vsomeip::sd::factory::get_instance();
}

namespace vsomeip {
namespace sd {

factory * factory::get_instance() {
	return factory_impl::get_instance();
}

} // namespace sd
} // namespace vsomeip

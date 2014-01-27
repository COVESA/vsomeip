//
// factory.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/service_discovery/impl/factory_impl.hpp>

namespace vsomeip {
namespace service_discovery {

factory * factory::get_default_factory() {
	return factory_impl::get_default_factory();
}

} // namespace service_discovery
} // namespace vsomeip




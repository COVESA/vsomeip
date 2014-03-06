//
// configuration.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/service_discovery/internal/configuration_impl.hpp>

namespace vsomeip {
namespace service_discovery {

configuration * configuration::get_instance() {
	return configuration_impl::get_instance();
}

} // namespace service_discovery
} // namespace vsomeip

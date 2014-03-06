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

#include <vsomeip/internal/configuration_impl.hpp>

namespace vsomeip {

configuration * configuration::get_instance() {
	return configuration_impl::get_instance();
}

} // namespace vsomeip

//
// factory.cpp
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <vsomeip/factory.hpp>
#include <vsomeip/impl/factory_impl.hpp>

namespace vsomeip {

factory* factory::get_default_factory() {
	return factory_impl::get_default_factory();
}

} // namespace vsomeip




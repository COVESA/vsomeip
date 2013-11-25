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

#include <vsomeip/sd/factory.h>
#include <vsomeip/sd/factoryimpl.h>

namespace vsomeip {

namespace sd {

Factory* Factory::getDefaultFactory() {
	return FactoryImpl::getDefaultFactory();
}

} // namespace sd

} // namespace vsomeip




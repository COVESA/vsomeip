//
// factoryimpl.cpp
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/sd/factoryimpl.h>
#include <vsomeip/sd/sdmessageimpl.h>

namespace vsomeip {

namespace sd {

Factory* FactoryImpl::getDefaultFactory() {
	static FactoryImpl s_factory;
	return &s_factory;
}

FactoryImpl::~FactoryImpl() {
}

SdMessage * FactoryImpl::createServiceDiscoveryMessage() const {
	return new SdMessageImpl;
}

} // namespace vsomeip

} // namespace vsomeip



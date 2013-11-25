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

#include <vsomeip/factoryimpl.h>
#include <vsomeip/applicationmessageimpl.h>

namespace vsomeip {

Factory* FactoryImpl::getDefaultFactory() {
	static FactoryImpl s_factory;
	return &s_factory;
}

FactoryImpl::~FactoryImpl() {
}

ApplicationMessage* FactoryImpl::createApplicationMessage() const {
	return new ApplicationMessageImpl;
}

}; // namespace vsomeip


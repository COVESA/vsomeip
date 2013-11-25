//
// factoryimpl.h
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_LIBRARY_FACTORYIMPL_H__
#define __VSOMEIP_SD_LIBRARY_FACTORYIMPL_H__

#include <vsomeip/factoryimpl.h>
#include <vsomeip/sd/factory.h>

namespace vsomeip {

namespace sd {

class FactoryImpl : virtual public Factory, virtual public vsomeip::FactoryImpl {
public:
	virtual ~FactoryImpl();

	virtual SdMessage* createServiceDiscoveryMessage() const;

	static vsomeip::sd::Factory * getDefaultFactory();
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_FACTORY_H__

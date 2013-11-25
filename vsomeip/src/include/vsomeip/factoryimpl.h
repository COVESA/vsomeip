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

#ifndef __VSOMEIP_LIBRARY_FACTORYIMPL_H__
#define __VSOMEIP_LIBRARY_FACTORYIMPL_H__

#include <vsomeip/factory.h>

namespace vsomeip {

class FactoryImpl : virtual public Factory {
public:
	virtual ~FactoryImpl();

	ApplicationMessage* createApplicationMessage() const;

	static Factory *getDefaultFactory();
};

}; // namespace vsomeip

#endif // __VSOMEIP_FACTORY_H__

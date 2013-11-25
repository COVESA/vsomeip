//
// factory.h
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_FACTORY_H__
#define __VSOMEIP_SD_FACTORY_H__

#include <vsomeip/factory.h>

namespace vsomeip {

namespace sd {

class SdMessage;

class Factory : virtual public vsomeip::Factory {
public:
	virtual ~Factory() {};

	virtual SdMessage * createServiceDiscoveryMessage() const = 0;

	static Factory * getDefaultFactory();
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_FACTORY_H__


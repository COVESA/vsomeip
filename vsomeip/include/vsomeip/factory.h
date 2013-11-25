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

#ifndef __VSOMEIP_FACTORY_H__
#define __VSOMEIP_FACTORY_H__

namespace vsomeip {

class ApplicationMessage;

class Factory {
public:
	virtual ~Factory() {};

	virtual ApplicationMessage * createApplicationMessage() const = 0;

	static Factory * getDefaultFactory();
};

}; // namespace vsomeip

#endif // __VSOMEIP_FACTORY_H__

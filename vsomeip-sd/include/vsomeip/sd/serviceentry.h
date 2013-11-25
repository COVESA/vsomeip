//
// serviceentry.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_SERVICEENTRY_H__
#define __VSOMEIP_SD_SERVICEENTRY_H__

#include <vsomeip/sd/entry.h>

namespace vsomeip {

namespace sd {

class ServiceEntry : virtual public Entry {
public:
	virtual ~ServiceEntry() {};

	virtual MinorVersion getMinorVersion() const = 0;
	virtual void setMinorVersion(MinorVersion a_minorVersion) = 0;
};

} // namespace sd

} // namespace vsomeip


#endif /* SERVICEENTRY_H_ */

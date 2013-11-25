//
// protectionoption.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_PROTECTIONOPTION_H__
#define __VSOMEIP_SD_PROTECTIONOPTION_H__

#include <vsomeip/sd/option.h>
#include <vsomeip/sd/types.h>

namespace vsomeip {

namespace sd {

class ProtectionOption: virtual public Option {
public:
	virtual ~ProtectionOption() {};

	virtual AliveCounter getAliveCounter() const = 0;
	virtual void setAliveCounter(AliveCounter a_aliveCounter) = 0;

	virtual Crc getCrc() const = 0;
	virtual void setCrc(Crc a_crc) = 0;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_PROTECTIONOPTION_H__

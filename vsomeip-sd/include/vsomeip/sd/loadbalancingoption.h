//
// loadbalancingoption.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_LOADBALANCINGOPTION_H__
#define __VSOMEIP_SD_LOADBALANCINGOPTION_H__

#include <vsomeip/sd/option.h>
#include <vsomeip/sd/types.h>

namespace vsomeip {

namespace sd {

class LoadBalancingOption : virtual public Option {
public:
	virtual ~LoadBalancingOption() {};

	virtual Priority getPriority() const = 0;
	virtual void setPriority(Priority a_priority) = 0;

	virtual Weight getWeight() const = 0;
	virtual void setWeight(Weight a_weight) = 0;
};

} // namespace sd

} // namespace vsomeip


#endif // __VSOMEIP_SD_LOADBALANCINGOPTION_H__

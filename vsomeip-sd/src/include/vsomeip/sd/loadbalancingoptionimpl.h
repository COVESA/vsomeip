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

#ifndef __VSOMEIP_LIBRARY_SD_LOADBALANCINGOPTION_H__
#define __VSOMEIP_LIBRARY_SD_LOADBALANCINGOPTION_H__

#include <vsomeip/sd/loadbalancingoption.h>

#include "optionimpl.h"

namespace vsomeip {

namespace sd {

class LoadBalancingOptionImpl : virtual public LoadBalancingOption, virtual public OptionImpl {
public:
	LoadBalancingOptionImpl();
	virtual ~LoadBalancingOptionImpl();
	bool operator ==(const Option& a_option) const;

	Priority getPriority() const;
	void setPriority(Priority a_priority);

	Weight getWeight() const;
	void setWeight(Weight a_weight);

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

private:
	Priority m_priority;
	Weight m_weight;
};

} // namespace sd

} // namespace vsomeip


#endif // __VSOMEIP_LIBRARY_SD_LOADBALANCINGOPTION_H__

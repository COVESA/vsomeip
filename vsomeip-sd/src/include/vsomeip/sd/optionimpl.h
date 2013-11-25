//
// optionimpl.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_OPTIONIMPL_H__
#define __VSOMEIP_LIBRARY_SD_OPTIONIMPL_H__

#include <vsomeip/serializable.h>
#include <vsomeip/sd/option.h>

#include "sdmessagepartimpl.h"

namespace vsomeip {

namespace sd {

class SdMessageImpl;

class OptionImpl : virtual public Option, virtual public SdMessagePartImpl {
public:
	OptionImpl();
	virtual ~OptionImpl();

	uint16_t getLength() const;
	OptionType getType() const;

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

protected:
	uint16_t m_length;
	OptionType m_type;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_LIBRARY_SD_OPTIONIMPL_H__

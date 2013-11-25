//
// protectionoptionimpl.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_PROTECTIONOPTION_H__
#define __VSOMEIP_LIBRARAY_SD_PROTECTIONOPTION_H__

#include <vsomeip/sd/protectionoption.h>
#include "optionimpl.h"

namespace vsomeip {

namespace sd {

class ProtectionOptionImpl: virtual public ProtectionOption, virtual public OptionImpl {
public:
	ProtectionOptionImpl();
	virtual ~ProtectionOptionImpl();
	bool operator ==(const Option& a_option) const;

	AliveCounter getAliveCounter() const;
	void setAliveCounter(AliveCounter a_aliveCounter);

	Crc getCrc() const;
	void setCrc(Crc a_crc);

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

private:
	AliveCounter m_aliveCounter;
	Crc m_crc;
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_SD_PROTECTIONOPTION_H__

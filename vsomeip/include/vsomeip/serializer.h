//
// serializer.h
//
// Date: 	Nov 5, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SERIALIZER_H__
#define __VSOMEIP_SERIALIZER_H__

#include <vsomeip/types.h>

namespace vsomeip {

class Serializable;

class Serializer {
public:
	//
	virtual ~Serializer();

	// API that must be implemented by specific Serializer
	virtual bool serialize8(const uint8_t a_data) = 0;
	virtual bool serialize16(const uint16_t a_data) = 0;
	virtual bool serialize24(const uint32_t a_data) = 0;
	virtual bool serialize32(const uint32_t a_data) = 0;
	virtual bool serializeX(const uint8_t *a_data, const uint32_t a_length) = 0;

	// Method that needs to be called to serialize an object
	bool serialize(Serializable *a_serializable);
};

} // namespace vsomeip

#endif /* __VSOMEIP_SERIALIZER_H__ */

//
// deserializer.h
//
// Date: 	Nov 5, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_DESERIALIZER_H__
#define __VSOMEIP_DESERIALIZER_H__

#include <vsomeip/types.h>

namespace vsomeip {

class Message;

class Deserializer {
public:
	virtual Deserializer *copy(bool a_isDeepCopy) = 0;
	virtual ~Deserializer() {};

	virtual bool deserialize8(uint8_t& a_data) = 0;
	virtual bool deserialize16(uint16_t& a_data) = 0;
	virtual bool deserialize24(uint32_t& a_data) = 0;
	virtual bool deserialize32(uint32_t& a_data) = 0;
	virtual bool deserializeX(uint8_t* a_data, uint32_t a_length) = 0;

	virtual Message * deserializeMessage() = 0;

	virtual uint32_t getContentLength() const = 0;
	virtual void setContentLength(uint32_t a_contentLength) = 0;
};

} // namespace vsomeip

#endif // __VSOMEIP_DESERIALIZER_H__

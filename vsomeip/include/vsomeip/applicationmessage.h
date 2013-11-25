//
// message.h
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_APPLICATIONMESSAGE_H__
#define __VSOMEIP_APPLICATIONMESSAGE_H__

#include <vsomeip/types.h>
#include <vsomeip/message.h>

namespace vsomeip {

class ApplicationMessage : virtual public Message {
public:
	virtual ~ApplicationMessage() {};

	virtual uint8_t * getPayload() const = 0;
	virtual uint32_t getPayloadLength() const = 0;
	virtual void setPayload(const uint8_t *a_buffer, uint32_t a_length) = 0;
};

} // namespace vsomeip


#endif // __VSOMEIP_APPLICATIONMESSAGE_H__

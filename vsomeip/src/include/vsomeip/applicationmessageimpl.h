//
// messageimpl.h
//
// Date: 	Oct 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_APPLICATIONMESSAGEIMPL_H__
#define __VSOMEIP_LIBRARY_APPLICATIONMESSAGEIMPL_H__

#include <vsomeip/applicationmessage.h>
#include "messageimpl.h"

namespace vsomeip {

class ApplicationMessageImpl : virtual public ApplicationMessage, virtual public MessageImpl {
public:
	ApplicationMessageImpl();
	virtual ~ApplicationMessageImpl();

	uint8_t* getPayload() const;
	uint32_t getPayloadLength() const;
	void setPayload(const uint8_t *a_buffer, uint32_t a_length);

	bool serialize(Serializer *a_serializer) const;
	bool deserialize(Deserializer *a_deserializer);

private:
	uint8_t *m_payload;
	uint32_t m_payloadLength;
};

} // namespace vsomeip


#endif // __VSOMEIP_LIBRARY_APPLICATIONMESSAGE_H__

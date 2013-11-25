//
// bufferdeserializer.h
//
// Date: 	Nov 14, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_BUFFERDESERIALIZER_H__
#define __VSOMEIP_SD_BUFFERDESERIALIZER_H__

#include <vsomeip/sd/deserializer.h>

#include <vsomeip/bufferdeserializer.h>

namespace vsomeip {

namespace sd {

class BufferDeserializer: virtual public Deserializer,
		virtual public vsomeip::BufferDeserializer {
public:
	BufferDeserializer();
	BufferDeserializer(const BufferDeserializer& a_deserializer,
			bool a_isDeepCopy);
	Deserializer *copy(bool a_isDeepCopy);
	virtual ~BufferDeserializer();

	Message * deserializeMessage();
	Entry * deserializeEntry();
	Option * deserializeOption();
};

} // namespace sd

} // namespace vsomeip

#endif /* __VSOMEIP_SD_BUFFERDESERIALIZER_H__ */

//
// bufferserializer.h
//
// Date: 	Nov 5, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_BUFFERSERIALIZER_H__
#define __VSOMEIP_BUFFERSERIALIZER_H__

#include <vsomeip/serializer.h>

namespace vsomeip {

class BufferSerializer : virtual public Serializer {
public:
	BufferSerializer();
	virtual ~BufferSerializer();

	// API that must be implemented by specific Serializer
	bool serialize8(const uint8_t a_data);
	bool serialize16(const uint16_t a_data);
	bool serialize24(const uint32_t a_data);
	bool serialize32(const uint32_t a_data);
	bool serializeX(const uint8_t *a_data, const uint32_t a_length);

	void getBufferInfo(uint8_t*& a_buffer, uint32_t& a_length) const ;
	void createBuffer(uint8_t a_length);
	void setBuffer(uint8_t *a_buffer, uint32_t a_length);

private:
	uint8_t *m_buffer;
	uint32_t m_length;
	bool m_isOwningBuffer;

	uint8_t *m_last;
	uint32_t m_remaining;
};

} // namespace vsomeip

#endif /* BUFFERSERIALIZER_H_ */

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

#ifndef __VSOMEIP_BUFFERDESERIALIZER_H__
#define __VSOMEIP_BUFFERDESERIALIZER_H__

#include <vsomeip/deserializer.h>

namespace vsomeip {

class BufferDeserializer : virtual public Deserializer {
public:
	BufferDeserializer();
	BufferDeserializer(const BufferDeserializer& a_deserializer, bool a_isDeepCopy);
	Deserializer *copy(bool a_isDeepCopy);
	virtual ~BufferDeserializer();

	bool deserialize8(uint8_t& a_data);
	bool deserialize16(uint16_t& a_data);
	bool deserialize24(uint32_t& a_data);
	bool deserialize32(uint32_t& a_data);
	bool deserializeX(uint8_t* a_data, uint32_t a_length);

	Message * deserializeMessage();

	uint32_t getContentLength() const;
	void setContentLength(uint32_t a_contentLength);

	// Specific part
	void getBufferInfo(uint8_t*& a_buffer, uint32_t& a_length) const;
	void createBuffer(uint8_t a_length);
	void setBuffer(uint8_t *a_buffer, uint32_t a_length);

protected:
	uint8_t *m_buffer;
	uint32_t m_length;
	bool m_isOwningBuffer;
};

} // namespace vsomeip

#endif /* __VSOMEIP_BUFFERDESERIALIZER_H__ */

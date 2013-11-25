//
// bufferdeserializer.cpp
//
// Date: 	Nov 14, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <cstring>

#include <vsomeip/macros.h>
#include <vsomeip/applicationmessageimpl.h>

#include <vsomeip/bufferdeserializer.h>

namespace vsomeip {

BufferDeserializer::BufferDeserializer() {
	m_buffer = 0;
	m_length = 0;
	m_isOwningBuffer = false;
}

BufferDeserializer::BufferDeserializer(const BufferDeserializer& a_deserializer, bool a_isDeepCopy) {
	m_length = a_deserializer.m_length;
	if (a_isDeepCopy && a_deserializer.m_buffer) {
		m_buffer = new uint8_t[m_length];
		m_isOwningBuffer = true;
		if (m_buffer) {
			memcpy(m_buffer, a_deserializer.m_buffer, m_length);
		} else {
			// TODO: throw exception here!!!
		}
	} else {
		m_buffer = a_deserializer.m_buffer;
	}
}

Deserializer *BufferDeserializer::copy(bool a_isDeepCopy) {
	return new BufferDeserializer(*this, a_isDeepCopy);
}

BufferDeserializer::~BufferDeserializer() {
	if (m_isOwningBuffer)
		delete [] m_buffer;
}

uint32_t BufferDeserializer::getContentLength() const {
	return m_length;
}

void BufferDeserializer::setContentLength(uint32_t a_contentLength) {
	m_length = a_contentLength;
}

bool BufferDeserializer::deserialize8(uint8_t& a_data) {
	if (m_length < 1)
		return false;

	a_data = *m_buffer++;
	m_length--;

	return true;
}

bool BufferDeserializer::deserialize16(uint16_t& a_data) {
	if (m_length < 2)
		return false;

	a_data = VSOMEIP_BYTES_TO_WORD(*m_buffer, *(m_buffer+1));
	m_buffer += 2;
	m_length -= 2;

	return true;
}

bool BufferDeserializer::deserialize24(uint32_t& a_data) {
	if (m_length < 2)
		return false;

	a_data = VSOMEIP_BYTES_TO_LONG(0x0, *m_buffer, *(m_buffer+1), *(m_buffer+2));
	m_buffer += 3;
	m_length -= 3;

	return true;
}

bool BufferDeserializer::deserialize32(uint32_t& a_data) {
	if (m_length < 2)
		return false;

	a_data = VSOMEIP_BYTES_TO_LONG(*m_buffer, *(m_buffer+1), *(m_buffer+2), *(m_buffer+3));
	m_buffer += 4;
	m_length -= 4;

	return true;
}

bool BufferDeserializer::deserializeX(uint8_t* a_data, uint32_t a_length) {
	if (m_length < a_length)
		return false;

	memcpy(a_data, m_buffer, a_length);
	m_buffer += a_length;
	m_length -= a_length;

	return true;
}

Message * BufferDeserializer::deserializeMessage() {
	Message* l_message = 0;

	l_message = new ApplicationMessageImpl;
	if (0 != l_message) {
		if (false == l_message->deserialize(this)) {
			delete l_message;
			l_message = 0;
		}
	}

	return l_message;
}

void BufferDeserializer::getBufferInfo(uint8_t*& a_buffer, uint32_t& a_length) const {
	a_buffer = m_buffer;
	a_length = m_length;
}

void BufferDeserializer::setBuffer(uint8_t *a_buffer, uint32_t a_length) {
	if (m_isOwningBuffer)
		delete [] m_buffer;

	m_buffer = a_buffer;
	m_length = a_length;
	m_isOwningBuffer = false;
}

} // namespace vsomeip

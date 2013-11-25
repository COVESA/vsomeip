//
// BufferSerializer.cpp
//
// Date: 	Nov 5, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <cstring>
#include <vsomeip/macros.h>
#include <vsomeip/bufferserializer.h>

namespace vsomeip {

BufferSerializer::BufferSerializer() {
	m_buffer = 0;
	m_length = 0;
	m_isOwningBuffer = false;

	m_last = 0;
	m_remaining = 0;
}

BufferSerializer::~BufferSerializer() {
	if (m_isOwningBuffer)
		delete [] m_buffer;
}

bool BufferSerializer::serialize8(const uint8_t a_data) {
	if (m_remaining < 1)
		return false;

	*m_last++ = a_data;
	m_remaining--;
	return true;
}

bool BufferSerializer::serialize16(const uint16_t a_data) {
	if (m_remaining < 2)
		return false;

	*m_last++ = VSOMEIP_WORD_BYTE1(a_data);
	*m_last++ = VSOMEIP_WORD_BYTE0(a_data);
	m_remaining -= 2;
	return true;
}

bool BufferSerializer::serialize24(const uint32_t a_data) {
	if (m_remaining < 3)
		return false;

	*m_last++ = VSOMEIP_LONG_BYTE2(a_data);
	*m_last++ = VSOMEIP_LONG_BYTE1(a_data);
	*m_last++ = VSOMEIP_LONG_BYTE0(a_data);
	m_remaining -= 3;
	return true;
}

bool BufferSerializer::serialize32(const uint32_t a_data) {
	if (m_remaining < 4)
		return false;

	*m_last++ = VSOMEIP_LONG_BYTE3(a_data);
	*m_last++ = VSOMEIP_LONG_BYTE2(a_data);
	*m_last++ = VSOMEIP_LONG_BYTE1(a_data);
	*m_last++ = VSOMEIP_LONG_BYTE0(a_data);
	m_remaining -= 4;
	return true;
}

bool BufferSerializer::serializeX(const uint8_t *a_data, const uint32_t a_length) {
	if (m_remaining < a_length)
		return false;

	memcpy(m_last, a_data, a_length);
	m_last += a_length;
	m_remaining -= a_length;

	return true;
}


void BufferSerializer::getBufferInfo(uint8_t*& a_buffer, uint32_t& a_length) const {
	a_buffer = m_buffer;
	a_length = m_length;
}

void BufferSerializer::createBuffer(uint8_t a_length) {
	if (m_isOwningBuffer)
		delete [] m_buffer;

	if (a_length > 0) {
		m_buffer = new uint8_t[a_length];
		if (m_buffer) {
			m_length = a_length;
			m_isOwningBuffer = true;

			m_last = m_buffer;
			m_remaining = m_length;
		} else {
			m_length = 0;
			m_isOwningBuffer = false;

			m_last = 0;
			m_remaining = 0;
		}
	}
}

void BufferSerializer::setBuffer(uint8_t *a_buffer, uint32_t a_length) {
	if (m_isOwningBuffer)
		delete [] m_buffer;

	m_buffer = a_buffer;
	m_length = a_length;
	m_isOwningBuffer = false;

	m_last = m_buffer;
	m_remaining = m_length;
}


} // namespace vsomeip

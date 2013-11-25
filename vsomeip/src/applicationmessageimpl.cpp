//
// applicationmessageimpl.cpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <cstring>

#include <vsomeip/serializer.h>
#include <vsomeip/deserializer.h>

#include <vsomeip/applicationmessageimpl.h>

namespace vsomeip {

ApplicationMessageImpl::ApplicationMessageImpl() {
	m_payload = 0;
	m_payloadLength = 0;
}

ApplicationMessageImpl::~ApplicationMessageImpl() {
	delete [] m_payload;
	m_payloadLength = 0;
}

uint8_t* ApplicationMessageImpl::getPayload() const {
	return m_payload;
}

uint32_t ApplicationMessageImpl::getPayloadLength() const {
	return m_payloadLength;
}

void ApplicationMessageImpl::setPayload(const uint8_t *a_payload, uint32_t a_payloadLength) {
	// clear payload
	delete [] m_payload;
	m_payload = 0;
	m_payloadLength = 0;
	setLength(VSOMEIP_MESSAGE_FULL_HEADER_LENGTH - VSOMEIP_MESSAGE_FIXED_HEADER_LENGTH);

	// if applicable, set payload
	if (a_payload && VSOMEIP_MESSAGE_MAX_LENGTH > a_payloadLength) {
		m_payload = new uint8_t[m_payloadLength];
		if (m_payload) {
			memcpy(m_payload, a_payload, a_payloadLength);
			m_payloadLength = a_payloadLength;
			setLength(getLength() + m_payloadLength);
		}
	}
}

bool ApplicationMessageImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful = MessageImpl::serialize(a_serializer);
	a_serializer->serializeX(m_payload, m_payloadLength);
	return l_isSuccessful;
}

bool ApplicationMessageImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = MessageImpl::deserialize(a_deserializer);
	a_deserializer->deserializeX(m_payload, m_payloadLength);
	return l_isSuccessful;
}

} // namespace vsomeip




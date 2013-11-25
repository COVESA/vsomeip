//
// messageimpl.cpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/serializer.h>
#include <vsomeip/deserializer.h>

#include <vsomeip/macros.h>
#include <vsomeip/messageimpl.h>

namespace vsomeip {

MessageImpl::MessageImpl() {
	m_serviceId = 0x0;
	m_methodId = 0x0;
	m_length = VSOMEIP_MESSAGE_FIXED_HEADER_LENGTH;
	m_clientId = 0x0;
	m_sessionId = 0x0;
	m_protocolVersion = VSOMEIP_PROTOCOL_VERSION;
	m_interfaceVersion = 0x0;
	m_messageType = MessageType::UNKNOWN;
	m_returnCode = ReturnCode::UNKNOWN;
}

MessageImpl::~MessageImpl() {
}

MessageId MessageImpl::getMessageId() const {
	return static_cast<MessageId>(VSOMEIP_WORDS_TO_LONG(m_serviceId, m_methodId));
}

void MessageImpl::setMessageId(MessageId a_messageId) {
	m_serviceId = static_cast<ServiceId>(VSOMEIP_LONG_WORD0(a_messageId));
	m_methodId = static_cast<MethodId>(VSOMEIP_LONG_WORD1(a_messageId));
}

ServiceId MessageImpl::getServiceId() const {
	return m_serviceId;
}

void MessageImpl::setServiceId(ServiceId a_serviceId) {
	m_serviceId = a_serviceId;
}

MethodId MessageImpl::getMethodId() const {
	return m_methodId;
}

void MessageImpl::setMethodId(MethodId a_methodId) {
	m_methodId = a_methodId;
}

uint32_t MessageImpl::getLength() const {
	return m_length;
}

RequestId MessageImpl::getRequestId() const {
	return static_cast<RequestId>(VSOMEIP_WORDS_TO_LONG(m_clientId, m_sessionId));
}

void MessageImpl::setRequestId(RequestId a_requestId) {
	m_clientId = static_cast<ClientId>(VSOMEIP_LONG_WORD0(a_requestId));
	m_sessionId = static_cast<SessionId>(VSOMEIP_LONG_WORD1(a_requestId));
}


ClientId MessageImpl::getClientId() const {
	return m_clientId;
}

void MessageImpl::setClientId(ClientId a_clientId) {
	m_clientId = a_clientId;
}

SessionId MessageImpl::getSessionId() const {
	return m_sessionId;
}

void MessageImpl::setSessionId(SessionId a_sessionId) {
	m_sessionId = a_sessionId;
}

ProtocolVersion MessageImpl::getProtocolVersion() const {
	return m_protocolVersion;
}

InterfaceVersion MessageImpl::getInterfaceVersion() const {
	return m_interfaceVersion;
}

void MessageImpl::setInterfaceVersion(InterfaceVersion a_interfaceVersion) {
	m_interfaceVersion = a_interfaceVersion;
}

MessageType MessageImpl::getMessageType() const {
	return m_messageType;
}

void MessageImpl::setMessageType(MessageType a_messageType) {
	m_messageType = a_messageType;
}

ReturnCode MessageImpl::getReturnCode() const {
	return m_returnCode;
}

void MessageImpl::setReturnCode(ReturnCode a_returnCode) {
	m_returnCode = a_returnCode;
}

void MessageImpl::setProtocolVersion(ProtocolVersion a_protocolVersion) {
	m_protocolVersion = a_protocolVersion;
}

void MessageImpl::setLength(uint32_t a_length) {
	m_length = a_length;
}

bool MessageImpl::serialize(Serializer *a_serializer) const {
	bool l_result;

	l_result = a_serializer->serialize16(m_serviceId);
	l_result = l_result && a_serializer->serialize16(m_methodId);
	l_result = l_result && a_serializer->serialize32(m_length);
	l_result = l_result && a_serializer->serialize16(m_clientId);
	l_result = l_result && a_serializer->serialize16(m_sessionId);
	l_result = l_result && a_serializer->serialize8(m_protocolVersion);
	l_result = l_result && a_serializer->serialize8(m_interfaceVersion);
	l_result = l_result && a_serializer->serialize8(static_cast<uint8_t>(m_messageType));
	l_result = l_result && a_serializer->serialize8(static_cast<uint8_t>(m_returnCode));

	return l_result;
}

bool MessageImpl::deserialize(Deserializer *a_deserializer) {
	bool l_result;
	uint8_t l_deserializedByte;

	l_result = a_deserializer->deserialize16(m_serviceId);
	l_result = l_result && a_deserializer->deserialize16(m_methodId);
	l_result = l_result && a_deserializer->deserialize32(m_length);
	l_result = l_result && a_deserializer->deserialize16(m_clientId);
	l_result = l_result && a_deserializer->deserialize16(m_sessionId);
	l_result = l_result && a_deserializer->deserialize8(m_protocolVersion);
	l_result = l_result && a_deserializer->deserialize8(m_interfaceVersion);
	l_result = l_result && a_deserializer->deserialize8(l_deserializedByte);
	m_messageType = static_cast<MessageType>(l_deserializedByte);
	l_result = l_result && a_deserializer->deserialize8(l_deserializedByte);
	m_returnCode = static_cast<ReturnCode>(l_deserializedByte);

	return l_result;
}

} // namespace vsomeip





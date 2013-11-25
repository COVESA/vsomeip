//
// messageheaderimpl.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_MESSAGEIMPL_H__
#define __VSOMEIP_LIBRARY_MESSAGEIMPL_H__

#include <vsomeip/message.h>

namespace vsomeip {

class MessageImpl : virtual public Message {

public:
	MessageImpl();
	virtual ~MessageImpl();

	MessageId getMessageId() const;
	void setMessageId(MessageId a_messageId);

	ServiceId getServiceId() const;
	void setServiceId(ServiceId a_serviceId);

	MethodId getMethodId() const;
	void setMethodId(MethodId a_methodId);

	uint32_t getLength() const;

	RequestId getRequestId() const;
	void setRequestId(RequestId a_requestId);

	ClientId getClientId() const;
	void setClientId(ClientId a_clientId);

	SessionId getSessionId() const;
	void setSessionId(SessionId a_sessionId);

	ProtocolVersion getProtocolVersion() const;

	InterfaceVersion getInterfaceVersion() const;
	void setInterfaceVersion(InterfaceVersion a_interfaceVersion);

	MessageType getMessageType() const;
	void setMessageType(MessageType a_messageType);

	ReturnCode getReturnCode() const;
	void setReturnCode(ReturnCode a_returnCode);

	void setLength(uint32_t a_length);

	void setProtocolVersion(ProtocolVersion a_protocolVersion);

	bool serialize(Serializer *a_serializer) const;
	bool deserialize(Deserializer *a_deserializer);

protected:
	ServiceId m_serviceId;
	MethodId m_methodId;

	uint32_t m_length;

	ClientId m_clientId;
	SessionId m_sessionId;

	ProtocolVersion m_protocolVersion;
	InterfaceVersion m_interfaceVersion;
	MessageType m_messageType;
	ReturnCode m_returnCode;
};

} // namespace vsomeip

#endif // __VSOMEIP_LIBRARY_MESSAGEIMPL_H__

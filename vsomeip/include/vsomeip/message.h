//
// message.h
//
// Date: 	Oct 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_MESSAGE_H__
#define __VSOMEIP_MESSAGE_H__

#include <vsomeip/serializable.h>
#include <vsomeip/types.h>

namespace vsomeip {

class Message : public Serializable {
public:
	virtual ~Message() {};

	virtual MessageId getMessageId() const = 0;
	virtual void setMessageId(MessageId a_messageId) = 0;

	virtual ServiceId getServiceId() const = 0;
	virtual void setServiceId(ServiceId a_serviceId) = 0;

	virtual MethodId getMethodId() const = 0;
	virtual void setMethodId(MethodId a_methodId) = 0;

	virtual uint32_t getLength() const = 0;

	virtual RequestId getRequestId() const = 0;
	virtual void setRequestId(RequestId a_requestId) = 0;

	virtual ClientId getClientId() const = 0;
	virtual void setClientId(ClientId a_clientId) = 0;

	virtual SessionId getSessionId() const = 0;
	virtual void setSessionId(SessionId a_sessionId) = 0;

	virtual ProtocolVersion getProtocolVersion() const = 0;

	virtual InterfaceVersion getInterfaceVersion() const = 0;
	virtual void setInterfaceVersion(InterfaceVersion a_interfaceVersion) = 0;

	virtual MessageType getMessageType() const = 0;
	virtual void setMessageType(MessageType a_messageType) = 0;

	virtual ReturnCode getReturnCode() const = 0;
	virtual void setReturnCode(ReturnCode a_returnCode) = 0;
};

} // namespace vsomeip


#endif // __VSOMEIP_MESSAGE_H__

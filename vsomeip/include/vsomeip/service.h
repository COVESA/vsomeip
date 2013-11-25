//
// client.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_CLIENT_H__
#define __VSOMEIP_CLIENT_H__

#include <vsomeip/config.h>
#include <vsomeip/message.h>

namespace vsomeip {

class Service {
public:
	Service();
	Service(int a_domain, int a_type, int a_protocol, int m_port);
	virtual ~Service();

	void init();
	void start();

	bool sendMessage(const Message& a_message);
	virtual void receiveMessage(const Message& a_message) = 0;

	int getDomain() const;
	void setDomain(int a_domain);

	int getType() const;
	void setType(int a_type);

	int getProtocol() const;
	void setProtocol(int a_protocol);

private:
	int m_domain;
	int m_type;
	int m_protocol;

	int m_port;
	int m_socket;
};

} // namespace vsomeip

#endif // __VSOMEIP_CLIENT_H__

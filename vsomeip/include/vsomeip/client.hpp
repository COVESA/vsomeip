//
// client.hpp
//
// Date: 	Jan 13, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_CLIENT_HPP
#define VSOMEIP_CLIENT_HPP

namespace vsomeip {

class message;
class service;

class receiver;

class client {
public:
	virtual ~client() {};

	virtual void send(const service& _service, const message &_message) = 0;

	virtual void register_receiver(receiver *_receiver) = 0;
	virtual void unregister_receiver(receiver *_receiver) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_HPP

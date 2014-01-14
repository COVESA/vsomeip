//
// receiver.hpp
//
// Date: 	Jan 14, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_RECEIVER_HPP
#define VSOMEIP_RECEIVER_HPP

namespace vsomeip {

class message;

class receiver {
public:
	virtual ~receiver() {};

	virtual void receive(const message &_message) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_RECEIVER_HPP

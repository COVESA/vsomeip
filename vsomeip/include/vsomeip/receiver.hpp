//
// receiver.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_RECEIVER_HPP
#define VSOMEIP_RECEIVER_HPP

namespace vsomeip {

class message_base;

class receiver {
public:
	virtual ~receiver() {};

	virtual void receive(const message_base *_message) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_RECEIVER_HPP

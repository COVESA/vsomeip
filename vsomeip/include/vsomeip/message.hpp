//
// message.hpp
//
// Date: 	Nov 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_MESSAGE_HPP
#define VSOMEIP_MESSAGE_HPP

#include <vsomeip/payload.hpp>
#include <vsomeip/message_base.hpp>

namespace vsomeip {

class message : virtual public message_base {
public:
	virtual ~message() {};

	virtual payload & get_payload() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_HPP

//
// message.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_MESSAGE_HPP
#define VSOMEIP_MESSAGE_HPP

#include <vsomeip/payload.hpp>
#include <vsomeip/message_base.hpp>

namespace vsomeip {

/// Interface for application messages, consisting of header and payload.
/// The header is accessible by methods inherited from the #message_base
/// interface.
class message : virtual public message_base {
public:
	virtual ~message() {};

    /// Get a reference to the payload of the message object.
    /// \returns Reference to the messages payload
	virtual payload & get_payload() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_HPP

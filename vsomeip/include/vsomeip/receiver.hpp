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

/// Applications must create a class derived from receiver and register
/// one or more instances to #client and/or #service instances in order
/// to receive Some/IP messages.
class receiver {
public:
	virtual ~receiver() {};

    /// Method that is called whenever a Some/IP client or service has
    /// received a message. 
    /// \param _message Pointer to the message instance
    /// \warning The message instance will be deleted by the client or
    /// service after the receive function has returned.
	virtual void receive(const message_base *_message) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_RECEIVER_HPP

//
// service.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_HPP
#define VSOMEIP_SERVICE_HPP

#include <vsomeip/participant.hpp>

namespace vsomeip {

class endpoint;

/// \interface service
/// Interface of a Some/IP service. An application needs to create one service
/// instance for each endpoint it wants to use to provide a service. Service 
/// instances can be created by a call to factory::create_service.
class service
		: virtual public participant {
public:    
    /// Allows to send an already serialized Some/IP message. The behavior is
	/// equal to sending a Some/IP message using the provided message structure.
	/// \param _data Pointer to the serialized message data.
	/// \param _size Amount of data that shall be sent.
    /// \param _endpoint target endpoint the message shall be sent to
	/// \param _flush Flag that must be set to true (default) if the the message
	/// shall be sent directly or to false if the packetizer shall be used.
	/// \warning This method is not thread safe! If you want to call it on the same object
	/// from different threads, you need to synchronize the calls.
	virtual bool send(const uint8_t *_data, uint32_t _size,
						endpoint *_target, bool _flush = true) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICE_HPP

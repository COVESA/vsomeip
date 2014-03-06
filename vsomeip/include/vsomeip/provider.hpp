//
// provider.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_PROVIDER_HPP
#define VSOMEIP_PROVIDER_HPP

#include <vsomeip/participant.hpp>

namespace vsomeip {

class endpoint;

/// \interface service
/// Interface of a Some/IP service. An application needs to create one service
/// instance for each endpoint it wants to use to provide a service. Service 
/// instances can be created by a call to factory::create_service.
class provider
		: virtual public participant {
public:
	/// Send a Some/IP message using the message structure provided by
	/// vsomeip. A call to send immediately returns and the message is sent
	/// asynchronously. The point in time the message is sent also depends on
	/// the setting for the parameter _flush. If _flush is set to false, the
	/// message is queued and sent together with other messages in a single
	/// data packet as soon as a call to send with parameter _flush set to true
	/// follows or the maximum data packet size is exceeded or no flushing was
	/// done for at least VSOMEIP_FLUSH_TIMEOUT milliseconds.
	/// \param _message Pointer to the message object that shall be sent.
	/// \param _flush Flag that must be set to true (default) if the the message
	/// shall be sent directly or to false if the packetizer shall be used.
	/// \warning This method is not thread safe! If you want to call it on the
	/// same object from different threads, you need to synchronize the calls.
	virtual bool send(const message_base *_message, bool _flush = true) = 0;
    
    /// Send an already serialized Some/IP message. The behavior is
	/// equal to sending a Some/IP message using the provided message structure.
	/// \param _data Pointer to the serialized message data.
	/// \param _size Amount of data that shall be sent.
    /// \param _target target endpoint the message shall be sent to
	/// \param _flush Flag that must be set to true (default) if the the message
	/// shall be sent directly or to false if the packetizer shall be used.
	/// \warning This method is not thread safe! If you want to call it on the
	/// same object from different threads, you need to synchronize the calls.
	virtual bool send(const uint8_t *_data, uint32_t _size,
						endpoint *_target, bool _flush = true) = 0;

	/// Flush data stored in packetizer.
	/// \param _target Either the endpoint the flushing is targeted at or 0.
	/// In case of 0, all packetizer content will be flushed.
	/// \returns true if data was flushed, false otherwise
	virtual bool flush(endpoint *_target = 0) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_PROVIDER_HPP

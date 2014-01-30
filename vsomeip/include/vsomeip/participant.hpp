//
// participant.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_PARTICIPANT_HPP
#define VSOMEIP_PARTICIPANT_HPP

#include <vsomeip/constants.hpp>

#ifdef USE_VSOMEIP_STATISTICS
#include <vsomeip/statistics_owner.hpp>
#endif

namespace vsomeip {

class message_base;
class receiver;

/// \interface participant
/// Common base class for participants (clients and services) in a Some/IP
/// communication. This interface provides the methods control the lifecycle
/// of the participant as well as access to the service registration that is
/// used to filter incoming messages and the Some/IP synchronization mechanism
/// (Magic Cookies).
class participant
#ifdef USE_VSOMEIP_STATISTICS
: virtual public statistics_owner
#endif
{
public:
	/// Starts the client or service. Before being called, the participant
	/// will neither send nor receive Some/IP messages.
	virtual void start() = 0;
	/// Stops the client or service. After being called, the participant
	/// will neither send nor receive Some/IP messages.
	virtual void stop() = 0;

	/// Register a receiver for a specific Some/IP method.
	/// \param _receiver Pointer to the receiver that is called for incoming
	/// messages.
	/// \param _service_id Identifier of the Some/IP service the receiver wants
	/// to register for.
	/// \param _method_id Identifier of the Some/IP method the receiver want to
	/// register for.
	/// \warning This method is not thread safe. If an application must call it
	/// from different thread contexts, it needs to ensure synchronization.
	virtual void register_for(receiver *_receiver,
			 	 	 	 	 	 service_id _service_id,
			 	 	 	 	 	 method_id _method_id) = 0;
	/// Unregister a receiver for a specific Some/IP method.
	/// \param _receiver Pointer to the receiver that is called for incoming
	/// messages.
	/// \param _service_id Identifier of the Some/IP service the receiver wants
	/// to unregister for.
	/// \param _method_id Identifier of the Some/IP method the receiver want to
	/// unregister for.
	/// \warning This method is not thread safe. If an application must call it
	/// from different thread contexts, it needs to ensure synchronization.
	virtual void unregister_for(receiver * receiver,
								   service_id _service_id,
								   method_id _method_id) = 0;

	/// Consume one queued event. This e.g. triggers one send or receive operation
	/// to be actually executed.
	/// \warning Must be called from a single thread.
	virtual std::size_t poll_one() = 0;
	/// Consumes all queued events. This e.g. triggers send or receive operations
	/// to be actually executed.
	/// \warning Must be called from a single thread.
	virtual std::size_t poll() = 0;
	/// Consumes all queued events. This e.g. triggers send or receive operations
	/// to be actually executed. Different to #poll, run is blocking until all queued
	/// events are consumed.
	/// \warning Must be called from a single thread.
	virtual std::size_t run() = 0;
	/// After being called, the client or service sends a magic cookie message within
	/// each data packet it transfers to the remote side.
	virtual void enable_magic_cookies() = 0;
	/// After being called, the client or service doen not send magic cookie messages
	/// anymore.
	virtual void disable_magic_cookies() = 0;
};

} // namespace vsomeip

#endif /* VSOMEIP_PARTICIPANT_HPP */

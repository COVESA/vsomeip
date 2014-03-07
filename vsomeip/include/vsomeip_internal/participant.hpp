//
// participant.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PARTICIPANT_HPP
#define VSOMEIP_INTERNAL_PARTICIPANT_HPP

#include <vsomeip_internal/constants.hpp>

#ifdef USE_VSOMEIP_STATISTICS
#include <vsomeip_internal/statistics_owner.hpp>
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
	virtual ~participant() {};

	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void register_for(service_id _service_id, instance_id _instance_id) = 0;
	virtual void unregister_for(service_id _service_id, method_id _method_id) = 0;

	virtual void enable_magic_cookies() = 0;
	virtual void disable_magic_cookies() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_PARTICIPANT_HPP

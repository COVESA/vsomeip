//
// client.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_CLIENT_HPP
#define VSOMEIP_CLIENT_HPP

#ifdef USE_VSOMEIP_STATISTICS
#include <vsomeip/statistics_owner.hpp>
#endif

namespace vsomeip {

class message;
class endpoint;

class receiver;

class client
#ifdef USE_VSOMEIP_STATISTICS
: virtual public statistics_owner
#endif
{
public:
	virtual ~client() {};

	virtual void connect() = 0;
	virtual void disconnect() = 0;

	virtual void send(const message &_message, bool _flush = true) = 0;

	virtual void register_receiver(receiver *_receiver) = 0;
	virtual void unregister_receiver(receiver *_receiver) = 0;

	virtual size_t poll_one() = 0;
	virtual size_t poll() = 0;
	virtual size_t run() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_HPP

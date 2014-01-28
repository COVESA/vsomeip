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

class service
		: virtual public participant {
public:
	virtual bool send(const message_base *_message, bool _flush = true) = 0;
	virtual bool send(const uint8_t *_data, uint32_t _size,
						endpoint *_target, bool _flush = true) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICE_HPP

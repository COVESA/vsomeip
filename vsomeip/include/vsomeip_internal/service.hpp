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

#ifndef VSOMEIP_INTERNAL_SERVICE_HPP
#define VSOMEIP_INTERNAL_SERVICE_HPP

#include <vsomeip_internal/participant.hpp>

namespace vsomeip {

class endpoint;

class service
		: virtual public participant {
public:
	virtual ~service() {};

	virtual bool send(const uint8_t *_data, uint32_t _size,
						endpoint *_target, bool _flush = true) = 0;
	virtual bool flush(endpoint *_target = 0) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SERVICE_HPP

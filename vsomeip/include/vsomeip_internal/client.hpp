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

#ifndef VSOMEIP_INTERNAL_CLIENT_HPP
#define VSOMEIP_INTERNAL_CLIENT_HPP

#include <vsomeip_internal/participant.hpp>

namespace vsomeip {

class client
		: virtual public participant {
public:
	virtual ~client() {};

	virtual bool send(
			const uint8_t *_data, uint32_t _size, bool _flush = true) = 0;
	virtual bool flush() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_CLIENT_HPP

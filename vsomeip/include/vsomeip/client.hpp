//
// client.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_CLIENT_HPP
#define VSOMEIP_CLIENT_HPP

#include <vsomeip/participant.hpp>

namespace vsomeip {

class client
		: virtual public participant {
public:
	virtual bool send(const message_base *_message, bool _flush = true) = 0;
	virtual bool send(const uint8_t *_data, uint32_t _size,
						bool _flush = true) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_HPP

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

#include <cstddef>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/client_base.hpp>

namespace vsomeip {

class consumer;
class endpoint;
class provider;

class client
	: virtual public client_base {
public:
	virtual ~client() {};

	virtual consumer * create_consumer(const endpoint *_target) = 0;
	virtual provider * create_provider(const endpoint *_source) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_HPP

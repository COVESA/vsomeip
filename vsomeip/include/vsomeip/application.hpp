//
// application.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_APPLICATION_HPP
#define VSOMEIP_APPLICATION_HPP

#include <cstddef>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class client;
class endpoint;
class service;

class application {
public:
	virtual ~application() {};

	virtual client * create_client(const endpoint *_target) = 0;
	virtual service * create_service(const endpoint *_source) = 0;

	virtual std::size_t poll_one() = 0;
	virtual std::size_t poll() = 0;
	virtual std::size_t run() = 0;
};

} // namespace vsomeip

#endif

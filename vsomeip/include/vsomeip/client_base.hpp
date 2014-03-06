//
// client_base.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_CLIENT_BASE_HPP
#define VSOMEIP_CLIENT_BASE_HPP

#include <cstddef>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class client_base {
public:
	virtual ~client_base() {};

	virtual std::size_t poll_one() = 0;
	virtual std::size_t poll() = 0;
	virtual std::size_t run() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_BASE_HPP

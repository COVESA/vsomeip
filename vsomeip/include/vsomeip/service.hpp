//
// service.hpp
//
// Date: 	Jan 15, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_HPP
#define VSOMEIP_SERVICE_HPP

#include <cstddef>

namespace vsomeip {

class service {
public:
	virtual ~service() {};

	virtual void start() = 0;
	virtual void stop() = 0;

	virtual std::size_t poll_one() = 0;
	virtual std::size_t poll() = 0;
	virtual std::size_t run() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICE_HPP

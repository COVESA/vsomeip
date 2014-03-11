//
// factory.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_FACTORY_HPP
#define VSOMEIP_FACTORY_HPP

#include <string>

namespace vsomeip {

class application;

class factory {
public:
	virtual ~factory() {};

	static factory * get_instance();
	virtual application * create_application(
								const std::string& _name) const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_FACTORY_HPP

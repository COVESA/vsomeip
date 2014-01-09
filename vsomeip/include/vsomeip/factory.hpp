//
// factory.hpp
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_FACTORY_HPP
#define VSOMEIP_FACTORY_HPP

namespace vsomeip {

class message;

class factory {
public:
	virtual ~factory() {};
	static factory * get_default_factory();

	virtual message * create_message() const = 0;
};

}; // namespace vsomeip

#endif // VSOMEIP_FACTORY_HPP

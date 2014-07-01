//
// payload_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PAYLOAD_OWNER_HPP
#define VSOMEIP_INTERNAL_PAYLOAD_OWNER_HPP

namespace vsomeip {

class payload_owner {
public:
	virtual ~payload_owner() {};

	virtual void notify() const = 0;
};

}

#endif // VSOMEIP_INTERNAL_PAYLOAD_OWNER_HPP

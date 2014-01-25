//
// statistics_owner.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2024 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_STATISTICS_OWNER_HPP
#define VSOMEIP_STATISTICS_OWNER_HPP

namespace vsomeip {

class statistics_owner {
public:
	virtual ~statistics_owner() {};

	virtual const class statistics * get_statistics() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_STATISTICS_OWNER_HPP

//
//	owner_base.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_OWNER_BASE_HPP
#define VSOMEIP_INTERNAL_OWNER_BASE_HPP

#include <string>

namespace vsomeip {

class owner_base {
public:
	owner_base(const std::string &_name);
	virtual ~owner_base();

protected:
	std::string name_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_OWNER_BASE_HPP

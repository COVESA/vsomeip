//
// owner_base.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/owner_base.hpp>

namespace vsomeip {

owner_base::owner_base(const std::string &_name)
	: name_(_name) {
}

owner_base::~owner_base() {
}

} // namespace vsomeip





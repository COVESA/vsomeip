//
// proxy_owner.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/proxy_owner.hpp>

namespace vsomeip {

proxy_owner::proxy_owner(const std::string &_name)
	: owner_base(_name),
	  proxy_(0) {
}

proxy_owner::~proxy_owner() {
}

} // namespace vsomeip






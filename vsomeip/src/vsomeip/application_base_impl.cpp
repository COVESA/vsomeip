//
// application_base_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/application_base_impl.hpp>

namespace vsomeip {

application_base_impl::application_base_impl(const std::string &_name)
	: log_owner(_name),
	  proxy_owner(_name),
	  owner_base(_name) {
}

application_base_impl::~application_base_impl() {
}

} // namespace vsomeip
